/**
 Jiguo: a component that holds the mapping of file unique file id and
 its name (torrent path, tcp connection.....)
 */

#include <cos_component.h>
#include <sched.h>
#include <cos_synchronization.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <cbuf.h>
#include <mem_mgr_large.h>
#include <valloc.h>

#include <uniq_map.h>

static cos_lock_t uniq_map_lock;
#define LOCK() if (lock_take(&uniq_map_lock)) BUG();
#define UNLOCK() if (lock_release(&uniq_map_lock)) BUG();

typedef int uid;

COS_MAP_CREATE_STATIC(uniq_map_ids);

struct obj_id {
	uid id;
	char *name;
	struct obj_id *next, *prev;
};

struct obj_id obj_id_list;

#define MBOX_BUFFER_SZ 256   // this is for mbox RB 

/********************/
/*   trie           */
/********************/
#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])
 
// Alphabet size (# of symbols) and "\"  -- the path can be "foo/bar"
#define ALPHABET_SIZE (27)
// Converts key current character into index
// use only 'a' through 'z' and lower case
#define CHAR_TO_INDEX(c) ((int)c - (int)'a')
 
// trie node
typedef struct trie_node trie_node_t;
struct trie_node
{
	int value;
	int head;
	int tail;
	int cbid[MBOX_BUFFER_SZ];
	int size[MBOX_BUFFER_SZ];
	trie_node_t *children[ALPHABET_SIZE];
};
 
// trie ADT
typedef struct trie trie_t;
struct trie
{
	trie_node_t *root;
	int count;
};

trie_t trie;
 
// Returns new trie node (initialized to NULLs)
trie_node_t *
getNode(void)
{
	trie_node_t *pNode = NULL;
 
	pNode = (trie_node_t *)malloc(sizeof(trie_node_t));
 
	if( pNode )
	{
		int i;
 
		pNode->value = 0;

		for(i = 0; i < MBOX_BUFFER_SZ; i++)
		{
			pNode->cbid[i] = 0;
			pNode->size[i] = 0;
		}
		pNode->head = 0; // at the beginning of the cbid array
		pNode->tail = 0;

		for(i = 0; i < ALPHABET_SIZE; i++)
		{
			pNode->children[i] = NULL;
		}
	}
 
	return pNode;
}
 
// Initializes trie (root is dummy node)
static void 
initialize(trie_t *pTrie)
{
	pTrie->root = getNode();
	pTrie->count = 0;

	cos_map_add(&uniq_map_ids, pTrie->root);  // this will make the uid start from 1
}
 
// If not present, inserts key into trie
// If the key is prefix of trie node, just marks leaf node
static int
insert(trie_t *pTrie, char key[])
{
	int level;
	int length = strlen(key);
	int index;
	trie_node_t *pCrawl;
 
	pTrie->count++;
	pCrawl = pTrie->root;
 
	for( level = 0; level < length; level++ )
	{
		index = CHAR_TO_INDEX(key[level]);
		if( !pCrawl->children[index] )
		{
			pCrawl->children[index] = getNode();
		}
 
		pCrawl = pCrawl->children[index];
	}
 
	// mark last node as leaf
	/* pCrawl->value = pTrie->count; */
	/* unique id, starts from 0 */
	pCrawl->value = cos_map_add(&uniq_map_ids, pCrawl);
	pCrawl->cbid[pCrawl->head] = 0;
	pCrawl->size[pCrawl->head] = 0;
	assert(pCrawl->value >= 0);

	return 	pCrawl->value;
}
 
// Returns non zero, if key presents in trie
static int 
search(trie_t *pTrie, char key[])
{
	int level;
	int length = strlen(key);
	int index;
	int ret = 0;
	trie_node_t *pCrawl;
 
	pCrawl = pTrie->root;
 
	for( level = 0; level < length; level++ )
	{
		index = CHAR_TO_INDEX(key[level]);
 
		if( !pCrawl->children[index] )
		{
			return 0;
		}
 
		pCrawl = pCrawl->children[index];
	}
 
	if (pCrawl && pCrawl->value) {
		ret = pCrawl->value;
		/* printc("value %d\n", ret); */
	}
	
	return ret;
	/* return (0 != pCrawl && pCrawl->value); */
}

/* lookup uid for a name string */
uid
uniq_map_lookup(spdid_t spdid, cbuf_t cb, int sz)
{
	uid ret = -1;
	struct obj_id *item, *list;
	char *str, *d_str;
	int parent_tid = -1;
	int server_tid = -1;
	
	struct uniqmap_data *d;

	LOCK();
	
	d = (struct uniqmap_data *)cbuf2buf(cb, sz);
	parent_tid = d->parent_tid;
	server_tid = d->server_tid;
	str = d->data;	
	printc("str passed in uniqmap %s\n", str);
	*(str+sz) = '\0';  	/* previous cbuf might still contain data */

	printc("str passed in uniqmap %s\n", str);
	/* printc("ser_tid passed in uniqmap %d\n", server_tid); */
	printc("sz passed in uniqmap %d\n", sz);

	/* assert(0); */
	
	if (!(ret = search(&trie, str))) {
		ret = insert(&trie, str);
		printc("insert returns id %d\n", ret);
		assert(ret);
	}
	
	printc("uniq_map_lookup  %d\n", ret);

	UNLOCK();

	return ret;
}

/* public function. Only used by torrent for now, could be used by network... */
int 
uniq_map_add(spdid_t spdid, int uniq_id, cbuf_t cbid, int sz)
{
	int ret = 0;
	uid ret_uid = 0;
	struct obj_id *item;
	char *str, *param;
	trie_node_t *pCrawl;

	assert(uniq_id && cbid);
	
	LOCK();

	pCrawl = cos_map_lookup(&uniq_map_ids, uniq_id);
	assert(pCrawl);
	pCrawl->cbid[pCrawl->head] = cbid;
	pCrawl->size[pCrawl->head] = sz;
	pCrawl->head = (pCrawl->head+1)%MBOX_BUFFER_SZ;
	/* printc("\n\n uniq_map_add -- pCrawl->head %d\n\n", pCrawl->head); */

	/* int i; */
	/* printc("\n\n ******* map_add----\n"); */
	/* for (i = 0; i < 20; i++) { */
	/* 	printc("saved cbid %d\n", pCrawl->cbid[i]); */
	/* } */
	/* printc("*******\n\n"); */
	
	/* assert(0); */
	/* printc("cbid saved to the node is %d (%d th) (found a node in trie for nuqi_id %d)\n",  */
	/*        pCrawl->cbid[pCrawl->cb_indx], pCrawl->cb_indx, uniq_id); */

	UNLOCK();

	/* printc("get fid for str %s\n", str); */
	return ret;
}

int
uniq_map_del(spdid_t spdid, int uniq_id, cbuf_t cbid)
{
	struct obj_id *item, *list;
	trie_node_t *pCrawl;
	
	int ret = 0;
	
	assert(uniq_id && cbid);

	LOCK();

	pCrawl = cos_map_lookup(&uniq_map_ids, uniq_id);
	assert(pCrawl);
	pCrawl->cbid[pCrawl->tail] = 0;
	pCrawl->size[pCrawl->tail] = 0;
	pCrawl->tail = (pCrawl->tail+1)%MBOX_BUFFER_SZ;

	/* printc("\n\n uniq_map_del -- pCrawl->tail %d\n\n", pCrawl->tail); */

	/* int i; */
	/* printc("\n\n ******* map_del----\n"); */
	/* for (i = 0; i < 20; i++) { */
	/* 	printc("saved cbid %d\n", pCrawl->cbid[i]); */
	/* } */
	/* printc("*******\n\n"); */

	/* cos_map_del(&uniq_map_ids, uniq_id); */  // this should do in trelease

	UNLOCK();

	return ret;
}


// for mbox, return (cbid & 0xFF00 | sz & 0x00FF)
// if cnt is 0, return the total entries
// if cnt is not 0, return [cnt] value
int
uniq_map_reflection(spdid_t spdid, int uniq_id, int cnt)
{
	struct obj_id *item, *list;
	trie_node_t *pCrawl;
	
	int ret = 0;
	
	assert(uniq_id && spdid);

	LOCK();

	pCrawl = cos_map_lookup(&uniq_map_ids, uniq_id);
	assert(pCrawl);

	if (!cnt) {
		/* printc("\n\n uniq_map_reflection -- pCrawl->tail %d pCrawl->head %d\n\n",  */
		/*        pCrawl->tail, pCrawl->head); */
		ret = pCrawl->head - pCrawl->tail;
		/* printc("ret %d\n", ret); */
		assert(ret >= 0);
		if (!pCrawl->cbid[pCrawl->head - 1]) ret = 0;  // no data
	} else {
		int cbid = pCrawl->cbid[pCrawl->tail + cnt - 1];
		int sz   = pCrawl->size[pCrawl->tail + cnt - 1];
		ret = ((cbid << 16) & 0xFFFF0000) | (sz & 0xFFFF);
	}

	UNLOCK();

	/* printc("reflection uniq returns %p\n", ret); */
	return ret;
}

void 
cos_init(void *d)
{
	lock_static_init(&uniq_map_lock);
     
	cos_map_init_static(&uniq_map_ids);
	memset(&obj_id_list, 0, sizeof(struct obj_id)); /* initialize the head */
	INIT_LIST(&obj_id_list, next, prev);

	initialize(&trie); // initialize trie
	
	return;
}
 
