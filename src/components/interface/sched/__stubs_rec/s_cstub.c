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

#include <objtype.h>

volatile unsigned long long overhead_start, overhead_end;

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct blocked_thd {
	int id;
	int dep_thd;
	struct blocked_thd *next, *prev;
};

struct rec_data_blk {
	spdid_t       spdid;
	struct blocked_thd blkthd;
};

CVECT_CREATE_STATIC(rec_blk_vect);
CSLAB_CREATE(rdblk, sizeof(struct rec_data_blk));

static struct rec_data_blk *
rdblk_lookup(int id)
{ 
	return cvect_lookup(&rec_blk_vect, id); 
}

static struct rec_data_blk *
rdblk_alloc(int spdid)
{
	struct rec_data_blk *rd;

	rd = cslab_alloc_rdblk();
	assert(rd);
	cvect_add(&rec_blk_vect, rd, spdid);
	return rd;
}

static void
rdblk_dealloc(struct rec_data_blk *rd)
{
	assert(rd);
	cslab_free_rdblk(rd);
}

static void
rdblk_addblk(struct rec_data_blk *rd, struct blocked_thd *ptr_blkthd, int dep_thd)
{
	assert(rd && ptr_blkthd);

	ptr_blkthd->id = cos_get_thd_id();
	ptr_blkthd->dep_thd = dep_thd;

	INIT_LIST(ptr_blkthd, next, prev);
	ADD_END_LIST(&rd->blkthd, ptr_blkthd, next, prev);
	/* printc("\n[[[[sched_block: tracking...ptr_blkthd %p (thd %d  spd %d rd->blkthd addr %p)]]]\n\n", ptr_blkthd, cos_get_thd_id(), rd->spdid, &rd->blkthd); */
       
	return;
}

// track blocked threads here for all clients (on each thread stack)
int __sg_sched_block(spdid_t spdid, int dependency_thd)
{
	struct blocked_thd blk_thd;
	struct rec_data_blk *rd = NULL;

	rdtscll(overhead_start);

	// add to list
	cos_sched_lock_take();

	rd = rdblk_lookup(spdid);
	if (unlikely(!rd)) {
		rd = rdblk_alloc(spdid);
		assert(rd);
		INIT_LIST(&rd->blkthd, next, prev);
		rd->spdid = spdid;
	}

	assert(rd && rd->spdid == spdid);
	rdblk_addblk(rd, &blk_thd, dependency_thd);
	cos_sched_lock_release();

	rdtscll(overhead_end);
	unsigned long long  tmp_overhead = overhead_end - overhead_start;

	sched_block(spdid, dependency_thd);

	rdtscll(overhead_start);

	// remove from list in both normal path and reflect path
	cos_sched_lock_take();
	if (!EMPTY_LIST(&blk_thd, next, prev)) REM_LIST(&blk_thd, next, prev);
	/* printc("sched_block: removing...blkthd %p (thd %d)\n", &blk_thd, cos_get_thd_id()); */
	cos_sched_lock_release();

	/* rdtscll(overhead_end); */
	/* printc("sched_block interface overhead %llu\n",  */
	/*        overhead_end - overhead_start + tmp_overhead); */
	/* printc("thd %d returns from sched_block server interface (back to spd %d)\n",  */
	/*        cos_get_thd_id(), spdid); */

	return 0;
}


/* This is scheduler server interface function for client reflection
   src_spd : component from which the thread is blocked from
   return  : id of the thread that needs to be woken up
*/
int __sg_sched_reflect(spdid_t spd, int src_spd, int cnt)
{
        struct rec_data_blk *rd = NULL;
	struct blocked_thd *blk_thd;
	int ret = 0;

	assert(src_spd);
	cos_sched_lock_take();
	/* printc("scheduler server side stub (thd %d)\n", cos_get_thd_id()); */
	/* printc("passed reflection: spd %d src_spd %d\n", spd, src_spd); */

	rd = rdblk_lookup(src_spd);
	if (!rd) goto done;  // if there is no thread blocked from src_spd, return
	
	if (cnt) {
		for (blk_thd = FIRST_LIST(&rd->blkthd, next, prev);
		     blk_thd != &rd->blkthd;
		     blk_thd = FIRST_LIST(blk_thd, next, prev)){
			printc("(cnt)blocked thds %d\n", blk_thd->id);
			ret++;
		}
	} else {

		/* for (blk_thd = FIRST_LIST(&rd->blkthd, next, prev); */
		/*      blk_thd != &rd->blkthd; */
		/*      blk_thd = FIRST_LIST(blk_thd, next, prev)){ */
		/* 	printc("(before)blocked thds %d\n", blk_thd->id); */
		/* } */

		blk_thd = FIRST_LIST(&rd->blkthd, next, prev);
		if (!EMPTY_LIST(blk_thd, next, prev)) REM_LIST(blk_thd, next, prev);
		ret = blk_thd->id;
	}
done:
	cos_sched_lock_release();
	return ret;
}

// coalocated DS with object in spd??
