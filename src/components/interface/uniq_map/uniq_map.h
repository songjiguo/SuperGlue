/**
   mapping unique id and path for torrent
 */

#ifndef UNIQMAP_H
#define UNIQMAP_H

#include <cos_component.h>

/* Data structure used to pass path name and server side tid here */
struct uniqmap_data {
	int server_tid;
	int sz;
	char data[0];
};

int uniq_map_add(spdid_t spdid, int uniq_id, cbuf_t cb, int sz);
int uniq_map_del(spdid_t spdid, int uniq_id, cbuf_t cbid);
int uniq_map_lookup(spdid_t spdid, cbuf_t cb, int sz);

// for mbox, return (cbid & 0xFF00 | sz & 0x00FF)
int uniq_map_reflection(spdid_t spdid, int uniq_id, int cnt);


#endif /* UNIQMAP_H*/ 
