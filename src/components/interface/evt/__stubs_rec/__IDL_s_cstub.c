/* IDL generated code ver 0.1 ---  Mon Nov  2 20:22:07 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <evt.h>

struct track_block {
	int evtid;
	struct track_block *next, *prev;
};
struct track_block tracking_block_list[MAX_NUM_SPDS];

static inline int block_ser_if_block_track_evt_wait(spdid_t spdid, long evtid)
{
	int ret = 0;
	struct track_block tb;	// track on stack

	;

	if (unlikely(!tracking_block_list[spdid].next)) {
		INIT_LIST(&tracking_block_list[spdid], next, prev);
	}
	INIT_LIST(&tb, next, prev);
	tb.evtid = evtid;
	ADD_LIST(&tracking_block_list[spdid], &tb, next, prev);

	;

	ret = evt_wait(spdid, evtid);

	;

	REM_LIST(&tb, next, prev);

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

	;

	if (!tracking_block_list[spdid].next)
		goto done;
	if (EMPTY_LIST(&tracking_block_list[spdid], next, prev))
		goto done;

	for (tb = FIRST_LIST(&tracking_block_list[spdid], next, prev);
	     tb != &tracking_block_list[spdid];
	     tb = FIRST_LIST(tb, next, prev)) {

		;

		evt_trigger(spdid, tb->evtid);

		;
	}

 done:
	;

	return;
}

void __ser_evt_client_fault_notification(int spdid)
{
	block_ser_if_client_fault_notification(spdid);
	return;
}
