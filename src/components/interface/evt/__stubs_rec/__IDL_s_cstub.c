/* IDL generated code ver 0.1 ---  Fri Nov 27 10:24:09 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <evt.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#include <cos_synchronization.h>
extern cos_lock_t evt_lock;

struct track_block {
	long evtid;
	struct track_block *next, *prev;
};
struct track_block tracking_block_list[MAX_NUM_SPDS];

static inline int block_ser_if_block_track_evt_wait(spdid_t spdid, long evtid)
{
	int ret = 0;
	struct track_block tb;	// track on stack

	lock_take(&evt_lock);
	;

	if (unlikely(!tracking_block_list[spdid].next)) {
		INIT_LIST(&tracking_block_list[spdid], next, prev);
	}
	INIT_LIST(&tb, next, prev);
	tb.evtid = evtid;
	ADD_LIST(&tracking_block_list[spdid], &tb, next, prev);

	lock_release(&evt_lock);
	;

	ret = evt_wait(spdid, evtid);

	lock_take(&evt_lock);
	;

	REM_LIST(&tb, next, prev);

	lock_release(&evt_lock);
	;

	return ret;
}

long __ser_evt_wait(spdid_t spdid, long evtid)
{
	return block_ser_if_block_track_evt_wait(spdid, evtid);
}

static inline void block_ser_if_client_fault_notification(int spdid)
{
	struct track_block *tb;

	lock_take(&evt_lock);
	;

	if (!tracking_block_list[spdid].next)
		goto done;
	if (EMPTY_LIST(&tracking_block_list[spdid], next, prev))
		goto done;

	for (tb = FIRST_LIST(&tracking_block_list[spdid], next, prev);
	     tb != &tracking_block_list[spdid];
	     tb = FIRST_LIST(tb, next, prev)) {

		lock_release(&evt_lock);
		;

		evt_trigger(spdid, tb->evtid);

		lock_take(&evt_lock);
		;
	}

 done:
	lock_release(&evt_lock);
	;

	return;
}

void __ser_evt_client_fault_notification(int spdid)
{
	block_ser_if_client_fault_notification(spdid);
	return;
}
