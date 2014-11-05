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

/* 2 threads, operate on the different files(T1 writes foo/bar/who, T2 writes foo/boo/who) */

/* #define TEST_RAMFS_TSPLIT_BEFORE */
/* #define TEST_RAMFS_TSPLIT_AFTER */
#define TEST_RAMFS_TREADP
/* #define TEST_RAMFS_TWRITEP_BEFORE */
/* #define TEST_RAMFS_TWRITEP_AFTER */
/* #define TEST_RAMFS_TRELEASE */
/* #define TEST_RAMFS_TMERGE */

typedef int td_t;
static const td_t td_null = 0, td_root = 1;
typedef enum {
	TOR_WRITE = 0x1,
	TOR_READ  = 0x2,
	TOR_SPLIT = 0x4,
	TOR_NONPERSIST = 0x8,
	TOR_RW    = TOR_WRITE | TOR_READ, 
	TOR_ALL   = TOR_RW    | TOR_SPLIT /* 0 is a synonym */
} tor_flags_t;

td_t tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
void trelease(spdid_t spdid, td_t tid);
int tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len);
int tread(spdid_t spdid, td_t td, int cbid, int sz);
int treadp(spdid_t spdid, td_t td, int len, int *off, int *sz);
int twrite(spdid_t spdid, td_t td, int cbid, int sz);
int twritep(spdid_t spdid, td_t td, int cbid, int sz);
int trmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, char *retval, unsigned int max_rval_len);
int twmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, const char *val, unsigned int vlen);

static inline int
tread_pack(spdid_t spdid, td_t td, char *data, int len)
{
	cbufp_t cb;
	char *d;
	int ret = 0;
	int off, sz;
	sz = len; // for ramfs, "arbitrary" bytes can be read off
	
	cb = treadp(spdid, td, len, &off, &sz);
	printc("after treadp in tread_pack len %d sz %d\n", len, sz);
	if (!cb < 0) return 0;
	d = cbufp2buf(cb, sz);
	printc("data %s\n", d);
	memcpy(data, d, sz);
	cbufp_deref(cb);
	return sz;
}

static inline int
twrite_pack(spdid_t spdid, td_t td, char *data, int len)
{
	cbufp_t cb;
	char *d;
	int ret;

	d = cbufp_alloc(len, &cb);
	if (!d) return -1;
	cbufp_send(cb);
	memcpy(d, data, len);
	ret = twritep(spdid, td, cb, len);
	cbufp_deref(cb);
	
	return ret;
}

/* //int trmeta(td_t td, char *key, int flen, char *value, int vlen); */
/* struct trmeta_data { */
/* 	short int value, end; /\* offsets into data *\/ */
/* 	char data[0]; */
/* }; */
/* int trmeta(td_t td, int cbid, int sz); */

/* //int twmeta(td_t td, char *key, int flen, char *value, int vlen); */
/* struct twmeta_data { */
/* 	short int value, end; /\* offsets into data *\/ */
/* 	char data[0]; */
/* }; */
/* int twmeta(td_t td, int cbid, int sz); */

#endif /* TORRENT_H */ 
