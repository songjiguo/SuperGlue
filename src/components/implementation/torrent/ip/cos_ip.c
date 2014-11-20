/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

/* 
 * This is a place-holder for when all of IP is moved here from
 * cos_net.  There should really be no performance difference between
 * having this empty like this, and having IP functionality in here.
 * If anything, due to cache (TLB) effects, having functionality in
 * here will be slower.
 */
#include <cos_component.h>
#include <torlib.h>
#include <torrent.h>
#include <cos_synchronization.h>
#include <cbuf.h>

#include <sched.h>
#include <periodic_wake.h>   // for debug only
static volatile unsigned long ip_tread_cnt = 0;
static volatile unsigned long ip_twrite_cnt = 0;
static volatile int debug_thd = 0;

extern td_t parent_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void parent_trelease(spdid_t spdid, td_t tid);
extern int parent_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int parent_twrite(spdid_t spdid, td_t td, int cbid, int sz);

/* required so that we can have a rodata section */
const char *name = "cos_ip";

/* int ip_xmit(spdid_t spdid, struct cos_array *d) */
/* { */
/* 	return netif_event_xmit(cos_spd_id(), d); */
/* } */

/* int ip_wait(spdid_t spdid, struct cos_array *d) */
/* { */
/* 	return netif_event_wait(cos_spd_id(), d); */
/* } */

/* int ip_netif_release(spdid_t spdid) */
/* { */
/* 	return netif_event_release(cos_spd_id()); */
/* } */

/* int ip_netif_create(spdid_t spdid) */
/* { */
/* 	return netif_event_create(cos_spd_id()); */
/* } */

td_t 
tsplit(spdid_t spdid, td_t tid, char *param, int len, 
       tor_flags_t tflags, long evtid)
{
	td_t ret = -ENOMEM, ntd;
	struct torrent *t;

	if (tid != td_root) return -EINVAL;
	ntd = parent_tsplit(cos_spd_id(), tid, param, len, tflags, evtid);
	if (ntd <= 0) ERR_THROW(ntd, err);

	t = tor_alloc((void*)ntd, tflags);
	if (!t) ERR_THROW(-ENOMEM, err);
	ret = t->td;
err:
	return ret;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;
	td_t ntd;

	if (!tor_is_usrdef(td)) return;
	t = tor_lookup(td);
	if (!t) goto done;
	ntd = (td_t)t->data;
	parent_trelease(cos_spd_id(), ntd);
	tor_free(t);
done:
	return;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	return -ENOTSUP;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	td_t ntd;
	struct torrent *t;
	char *buf, *nbuf;
	int ret = -1;
	cbuf_t ncbid;

	if (tor_isnull(td)) return -EINVAL;
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	assert(t->data);
	ntd = (td_t)t->data;

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	nbuf = cbuf_alloc(sz, &ncbid);
	assert(nbuf);
	memcpy(nbuf, buf, sz);
	ret = parent_twrite(cos_spd_id(), ntd, ncbid, sz);
	/* ip_twrite_cnt++; */
	cbuf_free(ncbid);
done:
	return ret;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	td_t ntd;
	struct torrent *t;
	char *buf, *nbuf;
	int ret = -1;
	cbuf_t ncbid;

	if (tor_isnull(td)) return -EINVAL;
	t = tor_lookup(td);
	if (!t)                      ERR_THROW(-EINVAL, done);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	assert(t->data);
	ntd = (td_t)t->data;

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	nbuf = cbuf_alloc(sz, &ncbid);
	assert(nbuf);
	/* printc("tip_tif_tread (thd %d)\n", cos_get_thd_id()); */
	ret = parent_tread(cos_spd_id(), ntd, ncbid, sz);
	if (ret < 0) goto free;
	/* ip_tread_cnt++; */
	memcpy(buf, nbuf, ret);
free:
	/* cbufp_deref(ncbid); */  // should keep this cbufp alive in netif for FT purpose?  Jiguo
	cbuf_free(ncbid);
done:
	return ret;
}

int
__twmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, const char *val, unsigned int vlen) {
	
	return 0;
}

void cos_init(void)
{
	static volatile int first = 1;
	
	union sched_param sp;

#ifdef DEBUG_PERIOD
	if (cos_get_thd_id() == debug_thd) {
		if (periodic_wake_create(cos_spd_id(), 100)) BUG();
		while(1) {
			periodic_wake_wait(cos_spd_id());
			printc("ip_tread_cnt %ld ip_twrite_cnt %ld\n", ip_tread_cnt, ip_twrite_cnt);
			ip_tread_cnt = 0;
			ip_twrite_cnt = 0;
		}
	}
#endif
	if (first) {
		first = 0;

#ifdef DEBUG_PERIOD		
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		debug_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
#endif

		torlib_init();
	}
}
