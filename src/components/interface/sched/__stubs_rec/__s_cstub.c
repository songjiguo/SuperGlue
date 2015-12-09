/* scheduler reflection server stub interface : Jiguo 

 * Track all threads that blocks from a client

 * In order to reflect on scheduler, the threads that are blocked from
 * different components are tracked here. There are 2 ways to reflect
 * thread for recovering objects: 1) per object reflection 2) per
 * component reflection (including all threads blocked from that
 * component). 

 * For example, if fault component is faulty, we can either reflect
 * all blocked threads for a specific lock (from lock's client
 * interface), or we can reflect all blocked threads from lock
 * component (from scheduler's server interface, which is here)

*/

#include <cos_component.h>
#include <sched.h>
#include <print.h>

#include "../../implementation/sched/cos_sched_sync.h"

volatile unsigned long long overhead_start, overhead_end;

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#include <cos_list.h>

struct blocked_thd {
	int id;
	int dep_thd;
	struct blocked_thd *next, *prev;
};

struct blocked_thd bthds[MAX_NUM_SPDS];

/* track blocked threads here for all clients (on each thread stack) */
int __sg_sched_block(spdid_t spdid, int dependency_thd)
{
	struct blocked_thd blk_thd;
	int ret = 0;
	// add to list
	cos_sched_lock_take();

	if (unlikely(!bthds[spdid].next)) {
		INIT_LIST(&bthds[spdid], next, prev);
	}
	INIT_LIST(&blk_thd, next, prev);
	blk_thd.id = cos_get_thd_id();
	blk_thd.dep_thd = dependency_thd;
	/* printc("add to the list..... thd %d\n", cos_get_thd_id()); */
	ADD_LIST(&bthds[spdid], &blk_thd, next, prev);

	cos_sched_lock_release();

	ret = sched_block(spdid, dependency_thd);

	// remove from list in both normal path and reflect path
	cos_sched_lock_take();
	/* printc("remove from the list..... thd %d\n", cos_get_thd_id()); */
	REM_LIST(&blk_thd, next, prev);
	cos_sched_lock_release();

	return ret;
}

void __ser_sched_client_fault_notification(int spdid)
{
	struct blocked_thd *blk_thd;
	int ret = 0;

	/* printc("scheduler server side stub (thd %d from spd %d)\n",  */
	/*        cos_get_thd_id(), spdid); */

	cos_sched_lock_take();
	/* printc("scheduler server side stub (thd %d)\n", cos_get_thd_id()); */
	/* printc("passed reflection: spd %d src_spd %d\n", spdid, src_spd); */

	if (!bthds[spdid].next) goto done;
	if (EMPTY_LIST(&bthds[spdid], next, prev)) goto done;

	for (blk_thd = FIRST_LIST(&bthds[spdid], next, prev);
	     blk_thd != &bthds[spdid];
	     blk_thd = FIRST_LIST(blk_thd, next, prev)){
		printc("(cnt)blocked thds %d\n", blk_thd->id);
		cos_sched_lock_release();
		sched_wakeup(spdid, blk_thd->id);
		cos_sched_lock_take();
	}
	
	/* if (cnt == 1) {   */
	/* 	for (blk_thd = FIRST_LIST(&bthds[spd], next, prev); */
	/* 	     blk_thd != &bthds[spd]; */
	/* 	     blk_thd = FIRST_LIST(blk_thd, next, prev)){ */
	/* 		printc("(cnt)blocked thds %d\n", blk_thd->id); */
	/* 		ret++; */
	/* 	} */
	/* } else { */
	/* 	blk_thd = FIRST_LIST(&bthds[spd], next, prev); */
	/* 	if (!EMPTY_LIST(blk_thd, next, prev)) REM_LIST(blk_thd, next, prev); */
	/* 	ret = blk_thd->id; */
	/* } */
done:
	cos_sched_lock_release();
	return ret;
}

unsigned long thd_timestamp_track[MAX_NUM_THREADS];
unsigned long __ser_sched_save_data_sched_timestamp()
{
	unsigned long ret = 0;
	printc("thd %d calling sched_timestamp\n", cos_get_thd_id());
	
	ret = sched_timestamp();
	
	thd_timestamp_track[cos_get_thd_id()] = ret;

	printc("sched_timestamp returns %lu\n", ret);

	return ret;
}

unsigned long __ser_sched_restore_data_sched_timestamp()
{
	printc("thd %d calling sched_get_creation_timestamp\n", cos_get_thd_id());
	return thd_timestamp_track[cos_get_thd_id()];
}
