/*
  Jiguo: reflection interface for event component. This interface will
  be used with any component that have threads blocked via
  evt_wait. So when that component has fault, the blocked threads can
  be woken up via here by triggering all events

  Note: this interface should be set by set_symbol_link together with
  other interface in recovery mode.

  Note: for now, just assume all threads are tracked. TODO: per
  component track */

#include <cos_component.h>
#include <evt.h>
#include <print.h>
#include <cos_list.h>

#include <name_server.h>

#include <cos_synchronization.h>
extern cos_lock_t evt_lock;

// the list of all event ids that be waited through this interface
struct trigger_evt {
	long		 evtid;
	struct trigger_evt *next, *prev;
};
struct trigger_evt evts_head[MAX_NUM_SPDS];

static int first_interface_lock = 0;

extern int ns_update(spdid_t spdid, int new_id, int old_id);

long __sg_evt_split_exist(spdid_t spdid, long parent, int group, long existing_id)
{
	long ret = 0;
	/* printc("ser:evt_split_exist passed in parent %d grp %d existing_id %d\n", */
	/*        parent, group, existing_id); */
	ret = evt_split_exist(spdid, parent, group, existing_id);
	
	return ret;
}


long __sg_evt_split(spdid_t spdid, long parent, int group)
{
	/* printc("ser:evt_split passed in parent %d grp %d\n",  */
	/*        parent, group); */
	return evt_split(spdid, parent, group);
}


long __sg_evt_wait(spdid_t spdid, long extern_evt)
{
	long ret = 0;
	struct trigger_evt te;
	assert(spdid && extern_evt);

	if (unlikely(!first_interface_lock)) {
		first_interface_lock = 1;
		lock_static_init(&evt_lock);
	}
	if (unlikely(!evts_head[spdid].next)) {
		INIT_LIST(&evts_head[spdid], next, prev);
	}

	lock_take(&evt_lock);
	
	INIT_LIST(&te, next, prev);
	te.evtid = extern_evt;
	ADD_LIST(&evts_head[spdid], &te, next, prev);

	lock_release(&evt_lock);
	ret = evt_wait(spdid, extern_evt);
	lock_take(&evt_lock);

	/* printc("return from evt_wait....(thd %d)\n", cos_get_thd_id()); */
        /* te is still on stack and will be popped off when return */
	REM_LIST(&te, next, prev);
	lock_release(&evt_lock);
	/* printc("evt_wait interface overhead %llu\n",  */
	/*        overhead_end - overhead_start + tmp_overhead); */

	return ret;
}


int __sg_evt_reflection(spdid_t spdid)
{
	long ret = 0;
	
	/* printc("thread %d is going to trigger all events\n", cos_get_thd_id()); */

	if (unlikely(!first_interface_lock)) {
		first_interface_lock = 1;
		lock_static_init(&evt_lock);
	}
	
	lock_take(&evt_lock);

	/* evt_trigger evt_ids tracked for all thd block waited
	 * through this interface */
	if (EMPTY_LIST(&evts_head[spdid], next, prev)) goto done;
	/* printc("mbox triggers all evts in spd %ld\n", cos_spd_id()); */
	struct trigger_evt *evt_t, *evt_next;
	for (evt_t = FIRST_LIST(&evts_head[spdid], next, prev);
	     evt_t != &evts_head[spdid];
	     evt_t = FIRST_LIST(evt_t, next, prev)){
		/* printc("found evt id %ld to trigger\n", evt_t->evtid); */
		evt_trigger(cos_spd_id(), evt_t->evtid);
	}
	/* printc("trigger all events done (thd %d)\n\n", cos_get_thd_id()); */

done:
	lock_release(&evt_lock);
	return ret;
}


extern int ns_upcall(spdid_t spdid, int id);

int __sg_evt_upcall_creator(spdid_t spdid, int evtid)
{
	int ret = 0;
	
	ns_upcall(spdid, evtid);
	
	return ret;
}
