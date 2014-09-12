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

COS_MAP_CREATE_STATIC(ns_evtids);

/* A list that tracks all ids occupied by the same event */
struct evt_node {
	long id;
	spdid_t spdid;
	long owner_id;   // first event id (used to delete all received ids)

        /* indicate that the client has received this id since the
	 * evt_create might fail in evt_manager before it returns back
	 * to the client, but after create the new id in name
	 * server */
	unsigned int received;  

	struct evt_node *next, *prev;
};

static long mapping_create(struct evt_node *e)
{
	return cos_map_add(&ns_evtids, e);
}

static inline struct evt_node *mapping_find(long id)
{
	struct evt_node *e = cos_map_lookup(&ns_evtids, id);
	if (NULL == e) return e;
	printc("e->id %ld id %ld\n", e->id, id);
	assert(e->id == id);
	return e;
}

static void mapping_free(long id)
{
	if (cos_map_del(&ns_evtids, id)) BUG();
}


int 
ns_getid(spdid_t spdid) 
{
	int ret = -1;
	struct evt_node *en;

	LOCK();

	en = (struct evt_node *)malloc(sizeof(struct evt_node));
	if (!en) goto done;
	
	INIT_LIST(en, next, prev);

	ret = mapping_create(en);
	en->id       = ret;
	en->owner_id = ret;   // this needs to be updated in ns_setid
	en->spdid    = spdid;
	en->received = 0;     // client not see this id yet
	printc("evt name server getting id %d\n", ret);
done:
	UNLOCK();
	return ret;
}

int 
ns_delid(spdid_t spdid, int id) 
{
	int ret = -1;
	struct evt_node *head, *m;

	printc("evt name server delete id %d\n", id);

	LOCK();
	
	m = mapping_find(id);
	if (!m) goto done;
	printc("found m (m id %ld and owner id %ld)\n", m->id, m->owner_id);
	if (m->owner_id != m->id) head = mapping_find(m->owner_id);
	else head = m;
	assert(head);
	printc("found head (head id %ld)\n", head->id);
	
	while(!EMPTY_LIST(head, next, prev)) {
		struct evt_node *en = NULL;
		en = FIRST_LIST(head, next, prev);
		REM_LIST(en, next, prev);
		mapping_free(en->id);
		free(en);
	}
	mapping_free(head->id);	
	free(head);
done:
	UNLOCK();
	return ret;
}

/* this function links the new id with the client id, only called
 * after fault (in evt manager after create new id due to a fault)
 * Also, we invalidate the old cli_id entry, so the lookup will see
*/
int
ns_setid(spdid_t spdid, int cli_id, int cur_id) 
{
	int ret = -1;
	struct evt_node *en_cli, *en_cur;

	printc("evt_ns: ns_setid\n");

	LOCK();

	en_cli = mapping_find(cli_id);
	if (unlikely(!en_cli)) goto done;

	if (cli_id == cur_id) {
		printc("set received for id %d\n", cli_id);
		en_cli->received = 1; // now we know that client has seen this id
		ret = 0;
		goto done;
	}

	en_cur = mapping_find(cur_id);
	if (!en_cur) goto done;
	printc("set received for id %d\n", cur_id);
	en_cur->received = 1; // now we know that client has seen this id after fault

	ADD_LIST(en_cli, en_cur, next, prev);
	
	/* struct evt_node *tmp = en_cli; */
	/* printc("[[[]]]\n"); */
	/* while(tmp) { */
	/* 	printc("found id %ld on list\n", tmp->id); */
	/* 	tmp = tmp->next; */
	/* 	if (tmp == en_cli) break; */
	/* } */
done:
	printc("evt_ns: ns_setid done\n");
	UNLOCK();
	return 0;
}

/* call this after the fault, or in cos_init once */
int
ns_del_norecevied() 
{
	int ret = -1;
	int i;
	struct evt_node *m;
	
	printc("evt name server delete not recevied\n");
	
	LOCK();
	
	for (i = 0 ; i < (int)COS_MAP_BASE ; i++) {
		m = mapping_find(i);
		if (m && !m->received) {
			printc("delete a not received %d\n", i);
			cos_map_del(&ns_evtids, i);
		}
	}

	UNLOCK();
	return ret;
}

/* for now, this reflection function is used to check if an entry is
 * presented */
int
ns_reflection(spdid_t spdid, int id) 
{
	printc("evt name server reflection id %d\n", id);

	LOCK();	

	int ret = -1;
	if (!mapping_find(id)) goto done;
	ret = 0;
done:
	UNLOCK();
	return ret;
}

void 
cos_init(void *d)
{
	lock_static_init(&uniq_map_lock);
	cos_map_init_static(&ns_evtids);
	if (mapping_create(NULL) != 0) BUG();
	return;
}
 
