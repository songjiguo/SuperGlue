/* Jiguo Song: ramfs FT */
/* Need deal 3 cases with cbufs 1) fault while writing, after cbuf2buf
   -- need save the contents on clients (make it read only?)  2) fault
   while writing, before cbuf2buf -- no need save the contents on
   clients (for now) 3) writing successfully, fault later (for now) */

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>
#include <cos_map.h>
#include <cos_list.h>

#include <torrent.h>
#include <cstub.h>

#include <stdio.h>
#include <stdlib.h>

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>


/* the state of an torrent file object */
enum {
	STATE_TSPLIT_PARENT,  // parent tsplit
	STATE_TSPLIT,         // current tsplit
	STATE_TREAD,
	STATE_TWRITE,
	STATE_TRELEASE,
	STATE_TMERGE,
	STATE_TRMETA,
	STATE_TWMETA
};

#if ! defined MAX_RECUR_LEVEL || MAX_RECUR_LEVEL < 1
#define MAX_RECUR_LEVEL 20
#endif

#define RD_PRINT 0

#if RD_PRINT == 1
#define print_rd(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define print_rd(fmt,...) 
#endif

static volatile unsigned long global_fault_cnt = 0;

/* recovery data and related utility functions */
struct rec_data_tor {
	td_t	p_tid;	// id which split from, root tid is 1
        td_t	s_tid;	// id that returned from the server (might be same)
        td_t	c_tid;	// id that viewed by the client (always unique)

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;
	long		 evtid;

	int              being_recovered;  // if we need track the data again?

	int              state;
	unsigned long	 fcnt;
#if (!LAZY_RECOVERY)
	int has_rebuilt;
	struct rec_data_tor *next, *prev;
#endif

};

#if (!LAZY_RECOVERY)
struct rec_data_tor* eager_list_head = NULL;
#endif

/**********************************************/
/* slab allocator and cvect for tracking recovery data */
/**********************************************/
COS_MAP_CREATE_STATIC(uniq_tids);

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

void print_rdtor_info(struct rec_data_tor *rd);

static struct rec_data_tor *
rd_lookup(td_t td)
{ 
	return cvect_lookup(&rec_vect, td); 
}

static struct rec_data_tor *
rd_alloc(td_t c_tid)    // should pass c_tid
{
	struct rec_data_tor *rd;
	rd = cslab_alloc_rd();
	assert(rd);
	cvect_add(&rec_vect, rd, c_tid);
	return rd;
}

static void
rd_dealloc(struct rec_data_tor *rd)
{
	assert(rd);
	if (cvect_del(&rec_vect, rd->c_tid)) BUG();
	cslab_free_rd(rd);
}


static struct rec_data_tor *
map_rd_lookup(td_t tid)
{ 
	return (struct rec_data_tor *)cos_map_lookup(&uniq_tids, tid);
}

static int
map_rd_create()
{
	struct rec_data_tor *rd = NULL;
	int map_id = 0;
	// ramfs return torrent id from 2 (rd_root is 1), and cos_map starts from 0
	// here want cos_map to return some ids at least from 2 and later
	while(1) {
		rd = cslab_alloc_rd();
		assert(rd);	
		map_id = cos_map_add(&uniq_tids, rd);
		/* printc("record added %d\n", map_id); */
		if (map_id >= 2) break;
		rd->s_tid = -1;  // -1 means that this is a dummy record
	}
	assert(map_id >= 2);
	return map_id;	
}

static void
map_rd_delete(td_t tid)
{
	assert(tid >= 0);
	struct rec_data_tor *rd;
	rd = map_rd_lookup(tid);
	assert(rd && rd->param);
	free(rd->param);   // free the memory for the path name saving
	cslab_free_rd(rd);
	cos_map_del(&uniq_tids, tid);
	return;
}

static char*
param_save(char *param, int param_len)
{
	char *l_param;

	assert(param && param_len > 0);

	l_param = malloc(param_len);
	if (!l_param) {
		printc("cannot malloc \n");
		BUG();
		return NULL;
	}
	strncpy(l_param, param, param_len);

	l_param[param_len] = '\0';   // zero out any thing left after the end

	printc("in param save: l_param %s param %s param_len %d\n", 
	       l_param, param, param_len);
	return l_param;
}

extern td_t server_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);

/* restore the server state */
static void
rd_recover_state(struct rec_data_tor *rd)
{
	struct rec_data_tor *prd, *tmp = NULL;
	char val[10]; // 2^32 use 10 bits

	assert(rd && rd->p_tid >= 1 && rd->c_tid > 1);	

	printc("in rd_recover_state: rd->p_tid %d\n", rd->p_tid);
	if (rd->p_tid > 1) {     // not tsplit from td_root
		assert((prd = map_rd_lookup(rd->p_tid)));
		prd->fcnt = global_fault_cnt;
		printc("in rd_recover_state: found a parent to be recovered rd->p_tid %d\n",
		       rd->p_tid);
		rd_recover_state(prd);
	}
	
	// has reached td_root, start rebuilding and no tracking...
	// tsplit returns the client id !!!!
	printc("\n recovery process calls tsplit again!!!...\n\n");
	printc("saved param is %s\n", rd->param);
	td_t tmp_tid = server_tsplit(cos_spd_id(), rd->p_tid, 
			      rd->param, rd->param_len, rd->tflags, rd->evtid);
	if (tmp_tid <= 1) return;
	printc("\n recovery process tsplit return!!!...(tmp_tid %d)\n\n", tmp_tid);
	
	assert((tmp = map_rd_lookup(tmp_tid)));
	rd->s_tid = tmp->s_tid;
	/* printc("got the new client side %d and its new server id %d\n",  */
	/*        tmp_tid, tmp->s_tid); */

        /* do not track the new tid for retsplitting.. (wish to avoid
	 * this) add this to ramfs as well */
	map_rd_delete(tmp_tid);  

	/* //Now bring the data back as well */
	/* printc("\nnow it is time to bring the data back...\n\n"); */
	
	/* rd->being_recovered = 1; */
	/* int ret = -1; */
	/* sprintf(val, "%d", rd->s_tid); */
	/* printc("val %s val_len %d (td %d)\n", val, strlen(val), rd->s_tid); */
	/* ret = twmeta(cos_spd_id(), rd->s_tid, "data", strlen("data"), val, strlen(val)); */
	/* assert(!ret); */
	
	/* printc("\nnow the data is brought back!!!!\n\n"); */
	return;
}

static struct rec_data_tor *
rd_update(td_t tid, int state)
{
        struct rec_data_tor *rd = NULL;

	/* In state machine, normally we do not track the td_root,
	 * Note: for torrent, tsplit/tread/twrite/trelease is always
	 * in the same component. This is different from evt */
	
	// first tsplit 
	if (tid <= 1 && state == STATE_TSPLIT_PARENT) goto done;
        rd = map_rd_lookup(tid);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == global_fault_cnt)) goto done;

	rd->fcnt = global_fault_cnt;

	/* STATE MACHINE */
	switch (state) {
	case STATE_TSPLIT_PARENT:
		/* we get here because the fault occurs during the
		 * tsplit. Then rd is referring to the parent torrent
		 * therefore we do not need update the state. We need
		 * start rebuilding parent's state */
	case STATE_TRELEASE:
		/* This is to close a file. Just replay and recover
		 * the data (which still backed up through cbuf) */
	case STATE_TREAD:
		/* The data read from a file will still be backed up
		 * (cbuf read only)  */
	case STATE_TWRITE:
		/* The data to be written to a file will be recorded
		 * (changed to read only in cbuf) in this
		 * function. However, we can not do this at the client
		 * interface like what we did for mbox. The reason is
		 * that to recover a file, we need the offset of piece
		 * of data that passed through cbuf and written into
		 * the file before the fault. twrite returns how many
		 * bytes actually written, not the offset. Without the
		 * offset, there is no way to know where the data
		 * should be put during the recovery (e.g., twmeta).

		 1. we still track the data in the server side (due to
		 the offset information) 

		 2. during the recovery, we call twmeta to write the
		 data back to a file and we can do this on the client
		 side. Bring that data back to a specific offset
		 position in a file */
	case STATE_TMERGE:
		/* This is the end of a file (delete). This is where
		 * we remove the tracked file info (e.g., in cbuf
		 * manager) */
		rd_recover_state(rd);
		break;
	case STATE_TWMETA:
	case STATE_TRMETA:
		/* Meta operation on the files. For recovery, these
		 * two functions should not track anymore. Though it
		 * might change the file access control and data, we
		 * do not track any data since these are still backed
		 * up through the cbugs in cbuf manager. */
		break;
	default:
		assert(0);
	}
	printc("thd %d restore ramfs done!!!\n", cos_get_thd_id());
done:
	return rd;
}

static void 
rd_cons(struct rec_data_tor *rd, td_t p_tid, td_t c_tid, td_t s_tid,
	char *param, int len, tor_flags_t tflags, long evtid)
{
	/* printc("rd_cons: ser_tid %d  cli_tid %d\n", ser_tid, cli_tid); */
	assert(rd);

	rd->p_tid	 = p_tid;
	rd->c_tid	 = c_tid;
	rd->s_tid	 = s_tid;
	rd->param	 = param;
	rd->param_len	 = len;
	rd->tflags	 = tflags;
	rd->evtid	 = evtid;
	rd->fcnt	 = global_fault_cnt;

	rd->being_recovered = 0;
	rd->state           = STATE_TSPLIT;

	return;
}

/************************************/
/******  client stub functions ******/
/************************************/
static int first = 0;

struct __sg_tsplit_data {
	td_t parent_tid;	
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

CSTUB_FN(td_t, tsplit)(struct usr_inv_cap *uc,
		       spdid_t spdid, td_t parent_tid, char * param,
		       int len, tor_flags_t tflags, long evtid)
{
	long fault = 0;
	td_t ret;

        struct __sg_tsplit_data *d = NULL;
	cbuf_t cb = 0;
	
        struct rec_data_tor *rd = NULL;
	int sz  = len + sizeof(struct __sg_tsplit_data);
        td_t		curr_ptid  = 0;
        td_t		cli_tid	   = 0;
        td_t		ser_tid	   = 0;

	printc("cli: tsplit passed in param %s\n", param);
	assert(parent_tid >= 1);
        assert(param && len >= 0);
        assert(param[len] == '\0'); 

        if (first == 0) {
		cos_map_init_static(&uniq_tids);
		first = 1;
	}

redo:
	printc("<<< In: call tsplit  (thread %d, spd %ld and parent tid %d) >>>\n", 
	       cos_get_thd_id(), cos_spd_id(), parent_tid);

	rd = rd_update(parent_tid, STATE_TSPLIT_PARENT);
	if (rd) {
		curr_ptid  = rd->s_tid;
	} else {
		curr_ptid  = parent_tid;
	}
	printc("<<< In: call tsplit (thread %d, , spd %ld and curr_parent tid %d) >>>\n", 
	       cos_get_thd_id(), cos_spd_id(), curr_ptid);

        d = cbuf_alloc(sz, &cb);
	assert(d);
	if (!d) return -1;
	d->parent_tid  = curr_ptid;
        d->tflags      = tflags;
	d->evtid       = evtid;
	d->len[0]      = 0;
	d->len[1]      = len;
	memcpy(&d->data[0], param, len + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		memset(&d->data[0], 0, len);
		cbuf_free(cb);
		printc("tsplit found a fault and ready to go to redo\n");
		goto redo;
	}
        cbuf_free(cb);

        ser_tid = ret;
	printc("passed in param %s (ser_tid %d)\n", param, ser_tid);
	assert(ser_tid >= 1);

	char *l_param = "";
	if (len > 0) {
		l_param = param_save(param, len);
		assert(l_param);
	}

	cli_tid = map_rd_create();
	assert(cli_tid >= 2);

	rd = map_rd_lookup(cli_tid);
        assert(rd);

        rd_cons(rd, curr_ptid, cli_tid, ser_tid, l_param, len, tflags, evtid);

	printc("tsplit done!!! return new client tid %d\n\n", cli_tid);
        return cli_tid;
}

CSTUB_FN(int, twritep)(struct usr_inv_cap *uc,
		       spdid_t spdid, td_t td, int cb, int sz)
{
	long fault = 0;
	td_t ret;

        struct rec_data_tor *rd;
redo:
        printc("<<< In: call twrite  (thread %d) >>>\n", cos_get_thd_id());
	rd = rd_update(td, STATE_TWRITE);
	assert(rd);

	CSTUB_INVOKE(ret, fault, uc, 4, spdid, rd->s_tid, cb, sz);

        if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

        /* we can not save the infor here since we need track the offset and
         * uniq file id any way */
	return ret;
}

CSTUB_FN(int, treadp)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t td, int len, int *off, int *sz)
{
	int ret;
	long fault = 0;


        printc("<<< In: call tread (thread %d, spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id());
        struct rec_data_tor *rd;
        volatile unsigned long long start, end;

redo:
        printc("treadp (spd %ld thd %d td %d)\n", cos_spd_id(), cos_get_thd_id(), td);
	rd = rd_update(td, STATE_TREAD);
	assert(rd);

	printc("treadp cli (before): len %d off %d sz %d\n", len, *off, *sz);

	CSTUB_INVOKE_3RETS(ret, fault, *off, *sz, uc, 3, spdid, rd->s_tid, len);
        if (unlikely(fault)) {
		printc("treadp found a fault and ready to go to redo\n");
		printc("treadp cli (in fault): ret %d len %d off %d sz %d\n", 
		       ret, len, *off, *sz);
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	printc("treadp cli (after): len %d off %d sz %d\n", len, *off, *sz);

	return ret;
}

struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};

CSTUB_FN(int, tmerge)(struct usr_inv_cap *uc, spdid_t spdid, td_t td, 
		      td_t td_into, char * param, int len)
{
	long fault = 0;
	td_t ret;
	
	struct __sg_tmerge_data *d;
        struct rec_data_tor *rd;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tmerge_data);

        /* printc("<<< In: call tmerge (thread %d) >>>\n", cos_get_thd_id()); */

        assert(param && len > 0);
	assert(param[len] == '\0');
	
redo:
        printc("<<< In: call tmerge (thread %d) >>>\n", cos_get_thd_id());
	rd = rd_update(td, STATE_TMERGE);
	assert(rd);

	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

        /* printc("c: tmerge td %d (server td %d) len %d param %s\n", td, rd->s_tid, len, param);	 */
	d->td = rd->s_tid;   	/* actual server side torrent id */
	d->td_into = td_into;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		cbuf_free(cb);
                goto redo;
	}

	cbuf_free(cb);

        if (!ret) map_rd_delete(rd->c_tid);
	
	return ret;
}


CSTUB_FN(void, trelease)(struct usr_inv_cap *uc,
			 spdid_t spdid, td_t tid)
{
	int ret;
	long fault = 0;

        /* printc("<<< In: call trelease (thread %d) >>>\n", cos_get_thd_id()); */
        struct rec_data_tor *rd;

redo:
        printc("<<< In: call trelease (thread %d) >>>\n", cos_get_thd_id());
	rd = rd_update(tid, STATE_TRELEASE);
	assert(rd);

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_tid);

        if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	map_rd_delete(rd->c_tid);

	return;
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
        struct rec_data_tor *rd;

        assert(key && retval && klen > 0 && max_rval_len > 0);
        assert(key[klen] == '\0' && sz <= PAGE_SIZE);

redo:
        printc("<<< In: call trmeta (thread %d) >>>\n", cos_get_thd_id());
	rd = rd_update(td, STATE_TRMETA);
	assert(rd);

        d = cbuf_alloc(sz, &cb);
        if (!d) return -1;

        d->td = rd->s_tid;
        d->klen = klen;
        d->retval_len = max_rval_len;
        memcpy(&d->data[0], key, klen + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);
        if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}


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
        struct rec_data_tor *rd;

        assert(key && val && klen > 0 && vlen > 0);
        assert(key[klen] == '\0' && val[vlen] == '\0' && sz <= PAGE_SIZE);

redo:
        printc("<<< In: call twmeta (thread %d) >>>\n", cos_get_thd_id());
	rd = rd_update(td, STATE_TWMETA);
	assert(rd);

        d = cbuf_alloc(sz, &cb);
        if (!d) assert(0); //return -1;

        d->td   = td;   // do not pass rd->s_tid since this is only for recovery
        d->klen = klen;
        d->vlen = vlen;
        memcpy(&d->data[0], key, klen + 1);
        memcpy(&d->data[klen + 1], val, vlen + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

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

void 
print_rdtor_info(struct rec_data_tor *rd)
{
	assert(rd);
	print_rd("rd->parent_tid %d  ",rd->parent_tid);
	print_rd("rd->s_tid %d  ",rd->s_tid);
	print_rd("rd->c_tid %d  ",rd->c_tid);

	print_rd("rd->param %s  ",rd->param);
	print_rd("rd->pram_len %d  ",rd->param_len);
	print_rd("rd->tflags %d  ",rd->tflags);
	print_rd("rd->evtid %ld  ",rd->evtid);

	print_rd("rd->fcnt %ld  ",rd->fcnt);
	print_rd("global_fault_cnt %ld  ",global_fault_cnt);

	print_rd("rd->offset %d \n ",rd->offset);

	return;
}

