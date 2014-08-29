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
     evt_trigger will pass the correct id to evt manager?  
     
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
struct blocked_thd {
	int thd;
	struct blocked_thd *next, *prev;
};

struct rec_data_evt {
	spdid_t spdid;
	int grp_id;             // evt is created by thread in its group
	unsigned long p_evtid;  // for tsplit only

	unsigned long c_evtid;
	unsigned long s_evtid;

	unsigned long fcnt;
	struct blocked_thd blkthd;
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
	cslab_free_rdevt(rd);
}

static void
rdevt_addblk(struct rec_data_evt *rd, struct blocked_thd *ptr_blkthd)
{
	assert(rd && ptr_blkthd);

	ptr_blkthd->thd = cos_get_thd_id();
	INIT_LIST(ptr_blkthd, next, prev);
	ADD_LIST(&rd->blkthd, ptr_blkthd, next, prev);
       
	return;
}


static int
get_unique(void)
{
	unsigned int i;
	cvect_t *v;

	v = &rec_evt_vect;
	for(i = 1 ; i <= CVECT_MAX_ID ; i++) {
		if (!cvect_lookup(v, i)) return i;
	}

	return -1;
}

static void 
rd_cons(struct rec_data_evt *rd, spdid_t spdid, unsigned long cli_evtid, unsigned long ser_evtid, unsigned long par_evtid, int grp_id)
{
	assert(rd);

	rd->spdid	 = spdid;
	rd->grp_id	 = grp_id;

	rd->p_evtid      = par_evtid;
	rd->c_evtid	 = cli_evtid;
	rd->s_evtid	 = ser_evtid;

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
update_rd(int evtid, int par_evtid, int cap)
{
        struct rec_data_evt *rd = NULL;

        rd = rdevt_lookup(evtid);
	if (unlikely(!rd)) {
		/* rd = rdevt_alloc(evtid); */
		/* assert(rd);	 */
		/* rd_cons(rd, cos_spd_id(), evtid, evtid, 0, cos_get_thd_id()); */
		/* INIT_LIST(&rd->blkthd, next, prev); */
		
		goto done;
	}
	if (likely(rd->fcnt == fcounter)) goto done;
	rd->fcnt = fcounter;
done:	
	return rd;
}

CSTUB_FN(unsigned long, __evt_create) (struct usr_inv_cap *uc,
				       spdid_t spdid)
{
	long fault = 0;
	unsigned long ret;
redo:
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	struct rec_data_evt *rd = NULL;
	
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		assert(0);
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
/* printc("evt cli: evt_create %d\n", cos_get_thd_id()); */
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);

	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if ((ser_eid = ret) <= 0) return ret;
	
	// if does exist, we need an unique id. Otherwise, create it
	if (unlikely(rdevt_lookup(ser_eid))) {
		cli_eid = get_unique();
		assert(cli_eid > 0 && cli_eid != ser_eid);
	} else {
		cli_eid = ser_eid;
	}
	rd = rdevt_alloc(cli_eid);
	assert(rd);
	
	rd_cons(rd, cos_spd_id(), cli_eid, ser_eid, 0, cos_get_thd_id());
	INIT_LIST(&rd->blkthd, next, prev);
	ret = cli_eid;

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

	struct blocked_thd blk_thd;
        struct rec_data_evt *rd = NULL;

        rd = update_rd(extern_evt, 0, uc->cap_no);
	if (!rd) {
		printc("try to wait for a non-existing evt\n");
		return -1;
	}
        rdevt_addblk(rd, &blk_thd);

	printc("evt cli: evt_wait %d (combined id %p)\n", cos_get_thd_id(), (void *)((extern_evt << 16) | rd->s_evtid));
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, (extern_evt << 16) | rd->s_evtid);
	
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		/* Only the thread who called event_create should
		   re-create the event since group id is the creator
		   (ownership) In the case other function failed, the
		   blocked waiting thread will be woken up and detect
		   the fault when returns back from scheduler.*/
		rd->s_evtid = __evt_create(cos_spd_id());
		/* evt_update_status(cos_spd_id(), rd->s_evtid); */
		
		printc("evt cli: evt_wait fault %d (create event %d again ofr cli evt %d)\n",
		       cos_get_thd_id(), rd->s_evtid, extern_evt);
		/* This will assume an event has occurred, no need to
		 * block wait. In the worst case, the processing
		 * thread will deal an event that does not happen
		 * actually when the fault occurs. So the event
		 * process thread should validate the event. We do not
		 * want to miss an event if it happens (fault can
		 * occur after the event arrives and the thread is
		 * woken up)
		 */
		ret = 0;
        }
        REM_LIST(&blk_thd, next, prev);

	return ret;
}

CSTUB_FN(int, evt_trigger) (struct usr_inv_cap *uc,
			    spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;

        rd = update_rd(extern_evt, 0, uc->cap_no);
        /* evt_trigger can be invoked from a different client. So do
	 * not check return value of update_rd */
	/* if (!rd) CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt); */
	/* else     CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_evtid); */
        /* printc("evt cli: evt_trigger %d\n", cos_get_thd_id()); */
	/* CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_evtid); */
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	
	if (unlikely (fault)){
		/* when fault occurs, all threads will be reflected and
		   woken up so it looks like an event trigger
		   anyway. No need to repeat trigger
		*/
		CSTUB_FAULT_UPDATE();
		ret = 0;
	}
	
	return ret;
}

CSTUB_FN(int, evt_free) (struct usr_inv_cap *uc,
			 spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;

        rd = update_rd(extern_evt, 0, uc->cap_no);
	if (!rd) {
		printc("try to free a non-existing evt\n");
		return -1;
	}
	rdevt_dealloc(rd);
/* printc("evt cli: evt_free %d\n", cos_get_thd_id()); */
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	
	if (unlikely (fault)) {
		CSTUB_FAULT_UPDATE();
	}

	return ret;
}
