/* IDL generated code ver 0.1 ---  Mon Oct 26 11:57:34 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <lock.h>

extern ul_t lock_component_alloc(spdid_t spdid);
extern int lock_component_take(spdid_t spdid, ul_t lock_id, u32_t thd_id);
extern int lock_component_pretake(spdid_t spdid, ul_t lock_id, u32_t thd_id);
extern int lock_component_release(spdid_t spdid, ul_t lock_id);
extern int lock_component_free(spdid_t spdid, ul_t lock_id);

struct track_block {
	int lock_id;
	struct track_block *next, *prev;
};
struct track_block tracking_block_list[MAX_NUM_SPDS];

static inline int block_ser_if_block_track_lock_component_take(spdid_t spdid,
							       ul_t lock_id,
							       u32_t thd_id)
{
	int ret = 0;
	struct track_block tb;	// track on stack

	do {
		if (sched_component_take(spdid))
			BUG();
	} while (0);

	if (unlikely(!tracking_block_list[spdid].next)) {
		INIT_LIST(&tracking_block_list[spdid], next, prev);
	}
	INIT_LIST(&tb, next, prev);
	tb.lock_id = lock_id;
	ADD_LIST(&tracking_block_list[spdid], &tb, next, prev);

	do {
		if (sched_component_release(spdid))
			BUG();
	} while (0);

	ret = lock_component_take(spdid, lock_id, thd_id);

	do {
		if (sched_component_take(spdid))
			BUG();
	} while (0);

	REM_LIST(&tb, next, prev);

	do {
		if (sched_component_release(spdid))
			BUG();
	} while (0);

	return ret;
}

int __ser_lock_component_take(spdid_t spdid, ul_t lock_id, u32_t thd_id)
{
	return block_ser_if_block_track_lock_component_take(spdid, lock_id,
							    thd_id);
}

static inline void block_ser_if_client_fault_notification(int spdid)
{
	struct track_block *tb;

	do {
		if (sched_component_take(spdid))
			BUG();
	} while (0);

	if (EMPTY_LIST(&tracking_block_list[spdid], next, prev))
		goto done;

	for (tb = FIRST_LIST(&tracking_block_list[spdid], next, prev);
	     tb != &tracking_block_list[spdid];
	     tb = FIRST_LIST(tb, next, prev)) {

		do {
			if (sched_component_release(spdid))
				BUG();
		} while (0);

		lock_component_release(spdid, tb->lock_id);

		do {
			if (sched_component_take(spdid))
				BUG();
		} while (0);
	}

 done:
	do {
		if (sched_component_release(spdid))
			BUG();
	} while (0);

	return;
}

void __ser_client_fault_notification(int spdid)
{
	block_ser_if_client_fault_notification(spdid);
	return;
}
