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

  NOTE: do not mix-use evt_create(e) and evt_split (eg). Only one
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

static unsigned long long meas_start, meas_end;
static int meas_flag = 0;

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

/* recovery data structure for evt */
struct rec_data_evt {
	spdid_t       spdid;
	long          c_evtid;
	long          s_evtid;

	long          p_evtid;  // parent event id (eg needs this)
	int           grp;      // same as above

	struct rec_data_evt *next, *prev;  // track all init events in a group

	unsigned int  state;
	unsigned long fcnt;
};
/* Assumption: a component has at most one group, all init events are
 * splited from the same component */
struct rec_data_evt *evts_grp_list_head;

/* the state of an event object (each operation expects to change) */
enum {
	EVT_STATE_CREATE,
	EVT_STATE_SPLIT,           // same as create, except it is for event group
	EVT_STATE_SPLIT_PARENT,    // split from a parent event id
	EVT_STATE_WAITING,
	EVT_STATE_TRIGGER,
	EVT_STATE_FREE
};

/**********************************************/
/* slab allocaevt and cvect for tracking evts */
/**********************************************/

CVECT_CREATE_STATIC(rec_evt_map);   // track each event object and its id (create/free/wait)
CSLAB_CREATE(rdevt, sizeof(struct rec_data_evt));

static struct rec_data_evt *
rdevt_lookup(int id)
{ 
	return (struct rec_data_evt *)cvect_lookup(&rec_evt_map, id); 
}

static struct rec_data_evt *
rdevt_alloc(int id)
{
	struct rec_data_evt *rd = NULL;
	
	assert(id >= 1);
	
	rd = cslab_alloc_rdevt();
	assert(rd);	
	cvect_add(&rec_evt_map, rd, id);   // this must be greater than 1

	INIT_LIST(rd, next, prev);

	return rd;
}

static void
rdevt_dealloc(struct rec_data_evt *rd)
{
	assert(rd);
	if (cvect_del(&rec_evt_map, rd->c_evtid)) BUG();

	REM_LIST(rd, next, prev);  // remove tracked event from a group

	cslab_free_rdevt(rd);
	return;
}

static void 
rd_cons(struct rec_data_evt *rd, spdid_t spdid, long c_evtid, 
	long s_evtid, long parent, int grp, int state)
{
	assert(rd);
	
	rd->spdid	 = spdid;
	rd->c_evtid	 = c_evtid;
	rd->s_evtid	 = s_evtid;
	rd->p_evtid	 = parent;
	rd->grp	         = grp;

	rd->state	 = state;
	rd->fcnt	 = fcounter;

	/* If there is parent and group, track here */
	if (parent == 0 && grp == 1) {   // parent evt
		evts_grp_list_head = rd;
	} else if (parent > 0 && !grp) {     // child evt
		assert(evts_grp_list_head->next);
		ADD_LIST(evts_grp_list_head, rd, next, prev);
	}

	return;
}


/* This is ugly: the recovery of init events in a group (e.g.,
 * connection manager) 
*/
static void
rd_recover_state(struct rec_data_evt *rd)
{
	struct rec_data_evt *prd = NULL, *tmp = NULL;
	long tmp_evtid;

	assert(rd && rd->c_evtid >= 1);
	
	printc("calling recover (thd %d)!!!!!!\n", cos_get_thd_id());

	if (!rd->p_evtid) {
		assert(!rd->grp || rd->grp == 1);
		printc("has no parent\n");
		tmp_evtid = c3_evt_split(cos_spd_id(), rd->p_evtid, 
					 rd->grp, rd->c_evtid);
		assert(tmp_evtid >= 1);
		rd->s_evtid = tmp_evtid;
		printc("re-split an s_evt %d\n", rd->s_evtid);
		printc("re-split an c_evt %d\n", rd->c_evtid);
		
		/* rebuild all child event here */
		if (rd->grp == 1) {
			assert(rd == evts_grp_list_head);
			for (tmp = FIRST_LIST(rd, next, prev) ;
			     tmp != rd;
			     tmp = FIRST_LIST(tmp, next, prev)) {
                                /* a child update its new re-created parent id */
				tmp->p_evtid = rd->s_evtid;
                                /* only re-create this child once */
				tmp->fcnt    = fcounter; 
				/* printc("\n>>found an evt %d (thd %d)\n",  */
				/*        tmp->c_evtid, cos_get_thd_id()); */
				/* printc("its parent evt %d\n", tmp->p_evtid); */
				tmp_evtid = c3_evt_split(cos_spd_id(), tmp->p_evtid,
							 tmp->grp, tmp->c_evtid);
				assert(tmp_evtid >= 1);
				tmp->s_evtid = tmp_evtid;
				/* printc("c3_tsplit new evtid %d\n", tmp->s_evtid); */
			}
		}
	} else {
		prd = rdevt_lookup(rd->c_evtid);
                /* assume parent event is in the same component and
		 * only one level */
		assert(prd);  
		rd_recover_state(prd);
	}
	
	return;
}

static struct rec_data_evt *
rd_update(int evtid, int state)
{
        struct rec_data_evt *rd = NULL;

	/* TAKE(cos_spd_id()); */

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
	
	/* STATE MACHINE */
	switch (state) {
	case EVT_STATE_CREATE:
                /* for create, if failed, just redo, should not be here */
		assert(0);  
		break;
	case EVT_STATE_SPLIT_PARENT:
	case EVT_STATE_SPLIT:
                /* for split, if failed, need restore any parent */
	case EVT_STATE_FREE:
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
	case EVT_STATE_WAITING:
		/* preempted by other thread here? This only becomes
		 * issue if the preemption occurs when the same object
		 * is being accessed. However, there are 2 situation:
		 * if the preempting thread is in the same component,
		 * they should wait since lock is taken. If the
		 * preemption thread is in the different component,
		 * name server look up will only see the last added
		 * entry, so it does not matter. Do we still need lock
		 * here???? Maybe not....*/
		printc("rd_recovery is in state %d\n", state);
		rd_recover_state(rd);
		break;
	case EVT_STATE_TRIGGER:
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
		
		/* No state here. We will look up the actual_evtid on the
		 * server side. Should not be here */
		/* rd_recover_state(rd); */
		assert(0);
		break;
	default:
		assert(0);
		break;
	}

	/* if (cos_get_thd_id() == 13) { */
	/* 	printc("evt rec: thread 13 is waking up thread 10\n"); */
	/* 	sched_wakeup(cos_spd_id(), 10);  // test */
	/* } */

done:	
	/* RELEASE(cos_spd_id());   // have to release the lock before block somewhere above */
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
        long ser_eid, cli_eid;
redo:
        /* printc("evt cli: evt_create %d\n", cos_get_thd_id()); */

#ifdef BENCHMARK_MEAS_CREATE
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		printc("see a fault during evt_create (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
#ifdef BENCHMARK_MEAS_CREATE
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		

		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	assert(ret > 0);
	/* printc("cli: evt_create create a new rd in spd %ld (server id %d)\n",  */
	/*        cos_spd_id(), ret); */
	rd = rdevt_alloc(ret);
	assert(rd);
	rd_cons(rd, cos_spd_id(), ret, ret, 0, 0, EVT_STATE_CREATE);
	
	return ret;
}

// no tracking here for create a new server side evt
CSTUB_FN(long, c3_evt_split) (struct usr_inv_cap *uc,
			      spdid_t spdid, long parent_evt, int grp, int old_evtid)
{
	long fault = 0;
	long ret;
redo:
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, parent_evt, grp, old_evtid);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	/* printc("evt cli: c3_evt_split returns a new evtid %d for old evtid %d\n",  */
	/*        ret, old_evtid); */
	return ret;
}


static int first = 0;

CSTUB_FN(long, evt_split) (struct usr_inv_cap *uc,
			   spdid_t spdid, long parent_evt, int grp)
{
	long fault = 0;
	long ret;

        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_parent = NULL;
        long ser_eid, cli_eid;
	long p_evt = parent_evt;

        if (first == 0) {
		cvect_init_static(&rec_evt_map);
		first = 1;
	}
redo:
        /* printc("evt cli: evt_split %d\n", cos_get_thd_id()); */
	if (parent_evt && !grp) {
		/* printc("evt cli: evt_split found parent evt %d\n", p_evt); */
		rd_parent = rd_update(parent_evt, EVT_STATE_SPLIT_PARENT);
		assert(rd_parent);
		p_evt = rd_parent->s_evtid;
	}

#ifdef BENCHMARK_MEAS_SPLIT
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, p_evt, grp);
	if (unlikely (fault)){
		printc("see a fault during evt_split (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
#ifdef BENCHMARK_MEAS_SPLIT
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		

		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	assert(ret > 0);
	/* printc("cli: evt_create create a new rd in spd %ld (server id %d)\n",  */
	/*        cos_spd_id(), ret); */
	rd = rdevt_alloc(ret);
	assert(rd);
	rd_cons(rd, cos_spd_id(), ret, ret, p_evt, grp, EVT_STATE_CREATE);

	return ret;
}

CSTUB_FN(long, evt_wait) (struct usr_inv_cap *uc,
			  spdid_t spdid, long extern_evt)
{
	long fault = 0;
	long ret;

        struct rec_data_evt *rd = NULL;
redo:
        // assume always in the same component as evt_split
        rd = rd_update(extern_evt, EVT_STATE_WAITING);
	assert(rd);   
	/* printc("evt cli: evt_wait thd %d (extern_evt %d rd->evt id %ld)\n", */
	/*        cos_get_thd_id(), extern_evt, rd->s_evtid); */

#ifdef BENCHMARK_MEAS_WAIT
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_evtid);
        if (unlikely (fault)){
		printc("(ret %d)see a fault during evt_wait (thd %d in spd %ld)\n",
		       ret, cos_get_thd_id(), cos_spd_id());
#ifdef BENCHMARK_MEAS_WAIT
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		
		CSTUB_FAULT_UPDATE();

		/* /\* If we are TCP timer, we need return and call */
		/*  * tcp_tmr to send ACK and queued data *\/ */
		/* if (cos_get_thd_id() == 12) return -22;   // test */

		goto redo;
        }

	assert(rd && extern_evt == rd->c_evtid);
	printc("evt:wait: cli: ret %d passed in extern_evt %d (thd %d)\n",
	       ret, extern_evt, cos_get_thd_id());

	return ret;
}


/* Note: this can be called from a different component, we need update
 * the tracking info here */
CSTUB_FN(int, evt_trigger) (struct usr_inv_cap *uc,
			    spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret, ser_evtid;
        struct rec_data_evt *rd = NULL;

redo:
	/* printc("evt cli: evt_trigger thd %d (evt id %ld)\n", cos_get_thd_id(), extern_evt); */
#ifdef BENCHMARK_MEAS_TRIGGER
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely(fault)){
		printc("see a fault during evt_trigger (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
#ifdef BENCHMARK_MEAS_TRIGGER
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		

		CSTUB_FAULT_UPDATE();
                /*
o		  1) fault occurs in evt_trigger (before
		  sched_wakeup).In this case, reflection will wake up
		  threads in blocking wait and replay. Then new event
		  will be created for evt_id. Replayed evt_trigger on
		  evt_id should see the new id.

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
		goto redo;
	}

	/* printc("evt cli: evt_trigger thd %d (ret %d)\n", cos_get_thd_id(), ret);	 */
	return ret;
}

CSTUB_FN(int, evt_free) (struct usr_inv_cap *uc,
			 spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;
redo:
	/* printc("evt cli: evt_free %d (evt id %ld)\n", cos_get_thd_id(), extern_evt); */
        rd = rd_update(extern_evt, EVT_STATE_FREE);
	assert(rd);
	assert(rd->c_evtid == extern_evt);

#ifdef BENCHMARK_MEAS_FREE
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_evtid);
	if (unlikely (fault)) {
		printc("see a fault during evt_free (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
#ifdef BENCHMARK_MEAS_FREE
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	rdevt_dealloc(rd);

	return ret;
}
