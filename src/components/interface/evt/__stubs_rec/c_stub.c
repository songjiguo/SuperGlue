/* Event recovery client stub interface. The interface tracking code
 is supposed to work for both group and non-group events 

 Issue: 
 1. virtual page allocated in malloc is increasing always (FIX later)
 2. thread woken up through reflection does not invoke trigger to update the state
     (add evt_update_status api to evt, but when to call this?)
     
  This interface is different from lock/sched/mm... since an event can
  be triggered from a completely different component. create, wait and
  free have to be done by the same component/interface, but trigger
  can be from the same component or a different one. See evt_trigger 

!!!! Here is an issue: if the event is triggered by a different thread
     in a different component, how do we ensure that next time,
     evt_trigger will pass the correct server side id to evt manager?
     
     Solution 1: a separate spd that maintains the mapping of client
     and server id. Need call this spd every time .... not good

     Solution 2: in the place where evt is created, ensure only the
     correct server side id is passed ... need check all places that
     call evt_create and those cached evt id ... not good

     Solution 3: track the client id and server id over the server
     side interface. After the fault (all previous records will be
     gone, but we only care about newly created server id), ensure
     that a new evt id is created first and tracked/updated at the
     server side. (this should be ensure since only lower thd triggers
     the higher thd). Also need pass both cli and ser id to track.
     Solution 3 seems a general solution for the case that there are
     client and server id, and the same object state can be changed
     over different client interfaces.  -- > see s_cstub.c
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
#define TAKE(spdid) 	do { if (sched_component_take(spdid))    return; } while (0)
#define RELEASE(spdid)	do { if (sched_component_release(spdid)) return; } while (0)

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

/* recovery data structure evt service */
struct rec_data_evt {
	spdid_t spdid;
	unsigned long c_evtid;

	unsigned int state;
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
	if (cvect_del(&rec_evt_vect, rd->c_evtid)) BUG();
	cslab_free_rdevt(rd);
}

static void 
rd_cons(struct rec_data_evt *rd, spdid_t spdid, unsigned long evtid, int state)
{
	assert(rd);

	rd->spdid	 = spdid;
	rd->c_evtid	 = evtid;
	rd->state	 = state;
	rd->fcnt	 = fcounter;

	return;
}

#ifdef REFLECTION
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

static void
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
	return;
}
#endif

static struct rec_data_evt *
update_rd(int evtid, int state)
{
        struct rec_data_evt *rd = NULL;

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

	 * Note: torrent is different. We can not just create a new
	 * object in the current component since each torrent is
	 * associated with a file (e.g, "path"). And event does not.
	 */
	int new_evtid;
	switch (state) {
	case EVT_CREATED:
                /* for create, if failed, just redo, should not be here */
		assert(0);  
		break;
	case EVT_FREED:
                /* here is one issue: if fault happens in evt_free
		 * (after delid in mapping_free in evt manager), the
		 * original id will be removed, then when replay
		 * evt_free, how to remove the re-created id? We can
		 * check if the entry is still presented in
		 * name_space. If not, which means that fault must
		 * happens after all related ids have been removed No
		 * need to replay. Otherwise, we can just create a new
		 * event and replay evt_free. This is basically a
		 * reflection on name_server
		 */
		// if the entry has been removed, return NULL
		if (ns_reflection(cos_spd_id(), evtid)) {
			assert(rd);
			rdevt_dealloc(rd);
			return NULL;
		}
		// otherwise, create a new one and remove all on replay
	case EVT_WAITING:
		new_evtid = __evt_create(cos_spd_id());
		assert(new_evtid > 0);
		printc("in update_rd (state %d): creating a new evt (%d)\n", 
		       state, new_evtid);
		assert(!evt_updateid(cos_spd_id(), evtid, new_evtid));
		break;
	case EVT_TRIGGERED:
		/* event should be in wait state already (after
		 * reflection on scheduler) */
		break;
	default:
		break;
	}

done:	
	return rd;
}

// only used to get a new server side id, no tracking
CSTUB_FN(unsigned long, __evt_create) (struct usr_inv_cap *uc,
				       spdid_t spdid)
{
	long fault = 0;
	unsigned long ret;
redo:
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		assert(0);  // assume fault does not occur during the recover
		goto redo;
	}
	
	return ret;
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
		printc("evt cli: goto redo\n");
		goto redo;
	}
	
	assert(ret > 0);
	assert(!evt_updateid(cos_spd_id(), ret, ret));

	printc("cli: evt_create create a new rd in spd %ld\n",
	       cos_spd_id());		
	rd = rdevt_alloc(ret);
	assert(rd);	
	rd_cons(rd, cos_spd_id(), ret, EVT_CREATED);

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
        rd = update_rd(extern_evt, EVT_WAITING);
	if (unlikely(!rd)) {
		printc("cli: evt_wait create a new rd in spd %ld\n",
		       cos_spd_id());
		rd = rdevt_alloc(extern_evt);
		assert(rd);	
		rd_cons(rd, cos_spd_id(), extern_evt, EVT_WAITING);
	}
	assert(rd);
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
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
        rd = update_rd(extern_evt, EVT_TRIGGERED);
	if (unlikely(!rd)) {
		printc("cli: evt_trigger create a new rd in spd %ld\n",
		       cos_spd_id());
		rd = rdevt_alloc(extern_evt);
		assert(rd);	
		rd_cons(rd, cos_spd_id(), extern_evt, EVT_TRIGGERED);
	}
	assert(rd);

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	return ret;
}

CSTUB_FN(int, evt_free) (struct usr_inv_cap *uc,
			 spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;
redo:
	printc("evt cli: evt_free %d (evt id %ld)\n", cos_get_thd_id(), extern_evt);
        rd = update_rd(extern_evt, EVT_FREED);
        /* Here !rd does not mean rd does not exist. Actually it has
	 * to exist in the same component. In evt_free, !rd means that
	 * evt has been removed from name server and evt manager
	 * before the fault occurs in evt_free 
	 
	 Note: rd must be deallocated in update_rd in this case */
	
	if (!rd) {
		printc("evt cli: in evt_free, id %ld has been removed\n", extern_evt);
		return ret; 
	}
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely (fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	rdevt_dealloc(rd);
	return ret;
}
