/* 
   Jiguo: mail box name server */

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

COS_MAP_CREATE_STATIC(ns_mboxids);

/* A list that tracks all ids occupied by the same event */
struct mbox_node {
	long id;
	spdid_t spdid;
        /* indicate that the client has received this id since the
	 * mbox_create might fail in mbox_manager before it returns back
	 * to the client, but after create the new id in name
	 * server */
	unsigned int received;  

	/* see the explanation in client interface, this will keep how
	 * many times a spd has failed before an object is rebuilt */
	long ser_fcounter;

	struct mbox_node *next, *prev;
};

static long mapping_create(struct mbox_node *e)
{
	return cos_map_add(&ns_mboxids, e);
}

static inline struct mbox_node *mapping_find(long id)
{
	struct mbox_node *e = cos_map_lookup(&ns_mboxids, id);
	if (NULL == e) return e;
	printc("e->id %ld id %ld\n", e->id, id);
	assert(e->id == id);
	return e;
}

static void mapping_free(long id)
{
	if (cos_map_del(&ns_mboxids, id)) BUG();
}


long
ns_alloc(spdid_t spdid) 
{
	int ret = -1;
	struct mbox_node *en;

	LOCK();

	en = (struct mbox_node *)malloc(sizeof(struct mbox_node));
	if (!en) goto done;
	
	INIT_LIST(en, next, prev);

	ret = mapping_create(en);
	en->id       = ret;
	en->spdid    = spdid;
	en->received = 0;     // client not see this id yet
	en->ser_fcounter = 0;
	printc("mbox name server getting id %d\n", ret);
done:
	UNLOCK();
	return ret;
}

/* Do not do this until cos_map of mboxids are full. Then we need
 * deallocate all those entries not being used, which requires to look
 * up in event manger to find who are being used. For now, just assume
 * this does not happen */
int 
ns_free(spdid_t spdid, int id) 
{
	int ret = -1;
	struct mbox_node *head, *m;

	printc("mbox name server delete id %d\n", id);

	LOCK();
	
	m = mapping_find(id);
	if (!m) goto done;
	
	while(!EMPTY_LIST(m, next, prev)) {
		struct mbox_node *en = FIRST_LIST(m, next, prev);
		REM_LIST(en, next, prev);
		mapping_free(en->id);
		free(en);
	}
	mapping_free(m->id);	
	free(m);
	ret = 0;
done:
	UNLOCK();
	return ret;
}

/* this function links the new id with the client id, (after create
 * new id normally or due to a fault). For now, par is ser_fcounter
 * passed here by client interface 

 Note: the order in which entry is added does not matter, since we
       always refer to the last added entry as long as the lock is
       taken here (in the case of preemption in the client) */
int
ns_update(spdid_t spdid, int cli_id, int cur_id, long par) 
{
	int ret = -1;
	struct mbox_node *en_cli, *en_cur;

	printc("mbox_ns: ns_update\n");

	LOCK();

	en_cli = mapping_find(cli_id);
	if (unlikely(!en_cli)) goto done;

	/* normal path on create */
	if (cli_id == cur_id) {  
		printc("set received for id %d\n", cli_id);
		en_cli->received = 1; // now we know that client has seen this id
		en_cli->ser_fcounter = par;
		ret = 0;
		goto done;
	}
	
        /* fault path on create */
	en_cur = mapping_find(cur_id);
	if (!en_cur) goto done;
	en_cur->received     = 1; // now we know that client has seen this id after fault
	printc("set event id %d 's ser_fcounter to be %ld\n", cur_id, par);
	en_cur->ser_fcounter = par;

	ADD_LIST(en_cli, en_cur, next, prev);
	
	ret = 0;
	/* struct mbox_node *tmp = en_cli; */
	/* printc("[[[]]]\n"); */
	/* while(tmp) { */
	/* 	printc("found id %ld on list\n", tmp->id); */
	/* 	tmp = tmp->next; */
	/* 	if (tmp == en_cli) break; */
	/* } */
done:
	printc("mbox_ns: ns_setid done\n");
	UNLOCK();
	return 0;
}

/* This function returns current for the original id */
long
ns_lookup(spdid_t spdid, int id) 
{
	int ret = 0;
	struct mbox_node *curr, *tmp;

	printc("mbox_ns: ns_lookup\n");

	LOCK();

	curr = mapping_find(id);
	if (!curr) goto done;
	tmp = FIRST_LIST(curr, next, prev);
	assert(tmp);
	ret = tmp->id;
	printc("mbox_ns: ns_lookup done. Fonud id %d\n", ret);
done:
	UNLOCK();
	return ret;
}

/* call this after the fault, or in cos_init once */
int
ns_invalidate() 
{
	int ret = -1;
	int i;
	struct mbox_node *m;
	
	printc("mbox name server delete not recevied\n");
	
	LOCK();
	
	for (i = 0 ; i < (int)COS_MAP_BASE ; i++) {
		m = mapping_find(i);
		if (m && !m->received) {
			printc("delete a not received %d\n", i);
			cos_map_del(&ns_mboxids, i);
		}
	}

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
	printc("mbox name server reflection id %d\n", id);

	LOCK();	

	struct mbox_node *en, *cur;
	long ret = -1;

	switch (type) {
	case 0:
		if (!mapping_find(id)) goto done;
		break;
	case 1:
		en = mapping_find(id);
		assert(en);
		cur = FIRST_LIST(en, next, prev);
		assert(cur);
		ret = cur->ser_fcounter;
		printc("get ser_fcounter %ld (for old event id %d)\n", ret, id);
		goto done;
	default:
		break;
	}

	ret = 0;
done:
	UNLOCK();
	return ret;
}

void 
cos_init(void *d)
{
	lock_static_init(&uniq_map_lock);
	cos_map_init_static(&ns_mboxids);
	/* struct mbox_node *en; */
	/* en = (struct mbox_node *)malloc(sizeof(struct mbox_node)); */
	/* assert(en);	 */
	mapping_create(NULL); // reserved for null torrent
	mapping_create(NULL);  // reserved for root torrent
	return;
}
 
