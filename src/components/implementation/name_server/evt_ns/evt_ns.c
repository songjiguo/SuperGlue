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

static cos_lock_t uniq_map_lock;
#define LOCK() if (lock_take(&uniq_map_lock)) BUG();
#define UNLOCK() if (lock_release(&uniq_map_lock)) BUG();

typedef int uid;

struct evt_node {
	long id;      // client side id
	long next_id; // current actual id

	int spdid;    // where the event is created/split
};

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

	LOCK();

	en = cslab_alloc_evt();
	assert(en);	
	ret = mapping_create(en);
	assert(ret >= 1);
	en->id       = ret;
	en->next_id  = ret;

	en->spdid    = cli_spdid;
	/* printc("evt name server getting id %d (from spd %d)\n", ret, cli_spdid); */

	UNLOCK();

	return ret;
}

/* Do not do this until cos_map of evtids are full. Then we need
 * deallocate all those entries not being used, which requires to look
 * up in event manger to find who are being used. For now, just assume
 * this does not happen */
int 
ns_free(spdid_t spdid, int id) 
{
	int ret = -1;
	struct evt_node *head, *m, *tmp;

	/* printc("(1) evt name server delete id %d\n", id); */

	LOCK();

	// the fault could happen after the entry has been removed!!!
	m = mapping_find(id);
	if (!m) goto done;
	printc("(1) ns_free from spd %d: passed id %d m->id %d m->next_id %d\n", 
	       spdid, id, m->id, m->next_id);
	if ((tmp = mapping_find(m->next_id)) && tmp != m) {
		printc("(2) evt name server delete tmp id %d m->next id %d\n", 
		       tmp->id, m->next_id);
		mapping_free(tmp->id);
		cslab_free_evt(tmp);
	}
	printc("(3) evt name server delete id %d\n", m->id);
	mapping_free(m->id);
	cslab_free_evt(m);
	ret = 0;
done:
	UNLOCK();
	return ret;
}

/* this function links the new id with the client id, (after create
 * new id normally or due to a fault). */
int
ns_update(spdid_t spdid, int cli_id, int cur_id, long par) 
{
	struct evt_node *en_cli, *en_cur, *tmp;

	/* printc("evt_ns: ns_update\n"); */
	LOCK();

	en_cli = mapping_find(cli_id);
	assert(en_cli);
	en_cur = mapping_find(cur_id);
	assert(en_cur && en_cur->next_id == en_cur->id);
	
	if ((tmp = mapping_find(en_cli->next_id)) && tmp != en_cli) {
		mapping_free(tmp->id);
		cslab_free_evt(tmp);
	}
	// point to each other
	en_cli->next_id = en_cur->id;
	en_cur->next_id = en_cli->id;
	/* printc("evt_ns: ns_setid done\n"); */

	UNLOCK();

	return 0;
}

/* This function returns current for the original id */
long
ns_lookup(spdid_t spdid, int id) 
{
	int ret = 0;
	struct evt_node *curr, *tmp;

	printc("evt_ns: ns_lookup (id %d by thd %d)\n", id, cos_get_thd_id());

	LOCK();

	curr = mapping_find(id);
	/* if (!curr) { */
	/* 	printc("evt_ns: ns_lookup can not find new id for old id %d\n", id); */
	/* 	ret = -22; */
	/* 	goto done; */
	/* } */
	/* if (!curr) goto done; */
	assert(curr);
	ret = curr->next_id;
	printc("evt_ns: ns_lookup done. Fonud new id %d for old id %d\n", ret, id);
done:
	UNLOCK();
	return ret;
}

/* for now, this reflection function is used to check if an entry is
 * presented 
 type 0: check if entry for id is presented
 type 1: check how many times the server has failed (at least the entry for a cli_id sees)

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

void 
cos_init(void *d)
{
	lock_static_init(&uniq_map_lock);
	cos_map_init_static(&ns_evtids);
	if (mapping_create((void*)1) != 0) BUG();
	return;
}
 
