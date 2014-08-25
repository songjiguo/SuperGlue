/* Jiguo Song: ramfs FT */
/* Note: 
  1. Need deal 3 cases with cbufs
     1) fault while writing, after cbuf2buf -- need save the contents on clients (make it read only?)
     2) fault while writing, before cbuf2buf -- no need save the contents on clients (for now)
     3) writing successfully, fault later (for now)
*/


#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <torrent.h>
#include <cstub.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#define RD_PRINT 0

#if RD_PRINT == 1
#define print_rd(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define print_rd(fmt,...) 
#endif

static unsigned long fcounter = 0;

/* recovery data and related utility functions */
struct rec_data_tor {
	td_t	parent_tid;	// id which split from, root tid is 1
        td_t	s_tid;		// id that returned from the server (might be same)
        td_t	c_tid;		// id that viewed by the client (always unique)

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;
	long		 evtid;

	unsigned long	 fcnt;
};

/**********************************************/
/* slab allocator and cvect for tracking recovery data */
/**********************************************/

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

void print_rd_info(struct rec_data_tor *rd);

static struct rec_data_tor *
rd_lookup(td_t td)
{ return cvect_lookup(&rec_vect, td); }

static struct rec_data_tor *
rd_alloc(void)
{
	struct rec_data_tor *rd;
	rd = cslab_alloc_rd();
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

	rd->parent_tid	 = tid;
	rd->s_tid	 = ser_tid;
	rd->c_tid	 = cli_tid;
	rd->param	 = param;
	rd->param_len	 = len;
	rd->tflags	 = tflags;
	rd->evtid	 = evtid;
	rd->fcnt	 = fcounter;

	return;
}


/* restore the server state */
static void
reinstate(td_t tid)  	/* relate the flag to the split/r/w fcnt */
{
	td_t ret;
	struct rec_data_tor *rd;
	if (!(rd = rd_lookup(tid))) return; 	/* root_tid */

	/* printc("<<<<<< thread %d trying to reinstate....tid %d\n", cos_get_thd_id(), rd->c_tid); */
	/* printc("parent tid %d\n", rd->parent_tid); */

	/* printc("param reinstate %s of length %d\n", rd->param, rd->param_len); */
	ret = __tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid, rd->c_tid);
	if (ret < 1) {
		printc("re-split failed %d\n", tid);
		BUG();
	}
	if (ret > 0) rd->s_tid = ret;  	/* update the ramfs side tid */
	rd->fcnt = fcounter;

	/* printc("already reinstate c_tid %d s_tid %d >>>>>>>>>>\n", rd->c_tid, rd->s_tid); */
	return;
}

static void
rebuild_fs(td_t tid)
{
	unsigned long long start, end;
	/* printc("\n <<<<<<<<<<<< recovery starts >>>>>>>>>>>>>>>>\n\n"); */
	/* rdtscll(start); */

	reinstate(tid);

	/* rdtscll(end); */
	/* printc("rebuild fs cost %llu\n", end-start); */
	/* printc("\n<<<<<<<<<<<< recovery ends >>>>>>>>>>>>>>>>\n\n"); */
}


static struct rec_data_tor *
update_rd(td_t tid)
{
        struct rec_data_tor *rd;

        rd = rd_lookup(tid);
	if (!rd) return NULL;
	/* fast path */
	if (likely(rd->fcnt == fcounter)) return rd;

	/* printc("rd->fcnt %lu fcounter %lu\n",rd->fcnt,fcounter); */

	rebuild_fs(tid);

	printc("rebuild fs is done\n\n");
	return rd;
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

/************************************/
/******  client stub functions ******/
/************************************/

struct __sg_tsplit_data {
	td_t tid;	
	int flag;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

/* Split the new torrent and get new ser object */
/* Client only needs know how to find server side object */

static int aaa = 0;

CSTUB_FN_ARGS_6(td_t, tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid)

/* printc("\ncli interface... param... %s\n", param); */
printc("<<< In: call tsplit  (thread %d spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id());
        if (cos_get_thd_id() == 13) aaa++;   		/* test only */
/* printc("thread %d aaa is %d\n", cos_get_thd_id(), aaa); */
        ret = __tsplit(spdid, tid, param, len, tflags, evtid, 0);

CSTUB_POST

CSTUB_FN_ARGS_7(td_t, __tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid, td_t, flag)

        struct __sg_tsplit_data *d;
        struct rec_data_tor *rd, *rd_p, *rd_c;

	char *l_param;
	cbuf_t cb;
        int		sz	   = 0;
        td_t		cli_tid	   = 0;
        td_t		ser_tid	   = 0;
        tor_flags_t	flags	   = tflags;
        td_t		parent_tid = tid;

printc("len %d param %s\n", len, param);
	unsigned long long start, end;
        assert(param && len >= 0);
        assert(param[len] == '\0'); 

        sz = len + sizeof(struct __sg_tsplit_data);
        /* replay on slow path */
        if (unlikely(flag > 0)) reinstate(tid);
        if ((rd_p = rd_lookup(parent_tid))) parent_tid = rd_p->s_tid;

redo:
        d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

        printc("parent_tid: %d\n", parent_tid);
        d->tid	       = parent_tid;
        d->flag = flag;
        d->tflags      = flags;
	d->evtid       = evtid;
	d->len[0]      = 0;
	d->len[1]      = len;
        printc("c: subpath name %s len %d\n", param, len);
	memcpy(&d->data[0], param, len);

#ifdef TEST_3
        if (aaa == 6 && cos_spd_id() == 17) { //for TEST_3 only   
#else
        if (aaa == 6) {
#endif
		d->flag = -10; /* test purpose only */
		aaa = 100;
		/* rdtscll(start); */
	}

CSTUB_ASM_4(__tsplit, spdid, cb, sz, flag)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		memset(&d->data[0], 0, len);
		cbuf_free(d);
		
		rebuild_fs(tid);
		
		printc("rebuild is done\n\n");
		/* rdtscll(end); */
		/* printc("entire cost %llu\n", end-start); */
		goto redo;
	}

        printc("ret from ramfs: %d\n",ret);
        memset(&d->data[0], 0, len);
        cbuf_free(d);

        if (unlikely(flag > 0 || flag == -1)) return ret;

        ser_tid = ret;
        l_param = param_save(param, len);
        rd = rd_alloc();
        assert(rd);

        if (unlikely(rd_lookup(ser_tid))) {
		cli_tid = get_unique();
		assert(cli_tid > 0 && cli_tid != ser_tid);
		/* printc("found existing tid %d >> get new cli_tid %d\n", ser_tid, cli_tid); */
	} else {
		cli_tid = ser_tid;
	}

        /* client side tid should be guaranteed to be unique now */
        rd_cons(rd, tid, ser_tid, cli_tid, l_param, len, tflags, evtid);
	if (cvect_add(&rec_vect, rd, cli_tid)) {
		printc("can not add into cvect\n");
		BUG();
	}

        ret = cli_tid;
	printc("tsplit done!!!\n\n");
CSTUB_POST

struct __sg_twmeta_data {
	td_t td;
	cbuf_t cb;
	int sz;
	int offset;
	int flag;
};

CSTUB_FN_ARGS_6(int, twmeta, spdid_t, spdid, td_t, td, cbuf_t, cb, int, sz, int, offset, int, flag)
        struct rec_data_tor *rd;
	struct __sg_twmeta_data *d;
	cbuf_t cb_m;
	int sz_m = sizeof(struct __sg_twmeta_data);
redo:
	d = cbuf_alloc(sz_m, &cb_m);
	if (!d) return -1;

	/* int buf_sz; */
	/* u32_t id; */
	/* cbuf_unpack(cb, &id, (u32_t*)&buf_sz); */
        /* printc("cbid is %d\n", id); */

	d->td	  = td;
	d->cb	  = cb;
        d->sz	  = sz;
        d->offset = offset;
        d->flag   = flag;

CSTUB_ASM_3(twmeta, spdid, cb_m, sz_m)
        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		cbuf_free(d);
                goto redo;
	}

	cbuf_free(d);
CSTUB_POST

struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};
CSTUB_FN_ARGS_5(int, tmerge, spdid_t, spdid, td_t, td, td_t, td_into, char *, param, int, len)
	struct __sg_tmerge_data *d;
        struct rec_data_tor *rd;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tmerge_data);

        /* printc("<<< In: call tmerge (thread %d) >>>\n", cos_get_thd_id()); */

        assert(param && len > 0);
	assert(param[len] == '\0');

redo:
        rd = update_rd(td);
	if (!rd) {
		printc("try to merge a non-existing tor\n");
		return -1;
	}

	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

        /* printc("c: tmerge td %d (server td %d) len %d param %s\n", td, rd->s_tid, len, param);	 */
	d->td = rd->s_tid;   	/* actual server side torrent id */
	d->td_into = td_into;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);

CSTUB_ASM_3(tmerge, spdid, cb, sz)
        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		cbuf_free(d);
                goto redo;
	}

	cbuf_free(d);
        if (!ret) rd_dealloc(rd); /* this must be a leaf */


CSTUB_POST


CSTUB_FN_ARGS_2(int, trelease, spdid_t, spdid, td_t, tid)

        /* printc("<<< In: call trelease (thread %d) >>>\n", cos_get_thd_id()); */
        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);
	if (!rd) {
		printc("try to release a non-existing tor\n");
		return -1;
	}

CSTUB_ASM_2(trelease, spdid, rd->s_tid)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
                goto redo;
	}
        assert(rd);

CSTUB_POST

CSTUB_FN_ARGS_4(int, tread, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz)

        /* printc("<<< In: call tread (thread %d, spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id()); */
        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);
	if (!rd) {
		printc("try to read a non-existing tor\n");
		return -1;
	}

CSTUB_ASM_4(tread, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
                goto redo;
	}

        print_rd_info(rd);

CSTUB_POST


CSTUB_FN_ARGS_4(int, twrite, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz)

        /* printc("<<< In: call twrite  (thread %d) >>>\n", cos_get_thd_id()); */
        struct rec_data_tor *rd;

redo:

        rd = update_rd(tid);
	if (!rd) {
		printc("try to write a non-existing tor\n");
		return -1;
	}

CSTUB_ASM_4(twrite, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
                goto redo;
	}

        assert(rd);

        print_rd_info(rd);

CSTUB_POST

void 
print_rd_info(struct rec_data_tor *rd)
{
	assert(rd);
	return;
	print_rd("rd->parent_tid %d  ",rd->parent_tid);
	print_rd("rd->s_tid %d  ",rd->s_tid);
	print_rd("rd->c_tid %d  ",rd->c_tid);

	print_rd("rd->param %s  ",rd->param);
	print_rd("rd->pram_len %d  ",rd->param_len);
	print_rd("rd->tflags %d  ",rd->tflags);
	print_rd("rd->evtid %ld  ",rd->evtid);

	print_rd("rd->fcnt %ld  ",rd->fcnt);
	print_rd("fcounter %ld  ",fcounter);

	print_rd("rd->offset %d \n ",rd->offset);

	return;
}

