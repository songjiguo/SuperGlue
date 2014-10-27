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

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
#define C_TAKE(spdid) 	do { if (sched_component_take(spdid))    return 0; } while (0)
#define C_RELEASE(spdid)	do { if (sched_component_release(spdid)) return 0; } while (0)

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

// the list of all event ids that be waited through this interface
struct trigger_evt {
	long		 evtid;
	struct trigger_evt *next, *prev;
};
struct trigger_evt *evts_head = NULL;
CSLAB_CREATE(te, sizeof(struct trigger_evt));

static struct trigger_evt *
te_alloc(int eid)
{
	struct trigger_evt *te;

	te = cslab_alloc_te();
	assert(te);
	te->evtid = eid;
	INIT_LIST(te, next, prev);

	return te;
}

static void
te_dealloc(struct trigger_evt *te)
{
	assert(te);
	cslab_free_te(te);
	return;
}

long __sg_evt_wait(spdid_t spdid, long extern_evt)
{
	long ret = 0;
	struct trigger_evt te;
	assert(spdid && extern_evt);

	C_TAKE(cos_spd_id());
	/* printc("thd %d is going to evt_wait\n", cos_get_thd_id()); */

	if (unlikely(!evts_head)) {
		/* printc("Init evts_head\n"); */
		evts_head = te_alloc(0);
		assert(evts_head);
	}
	
	/* te = te_alloc(extern_evt); */
	/* assert(te); */
	te.evtid = extern_evt;
	/* printc("add to the list\n"); */
	ADD_LIST(evts_head, &te, next, prev);
	/* printc("add to the list done %p \n", (void *)evts_head->next->evtid); */
	
	C_RELEASE(cos_spd_id());
	ret = evt_wait(spdid, extern_evt);
	C_TAKE(cos_spd_id());

	/* printc("return from evt_wait....(thd %d)\n", cos_get_thd_id()); */

        /* te is still on stack and will be popped off when return */
	REM_LIST(&te, next, prev);  	

	C_RELEASE(cos_spd_id());
	return ret;
}


int __sg_evt_trigger_all(spdid_t spdid)
{
	long ret = 0;
	
	/* printc("thread %d is going to trigger all events\n", cos_get_thd_id()); */
	
	C_TAKE(cos_spd_id());

	/* evt_trigger evt_ids tracked for all thd block waited
	 * through this interface */
	if (!evts_head) goto done;
	/* printc("mbox triggers all evts in spd %ld\n", cos_spd_id()); */
	struct trigger_evt *evt_t, *evt_next;
	for (evt_t = FIRST_LIST(evts_head, next, prev);
	     evt_t != evts_head;
	     evt_t = FIRST_LIST(evt_t, next, prev)){
		/* printc("found evt id %ld to trigger\n", evt_t->evtid); */
		evt_trigger(cos_spd_id(), evt_t->evtid);
	}
	/* printc("trigger all events done (thd %d)\n\n", cos_get_thd_id()); */

done:
	C_RELEASE(cos_spd_id());
	return ret;
}
