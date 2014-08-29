/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef TORRENT_H
#define TORRENT_H

#include <cos_component.h>
#include <cbuf_c.h>
#include <cbuf.h>
#include <evt.h>

//#define TSPLIT_FAULT
//#define TWRITE_FAULT
//#define TREAD_FAULT
//#define TRELEASE_FAULT

#define TEST_0	   /* unit test */
//#define TEST_1   /* single thread fails (T writes to foo/bar/who) */
//#define TEST_2   /* 2 threads, operate on the same files, one fails, (Both writes to foo/bar/who) */
//#define TEST_3   /* 2 threads, operate on the different files(T1 writes foo/bar/who, T2 writes foo/boo/who) */
#ifdef TEST_3
#define TEST_4     /* two component A and B, A writes and B will fault later */
#endif

/* we can track all leaf torrents that are requested to split from
 * this client so they and their parents can be rebuilt*/
/* assume that only the spd which splits the torrent can read/write torrent */

/* we only replay the action when necessary */
/* this is right way to do and should fit into RTA */

#if (!LAZY_RECOVERY)
void eager_recovery_all();
#endif

/* torrent descriptor */
typedef int td_t;
static const td_t td_null = 0, td_root = 1;
typedef enum {
	TOR_WRITE = 0x1,
	TOR_READ  = 0x2,
	TOR_SPLIT = 0x4,
	TOR_RW    = TOR_WRITE | TOR_READ, 
	TOR_ALL   = TOR_RW    | TOR_SPLIT /* 0 is a synonym */
} tor_flags_t;

td_t tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
// non-tracking version of tsplit
td_t __tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);

void trelease(spdid_t spdid, td_t tid);
int tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len);
int tread(spdid_t spdid, td_t td, cbuf_t cb, int sz);
int twrite(spdid_t spdid, td_t td, cbuf_t cb, int sz);

/* FIXME: this should be more general */
int twmeta(spdid_t spdid, td_t td, int cbid, int sz, int offset, int flag);

static inline int
tread_pack(spdid_t spdid, td_t td, char *data, int len)
{
	cbufp_t cb;
	char *d;
	int ret;

	d = cbufp_alloc(len, &cb);
	if (!d) return -1;
	cbufp_send(cb);

	ret = tread(spdid, td, cb, len);
	memcpy(data, d, len);
	cbufp_deref(cb);
	
	return ret;
}

static inline int
twrite_pack(spdid_t spdid, td_t td, char *data, int len)
{
	cbufp_t cb;
	char *d;
	int ret;

	d = cbufp_alloc(len, &cb);
	if (!d) return -1;
	cbufp_send_deref(cb);

	memcpy(d, data, len);
	ret = twrite(spdid, td, cb, len);
	return ret;
}


/* //int trmeta(td_t td, char *key, int flen, char *value, int vlen); */
/* struct trmeta_data { */
/* 	short int value, end; /\* offsets into data *\/ */
/* 	char data[0]; */
/* }; */
/* int trmeta(td_t td, int cbid, int sz); */

#endif /* TORRENT_H */ 
