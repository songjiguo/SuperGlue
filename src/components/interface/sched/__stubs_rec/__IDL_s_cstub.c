/* IDL generated code ver 0.1 ---  Fri Nov 27 10:24:09 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <sched.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#include <../../../implementation/sched/cos_sched_sync.h>

struct track_block {
	int thdid;
	struct track_block *next, *prev;
};
struct track_block tracking_block_list[MAX_NUM_SPDS];
unsigned long thd_tracking_data[MAX_NUM_THREADS];

static inline int block_ser_if_block_track_sched_block(spdid_t spdid,
						       u16_t dependency_thd)
{
	int ret = 0;
	struct track_block tb;	// track on stack

	cos_sched_lock_take();
	;

	if (unlikely(!tracking_block_list[spdid].next)) {
		INIT_LIST(&tracking_block_list[spdid], next, prev);
	}
	INIT_LIST(&tb, next, prev);
	tb.thdid = cos_get_thd_id();
	ADD_LIST(&tracking_block_list[spdid], &tb, next, prev);

	cos_sched_lock_release();
	;

	ret = sched_block(spdid, dependency_thd);

	cos_sched_lock_take();
	;

	REM_LIST(&tb, next, prev);

	cos_sched_lock_release();
	;

	return ret;
}

int __ser_sched_block(spdid_t spdid, u16_t dependency_thd)
{
	return block_ser_if_block_track_sched_block(spdid, dependency_thd);
}

static inline void block_ser_if_save_data()
{
	unsigned long ret = 0;
	ret = sched_timestamp();
	thd_tracking_data[cos_get_thd_id()] = ret;
	return;
}

ul_t __ser_sched_save_data_sched_timestamp()
{
	block_ser_if_save_data();
	return;
}

static inline ul_t block_ser_if_restore_data()
{
	return thd_tracking_data[cos_get_thd_id()];
}

ul_t __ser_sched_restore_data_sched_timestamp()
{

	return block_ser_if_restore_data();
}

static inline void block_ser_if_client_fault_notification(int spdid)
{
	struct track_block *tb;

	cos_sched_lock_take();
	;

	if (!tracking_block_list[spdid].next)
		goto done;
	if (EMPTY_LIST(&tracking_block_list[spdid], next, prev))
		goto done;

	for (tb = FIRST_LIST(&tracking_block_list[spdid], next, prev);
	     tb != &tracking_block_list[spdid];
	     tb = FIRST_LIST(tb, next, prev)) {

		cos_sched_lock_release();
		;

		sched_wakeup(spdid, tb->thdid);

		cos_sched_lock_take();
		;
	}

 done:
	cos_sched_lock_release();
	;

	return;
}

void __ser_sched_client_fault_notification(int spdid)
{
	block_ser_if_client_fault_notification(spdid);
	return;
}
