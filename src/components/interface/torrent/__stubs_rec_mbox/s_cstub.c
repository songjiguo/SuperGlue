#include <cos_component.h>
#include <torrent.h>

struct __sg_tsplit_data {
	td_t tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};
td_t __sg_tsplit(spdid_t spdid, cbuf_t cbid, int len)
{
	struct __sg_tsplit_data *d;

	d = cbuf2buf(cbid, len);
	if (unlikely(!d)) return -5;

	/* printc("__tsplit ser: spdid %d thd %d tid %d evtid %ld (cbid %d)\n",  */
	/*        spdid, cos_get_thd_id(), d->tid, d->evtid, cbid); */

	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -2; 
	if (unlikely(d->len[0] > d->len[1])) return -3;
	if (unlikely(((int)(d->len[1] + sizeof(struct __sg_tsplit_data))) != len)) return -4;
	if (unlikely(d->tid == 0)) return -EINVAL;

	/* printc("server interface: calling tsplit....thd %d\n", cos_get_thd_id()); */
	
	return tsplit(spdid, d->tid, &d->data[0], 
		      d->len[1] - d->len[0], d->tflags, d->evtid);
}

int
__sg_treadp(spdid_t spdid, int tid, int __pad0, int __pad1, int *off_len)
{
	int ret = 0;
	printc("passed: treadp ser (before): td %d len %d off_len[0] %d off_len[1] %d\n",
	       tid, 0, off_len[0], off_len[1]);
        ret = treadp(spdid, tid, 0, &off_len[0], &off_len[1]);
	printc("treadp ser (after): ret %d off_len[0] %d off_len[1] %d\n",
	       ret, off_len[0], off_len[1]);
	return ret;
}

struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};

int
__sg_tmerge(spdid_t spdid, cbuf_t cbid, int len)
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

struct __sg_trmeta_data {
        td_t td;
        int klen, retval_len;
        char data[0];
};
int
__sg_trmeta(spdid_t spdid, cbuf_t cbid, int len)
{
        struct __sg_trmeta_data *d;

        d = cbuf2buf(cbid, len);
        if (unlikely(!d)) return -5;
        /* mainly to inform the compiler that optimizations are possible */
        if (unlikely(d->klen <= 0)) return -2; 
        if (unlikely(d->retval_len <= 0)) return -3;
        if (unlikely(d->td == 0)) return -EINVAL;

        return trmeta(spdid, d->td, &d->data[0], d->klen, 
                        &d->data[d->klen + 1], d->retval_len);
}

struct __sg_twmeta_data {
        td_t td;
        int klen, vlen;
        char data[0];
};
int
__sg_twmeta(spdid_t spdid, cbuf_t cbid, int len)
{
        struct __sg_twmeta_data *d;

        d = cbuf2buf(cbid, len);
        if (unlikely(!d)) return -5;
        /* mainly to inform the compiler that optimizations are possible */
        if (unlikely(d->klen <= 0)) return -2; 
        if (unlikely(d->vlen <= 0)) return -2; // TODO: write "" to td->data?
        if (unlikely(d->td == 0)) return -EINVAL;

        return twmeta(spdid, d->td, &d->data[0], d->klen, 
                        &d->data[d->klen + 1], d->vlen);
}
