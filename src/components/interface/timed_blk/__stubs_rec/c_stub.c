/*
Jiguo: 06/13  fault tolerance support interface for timed_event


Need: new amnt, and might have blocked in scheduler when te failed

1. passed amnt needs to be accurate right after the failure
2. already tracked timed_event needs to be re-passed the accurate amnt (this could be much later)
3. server side needs the following DS:
    . &events (event_block will insert event)
    . &periodic (periodic_wake_create will insert this)

For these threads that have been blocked in scheduler which the fault
occurs, the detection can rely on the kernel check (e.g, wake up all
blocked threads upon a fault, when switch to that thread, it should
realize TE has faulted before )

sched_wakup(spd)?? to wake up  all threads that called sched_block from spd??

*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>


#include <timed_blk.h>
#include <cstub.h>

extern int sched_wakeup_spd_all(spdid_t spdid);
extern unsigned long sched_timestamp(void);
extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

typedef unsigned long long event_time_t;
static volatile event_time_t ticks;
static unsigned long global_fault_cnt = 0;

struct rec_data_te {
	unsigned int id;
	event_time_t last_ticks;
	unsigned int amnt;
	unsigned long fcnt;
};

CSLAB_CREATE(rd, sizeof(struct rec_data_te));
CVECT_CREATE_STATIC(rec_vect);

static struct rec_data_te *
rd_lookup(int id)
{ return cvect_lookup(&rec_vect, id); }

static struct rec_data_te *
rd_alloc(void)
{
	struct rec_data_te *rd;
	rd = cslab_alloc_rd();
	return rd;
}

static void
rd_dealloc(struct rec_data_te *rd)
{
	assert(rd);
	if (cvect_del(&rec_vect, rd->id)) BUG();
	cslab_free_rd(rd);
}

static void 
rd_cons(struct rec_data_te *rd, unsigned int tid, event_time_t last_ticks, unsigned int amnt)
{
	assert(rd);

	rd->id   	 = tid;
	rd->last_ticks	 = last_ticks;
	rd->amnt	 = amnt;
	rd->fcnt	 = global_fault_cnt;

	return;
}

static struct rec_data_te *
update_rd(unsigned int id)
{
        struct rec_data_te *rd;

        rd = rd_lookup(id);
	if (!rd) return NULL;
	/* fast path */
	if (likely(rd->fcnt == global_fault_cnt)) return rd;
	assert(rd);
	rd->fcnt	 = global_fault_cnt;
	return rd;
}

// interface function...

CSTUB_FN_ARGS_2(int, timed_event_block, spdid_t, spdinv, unsigned int, amnt)

	/* printc("Here is TE cli interface\n"); */
        unsigned int cur_amnt = amnt;
	struct rec_data_te *rd;
redo:
	rd = update_rd(cos_get_thd_id());
	if (!rd) {
		rd = rd_alloc();
		assert(rd);
		rd_cons(rd, cos_get_thd_id(), sched_timestamp(), cur_amnt);
		if (cvect_add(&rec_vect, rd, rd->id)) {
			printc("can not add into cvect\n");
			BUG();
		}
	} else {
		assert(rd->id = cos_get_thd_id());

		rd->amnt = cur_amnt;
		rd->last_ticks = sched_timestamp();
	}

CSTUB_ASM_2(timed_event_block, spdinv, rd->amnt)
        if (unlikely(fault)) {
		global_fault_cnt++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		/* printc("A fault did happen!!!\n"); */

		/* event_time_t curr_ticks = sched_timestamp(); */
		/* cur_amnt = rd->amnt - (unsigned int)(curr_ticks-rd->last_ticks);  // minus the ticks used for the recovery */
		/* printc("cur_amnt in cli interface is %d\n", cur_amnt); */
                goto redo;
	}

CSTUB_POST


CSTUB_FN_ARGS_2(int, timed_event_wakeup, spdid_t, spdinv, unsigned short int, thd_id)

	struct rec_data_te *rd;
redo:
	rd = update_rd(cos_get_thd_id());
        assert(rd);   
	assert(rd->id = cos_get_thd_id());

CSTUB_ASM_2(timed_event_wakeup, spdinv, thd_id)

        if (unlikely(fault)) {
		global_fault_cnt++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
                goto redo;
	}

CSTUB_POST


