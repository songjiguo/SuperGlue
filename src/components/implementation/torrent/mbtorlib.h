/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef MTORLIB_H
#define MTORLIB_H

#include <../../interface/mbtorrent/mbtorrent.h>
#include <cos_map.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct torrent {
	td_t td;

	td_t par_td;   // Jiguo: used to find the parent torrent
	td_t cur_td;   // Jiguo: used to find the current torrent

	u32_t offset;
	int flags;
	long evtid;
	void *data;
};
extern cos_map_t torrents;
extern struct torrent null_torrent, root_torrent;

/* extern cvect_t tormap_vect; */
/* extern void print_tormap_cvect(); */

/* static inline struct torrent * */
/* tor_lookup(td_t td) */
/* { */
/* 	struct torrent *t; */
	
/* 	printc("tor lookup: tor id %d\n", td); */
/* 	print_tormap_cvect(); */
/* 	t = cvect_lookup(&tormap_vect, td); */
	
/* 	if (unlikely(!t)) return NULL; */
/* 	struct torrent *curr= cvect_lookup(&tormap_vect, t->cur_td); */
/* 	assert(curr); */
/* 	printc("tor_lookup: found old id %d (new id %d)\n", td, t->cur_td); */
/* 	return curr; */
/* } */

static inline struct torrent *
tor_lookup(td_t td)
{
	struct torrent *t;
	
	t = cos_map_lookup(&torrents, td);
	if (!t) return NULL;
	assert(t->td == td);

	return t;
}


static inline int 
tor_isnull(td_t td)
{
	return td == td_null;
}

static inline int
tor_is_usrdef(td_t td)
{
	return !(td == td_null || td == td_root);
}

int tor_cons(struct torrent *t, void *data, int flags);
struct torrent *tor_alloc(void *data, int flags);
void tor_free(struct torrent *t);
void torlib_init(void);

#endif
