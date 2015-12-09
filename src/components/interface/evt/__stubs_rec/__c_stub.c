/* Jiguo: Event c^3 client stub interface. The interface tracking code
   is supposed to work for both group and non-group events
*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <objtype.h>
#include <sched.h>

#include <evt.h>
#include <cstub.h>

#include <cos_map.h>

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

volatile unsigned long long ubenchmark_start, ubenchmark_end;

/* global fault counter, only increase, never decrease */
static unsigned long global_fault_cnt;

/* recovery data structure for evt */
struct rec_data_evt {
	spdid_t       spdid;   // where the event is created
	long          c_evtid;
	long          s_evtid;

	long          p_evtid;  // parent event id (eg needs this)
	int           grp;      // same as above

	unsigned int  state;
	unsigned long fcnt;
};

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

	printc("rde->spdid %d ", rde->spdid);
	printc("rde->p_evtid %d\n", rde->p_evtid);
	printc("rde->c_evtid %d ", rde->c_evtid);
	printc("rde->s_evtid %d\n", rde->s_evtid);
	printc("rde->grp %d\n", rde->grp);

	return;
}
/**********************************************/
/* slab allocaevt and cmap for tracking evts */
/**********************************************/

CVECT_CREATE_STATIC(rec_evt_map);
//COS_MAP_CREATE_STATIC(rec_evt_map);
CSLAB_CREATE(rdevt, sizeof(struct rec_data_evt));

static int first = 0;  // init rec_evt_map

static struct rec_data_evt *
rdevt_lookup(int id)
{ 
	return (struct rec_data_evt *)cvect_lookup(&rec_evt_map, id); 
}

static struct rec_data_evt *
rdevt_alloc(int id)
{
	struct rec_data_evt *rd = NULL;

	rd = cslab_alloc_rdevt();
	assert(rd);
	cvect_add(&rec_evt_map, rd, id);
	return rd;

	/* while(1) { */
	/* 	rd = cslab_alloc_rdevt(); */
	/* 	assert(rd); */
	/* 	id = cos_map_add(&rec_evt_map, rd); */
	/* 	if (id > 1) break; */
	/* 	rd->s_evtid = -1; */
	/* } */
	/* assert(id > 1); */
	/* return id; */
}

static void
rdevt_dealloc(struct rec_data_evt *rd)
{
	assert(rd);
	if (cvect_del(&rec_evt_map, rd->c_evtid)) BUG();
	cslab_free_rdevt(rd);
	return;

	/* struct rec_data_evt *rd = NULL; */
	/* assert(id > 0); */
	/* rd = rdevt_lookup(id); */
	/* assert(rd); */
	/* cslab_free_rdevt(rd); */
	/* cos_map_del(&rec_evt_map, id); */
	/* return; */
}

static void 
rd_cons(struct rec_data_evt *rd, long c_evtid, 
	long s_evtid, long parent, int grp, int state)
{
	assert(rd);
	
	rd->spdid	 = cos_spd_id();
	rd->c_evtid	 = c_evtid;
	rd->s_evtid	 = s_evtid;
	rd->p_evtid	 = parent;
	rd->grp	         = grp;

	rd->state	 = state;
	rd->fcnt	 = global_fault_cnt;
	
	return;
}

static struct rec_data_evt *
rd_update(int evtid, int state)
{
        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_parent = NULL;
	int id, pevtid;

	// root
	if (!evtid) {
		printc("found the root, just return and create\n");
		return;
	}

	printc("AAAAA\n");
        rd = rdevt_lookup(evtid);
	printc("BBBBB\n");
	if (unlikely(!rd)) {
		printc("cli: evt_upcal_creator %d\n", evtid);
		evt_upcall_creator(cos_spd_id(), evtid);
		goto done;
	}
	/* printc("CCCCC\n"); */
	/* printc("evtid %d rd->fcnt %lu global_fault_cnt %lu\n", */
	/*        evtid, rd->fcnt, global_fault_cnt); */
	if (likely(rd->fcnt == global_fault_cnt)) goto done;
	
	rd->fcnt = global_fault_cnt;

	/* printc("cli:rd_recover_state\n"); */
	/* print_rde_info(rd); */

again:
	pevtid = rd->p_evtid;

	id = evt_split_exist(rd->spdid, pevtid, rd->grp, evtid);
	if (id == -EINVAL) {
		printc("the parent id (%d) needs to be created now!!!!\n",
			rd->p_evtid);
		rd_update(pevtid, EVT_SPLIT);
		goto again;
	}
	if (id < 0) {  // has been freed
		rdevt_dealloc(rd);
		return NULL;
	}

	assert(id);
	/* printc("cli: update s_evtid %d\n", id); */
	rd->s_evtid = id;

done:	
	return rd;
}

void
evt_cli_if_recover_upcall_entry(long evtid)
{
	assert(rd_update(evtid, EVT_SPLIT));
	return;
	
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

        if (first == 0) {
		cos_map_init_static(&rec_evt_map);
		first = 1;
	}

redo:
        /* printc("evt cli: evt_create %d\n", cos_get_thd_id()); */

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		/* printc("see a fault during evt_create (thd %d in spd %ld)\n", */
		/*        cos_get_thd_id(), cos_spd_id()); */
		/* CSTUB_FAULT_UPDATE(); */
		goto redo;
	}
	
	assert(ret > 0);
	/* printc("cli: evt_create create a new rd in spd %ld (server id %d)\n",  */
	/*        cos_spd_id(), ret); */
	rd = rdevt_alloc(ret);
	assert(rd);
	rd_cons(rd, ret, ret, 0, 0, EVT_STATE_CREATE);
	
	return ret;
}

CSTUB_FN(long, evt_split) (struct usr_inv_cap *uc,
			   spdid_t spdid, long parent_evt, int grp)
{
	long fault = 0;
	long ret;

        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_parent = NULL;
        long ser_eid, cli_eid;

        if (first == 0) {
		cvect_init_static(&rec_evt_map);
		first = 1;
	}
redo:
	rd_update(parent_evt, EVT_SPLIT);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, parent_evt, grp);
	if (unlikely (fault)){
		printc("evt_split sees a fault\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	/* printc("cli: evt_split ret %d\n", ret); */
	/* if (ret < 0) { */
	/* 	rd_update(parent_evt, EVT_SPLIT); */
	/* 	goto redo; */
	/* } */
	
	rd = rdevt_alloc(ret);
	assert(rd);
	rd_cons(rd, ret, ret, parent_evt, grp, EVT_STATE_CREATE);

	return ret;
}

static int evt_wait_ubenchmark_flag;
CSTUB_FN(long, evt_wait) (struct usr_inv_cap *uc,
			  spdid_t spdid, long extern_evt)
{
	long fault = 0;
	long ret;
	
	long ret_eid;
        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_cli = NULL;
        struct rec_data_evt *rd_ret = NULL;
        if (first == 0) {
		cvect_init_static(&rec_evt_map);
		first = 1;
	}

redo:
	rdtscll(ubenchmark_end);
	if (evt_wait_ubenchmark_flag) {
		evt_wait_ubenchmark_flag = 0;
		printc("evt_wait (man):recover per object end-end cost: %llu\n",
		       ubenchmark_end - ubenchmark_start);
	}

	/* printc("evt cli: evt_wait thd %d in spd %ld (extern_evt %d)\n", */
	/*        cos_get_thd_id(), cos_spd_id(), extern_evt); */

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
        if (unlikely (fault)){
		printc("cli: see a fault during evt_wait evt %d (thd %d in spd %ld)\n",
		       extern_evt, cos_get_thd_id(), cos_spd_id());
		evt_wait_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		/* rd_update(extern_evt, EVT_STATE_WAITING); */
		/* return -1; */
		goto redo;
        }

	if (unlikely(ret == -EINVAL)) {
		printc("cli: evt_wait returns -EINVAL\n");
		rd_update(extern_evt, EVT_STATE_TRIGGER);
		goto redo;
	}

	return ret;
}


CSTUB_FN(int, evt_trigger) (struct usr_inv_cap *uc,
			    spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret, ser_evtid;
        struct rec_data_evt *rd = NULL;

        if (first == 0) {
		cvect_init_static(&rec_evt_map);
		first = 1;
	}

	printc("evt cli: evt_trigger thd %d from spd %ld (evt id %ld)\n",
	       cos_get_thd_id(), cos_spd_id(), extern_evt);
redo:
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely(fault)){
		printc("cli: see a fault during evt_trigger evt %d (thd %d in spd %ld)\n",
		       extern_evt, cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
		/* return -1; */
	}

	/* trigger has not tracking to tell if a descriptor exists, so
	 * when it is called, we might just return -EVINAL from the
	 * evt component after it has been recovered before. Notice
	 * that this logic is used to be in the evt component, now it
	 * is moved here, this is why we call evt_upcall_creator*/
	if (unlikely(ret == -EINVAL)) {
		printc("cli: evt_trigger returns -EINVAL\n");
		rd_update(extern_evt, EVT_STATE_TRIGGER);
		goto redo;
	}

	/* return 0; */
	return ret;  // remember to check this when debug the web server
}

CSTUB_FN(int, evt_free) (struct usr_inv_cap *uc,
			 spdid_t spdid, long extern_evt)
{
	long fault = 0;
	int ret;

        struct rec_data_evt *rd = NULL;
        struct rec_data_evt *rd_cli = NULL;

        if (first == 0) {
		cvect_init_static(&rec_evt_map);
		first = 1;
	}
redo:
	rd_update(extern_evt, EVT_STATE_FREE);
	if (!rd) return 0;
	/* printc("evt cli: evt_free(1) %d (evt id %ld in spd %ld)\n", */
	/*        cos_get_thd_id(), extern_evt, cos_spd_id()); */
	
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, extern_evt);
	if (unlikely (fault)) {
		/* printc("see a fault during evt_free (thd %d in spd %ld)\n", */
		/*        cos_get_thd_id(), cos_spd_id()); */
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	rdevt_dealloc(rd);

	return ret;
}
