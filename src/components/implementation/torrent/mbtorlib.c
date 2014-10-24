/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */


// Jiguo: change the way how id is allocated here 

#include <mbtorlib.h>

static cos_lock_t fs_lock;
#define LOCK() if (lock_take(&fs_lock)) BUG();
#define UNLOCK() if (lock_release(&fs_lock)) BUG();

#define META_TD         "td"
#define META_OFFSET     "offset"
#define META_FLAGS      "flags"
#define META_EVTID      "evtid"

/* Default torrent implementations */
__attribute__((weak)) int
treadp(spdid_t spdid, int sz, int *off, int *len)
{
        return -ENOTSUP;
}
__attribute__((weak)) int
twritep(spdid_t spdid, td_t td, int cbid, int sz)
{
        return -ENOTSUP;
}

/* CVECT_CREATE_STATIC(tormap_vect);  // cache purpose */

/* void print_tormap_cvect() */
/* { */
/* 	int i; */
/* 	for (i = 0 ; i < (int)CVECT_BASE ; i++) { */
/* 		if (cvect_lookup(&tormap_vect, i))  */
/* 			printc("on cvect we found entry at %d\n", i); */
/* 	} */
/* } */

COS_MAP_CREATE_STATIC(torrents);
struct torrent null_torrent, root_torrent;

int
trmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, char *retval, unsigned int max_rval_len)
{
        /* spdid is not used ? */

        struct torrent *t;

        LOCK();
        t = tor_lookup(td);
        if (!t) {UNLOCK(); return -1;} // we need to have a unified return point which include an UNLOCK()

        if (strlen(key) != klen) return -1;

        if (strncmp(key, META_TD, klen) == 0) {
                sprintf(retval, "%d", t->td);
        }
        else if (strncmp(key, META_OFFSET, klen) == 0) {
                sprintf(retval, "%ld", (long)t->offset);
        }
        else if (strncmp(key, META_FLAGS, klen) == 0) {
                sprintf(retval, "%d", t->flags);
        }
        else if (strncmp(key, META_EVTID, klen) == 0) {
                sprintf(retval, "%ld", t->evtid);
        }
        else {UNLOCK(); return -1;}

        UNLOCK();
        if (strlen(retval) > max_rval_len) return -1;

        return strlen(retval);
}

int
twmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, const char *val, unsigned int vlen)
{
        /* spdid is not used ? */

        struct torrent *t;

        LOCK();
        t = tor_lookup(td);
        if (!t) {UNLOCK(); return -1;}

        if (strlen(key) != klen) return -1;
        if (strlen(val) != vlen) return -1;

        if(strncmp(key, META_TD, klen) == 0) {
                t->td = atoi(val); // type of td need to be confirmed
        }
        else if(strncmp(key, META_OFFSET, klen) == 0) {
                t->offset = atoi(val);
        }
        else if(strncmp(key, META_FLAGS, klen) == 0) {
                t->flags = atoi(val); // type of flags need to be confirmed
        }
        else if(strncmp(key, META_EVTID, klen) == 0) {
                t->evtid = atol(val); // type need to be confirment
        }
        else { UNLOCK(); return -1;}

        UNLOCK();
        return 0;
}

extern long ns_alloc(spdid_t spdid);

int tor_cons(struct torrent *t, void *data, int flags)
{
	td_t td;
	assert(t);
	
	td        = (td_t)cos_map_add(&torrents, t);
	if (td == -1) return -1;
	/* Using the separate name server will incur the invocation
	 * overhead. The better solution is still to maintain the
	 * client and server side id mappings over the client
	 * interface. */
	/* td = (td_t) ns_alloc(cos_spd_id()); */
	/* assert(td >= 0); */
	/* cvect_add(&tormap_vect, t, td); */
	/* t->cur_td = td;    // Jiguo: initialize */
	/* printc("cvect add..... --> id %d\n", td); */
	/* print_tormap_cvect(); */

	t->td     = td;
	t->data   = data;
	t->flags  = flags;
	t->offset = 0;

	return 0;
}

struct torrent *tor_alloc(void *data, int flags)
{
	struct torrent *t;

	t = malloc(sizeof(struct torrent));
	if (!t) return NULL;
	if (tor_cons(t, data, flags)) {
		free(t);
		return NULL;
	}
	return t;
}

/* will not deallocate ->data */
void tor_free(struct torrent *t)
{
	assert(t);
	if (cos_map_del(&torrents, t->td)) BUG();

	/* cvect_del(&tormap_vect, t->cur_td);   // remove the current entry */
	/* cvect_del(&tormap_vect, t->td);   // remove the original entry */
        /* for now, just keep allocating new id. Only delete all
	 * unsued ids when there is no more id to be allocated */
	/* ns_free(cos_spd_id(), t->td); */

	free(t);
}

static int first_fault = 0; // used to invalidate all entries on cache cvect
void torlib_init(void)
{
	cos_map_init_static(&torrents);
	/* save descriptors for the null and root spots */
	null_torrent.td = td_null;
	null_torrent.cur_td = td_null;
	if (td_null != cos_map_add(&torrents, NULL)) BUG();
	root_torrent.td = td_root;
	root_torrent.cur_td = td_root;
	if (td_root != cos_map_add(&torrents, &root_torrent)) BUG();

	/* cvect_init_static(&tormap_vect);  // Jiguo: for cache between faults */

        /* why need to delete such id?? After fault, we might find an
	 * entry at this position, so we delete first??? After fault,
	 * there are some entries occupied, due to BSS messed up since
	 * cache cvect is there? */
	/* if (unlikely(!first_fault)) { */
	/* 	first_fault = 1; */
	/* 	cvect_add(&tormap_vect, &null_torrent, td_null); */
	/* 	printc("very first before cvect add --> id %d\n", td_null); */
	/* 	print_tormap_cvect(); */
	/* 	int i; */
	/* 	for (i = 0 ; i < (int)CVECT_BASE ; i++) { */
	/* 		if (cvect_lookup(&tormap_vect, i)) cvect_del(&tormap_vect, i); */
	/* 	} */
	/* } */
	/* cvect_add(&tormap_vect, &null_torrent, td_null);	 */
	/* cvect_add(&tormap_vect, &root_torrent, td_root); */
	/* /\* ns_invalidate();   // removed all not received by client ids in name server *\/ */
}

