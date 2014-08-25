#include <torrent.h>

struct __sg_tsplit_data {
	td_t tid;
	int flag;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

td_t __sg_tsplit(spdid_t spdid, cbuf_t cb, int len, int flag)
{
	struct __sg_tsplit_data *d;

	td_t s_torid;
	
	d = cbuf2buf(cb, len);
	if (unlikely(!d)) return -5;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -2; 
	/* if (unlikely(d->len[0] >= d->len[1])) return -3; */
	if (unlikely(((int)(d->len[1] + sizeof(struct __sg_tsplit_data))) != len)) return -4;

	s_torid =  __tsplit(spdid, d->tid, &d->data[0],
			    d->len[1] - d->len[0], d->tflags, d->evtid, d->flag);
	/* printc("new obtained torretn id %d\n", s_torid); */
	return s_torid;
}

/* struct __sg_twmeta_data { */
/* 	td_t td; */
/* 	cbuf_t cb; */
/* 	int sz; */
/* 	int offset; */
/* 	int flag; */
/* }; */

/* int __sg_twmeta(spdid_t spdid, cbuf_t cb_m, int len_m) */
/* { */
/* 	struct __sg_twmeta_data *d; */

/* 	d = cbuf2buf(cb_m, len_m); */
/* 	if (unlikely(!d)) return -1; */
/* 	/\* mainly to inform the compiler that optimizations are possible *\/ */
/* 	if (unlikely(d->sz == 0)) return -1; */
/* 	if (unlikely(d->cb == 0)) return -1; */
/* 	if (unlikely(d->offset < 0)) return -1; */

/* 	return twmeta(spdid, d->td, d->cb, d->sz, d->offset, d->flag); */
/* } */


/* int __sg_twrite(spdid_t spdid, td_t tid, cbuf_t cb, int sz) */
/* { */
/* 	return twrite(spdid, tid, cb, sz);	 */
/* } */

/* int __sg_tread(spdid_t spdid, td_t tid, cbuf_t cb, int sz) */
/* { */
/* 	return tread(spdid, tid, cb, sz);	 */
/* } */


struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};
int __sg_tmerge(spdid_t spdid, cbuf_t cbid, int len)
{
	struct __sg_tmerge_data *d;

	d = cbuf2buf(cbid, len);
	if (unlikely(!d)) return -1;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -1; 
	if (unlikely(d->len[0] >= d->len[1])) return -1;
	if (unlikely(((int)(d->len[1] + (sizeof(struct __sg_tmerge_data)))) != len)) return -1;

	return tmerge(spdid, d->td, d->td_into, &d->data[0], d->len[1] - d->len[0]);
}

