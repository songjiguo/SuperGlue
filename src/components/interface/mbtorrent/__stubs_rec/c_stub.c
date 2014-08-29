/* 
   Jiguo Song: mailbox FT

   There are several issues with mailbox c^3, mainly due to the
   protocol which involves server, client and event. 

   After discuss with Gabe, it seems that temporary hack using
   TOR_WAIT is a reasonable solution to asynchronous tsplit (followed
   by evt_wait). Also tracking all evt_id passed through tsplit and
   then evt_trigger all of them seems a reasonable solution for any
   thread that blocks in scheduler through event component (this seems
   a general pattern for any situation that the object state is
   changed by multiple interfaces.)

   Make a separate list for tracking event ids since rd is not
   allocated until tsplit succeeds.

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

#define RD_RELFECTION_SPD = 7

/* // the list of all event ids that be waited through this interface */
/* struct trigger_evt { */
/* 	long		 evtid; */
/* 	struct trigger_evt *next, *prev; */
/* }; */
/* struct trigger_evt *evts_head = NULL; */
/* CSLAB_CREATE(te, sizeof(struct trigger_evt)); */

/* static struct trigger_evt * */
/* te_alloc(int eid) */
/* { */
/* 	struct trigger_evt *te; */

/* 	te = cslab_alloc_te(); */
/* 	assert(te); */
/* 	te->evtid = eid; */
/* 	INIT_LIST(te, next, prev); */

/* 	return te; */
/* } */

/* static void */
/* te_dealloc(struct trigger_evt *te) */
/* { */
/* 	assert(te); */
/* 	cslab_free_te(te); */
/* 	return; */
/* } */

// tracking data for recovering mailbox
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
	/* printc("rd_cons: ser_tid %d  cli_tid %d\n", ser_tid, cli_tid); */
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
	
	
	/* int wake_thd; */
	/* count_obj = sched_reflect(cos_spd_id(), cos_spd_id(), 1); */
	/* /\* printc("evt relfects on sched: %d objs\n", count_obj); *\/ */
	/* while (count_obj--) { */
	/* 	wake_thd = sched_reflect(cos_spd_id(), cos_spd_id(), 0); */
	/* 	printc("wake_thd %d\n", wake_thd); */
	/* 	sched_wakeup(cos_spd_id(), wake_thd); */
	/* } */

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
static td_t
reinstate(td_t tid)
{
	td_t ret = 0;
	struct rec_data_tor *rd;
	assert(tid > 0);

	printc("thd %d restoring mbox for torrent id %d \n", cos_get_thd_id(), tid);
	if (!(rd = rd_lookup(tid))) goto done;
        /* update the ramfs side tid, not need to alloc the new entry
	 * since we already found it */
	printc("found tracked torrent id %d \n", tid);
	td_t tmp = __tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid);
	printc("got the new server side id %d \n", tmp);
	assert(tmp > 0);
	rd->s_tid = tmp;
	assert(rd->parent_tid);
	rd->fcnt = fcounter;
	ret = rd->parent_tid;
	
	if (rd->evt_wait == TOR_WAIT) {
		printc("thd %d is waiting on event %ld \n", cos_get_thd_id(), rd->evtid);
		evt_wait(cos_spd_id(), rd->evtid);
		printc("thd %d is back from evt_wait %ld \n", cos_get_thd_id(), rd->evtid);
	}
done:
	return ret;
}

static struct rec_data_tor *
update_rd(td_t tid)
{
        struct rec_data_tor *rd;

        rd = rd_lookup(tid);
	if (!rd) return NULL;
	if (likely(rd->fcnt == fcounter)) return rd;
	td_t parent_tid;
	printc("thd %d restoring mbox \n", cos_get_thd_id());
	parent_tid = reinstate(tid);
	while (1) {
		parent_tid = reinstate(parent_tid);
		if (!parent_tid) break;
	}
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


/* static void */
/* track_evt (long evtid) */
/* { */
/* 	assert(evtid >= 0); */

/* 	if (!evts_head) { */
/* 		evts_head = te_alloc(555); */
/* 		assert(evts_head); */
/* 	} */

/* 	struct trigger_evt *te = te_alloc(evtid); */
/* 	assert(te); */
/* 	printc("add to the list\n"); */
/* 	ADD_LIST(evts_head, te, next, prev); */
/* 	printc("add to the list done %p \n", (void *)evts_head->next->evtid); */

/* 	return; */
/* } */


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

	/* /\* track evt so that when fault occurs, thread can trigger all */
	/*  * of them to wake up thread that blocks in scheduler via */
	/*  * evt_wait from mailbox  *\/ */
	/* if (tflags & TOR_WAIT) track_evt(evtid); */

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

CSTUB_FN(int, treadp)(struct usr_inv_cap *uc,
		spdid_t spdid, td_t td, int *off, int *sz)
{
	int ret;
	long fault = 0;
	CSTUB_INVOKE_3RETS(ret, fault, *off, *sz, uc, 2, spdid, td);
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

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	cbuf_free(cb);
	return ret;
}



/* /\* /\\* restore the server state *\\/ *\/ */
/* /\* static void *\/ */
/* /\* reinstate(td_t tid)  	/\\* relate the flag to the split/r/w fcnt *\\/ *\/ */
/* /\* { *\/ */
/* /\* 	td_t ret; *\/ */
/* /\* 	struct rec_data_tor *rd; *\/ */
/* /\* 	if (!(rd = rd_lookup(tid))) return; 	/\\* root_tid *\\/ *\/ */

/* /\* 	/\\* printc("<<<<<< thread %d trying to reinstate....tid %d\n", cos_get_thd_id(), rd->c_tid); *\\/ *\/ */
/* /\* 	/\\* printc("parent tid %d\n", rd->parent_tid); *\\/ *\/ */

/* /\* 	/\\* printc("param reinstate %s of length %d\n", rd->param, rd->param_len); *\\/ *\/ */
/* /\* 	ret = __tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid, rd->c_tid); *\/ */
/* /\* 	if (ret < 1) { *\/ */
/* /\* 		printc("re-split failed %d\n", tid); *\/ */
/* /\* 		BUG(); *\/ */
/* /\* 	} *\/ */
/* /\* 	if (ret > 0) rd->s_tid = ret;  	/\\* update the ramfs side tid *\\/ *\/ */
/* /\* 	rd->fcnt = fcounter; *\/ */

/* /\* 	/\\* printc("already reinstate c_tid %d s_tid %d >>>>>>>>>>\n", rd->c_tid, rd->s_tid); *\\/ *\/ */
/* /\* 	return; *\/ */
/* /\* } *\/ */

/* /\* static void *\/ */
/* /\* rebuild_fs(td_t tid) *\/ */
/* /\* { *\/ */
/* /\* 	unsigned long long start, end; *\/ */
/* /\* 	/\\* printc("\n <<<<<<<<<<<< recovery starts >>>>>>>>>>>>>>>>\n\n"); *\\/ *\/ */
/* /\* 	/\\* rdtscll(start); *\\/ *\/ */

/* /\* 	reinstate(tid); *\/ */

/* /\* 	/\\* rdtscll(end); *\\/ *\/ */
/* /\* 	/\\* printc("rebuild fs cost %llu\n", end-start); *\\/ *\/ */
/* /\* 	/\\* printc("\n<<<<<<<<<<<< recovery ends >>>>>>>>>>>>>>>>\n\n"); *\\/ *\/ */
/* /\* } *\/ */


/* static struct rec_data_tor * */
/* update_rd(td_t tid) */
/* { */
/*         struct rec_data_tor *rd; */

/*         rd = rd_lookup(tid); */
/* 	if (!rd) return NULL; */
/* 	/\* fast path *\/ */
/* 	if (likely(rd->fcnt == fcounter)) return rd; */

/* 	/\* /\\* printc("rd->fcnt %lu fcounter %lu\n",rd->fcnt,fcounter); *\\/ *\/ */

/* 	/\* rebuild_fs(tid); *\/ */

/* 	/\* printc("rebuild fs is done\n\n"); *\/ */
/* 	return rd; */
/* } */

/* static int */
/* get_unique(void) */
/* { */
/* 	unsigned int i; */
/* 	cvect_t *v; */

/* 	v = &rec_vect; */

/* 	/\* 1 is already assigned to the td_root *\/ */
/* 	for(i = 2 ; i < CVECT_MAX_ID ; i++) { */
/* 		if (!cvect_lookup(v, i)) return i; */
/* 	} */
	
/* 	if (!cvect_lookup(v, CVECT_MAX_ID)) return CVECT_MAX_ID; */

/* 	return -1; */
/* } */

/* static char* */
/* param_save(char *param, int param_len) */
/* { */
/* 	char *l_param; */
	
/* 	if (param_len == 0) return param; */

/* 	l_param = malloc(param_len); */
/* 	if (!l_param) { */
/* 		printc("cannot malloc \n"); */
/* 		BUG(); */
/* 	} */
/* 	strncpy(l_param, param, param_len); */

/* 	return l_param; */
/* } */

/* /\************************************\/ */
/* /\******  client stub functions ******\/ */
/* /\************************************\/ */

/* struct __sg_tsplit_data { */
/* 	td_t tid;	 */
/* 	int flag; */
/* 	tor_flags_t tflags; */
/* 	long evtid; */
/* 	int len[2]; */
/* 	char data[0]; */
/* }; */

/* Split the new torrent and get new ser object */
/* Client only needs know how to find server side object */

/* static int aaa = 0; */

/* CSTUB_FN_ARGS_6(td_t, tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid) */

/* /\* printc("\ncli interface... param... %s\n", param); *\/ */
/* printc("<<< In: call tsplit  (thread %d spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id()); */
/*         if (cos_get_thd_id() == 13) aaa++;   		/\* test only *\/ */
/* /\* printc("thread %d aaa is %d\n", cos_get_thd_id(), aaa); *\/ */
/*         ret = __tsplit(spdid, tid, param, len, tflags, evtid, 0); */

/* CSTUB_POST */

/* CSTUB_FN_ARGS_7(td_t, __tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid, td_t, flag) */

/*         struct __sg_tsplit_data *d; */
/*         struct rec_data_tor *rd, *rd_p, *rd_c; */

/* 	char *l_param; */
/* 	cbuf_t cb; */
/*         int		sz	   = 0; */
/*         td_t		cli_tid	   = 0; */
/*         td_t		ser_tid	   = 0; */
/*         tor_flags_t	flags	   = tflags; */
/*         td_t		parent_tid = tid; */

/* printc("len %d param %s\n", len, param); */
/* 	unsigned long long start, end; */
/*         assert(param && len >= 0); */
/*         assert(param[len] == '\0');  */

/*         sz = len + sizeof(struct __sg_tsplit_data); */
/*         /\* replay on slow path *\/ */
/*         if (unlikely(flag > 0)) reinstate(tid); */
/*         if ((rd_p = rd_lookup(parent_tid))) parent_tid = rd_p->s_tid; */

/* redo: */
/*         d = cbuf_alloc(sz, &cb); */
/* 	if (!d) return -1; */

/*         printc("parent_tid: %d\n", parent_tid); */
/*         d->tid	       = parent_tid; */
/*         d->flag = flag; */
/*         d->tflags      = flags; */
/* 	d->evtid       = evtid; */
/* 	d->len[0]      = 0; */
/* 	d->len[1]      = len; */
/*         printc("c: subpath name %s len %d\n", param, len); */
/* 	memcpy(&d->data[0], param, len); */

/* #ifdef TEST_3 */
/*         if (aaa == 6 && cos_spd_id() == 17) { //for TEST_3 only    */
/* #else */
/*         if (aaa == 6) { */
/* #endif */
/* 		d->flag = -10; /\* test purpose only *\/ */
/* 		aaa = 100; */
/* 		/\* rdtscll(start); *\/ */
/* 	} */

/* CSTUB_ASM_4(__tsplit, spdid, cb, sz, flag) */

/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/* 		memset(&d->data[0], 0, len); */
/* 		cbuf_free(d); */
		
/* 		rebuild_fs(tid); */
		
/* 		printc("rebuild is done\n\n"); */
/* 		/\* rdtscll(end); *\/ */
/* 		/\* printc("entire cost %llu\n", end-start); *\/ */
/* 		goto redo; */
/* 	} */

/*         printc("ret from ramfs: %d\n",ret); */
/*         memset(&d->data[0], 0, len); */
/*         cbuf_free(d); */

/*         if (unlikely(flag > 0 || flag == -1)) return ret; */

/*         ser_tid = ret; */
/*         l_param = param_save(param, len); */
/*         rd = rd_alloc(); */
/*         assert(rd); */

/*         if (unlikely(rd_lookup(ser_tid))) { */
/* 		cli_tid = get_unique(); */
/* 		assert(cli_tid > 0 && cli_tid != ser_tid); */
/* 		/\* printc("found existing tid %d >> get new cli_tid %d\n", ser_tid, cli_tid); *\/ */
/* 	} else { */
/* 		cli_tid = ser_tid; */
/* 	} */

/*         /\* client side tid should be guaranteed to be unique now *\/ */
/*         rd_cons(rd, tid, ser_tid, cli_tid, l_param, len, tflags, evtid); */
/* 	if (cvect_add(&rec_vect, rd, cli_tid)) { */
/* 		printc("can not add into cvect\n"); */
/* 		BUG(); */
/* 	} */

/*         ret = cli_tid; */
/* 	printc("tsplit done!!!\n\n"); */
/* CSTUB_POST */

/* struct __sg_twmeta_data { */
/* 	td_t td; */
/* 	cbuf_t cb; */
/* 	int sz; */
/* 	int offset; */
/* 	int flag; */
/* }; */

/* CSTUB_FN_ARGS_6(int, twmeta, spdid_t, spdid, td_t, td, cbuf_t, cb, int, sz, int, offset, int, flag) */
/*         struct rec_data_tor *rd; */
/* 	struct __sg_twmeta_data *d; */
/* 	cbuf_t cb_m; */
/* 	int sz_m = sizeof(struct __sg_twmeta_data); */
/* redo: */
/* 	d = cbuf_alloc(sz_m, &cb_m); */
/* 	if (!d) return -1; */

/* 	/\* int buf_sz; *\/ */
/* 	/\* u32_t id; *\/ */
/* 	/\* cbuf_unpack(cb, &id, (u32_t*)&buf_sz); *\/ */
/*         /\* printc("cbid is %d\n", id); *\/ */

/* 	d->td	  = td; */
/* 	d->cb	  = cb; */
/*         d->sz	  = sz; */
/*         d->offset = offset; */
/*         d->flag   = flag; */

/* CSTUB_ASM_3(twmeta, spdid, cb_m, sz_m) */
/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/* 		cbuf_free(d); */
/*                 goto redo; */
/* 	} */

/* 	cbuf_free(d); */
/* CSTUB_POST */

/* struct __sg_tmerge_data { */
/* 	td_t td; */
/* 	td_t td_into; */
/* 	int len[2]; */
/* 	char data[0]; */
/* }; */
/* CSTUB_FN_ARGS_5(int, tmerge, spdid_t, spdid, td_t, td, td_t, td_into, char *, param, int, len) */
/* 	struct __sg_tmerge_data *d; */
/*         struct rec_data_tor *rd; */
/* 	cbuf_t cb; */
/* 	int sz = len + sizeof(struct __sg_tmerge_data); */

/*         /\* printc("<<< In: call tmerge (thread %d) >>>\n", cos_get_thd_id()); *\/ */

/*         assert(param && len > 0); */
/* 	assert(param[len] == '\0'); */

/* redo: */
/*         rd = update_rd(td); */
/* 	if (!rd) { */
/* 		printc("try to merge a non-existing tor\n"); */
/* 		return -1; */
/* 	} */

/* 	d = cbuf_alloc(sz, &cb); */
/* 	if (!d) return -1; */

/*         /\* printc("c: tmerge td %d (server td %d) len %d param %s\n", td, rd->s_tid, len, param);	 *\/ */
/* 	d->td = rd->s_tid;   	/\* actual server side torrent id *\/ */
/* 	d->td_into = td_into; */
/*         d->len[0] = 0; */
/*         d->len[1] = len; */
/* 	memcpy(&d->data[0], param, len); */

/* CSTUB_ASM_3(tmerge, spdid, cb, sz) */
/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/* 		cbuf_free(d); */
/*                 goto redo; */
/* 	} */

/* 	cbuf_free(d); */
/*         if (!ret) rd_dealloc(rd); /\* this must be a leaf *\/ */


/* CSTUB_POST */


/* CSTUB_FN_ARGS_2(int, trelease, spdid_t, spdid, td_t, tid) */

/*         /\* printc("<<< In: call trelease (thread %d) >>>\n", cos_get_thd_id()); *\/ */
/*         struct rec_data_tor *rd; */

/* redo: */
/*         rd = update_rd(tid); */
/* 	if (!rd) { */
/* 		printc("try to release a non-existing tor\n"); */
/* 		return -1; */
/* 	} */

/* CSTUB_ASM_2(trelease, spdid, rd->s_tid) */

/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/*                 goto redo; */
/* 	} */
/*         assert(rd); */

/* CSTUB_POST */

/* CSTUB_FN_ARGS_4(int, tread, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz) */

/*         /\* printc("<<< In: call tread (thread %d, spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id()); *\/ */
/*         struct rec_data_tor *rd; */

/* redo: */
/*         rd = update_rd(tid); */
/* 	if (!rd) { */
/* 		printc("try to read a non-existing tor\n"); */
/* 		return -1; */
/* 	} */

/* CSTUB_ASM_4(tread, spdid, rd->s_tid, cb, sz) */

/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/*                 goto redo; */
/* 	} */

/*         print_rd_info(rd); */

/* CSTUB_POST */


/* CSTUB_FN_ARGS_4(int, twrite, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz) */

/*         /\* printc("<<< In: call twrite  (thread %d) >>>\n", cos_get_thd_id()); *\/ */
/*         struct rec_data_tor *rd; */

/* redo: */

/*         rd = update_rd(tid); */
/* 	if (!rd) { */
/* 		printc("try to write a non-existing tor\n"); */
/* 		return -1; */
/* 	} */

/* CSTUB_ASM_4(twrite, spdid, rd->s_tid, cb, sz) */

/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/*                 goto redo; */
/* 	} */

/*         assert(rd); */

/*         print_rd_info(rd); */

/* CSTUB_POST */

/* void  */
/* print_rd_info(struct rec_data_tor *rd) */
/* { */
/* 	assert(rd); */
/* 	return; */
/* 	print_rd("rd->parent_tid %d  ",rd->parent_tid); */
/* 	print_rd("rd->s_tid %d  ",rd->s_tid); */
/* 	print_rd("rd->c_tid %d  ",rd->c_tid); */

/* 	print_rd("rd->param %s  ",rd->param); */
/* 	print_rd("rd->pram_len %d  ",rd->param_len); */
/* 	print_rd("rd->tflags %d  ",rd->tflags); */
/* 	print_rd("rd->evtid %ld  ",rd->evtid); */

/* 	print_rd("rd->fcnt %ld  ",rd->fcnt); */
/* 	print_rd("fcounter %ld  ",fcounter); */

/* 	print_rd("rd->offset %d \n ",rd->offset); */

/* 	return; */
/* } */



   /* 1) need to know which tsplit is followed by evt_wait for recovery */
   /* -- solution for now is to add TOR_WAIT as a flag so that interface */
   /* can track/re-tsplit with a following evt_wait */
   
   /* 2) thread will be blocked in scheduler through evt_wait, */
   /* not through mailbox */
   /* -- solution for now is to hard code event component id */

   /* 3) This is a more general problem: assumes thd 1 blocks in */
   /* scheduler (via evt_wait, not mailbox in this case), and thd 2 is */
   /* faulty (could be in a different spd), then reflection on scheduler */
   /* will wake up thd 1 but not through evt_trigger -- however, state */
   /* (data structure) in event component is not updated  */

   /* -- solution for now is to find thread with TOE_WAIT and its evt_id */
   /*    and trigger */
