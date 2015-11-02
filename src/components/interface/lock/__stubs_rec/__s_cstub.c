/*
  Jiguo: reflection interface for lock component. Reflection will be
  used with any component that have threads blocked via
  lock_component_take (contention case). When that component has
  failed, the blocked threads can be woken up here

  Note: this interface should be set by set_symbol_link together with
  other interface in recovery mode.
*/

#include <cos_component.h>
#include <lock.h>
#include <print.h>
#include <cos_list.h>

#if (RECOVERY_ENABLE == 1)
#include <c3_test.h>
#endif

volatile unsigned long long overhead_start, overhead_end;

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
#define C_TAKE(spdid) 	do { if (sched_component_take(spdid))    return 0; } while (0)
#define C_RELEASE(spdid)	do { if (sched_component_release(spdid)) return 0; } while (0)

/* 1. When a lock is released, all blocked threads will be woken up
   2. A component might have multiple different locks being contended
   So, here we track locks per component in order to be reflected. So
   the client can release each lock, even a spd might have multiple
   different locks.
 */
struct track_lock {
	long  lockid;
	struct track_lock *next, *prev;
};

struct trigger_spdlocks {
	int spdid;
	struct track_lock list_head;
};

struct trigger_spdlocks spdlocks[MAX_NUM_SPDS];

static int first_lock_track = 0;

extern int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id);
extern int lock_component_release(spdid_t spd, unsigned long lock_id);
extern int lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd);
extern unsigned long lock_component_alloc(spdid_t spd);

int __sg_lock_component_pretake(spdid_t spd, unsigned long lock_id, unsigned short int thd)
{
	return lock_component_pretake(spd, lock_id, thd);
}

int __sg_lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd)
{
	int ret = 0;
	struct track_lock tl;
	assert(spd && lock_id && thd);

#ifdef BENCHMARK_MEAS_INV_OVERHEAD_NO_SERVER_TRACK_LOCK
	ret = lock_component_take(spd, lock_id, thd);
	return ret;
#else
	rdtscll(overhead_start);

	C_TAKE(cos_spd_id());
	/* printc("thd %d is going to lock_component_take\n", cos_get_thd_id()); */

	tl.lockid = lock_id;
	INIT_LIST(&tl, next, prev);
	if (unlikely(!first_lock_track)) {
		first_lock_track = 1;
		int i;
		/* printc("initialize lock tracking....\n"); */
		for (i = 0; i < MAX_NUM_SPDS; i++) {  // assume min spd id is 0
			spdlocks[i].spdid = i;
			spdlocks[i].list_head.lockid = 0;
			INIT_LIST(&spdlocks[i].list_head, next, prev);
		}
	} 
	/* printc("\n<<thd %d is adding on list in lock_component_take>>>\n", cos_get_thd_id()); */
	ADD_LIST(&spdlocks[spd].list_head, &tl, next, prev);
	C_RELEASE(cos_spd_id());

	rdtscll(overhead_end);
	unsigned long long  tmp_overhead = overhead_end - overhead_start;

	ret = lock_component_take(spd, lock_id, thd);
	/* printc("\n<<<thd %d is back from lock_component_take>>>\n", cos_get_thd_id()); */

	rdtscll(overhead_start);

	C_TAKE(cos_spd_id());
	/* printc("\n<<<thd %d is removing from lock_component_take list>>>\n", cos_get_thd_id()); */
	REM_LIST(&tl, next, prev);
	C_RELEASE(cos_spd_id());

	/* rdtscll(overhead_end); */
	/* printc("lock_component_take interface overhead %llu\n",  */
	/*        overhead_end - overhead_start + tmp_overhead); */

	return ret;
#endif
}

int __sg_lock_component_release(spdid_t spd, unsigned long lock_id)
{
	return lock_component_release(spd, lock_id);
}

int __sg_lock_component_alloc(spdid_t spd)
{
	return lock_component_alloc(spd);
}


int __sg_lock_reflect(spdid_t spdid)
{
	long ret = 0;
	struct track_lock *tl, *list_head, *tmp;
	
	printc("thread %d is going to release all locks from component %d\n",
	       cos_get_thd_id(), spdid);
	
	C_TAKE(cos_spd_id());
	
	list_head = &spdlocks[spdid].list_head;
	if (unlikely(!list_head->next)) goto done;
	if (unlikely(EMPTY_LIST(list_head, next, prev))) goto done;
	
	for (tl = FIRST_LIST(list_head, next, prev);
	     tl != list_head;) {
		/* printc("found lock id %ld to release\n", tl->lockid); */
		tmp = FIRST_LIST(tl, next, prev);
		C_RELEASE(cos_spd_id());
		lock_component_release(spdid, tl->lockid);
		C_TAKE(cos_spd_id());
		tl = tmp;
	}
	/* printc("all locks are released done (thd %d)\n\n", cos_get_thd_id()); */
	
done:
	C_RELEASE(cos_spd_id());
	return ret;
}
