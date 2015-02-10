/* Jiguo: Event c^3 client stub interface. The interface tracking code
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

   The recovery for event manager is quite complicated due to the fact
   that evt_trigger is hard to track (could be in different component
   and triggered by different thread). The recovery should be both
   efficient and general, and need to avoid waking up the timing
   thread at improper time.

   Possible solutions and issues:

   1) We can track where each evt_trigger is made from and the
   time_stamp over the evt_trigger client interface. After fault, the
   recovery thread can upcall into these components and push the
   time-stamp to evt_manager to help recovery (along with evt_wait
   time_stamp and fault time_stamp.) For example, if the evt_trigger
   time_stamp is less than fault time_stamp, we do the evt_trigger.
   
   Issue: overhead (track which spd does evt_trigger and time_stamp)

   2) We can check the return value of evt_wait and decide if it is
   woken up due to the recovery or the normal path. Need check both
   return value and current condition to decide.

   Issue: Need change the client where it calls evt_wait. Fortunately
   only time_evt is the one that can matter the system timing. Other
   places do not care. So this solution is not general, but should
   work and cause less overhead.
   
   One unsolved issue: currently we call scheduler server interface to
   sched_wakeup all threads that block from event manager, using a
   linked list. There is race condition there (the fault can happen
   before a thread is added to the blk_list, therefore that thread can
   never be invoked). One naive way to solve this: using a periodic
   thread to check all threads that still be in blocked status from
   event manager??? Have to wake up all new added threads as well.....

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

static unsigned int recover_all = 0;  // only tested/changed by the recovery thread

/* recovery data structure for evt */
struct rec_data_evt {
	spdid_t       spdid;
	long          c_evtid;
	long          s_evtid;

	long          p_evtid;  // parent event id (eg needs this)
	int           grp;      // same as above

	struct rec_data_evt *next, *prev;  // track all init events in a group
	struct rec_data_evt *p_next, *p_prev;  // link all parent event

	unsigned int  state;
	unsigned long fcnt;
};
/* Assumption: a component has at most one group, all init events are
 * splited from the same component. 
 *
 * Above assumption is not valid anymore. Now we will do eager
 * recovery. All rd without parent will be linked. Each rd can be the
 * parent of a group and each group can include multiple child rds.
 */
struct rec_data_evt *evts_grp_list_head = NULL;

/* the state of an event object (each operation expects to change) */
enum {
	EVT_STATE_CREATE,
	EVT_STATE_SPLIT,           // same as create, except it is for event group
	EVT_STATE_SPLIT_PARENT,    // split from a parent event id
	EVT_STATE_WAITING,
	EVT_STATE_TRIGGER,
	EVT_STATE_FREE
};

static void
print_rde_info(struct rec_data_evt *rde)
{
	assert(rde);

	printc("rde->spdid %d\n", rde->spdid);
	printc("rde->c_evtid %d\n", rde->c_evtid);
	printc("rde->s_evtid %d\n", rde->s_evtid);
	printc("rde->p_evtid %d\n", rde->p_evtid);
	printc("rde->grp %d\n", rde->grp);

	return;
}
/**********************************************/
/* slab allocaevt and cvect for tracking evts */
/**********************************************/

CVECT_CREATE_STATIC(rec_evt_map);   // track each event object and its id (create/free/wait)
CVECT_CREATE_STATIC(rec_evt_map_inv);   // for lookup s_id from c_id
CSLAB_CREATE(rdevt, sizeof(struct rec_data_evt));

static struct rec_data_evt *
rdevt_lookup(cvect_t *vect, int id)
{ 
	return (struct rec_data_evt *)cvect_lookup(vect, id); 
}

static struct rec_data_evt *
rdevt_alloc(cvect_t *vect, int id)
{
	struct rec_data_evt *rd = NULL;
	
	assert(id >= 1);
	
	rd = cslab_alloc_rdevt();
	assert(rd);	
	cvect_add(vect, rd, id);   // this must be greater than 1

	INIT_LIST(rd, next, prev);

	return rd;
}

static void
rdevt_dealloc(cvect_t *vect, struct rec_data_evt *rd, int id)
{
	assert(rd);
	if (cvect_del(vect, id)) BUG();

	REM_LIST(rd, next, prev);  // remove tracked event

	rd->spdid	 = 0;
	rd->c_evtid	 = 0;
	rd->s_evtid	 = 0;
	rd->p_evtid	 = 0;
	rd->grp	         = 0;
	rd->state	 = 0;
	rd->fcnt	 = 0;
	
	cslab_free_rdevt(rd);

	return;
}

static void 
rd_cons(struct rec_data_evt *rd, spdid_t spdid, long c_evtid, 
	long s_evtid, long parent, int grp, int state, int recovery)
{
	assert(rd);
	
	rd->spdid	 = spdid;
	rd->c_evtid	 = c_evtid;
	rd->s_evtid	 = s_evtid;
	rd->p_evtid	 = parent;
	rd->grp	         = grp;

	rd->state	 = state;
	rd->fcnt	 = fcounter;

	if (unlikely(recovery)) goto done;
	
	struct rec_data_evt *curr_parent  = NULL;
	/* If there is parent and group, track here. A child event
	 * should only be created after the parent event is created */
	/* if (parent == 0 && grp == 1) {   // parent evt */
	if (parent == 0) {   // parent evt, no matter whatever grp is
		INIT_LIST(rd, p_next, p_prev);
		/* printc("(thd %d spd %ld) create a parent event(%d) here\n", */
		/*        cos_get_thd_id(), cos_spd_id(), c_evtid); */
		if (unlikely(!evts_grp_list_head)) {
			evts_grp_list_head = rd;
		} else {
			ADD_LIST(evts_grp_list_head, rd, p_next, p_prev);
		}
	} else if (parent > 0 && !grp) {     // child evt
		assert(evts_grp_list_head);
		struct rec_data_evt *tmp;
		/* printc("(thd %d spd %ld) create a child event(%d, parent %d) here\n", */
		/*        cos_get_thd_id(), cos_spd_id(), c_evtid, parent); */
		/* printc("evts_grp_list_head->c_evtid %d s_evtid %d parent %d\n",  */
		/*        evts_grp_list_head->c_evtid, evts_grp_list_head->c_evtid, parent); */
		if (evts_grp_list_head->s_evtid == parent) {
			ADD_LIST(evts_grp_list_head, rd, next, prev);
			goto done;
		} else {
			for(tmp = FIRST_LIST(evts_grp_list_head, next, prev); 
			    tmp!= evts_grp_list_head; 
			    tmp = tmp->next) {
				if (tmp->s_evtid == parent) {
					/* printc("evts_tmp->c_evtid %d parent %d\n",  */
					/*        tmp->c_evtid, parent); */
					ADD_LIST(tmp, rd, next, prev);
					goto done;
				}
			}
		}
		assert(0);  // we must have found the parent rd!!!
	}
done:
	return;
}

/* This is ugly: the recovery of init events in a group (e.g.,
 * connection manager) 
*/
static void
rd_recover_state(struct rec_data_evt *rd)
{
	struct rec_data_evt *prd = NULL, *tmp = NULL, *tmp_new = NULL;
	long tmp_evtid;

	assert(rd && rd->c_evtid >= 1);
	
	/* printc("calling recover (thd %d)!!!!!!\n", cos_get_thd_id()); */

	if (!rd->p_evtid) {
		assert(!rd->grp || rd->grp == 1);
		/* printc("has no parent\n"); */
		/* printc("re-split an event in spd %d interface\n", rd->spdid); */

		tmp_evtid = c3_evt_split(cos_spd_id(), rd->p_evtid, rd->grp, rd->c_evtid);
		assert(tmp_evtid >= 1);

		rd->s_evtid = tmp_evtid;      // update the old rd's server side evtid
		/* tmp_new->c_evtid = rd->c_evtid; // update the new rd's client side evtid */
		/* printc("re-split an c_evt %d\n", rd->c_evtid); */
		/* printc("re-split an s_evt %d\n", rd->s_evtid); */

		/* rebuild all child events belong to the same group */
		if (rd->grp == 1) {
			assert(rd == evts_grp_list_head);
			for(tmp = FIRST_LIST(rd, next, prev) ;
			    tmp != rd;
			    tmp = FIRST_LIST(tmp, next, prev)) {
                                /* a child update its new re-created parent id */
				tmp->p_evtid = rd->s_evtid;
                                /* only re-create this child once */
				tmp->fcnt    = fcounter; 
				/* printc("\n>>found an evt %d (thd %d)\n", */
				/*        tmp->c_evtid, cos_get_thd_id()); */
				/* printc("its parent evt %d\n", tmp->p_evtid); */
				tmp_evtid = c3_evt_split(cos_spd_id(), tmp->p_evtid,
							 tmp->grp, tmp->c_evtid);
				assert(tmp_evtid >= 1);
				
				tmp_new = rdevt_lookup(&rec_evt_map, tmp_evtid);
				assert(!tmp_new);

				tmp->s_evtid = tmp_evtid;
				/* tmp_new->c_evtid = tmp->c_evtid; */
				/* printc("c3_tsplit new evtid %d\n", tmp->s_evtid); */
				/* printc("re-split an c_evt %d\n", tmp->c_evtid); */
				/* printc("re-split an s_evt %d\n", tmp->s_evtid);		 */
			}
		}
	} else {
		assert(0);  // for eager, we should not be here
		prd = rdevt_lookup(&rec_evt_map, rd->c_evtid);
                /* assume parent event is in the same component and
		 * only one level */
		assert(prd);  
		rd_recover_state(prd);
	}
	
	return;
}


/* This is the interface eager recovery function called from the
 * client component. TODO:find a better way to upcall into a component
 * and do the recovery over the interface. Automatically? */
void 
events_replay_all()
{
	printc("now thd %d is in spd %ld interface and ready to recover all events\n",
	       cos_get_thd_id(), cos_spd_id());
	
	recover_all = 1;

	struct rec_data_evt *rde = evts_grp_list_head;
	assert(rde && !rde->p_evtid); // there must be some events (therefore upcall)
	rd_recover_state(rde);

	int tmp_cnt = 0;
	// rest parent rd
	for(rde = FIRST_LIST(evts_grp_list_head, p_next, p_prev); 
	    rde!= evts_grp_list_head; 
	    rde = rde->p_next) {
		/* printc("evt cli: rd_recover_state (%d)\n", tmp_cnt); */
		/* print_rde_info(rde); */
		if (tmp_cnt++ > 100) assert(0);
		rd_recover_state(rde);
	}

	printc("thd %d in spd %ld interface has recovered all events\n",
	       cos_get_thd_id(), cos_spd_id());

	return;
}

static struct rec_data_evt *
rd_update(int evtid, int state)
{
        struct rec_data_evt *rd = NULL;

	/* TAKE(cos_spd_id()); */

        rd = rdevt_lookup(&rec_evt_map, evtid);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;
	
	rd->fcnt = fcounter;

	goto done;  // for eager recovery only

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
		/* recovery thread should have updated this in c3_evt_split */
		/* CSTUB_FAULT_UPDATE(); */
		goto redo;
	}
	
	assert(ret > 0);
	/* printc("cli: evt_create create a new rd in spd %ld (server id %d)\n",  */
	/*        cos_spd_id(), ret); */
	rd = rdevt_alloc(&rec_evt_map, ret);
	assert(rd);
	rd_cons(rd, cos_spd_id(), ret, ret, 0, 0, EVT_STATE_CREATE, 0);
	
	return ret;
}

/* There is no need to track a re-split server side evt. Also
 * ns_update will be called due to the evt_trigger cab called from
 * some component that is not tracked (try to avoid the overhead of
 * each evt_trigger) */
CSTUB_FN(long, c3_evt_split) (struct usr_inv_cap *uc,
			      spdid_t spdid, long parent_evt, int grp, int old_evtid)
{
	long fault = 0;
	long ret;
	
	// update the fault counter and cap.fcnt only once after the fault (eagerly)
	if (recover_all) {
		recover_all = 0;
		CSTUB_FAULT_UPDATE();
	}
redo:
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, parent_evt, grp, old_evtid);
	if (unlikely (fault)){
		/* recovery thread should have updated this above */
		/* CSTUB_FAULT_UPDATE(); */
		goto redo;
	}
	
	assert(ret > 0);
	/* printc("cli: c3_evt_split create a new rd in spd %ld (server id %d)\n", */
	/*        cos_spd_id(), ret); */
	
	/* The new created event should not be presented. evt_ns
	 * should guarantee this */
	struct rec_data_evt *rd = rdevt_lookup(&rec_evt_map, ret);
	assert(!rd);
	rd = rdevt_lookup(&rec_evt_map_inv, ret);
	if (!rd) rd = rdevt_alloc(&rec_evt_map_inv, ret);
	assert(rd);
	rd_cons(rd, cos_spd_id(), old_evtid, ret, 0, 0, EVT_STATE_CREATE, 1);

	/* printc("evt cli: c3_evt_split returns a new evtid %d for old evtid %d\n", */
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

                /* recovery thread should have done this in the eager
		 * recovery (c3_evt_split). See above.... If there is
		 * no event created over this interface ever, we do
		 * not care if the fault has failed over this
		 * interface -- on demand somehow */
		/* CSTUB_FAULT_UPDATE();  // keep this since the event has not been created */
		goto redo;
	}
	assert(ret > 0);
	/* printc("cli: evt_create create a new rd in spd %ld (server id %d)\n",  */
	/*        cos_spd_id(), ret); */

	rd = rdevt_alloc(&rec_evt_map, ret);
	assert(rd);
	rd_cons(rd, cos_spd_id(), ret, ret, p_evt, grp, EVT_STATE_CREATE, 0);

	return ret;
}

CSTUB_FN(long, evt_wait) (struct usr_inv_cap *uc,
			  spdid_t spdid, long extern_evt)
{
	long fault = 0;
	long ret;
	
	long ret_eid;
        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_cli = NULL;
        struct rec_data_evt *rd_ret = NULL;
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

	assert(rd->s_evtid > 0);
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_evtid);
        if (unlikely (fault)){
		printc("(ret %d)see a fault during evt_wait (thd %d in spd %ld)\n",
		       ret, cos_get_thd_id(), cos_spd_id());
#ifdef BENCHMARK_MEAS_WAIT
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		

		/* Here is the issue: the fault counter on cap is only
		 * updated over the interface of the component that
		 * split/create/free the event. evt_wait and
		 * evt_trigger could be in the same component or the
		 * different one. So we need check if the event is
		 * created here. If not, we need call
		 * CSTUB_FAULT_UPDATE when we "see" the fault */
		/* struct rec_data_evt *tmp = rdevt_lookup(extern_evt); */
		/* if (unlikely(!tmp)) CSTUB_FAULT_UPDATE(); */

		goto redo;
        }

	assert(rd && extern_evt == rd->c_evtid);
	int tmp_flag = 1;
	if (ret < 0) tmp_flag = -1;  // woken due to pretended event

	/* printc("cli: evt_wait ret %d passed in extern_evt %d (thd %d tmp_flag %d)\n", */
	/*        ret, extern_evt, cos_get_thd_id(), tmp_flag); */
	
	/* Look up the client side id from the returned server side
	 * id, if there is a re-split one. Here is the issue: a thread
	 * can wait on an event that belongs to a group. So evt_wait
	 * does not always return rd->s_evtid and we can not find
	 * rd->c_evtid from rd->s_evtid. The id returned from server
	 * side is not able to help find which client id in case
	 *
	 * Question: how to find client id from a return server
	 * id?. Note that the recovery depends on the client info.
	 *
	 * Solution: maintain another cvect
	 * (rec_evt_map_inverse). evt_ns can guarantee the uniqueness
	 * of event id (e.g., no where else in the system will see the
	 * same id)
	 */
	if (rd_cli = rdevt_lookup(&rec_evt_map_inv, ret*tmp_flag)) {
		ret_eid = rd_cli->c_evtid;
	} else ret_eid = ret*tmp_flag;
	
	assert(ret_eid > 0);
	return ret_eid*tmp_flag;
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
		struct rec_data_evt *tmp = rdevt_lookup(&rec_evt_map, extern_evt);
		if (unlikely(!tmp)) {
			printc("evt_trigger cli: (thd %d spd %ld) evt %d is not tracked\n",
			       cos_get_thd_id(), cos_spd_id(), extern_evt);
			CSTUB_FAULT_UPDATE();
		}
                /*
		  1) fault occurs in evt_trigger (before
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
	return 0;
	/* return ret; */
}

CSTUB_FN(int, evt_free) (struct usr_inv_cap *uc,
			 spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_cli = NULL;
redo:
	/* printc("evt cli: evt_free(1) %d (evt id %ld in spd %ld)\n",  */
	/*        cos_get_thd_id(), extern_evt, cos_spd_id()); */
        rd = rd_update(extern_evt, EVT_STATE_FREE);
	if (!rd) return 0;  // test
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
		/* CSTUB_FAULT_UPDATE(); */
		goto redo;
	}


	/* printc("evt cli: evt_free(2) %d (evt id %ld)\n", cos_get_thd_id(), rd->s_evtid); */
	rd_cli  = rdevt_lookup(&rec_evt_map_inv, rd->s_evtid);
	if (unlikely(rd_cli)) rdevt_dealloc(&rec_evt_map_inv, rd_cli, rd->s_evtid);
	rdevt_dealloc(&rec_evt_map, rd, rd->c_evtid);

	return ret;
}
