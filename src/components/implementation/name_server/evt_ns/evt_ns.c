/* 
   Jiguo: evt name server. Now the mapping between the old and new evt
   ids are cached in this component between faults (update after
   create a new id from name server and call name server to update the
   mapping between old id and new ids)
*/

#include <cos_component.h>
#include <sched.h>
#include <cos_synchronization.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <mem_mgr_large.h>
#include <valloc.h>

#include <name_server.h>

#include <c3_test.h>
#include <recovery_upcall.h>

static cos_lock_t uniq_map_lock;
#define LOCK() if (lock_take(&uniq_map_lock)) BUG();
#define UNLOCK() if (lock_release(&uniq_map_lock)) BUG();

typedef int uid;

struct evt_node {
	long id;      // client side id
	long actual_id; // current actual id

	/* The status that indicates if an event has been
	 * triggered. The issue is like this: the event manager can be
	 * corrupted after an event is triggered and woken up in
	 * evt_wait, but before it returns back to the client. In such
	 * as case, even the event can be rebuilt eagerly, we do not
	 * know if an event has been triggered. Under C^3, the thread
	 * that called evt_wait will call evt_wait again and will
	 * probably block again since it has no idea that the event
	 * has been triggered before the fault. 
	 * 
	 * Solution: since we call name_server in
	 * evt_trigger/evt_free/evt_wait anyway, we can update and
	 * check if an event has been triggered. If so, the upcall
	 * recovery thread can upcall to do evt_trigger instead. It
	 * seems that evt_trigger does not care where this trigger
	 * comes from and which thread this trigger is made by.
	 *
	 * status 0 -- not triggered
	 * status 1 -- triggered
	 *	 
	 */
	int status;    // not used any more. No way to change this every time
	
	/* where the event is created/split. Not used anymore due to
	 * the eager recovery (simply upcall to a component and
	 * recover any events being tracked) */
	int owner;   
	
};

unsigned int spd_evts_cnt[MAX_NUM_SPDS];

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>
CSLAB_CREATE(evt, sizeof(struct evt_node));
COS_MAP_CREATE_STATIC(ns_evtids);

static long mapping_create(struct evt_node *e)
{
	return cos_map_add(&ns_evtids, e);
}

static inline struct evt_node *mapping_find(long id)
{
	struct evt_node *e = cos_map_lookup(&ns_evtids, id);
	if (NULL == e) return e;
	/* printc("e->id %ld id %ld\n", e->id, id); */
	assert(e->id == id);
	return e;
}

static void mapping_free(long id)
{
	if (cos_map_del(&ns_evtids, id)) BUG();
}


long
ns_alloc(spdid_t server_spd, spdid_t cli_spdid)
{
	int ret = -1;
	struct evt_node *en;

	if (cos_get_thd_id() != 2) LOCK();
	/* LOCK(); */

	en = cslab_alloc_evt();
	assert(en);	
	ret = mapping_create(en);
	assert(ret >= 1);
	en->id       = ret;
	en->actual_id  = ret;
	en->owner    = cli_spdid;
	/* en->status    = 0; */

	spd_evts_cnt[cli_spdid]++;
	/* printc("evt name server getting id %d (from spd %d)\n", ret, cli_spdid); */

	if (cos_get_thd_id() != 2) UNLOCK();
	/* UNLOCK(); */

	return ret;
}

/* Do not do this until cos_map of evtids are full. Then we need
 * deallocate all those entries not being used, which requires to look
 * up in event manger to find who are being used. For now, just assume
 * this does not happen */
int 
ns_free(spdid_t spdid, spdid_t cli_spdid, int id) 
{
	int ret = -1;
	struct evt_node *head, *m, *tmp;

	/* printc("(1) evt name server delete id %d\n", id); */

	LOCK();

	// the fault could happen after the entry has been removed!!!
	m = mapping_find(id);
	if (!m) goto done;
	/* printc("(1) ns_free from spd %d: passed id %d m->id %d m->actual_id %d\n",  */
	/*        spdid, id, m->id, m->actual_id); */
	if ((tmp = mapping_find(m->actual_id)) && tmp != m) {
		/* printc("(2) evt name server delete tmp id %d m->next id %d\n",  */
		/*        tmp->id, m->actual_id); */
		mapping_free(tmp->id);
		tmp->actual_id = 0;
		tmp->id = 0;
		cslab_free_evt(tmp);
		spd_evts_cnt[cli_spdid]--;
	}
	/* printc("(3) evt name server delete id %d\n", m->id); */
	mapping_free(m->id);	
	/* m->status = 0; // ensure that the event status is not "triggered"  */
	m->actual_id = 0;
	m->id = 0;
	cslab_free_evt(m);
	spd_evts_cnt[cli_spdid]--;
	assert(spd_evts_cnt[cli_spdid] >= 0);
	ret = 0;
done:
	UNLOCK();
	return ret;
}

/* This function links the new id with the client id, (due to a
 * fault), and only called on the recovery path. However, we need deal
 * with the situation described for "status" above. The return value
 * is used to indicate if the event status needs to be updated to
 * "triggered" in evt manager.
 *
*/
int
ns_update(spdid_t spdid, int cli_id, int cur_id, long par) 
{
	struct evt_node *en_cli, *en_cur, *tmp;
	int ret = -1;

	/* printc("evt_ns: ns_update\n"); */
	assert(cos_get_thd_id() == 2);
	/* LOCK(); */

	en_cli = mapping_find(cli_id);
	if (!en_cli) goto done;
	en_cur = mapping_find(cur_id);
	if (!en_cur) goto done;
	
	assert(en_cur->actual_id == en_cur->id);
	
	// remove the previous pointed one if it is not itself
	if ((tmp = mapping_find(en_cli->actual_id)) && tmp != en_cli) {
		mapping_free(tmp->id);
		tmp->actual_id = 0;
		tmp->id = 0;
		cslab_free_evt(tmp);
	}
	// point to each other (current to the new created one)
	en_cli->actual_id = en_cur->id;
	en_cur->actual_id = en_cli->id;
	/* printc("evt_ns: ns_setid done\n"); */

	ret = 0;
	/* if (en_cur->status == 1 || en_cli->status == 1) ret = 1; */
	/* else ret = 0;	 */
done:
	/* UNLOCK(); */
	return ret;
}

/* This function returns current for the original id */
long
ns_lookup(spdid_t spdid, int id) 
{
	int ret = 0;
	struct evt_node *curr, *tmp;

	/* printc("evt_ns: ns_lookup (id %d by thd %d)\n", id, cos_get_thd_id()); */

	LOCK();

	curr = mapping_find(id);
	/* if (!curr) { */
	/* 	printc("evt_ns: ns_lookup can not find new id for old id %d\n", id); */
	/* 	ret = -22; */
	/* 	goto done; */
	/* } */
	/* if (!curr) goto done; */
	assert(curr);
	ret = curr->actual_id;
	/* printc("evt_ns: ns_lookup done. Fonud new id %d for old id %d\n", ret, id); */
done:
	UNLOCK();
	return ret;
}

/* For now, this reflection function is used to check if an entry is
 * presented type 0: check if entry for id is presented type 1: check
 * how many times the server has failed (at least the entry for a
 * cli_id sees) 
*/
long
ns_reflection(spdid_t spdid, int id, int type) 
{
/* 	printc("evt name server reflection id %d\n", id); */

/* 	LOCK();	 */

/* 	struct evt_node *en, *cur; */
/* 	long ret = -1; */

/* 	switch (type) { */
/* 	case 0: */
/* 		if (!mapping_find(id)) goto done; */
/* 		break; */
/* 	case 1: */
/* 		en = mapping_find(id); */
/* 		assert(en); */
/* 		cur = FIRST_LIST(en, next, prev); */
/* 		assert(cur); */
/* 		ret = cur->ser_fcounter; */
/* 		printc("get ser_fcounter %ld (for old event id %d)\n", ret, id); */
/* 		goto done; */
/* 	default: */
/* 		break; */
/* 	} */

/* 	ret = 0; */
/* done: */
/* 	UNLOCK(); */
/* 	return ret; */
	return 0;
}

/* The function used to upcall to each client to rebuild each event
 * state. This is only the faulty path and no need to take the lock */
int
ns_upcall(spdid_t spdid)
{
	int ret = 0;
	
	/* LOCK(); */
	
	/* printc("evt_ns: ready to upcall (thd %d call from spd %d)\n", */
	/*        cos_get_thd_id(), spdid); */
	
	int i;
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		if (!spd_evts_cnt[i]) continue;
		/* UNLOCK(); */
		/* printc("evt_ns has found spd %d has created evt (counter %d)\n", */
		/*        i, spd_evts_cnt[i]); */
		recovery_upcall(cos_spd_id(), COS_UPCALL_RECEVT, i, 0);

		/* LOCK(); */
	}
done:
	/* UNLOCK(); */
	return ret;
}

void 
cos_init(void *d)
{
	lock_static_init(&uniq_map_lock);
	cos_map_init_static(&ns_evtids);
	if (mapping_create((void*)1) != 0) BUG();

	// initialize per spd events tracking array
	int i;
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		spd_evts_cnt[i] = 0;
	}

	return;
}
 
