/* 
   Jiguo Song: mailbox FT

   There are several issues with mailbox c^3, mainly due to the
   protocol which involves server, client and event. 

   It seems that temporary hack using TOR_WAIT is a reasonable
   solution to asynchronous tsplit (followed by evt_wait). Also
   tracking all evt_id on event interface then evt_trigger all of them
   seems a solution for any thread that blocks in scheduler through
   event component (this seems a general pattern for any situation
   that the object state is changed by multiple interfaces)
   
   For tracking events, see event interface (server side)

*/

#include <cos_component.h>
#include <cos_debug.h>

#include <torrent.h>
#include <cstub.h>
#include <print.h>

#include <objtype.h>

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
#define C_TAKE(spdid) 	do { if (sched_component_take(spdid))    return; } while (0)
#define C_RELEASE(spdid)	do { if (sched_component_release(spdid)) return; } while (0)

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

static unsigned long fcounter = 0;

// tracking data structure
struct rec_data_tor {
	td_t	parent_tid;
        td_t	s_tid;
        td_t	c_tid;

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;

	long		 evtid;
	unsigned int     evt_wait;  // for evt_wait after split

	unsigned long	 fcnt;
};

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

static struct rec_data_tor *
rd_lookup(td_t td)
{ return cvect_lookup(&rec_vect, td); }

static struct rec_data_tor *
rd_alloc(int tid)
{
	struct rec_data_tor *rd;
	rd = cslab_alloc_rd();
	assert(rd);
	if (cvect_add(&rec_vect, rd, tid)) {
		printc("can not add into cvect\n");
		BUG();
	}
	return rd;
}

static void
rd_dealloc(struct rec_data_tor *rd)
{
	assert(rd);
	if (cvect_del(&rec_vect, rd->c_tid)) BUG();
	cslab_free_rd(rd);
}

static void
rd_cons(struct rec_data_tor *rd, td_t tid, td_t ser_tid, td_t cli_tid, char *param, int len, tor_flags_t tflags, long evtid)
{
	printc("rd_cons: ser_tid %d  cli_tid %d (its parent tid %d)\n", 
	       ser_tid, cli_tid, tid);
	assert(rd);

	C_TAKE(cos_spd_id());

	rd->parent_tid	 = tid;
	rd->s_tid	 = ser_tid;
	rd->c_tid	 = cli_tid;
	rd->param	 = param;
	rd->param_len	 = len;
	rd->tflags	 = tflags;
	rd->evtid	 = evtid;

	rd->fcnt	 = fcounter;

	rd->evt_wait	 = tflags & TOR_WAIT;  // add for now, wait after tsplit
	
	C_RELEASE(cos_spd_id());
	return;
}

static int
get_unique(void)
{
	unsigned int i;
	cvect_t *v;

	v = &rec_vect;

	/* 1 is already assigned to the td_root */
	for(i = 2 ; i < CVECT_MAX_ID ; i++) {
		if (!cvect_lookup(v, i)) return i;
	}
	
	if (!cvect_lookup(v, CVECT_MAX_ID)) return CVECT_MAX_ID;

	return -1;
}

static char*
param_save(char *param, int param_len)
{
	char *l_param;
	
	if (param_len == 0) return param;
	
	l_param = malloc(param_len);
	if (!l_param) {
		printc("cannot malloc \n");
		BUG();
	}
	strncpy(l_param, param, param_len);

	return l_param;
}

static void
param_del(char *param)
{
	assert(param);
	free(param);
	return;
}

#ifdef REFLECTION
static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}

extern int sched_reflect(spdid_t spdid, int src_spd, int cnt);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern vaddr_t mman_reflect(spdid_t spd, int src_spd, int cnt);
extern int mman_release_page(spdid_t spd, vaddr_t addr, int flags); 

extern int evt_trigger_all(spdid_t spdid);

static void
rd_reflection(int cap)
{
	assert(cap);

	C_TAKE(cos_spd_id());

	int count_obj = 0; // reflected objects
	int dest_spd = cap_to_dest(cap);
	
	// remove the mapped page for mailbox spd
	vaddr_t addr;
	count_obj = mman_reflect(cos_spd_id(), dest_spd, 1);
	printc("mbox relfects on mmgr: %d objs\n", count_obj);
	while (count_obj--) {
		addr = mman_reflect(cos_spd_id(), dest_spd, 0);
		/* printc("evt mman_release: %p addr\n", (void *)addr); */
		mman_release_page(cos_spd_id(), addr, dest_spd);
	}

	/* to reflect all threads blocked via evt. this reflection
	 * depends on the service. Here is for mailbox. */
	evt_trigger_all(cos_spd_id());

	C_RELEASE(cos_spd_id());
	printc("mailbox reflection done (thd %d)\n\n", cos_get_thd_id());
	return;
}
#endif

 /* Jiguo: only do this after split succeeds since we need unique
  * client id */
static td_t
rd_track(spdid_t spdid, td_t tid, char * param,
	 int len, tor_flags_t tflags, long evtid, td_t id)
{
	td_t ret = 0;

        struct rec_data_tor *rd, *rd_p, *rd_c;
	char *l_param;
        td_t		cli_tid	   = 0;
        tor_flags_t	flags	   = tflags;
        td_t		parent_tid = tid;

        td_t		ser_tid	   = id;	
        if (unlikely(rd_lookup(ser_tid))) {
		cli_tid = get_unique();
		assert(cli_tid > 0 && cli_tid != ser_tid);
	} else {
		cli_tid = ser_tid;
	}
        rd = rd_alloc(cli_tid);
        assert(rd);

        l_param = param_save(param, len);

	printc("thd %d is adding track torrent %d in spd %ld\n", 
	       cos_get_thd_id(), tid, cos_spd_id());
        /* client side tid should be guaranteed to be unique now */
        rd_cons(rd, tid, ser_tid, cli_tid, l_param, len, tflags, evtid);
        ret = cli_tid;
	
	return ret;
}

td_t __tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);

/* call the interface function that does not track the state since the
 * object has already been tracked, not deallocated yet, so basically
 * here we update the server side id */
static void
reinstate(struct rec_data_tor *rd)
{
	struct rec_data_tor *parent_rd;
	td_t parent_id;

	if (!rd) return;
	rd->fcnt = fcounter;

	printc("thd %d restoring mbox for torrent id %d (parent id %d evt id %ld) \n", 
	       cos_get_thd_id(), rd->c_tid, rd->parent_tid, rd->evtid);

	if (rd->parent_tid > 1) {
		parent_rd = rd_lookup(rd->parent_tid);
		if (!parent_rd) return;
                // recursively rebuild all parent torrent (might evt_wait)
		reinstate(parent_rd);   
	}
	
	assert(rd && rd->parent_tid >= 1);
        /* update the ramfs side tid, not need to alloc the new entry
	 * since we already found it */
	td_t tmp = __tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid);
	printc("got the new server side id %d \n", tmp);
	assert(tmp > 0);
	rd->s_tid = tmp;

	if (rd->evt_wait == TOR_WAIT) {
		printc("thd %d is waiting on event %ld \n", cos_get_thd_id(), rd->evtid);
		evt_wait(cos_spd_id(), rd->evtid);
		printc("thd %d is back from evt_wait %ld \n", cos_get_thd_id(), rd->evtid);
	}

	return;
}

static struct rec_data_tor *
update_rd(td_t tid)
{
        struct rec_data_tor *rd;

        rd = rd_lookup(tid);
	if (!rd) return NULL;
	if (likely(rd->fcnt == fcounter)) return rd;
	td_t parent_tid;
	printc("thd %d restoring mbox (passed tid %d) \n", cos_get_thd_id(), tid);
	printc("its parent torrent id %d \n", rd->parent_tid);

	reinstate(rd);

	/* parent_tid = reinstate(tid); */
	/* /\* This is bounded by the depth of a torrent *\/ */
	/* while (1) { */
	/* 	parent_tid = reinstate(parent_tid); */
	/* 	if (!parent_tid) break; */
	/* } */

	printc("thd %d restore mbox done!!!\n", cos_get_thd_id());
	return rd;
}

/************************************/
/******  client stub functions ******/
/************************************/

struct __sg_tsplit_data {
	td_t tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

CSTUB_FN(td_t, tsplit)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t tid, char * param,
		int len, tor_flags_t tflags, long evtid)
{
	long fault = 0;
	td_t ret;
	struct __sg_tsplit_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tsplit_data);

	printc("\n[[cli: thd %d is calling tsplit from spd %ld (parent tid %d, evt %ld)]]\n", 
	       cos_get_thd_id(), cos_spd_id(), tid, evtid);

        assert(param && len >= 0);
        assert(param[len] == '\0');

	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

redo:
        d->tid    = tid;   
	d->tflags = tflags;
	d->evtid  = evtid;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        if (unlikely(fault)) {
		printc("tsplit found a fault (thd %d)\n", cos_get_thd_id());
		CSTUB_FAULT_UPDATE();
		
		memset(&d->data[0], 0, len);
		/* this will restore the parent torrent (and up to the
		 * root recursively). Note that the current invocation
		 * is not tracked yet. So we need call updata_rd
		 * here */
		printc("updating tid %d\n", tid);
		update_rd(tid);
		goto redo;
	}
	
	// failed to tsplit
	if (ret < 1) goto done;

        /* memset(&d->data[0], 0, len); */

	// now ret is the client side unique id
	ret = rd_track(spdid, tid, param, len, tflags, evtid, ret);
	printc("tsplit done!!!\n\n");
done:
        cbuf_free(cb);
	return ret;
}


CSTUB_FN(int, twritep)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t tid, int cbid, int sz)
{
	int ret;
	long fault = 0;

        printc("<<< In: call twritep  (thread %d) >>>\n", cos_get_thd_id());
        struct rec_data_tor *rd;
redo:
        rd = update_rd(tid);
	if (!rd) {
		printc("try to write a non-existing mailbox\n");
		return -1;
	}
	
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, rd->s_tid, cbid, sz);

	printc("mb cli: twritep (thd %d) return, ret %d fault %ld\n", 
	       cos_get_thd_id(), ret, fault);

	if (unlikely (fault)){
		printc("twritep found a fault (thd %d)\n", cos_get_thd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	return ret;
}


CSTUB_FN(int, treadp)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t tid, int *off, int *sz)
{
	int ret;
	long fault = 0;
        printc("<<< In: call treadp  (thread %d) >>>\n", cos_get_thd_id());
        struct rec_data_tor *rd;
redo:
        rd = update_rd(tid);
	if (!rd) {
		printc("try to write a non-existing mailbox\n");
		return -1;
	}

	CSTUB_INVOKE_3RETS(ret, fault, *off, *sz, uc, 2, spdid, rd->s_tid);

	printc("mb cli: treadp (thd %d) return, ret %d fault %ld, off %d sz %d\n", 
	       cos_get_thd_id(), ret, fault, *off, *sz);
	
	if (unlikely(fault)){
		printc("treadp found a fault (thd %d)\n", cos_get_thd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}


CSTUB_FN(void, trelease)(struct usr_inv_cap *uc,
			 spdid_t spdid, td_t tid)
{
	int ret;
	long fault = 0;
        printc("<<< In: call trelease  (thread %d) >>>\n", cos_get_thd_id());
        struct rec_data_tor *rd;
redo:
        rd = update_rd(tid);
	if (!rd) {
		printc("try to write a non-existing mailbox\n");
		return;
	}
	
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_tid);
	
	if (unlikely(fault)){
		printc("trelease found a fault (thd %d)\n", cos_get_thd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	assert(rd);
	rd_dealloc(rd);

	return;
}


struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};

CSTUB_FN(int, tmerge)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	int ret;
	long fault = 0;
	struct __sg_tmerge_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tmerge_data);

        assert(param && len > 0);
	assert(param[len-1] == '\0');

	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

	d->td = td;
	d->td_into = td_into;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	cbuf_free(cb);
	return ret;
}





CSTUB_FN(int, tread)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret;
	long fault = 0;
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, td, cbid, sz);
	return ret;
}

CSTUB_FN(int, twrite)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret;
	long fault = 0;
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, td, cbid, sz);
	return ret;
}

struct __sg_trmeta_data {
        td_t td;
        int klen, retval_len;
        char data[0];
};

CSTUB_FN(int, trmeta)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t td, const char *key,
		unsigned int klen, char *retval, unsigned int max_rval_len)
{
	int ret;
	long fault = 0;
        cbuf_t cb;
        int sz = sizeof(struct __sg_trmeta_data) + klen + max_rval_len + 1;
        struct __sg_trmeta_data *d;

        assert(key && retval && klen > 0 && max_rval_len > 0);
        assert(key[klen] == '\0' && sz <= PAGE_SIZE);

        d = cbuf_alloc(sz, &cb);
        if (!d) return -1;

        d->td = td;
        d->klen = klen;
        d->retval_len = max_rval_len;
        memcpy(&d->data[0], key, klen + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        if (ret >= 0) {
                if ((unsigned int)ret > max_rval_len) { // as ret >= 0, cast it to unsigned int to omit compiler warning
                        cbuf_free(cb);
                        return -EIO;
                }
                memcpy(retval, &d->data[klen + 1], ret + 1);
        }
        cbuf_free(cb);
	return ret;
}

struct __sg_twmeta_data {
        td_t td;
        int klen, vlen;
        char data[0];
};

CSTUB_FN(int, twmeta)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t td, const char *key,
		unsigned int klen, const char *val, unsigned int vlen)
{
	int ret;
	long fault = 0;
        cbuf_t cb;
        int sz = sizeof(struct __sg_twmeta_data) + klen + vlen + 1;
        struct __sg_twmeta_data *d;

        assert(key && val && klen > 0 && vlen > 0);
        assert(key[klen] == '\0' && val[vlen] == '\0' && sz <= PAGE_SIZE);

        d = cbuf_alloc(sz, &cb);
        if (!d) assert(0); //return -1;

        d->td = td;
        d->klen = klen;
        d->vlen = vlen;
        memcpy(&d->data[0], key, klen + 1);
        memcpy(&d->data[klen + 1], val, vlen + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        cbuf_free(cb);
	return ret;
}

CSTUB_FN(td_t, __tsplit)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t tid, char * param,
		int len, tor_flags_t tflags, long evtid)
{
	long fault = 0;
	unsigned long ret;

	struct __sg_tsplit_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tsplit_data);

        assert(param && len >= 0);
        assert(param[len] == '\0');

	d = cbuf_alloc(sz, &cb);
	if (!d) return -6;
redo:
        d->tid    = tid;
	d->tflags = tflags;
	d->evtid  = evtid;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len + 1);

	printc("__tsplit cli: spdid %d tid %d evtid %ld (cbid %d)\n", 
	       spdid, tid, evtid, cb);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	if (unlikely (fault)){
		printc("re tsplit found a fault (thd %d)\n", cos_get_thd_id());		
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	cbuf_free(cb);
	return ret;
}
