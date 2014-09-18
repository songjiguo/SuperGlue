/*
  Event c^3 client stub interface. The interface tracking code
  is supposed to work for both group and non-group events 
  
  Issue:
  1. virtual page allocated in malloc is increasing always (FIX later)
  2. thread woken up through reflection does not invoke trigger to update the state
  (add evt_update_status api to evt, but when to call this?)
  
  This interface is different from lock/sched/mm... since an event
  can be triggered from a completely different component. create,
  wait and free have to be done by the same component/interface, but
  trigger can be from the same component or a different one. If the
  event is triggered by a different thread in a different component,
  how do we ensure that next time, evt_trigger will pass the correct
  server side id to evt manager?  Solution:a separate spd that
  maintains the mapping of client and server id. Need call this spd
  every time ....overhead, but a reasonable solution for now
  
  Important --- Only event manager and cbuf manager are components
  that can have global name space issue. For torrent interface, we
  restrict that the operation must be in the same component in which
  the torrent is created (they do not need name server!)
*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <objtype.h>
#include <sched.h>

#include <evt.h>
#include <cstub.h>

#include <name_server.h>

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
#define TAKE(spdid) 	do { if (sched_component_take(spdid))    return 0; } while (0)
#define RELEASE(spdid)	do { if (sched_component_release(spdid)) return 0; } while (0)

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;
static long ser_fcounter;  // mainly for the evt_trigger (see below explanation)

/* recovery data structure evt service */
struct rec_data_evt {
	spdid_t       spdid;
	unsigned long evtid;
	unsigned int  state;
	unsigned long fcnt;
};

/* the state of an event obect (each operation expects to change) */
enum {
	EVT_CREATED,
	EVT_WAITING,
	EVT_TRIGGERED,
	EVT_FREED
};

/**********************************************/
/* slab allocaevt and cvect for tracking evts */
/**********************************************/

CVECT_CREATE_STATIC(rec_evt_vect);
CSLAB_CREATE(rdevt, sizeof(struct rec_data_evt));

static struct rec_data_evt *
rdevt_lookup(int id)
{ 
	return cvect_lookup(&rec_evt_vect, id); 
}

static struct rec_data_evt *
rdevt_alloc(int evtid)
{
	struct rec_data_evt *rd;

	rd = cslab_alloc_rdevt();
	assert(rd);
	if (cvect_add(&rec_evt_vect, rd, evtid)) {
		printc("can not add into cvect\n");
		BUG();
	}
	return rd;
}

static void
rdevt_dealloc(struct rec_data_evt *rd)
{
	assert(rd);
	if (cvect_del(&rec_evt_vect, rd->evtid)) BUG();
	cslab_free_rdevt(rd);
}

static void 
rd_cons(struct rec_data_evt *rd, spdid_t spdid, unsigned long evtid, int state)
{
	assert(rd);

	rd->spdid	 = spdid;
	rd->evtid	 = evtid;
	rd->state	 = state;
	rd->fcnt	 = fcounter;

	return;
}

static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}

extern int sched_reflect(spdid_t spdid, int src_spd, int cnt);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern vaddr_t mman_reflect(spdid_t spd, int src_spd, int cnt);
extern int mman_release_page(spdid_t spd, vaddr_t addr, int flags); 

static int
rd_reflection(int cap)
{
	assert(cap);

	TAKE(cos_spd_id());

	int count_obj = 0; // reflected objects
	int dest_spd = cap_to_dest(cap);
	
	// remove the mapped page for evt spd
	vaddr_t addr;
	count_obj = mman_reflect(cos_spd_id(), dest_spd, 1);
	/* printc("evt relfects on mmgr: %d objs\n", count_obj); */
	while (count_obj--) {
		addr = mman_reflect(cos_spd_id(), dest_spd, 0);
		/* printc("evt mman_release: %p addr\n", (void *)addr); */
		mman_release_page(cos_spd_id(), addr, dest_spd);
	}

	// to reflect all threads blocked from evt component
	int wake_thd;
	count_obj = sched_reflect(cos_spd_id(), dest_spd, 1);
	/* printc("evt relfects on sched: %d objs\n", count_obj); */
	while (count_obj--) {
		wake_thd = sched_reflect(cos_spd_id(), dest_spd, 0);
		/* printc("wake_thd %d\n", wake_thd); */
		/* evt_trigger(cos_spd_id(), evt_id); */  // pointless to call evt mgr
		sched_wakeup(cos_spd_id(), wake_thd);
	}

	RELEASE(cos_spd_id());
	/* printc("evt reflection done (thd %d)\n\n", cos_get_thd_id()); */
	return 0;
}

/* extern int ns_update(spdid_t spdid, int old_id, int curr_id, long par); */
/* extern long ns_reflection(spdid_t spdid, int id, int type); */

static struct rec_data_evt *
rd_update(int evtid, int state)
{
        struct rec_data_evt *rd = NULL;

	TAKE(cos_spd_id());

        rd = rdevt_lookup(evtid);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;

	rd->fcnt = fcounter;

	/* Jiguo: if evt is created in a different component, do we
	 * need switch to recovery thread and upcall to the creator? I
	 * think we can just recreate a new event within in this
	 * component, name_server will be updated anyway. The original
	 * creator component will still see the old cli_id and the new
	 * one will be cached in evt between faults.  
	 
	 * However, based on the state machine, if this is
	 * evt_trigger, then the event should be in wait state
	 * first. If this is evt_wait, then the event should be in
	 * created state. (e.g, trigger sees a fault has occurred
	 * before, if the event is already in the waiting state (say
	 * after replay evt_wait, then no need to )
	 */
	switch (state) {
	case EVT_CREATED:
                /* for create, if failed, just redo, should not be here */
		assert(0);  
		break;
	case EVT_FREED:
                /* here is one issue: if fault happens in evt_free
		 * (after delid in mapping_free in evt manager), the
		 * original id will be removed in the name server,
		 * then when replay evt_free, how to remove the
		 * re-created id? We can check if the entry is still
		 * presented in name_space. If not, which means that
		 * fault must happens after all related ids have been
		 * removed No need to replay. Otherwise, we can just
		 * create a new event and replay evt_free. This is
		 * basically a reflection on name_server
		 */
		/* // if the entry has been removed, return NULL */
		/* if (ns_reflection(cos_spd_id(), evtid, 0)) { */
		/* 	assert(rd); */
		/* 	rdevt_dealloc(rd); */
		/* 	return NULL; */
		/* } */
		break;
		// otherwise, create a new one and remove all on replay
	case EVT_WAITING:
		printc("in rd_update (state %d) is creating a new evt by thd %d\n", 
		       state, cos_get_thd_id());
		RELEASE(cos_spd_id());
		assert(evt_re_create(cos_spd_id(), evtid) > 0);
		TAKE(cos_spd_id());
		/* preempted by other thread here? This only becomes
		 * issue if the preemption occurs when the same object
		 * is being accessed. However, there are 2 situation:
		 * if the preempting thread is in the same component,
		 * they should wait since lock is taken. If the
		 * preemption thread is in the different component,
		 * name server look up will only see the last added
		 * entry, so it does not matter. Do we still need lock
		 * here???? Maybe not....*/
		/* assert(!ns_update(cos_spd_id(), evtid, new_evtid, ser_fcounter)); */
		break;
	case EVT_TRIGGERED:
		/* Generally there are two rules to follow when
		   recover an object state:
		   A) follow the protocol of service
		   B) follow the priority of threads
		   
		   In the evt_trigger case, this needs to be careful
		   because two reasons: 1) to follow the protocol, an
		   event should be in wait state before evt_trigger is
		   invoked (e.g, assert(n_received <= n_wait)). 2)
		   to follow threads priorities, if the triggering
		   thread is at higher priority, then reflection on
		   scheduler might not be able to put the waiting
		   thread back to the wait state. So we can not
		   account on that the woken up thread from reflection
		   will definitely create the new event before us.

		   Solution: For simplicity now, always assume the
		   evt_trigger must follow rule A), not necessary to
		   rule B). Otherwise, we either need switch to other
		   thread to get to wait state first, or we need to
		   ignore the event. Or we can think this should be
		   ensured by state machine.

		   However, in evt_trigger thd A calls sched_wakeup to
		   wake up thd B that wait-blocked on the event. Say
		   if B is woken up, then later calls evt_free to free
		   the event. Then switch back A and if the fault
		   occurs at this point (after sched_wakeup), then the
		   event has been removed. Therefore, we need check if
		   the event is already removed. But the event with
		   the same id might be just created again (say,
		   evt_create -> wevt_wait -> evt_free is a loop).

		   Solution: check if the event is has been
		   freed. However, we can also need check if the event
		   is the same one after the fault occurs (this can
		   happen if evt_trigger triggers the new event with
		   the same id, for now we just assume it is ok with
		   protocol). One solution is to use server side fault
		   counter to differentiate the event with the same
		   id. Or we can update name server after sched_wakeup
		*/
		break;
	default:
		break;
	}

done:	
	RELEASE(cos_spd_id());   // have to release the lock before block somewhere above
	return rd;
}


/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN(long, evt_create) (struct usr_inv_cap *uc,
			    spdid_t spdid)
{
	long fault = 0;
	long ret;

        struct rec_data_evt *rd = NULL;
        unsigned long ser_eid, cli_eid;
redo:
        printc("evt cli: evt_create %d\n", cos_get_thd_id());
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		ser_fcounter = fault_update;
		printc("evt cli: goto redo\n");
		goto redo;
	}
	
	assert(ret > 0);
	/* assert(!ns_update(cos_spd_id(), ret, ret, ser_fcounter)); */

	printc("cli: evt_create create a new rd in spd %ld\n", cos_spd_id());		
	rd = rdevt_alloc(ret);
	assert(rd);	
	rd_cons(rd, cos_spd_id(), ret, EVT_CREATED);

	return ret;
}


/* This function is used to create a new server side id for the
 * old_extern_evt, and do the cache in the server. Here since
 * ev_trigger can come from a different component, so we cache in the
 * server side.

 * More generally if we keep the mapping between old id and new id in
 * the server (say, by caching), we need this function and we do not
 * need to track the mapping on the client side. However, if we keep
 * the mappings at client interface, we do not need this function
 * since the client can find the correct server id. 

 NO tracking here!!!! If failed, just replay
 */
CSTUB_FN(long, evt_re_create) (struct usr_inv_cap *uc,
			  spdid_t spdid, long old_extern_evt)
{
	long fault = 0;
	long ret;
redo:
	printc("evt cli: evt_re_create %d (evt id %ld)\n", cos_get_thd_id(), old_extern_evt);

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, old_extern_evt);
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		ser_fcounter = fault_update;
		goto redo;
        }

	return ret;
}

CSTUB_FN(long, evt_split) (struct usr_inv_cap *uc,
			   spdid_t spdid, long parent_evt, int grp)
{
	long fault = 0;
	long ret;
	
        struct rec_data_evt *rd = NULL;
redo:

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, parent_evt, grp);
	
	if (unlikely (fault)){
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		fcounter++;
		goto redo;
	}
	
	return ret;
}


CSTUB_FN(long, evt_wait) (struct usr_inv_cap *uc,
			  spdid_t spdid, long extern_evt)
{
	long fault = 0;
	long ret;

        struct rec_data_evt *rd = NULL;
redo:
	printc("evt cli: evt_wait %d (evt id %ld)\n", cos_get_thd_id(), extern_evt);
        // must be in the same component
        rd = rd_update(extern_evt, EVT_WAITING);
	assert(rd);   

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		ser_fcounter = fault_update;
		goto redo;
        }

	return ret;
}

CSTUB_FN(int, evt_trigger) (struct usr_inv_cap *uc,
			    spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;
        struct rec_data_evt *rd = NULL;
redo:
	printc("evt cli: evt_trigger %d (evt id %ld)\n", cos_get_thd_id(), extern_evt);
	rd = rd_update(extern_evt, EVT_TRIGGERED);

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		ser_fcounter = fault_update;
                /*
		  1) fault occurs in evt_trigger (before
		  sched_wakeup).In this case, reflection will wake up
		  threads in blocking wait and replay. Then new event
		  will be created for evt_id. Replayed evt_trigger on
		  evt_id will see the new id.

		  2) fault occurs in evt_trigger (after sched_wakeup).
		  However, in this case eve_id might be already freed
		  (e.g, in a create-wait-free loop) since the thread
		  has been triggered correctly and woken up. So no need
		  to trigger again. (otherwise, might trigger a
		  different id)

		  3) fault occurred while other thread in evt (say
		  evt_wait), then fault_notif make us realize the
		  fault when we call evt_trigger or return from
		  scheduler (e.g, called evt_trigger previously and
		  wake up other threads in scheduler). Reflection will
		  force the other thread to re-create the event and
		  wait. Then t1 should be able to trigger. However, if
		  entry is removed in evt by other thread already,
		  when return back from scheduler, we reflect evt to
		  see if need redo

		*/
		printc("decide if going to redo for evt_trigger here\n");
		if (evt_reflection(cos_spd_id(), extern_evt)) goto redo; // 1) and 3)
		else rd = rd_update(extern_evt, EVT_TRIGGERED);          // 2) and 3)
	}

	return ret;
}

CSTUB_FN(int, evt_free) (struct usr_inv_cap *uc,
			 spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;
	printc("evt cli: evt_free %d (evt id %ld)\n", cos_get_thd_id(), extern_evt);
        rd = rd_update(extern_evt, EVT_FREED);
	assert(rd);
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely (fault)) {
		CSTUB_FAULT_UPDATE();
		ser_fcounter = fault_update;
	}

	rdevt_dealloc(rd);

	return ret;
}
