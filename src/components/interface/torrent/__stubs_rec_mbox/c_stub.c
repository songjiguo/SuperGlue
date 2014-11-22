/* 
   Jiguo Song: mailbox FT

   There are several issues with mailbox c^3, mainly due to the
   protocol which involves server, client and event. 

 1.It seems that temporary hack using TOR_WAIT is a reasonable
   solution to asynchronous tsplit (followed by evt_wait). Also
   tracking all evt_id on event interface then evt_trigger all of them
   seems a solution for any thread that blocks in scheduler through
   event component (this seems a general pattern for any situation
   that the object state is changed by multiple interfaces)
   
   For tracking events, see event interface (server side)

 2.A general issue: high thread t1 and low thread t2. If t1 is blocked
   on t2 (e.g, lock contention), and the fault occurs while t2 is
   executing in the component c. If a default thread t3 (say this has
   higher priority than t2, but lower than t1) is created by t2 in c
   to initialize c, due to the dependency (t1 blocked on t2), instead
   of running t3 or t1, we will still run t2, which does not run
   instillation (e.g, cos_init)

   Conditions: 1) mailbox is loaded by booter, not llbooter. Thread is
   not able to be switched in booter. So we can not leverage recovery
   thread by calling ll_booter. 2) After reboot the faulty component
   c, a default thread is created to initialize c. However, current
   boot_spd_thd() in booter creates a new thread at priority +1 (like
   the default thread in above example) and it won't run due to
   dependency.

   Solution: 

   Step 1) change boot_spd_thd(spd) to boot_spd(spd, failed)
   so we always create a highest priority thread so the component can
   be initialized properly. 
   
   Step 2) evt_trigger_all wakes up any thread that is blocked vis
   evt_wait.

   Step 3) lock_trigger_all(spd, dest) removes the dependency (since
   all locks contended are literally free again due to the fault. We
   need release and update these lock objects in lock component)
   
   
   Note: step 3 has to be done after step 2 since step 3 might switch
   to higher thread and call evt_wait again (which is normal)

 3.Different from ramfs: 
   1) ramfs -- one file (path) can have multiple torrents associated
   2) mbox  -- one mailbox always have 3 torrent associated

   !!!!!! Not good. This will incur the extra invocation head and
   contradict the design goal!!!!!." Since we are using unique id from
   name server now, it should not be a problem to restore the
   data. However, mbox has 3 torrents so the tracking per mailbox data
   is ok. Ramfs can have multiple torrents so the tracking per file
   data is not realistic. Instead, for ramfs we need track per file
   data for a group of torrents. The point is the number of tracking
   data should be bounded by the number of object (mailbox or file),
   not by torrent."

4. We need reserve cbuf id before call twritep: fault can occurs at
   any point while running in mailbox. We want to make sure the same
   cbid is reserved before it is read off from RB (might be put back
   to freelist if not do so)

   We need un-reserve cbuf id after treadp: once a cbufp id is
   returned from treadp, we know that we have read that cbufid off RB.
   
   Question: does cbufp (reference) have already taken care this?
 */

#include <cos_component.h>
#include <cos_debug.h>

#include <mbtorrent.h>
#include <cstub.h>
#include <print.h>
#include <cos_map.h>

#include <objtype.h>

volatile unsigned long long mbox_overhead_start, mbox_overhead_end;

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

/* the state of an torrent object(mailbox needs 3 torrents: 2 server, 1
 * client) */
enum {
	STATE_TSPLIT_PARENT,  // before any tsplit -- replay a tsplit
	STATE_TSPLIT_SERVER,  // after server first tsplit -- for trelease
	STATE_TSPLIT_CLIENT,  // after client first tsplit -- for trelease
	STATE_TSPLIT_READY,  // after create new connection with "" -- for trelease
	STATE_TREAD,
	STATE_TWRITE,
	STATE_TRELEASE
};

static unsigned long long meas_start, meas_end;
static int meas_flag = 0;

static unsigned long fcounter = 0;

// tracking data structure
struct rec_data_tor {
	td_t	p_tid;   // parent torrent id

	/* The following two ids could be different after a server
	 * crash, we need this mapping to find the correct server
	 * id  */
        td_t	c_tid;    // torrent id in client's view
        td_t	s_tid;  // torrent id in server's view

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;

	long		 evtid;

	unsigned int     evt_wait;  // for evt_wait after split	

	int              being_recovered;  // used to indicate if need to track the data

	int              state;
	unsigned long	 fcnt;
};

COS_MAP_CREATE_STATIC(uniq_tids);
CSLAB_CREATE(rd, sizeof(struct rec_data_tor));

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
	assert(rd);
	cslab_free_rd(rd);
	cos_map_del(&uniq_tids, tid);
	return;
}

#include <uniq_map.h>   // obtain unique id for a string path name

/**********************************************/
/* tracking path name and cbid for data recovery */
/**********************************************/
struct tid_uniqid_data {
	int tid;
	int uniq_id;
};

CVECT_CREATE_STATIC(idmapping_vect);
CSLAB_CREATE(tiduniq, sizeof(struct tid_uniqid_data));

static struct tid_uniqid_data *
tiduniq_lookup(int tid)
{
	return cvect_lookup(&idmapping_vect, tid);
}

static struct tid_uniqid_data *
tiduniq_alloc(int tid)
{
	struct tid_uniqid_data *idmapping;

	idmapping = cslab_alloc_tiduniq();
	assert(idmapping);
	if (cvect_add(&idmapping_vect, idmapping, tid)) {
		printc("can not add into cvect\n");
		BUG();
	}
	idmapping->tid = tid;
	return idmapping;
}

static void
tiduniq_dealloc(struct tid_uniqid_data *idmapping)
{
	assert(idmapping);
	if (cvect_del(&idmapping_vect, idmapping->tid)) BUG();
	cslab_free_tiduniq(idmapping);
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

static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}

/* return the state of a torrent object, only called after tsplit when
 * creating mail box. This is specific for mail box protocol */
static int
rd_get_state(char *param, tor_flags_t tflags)
{
	if (tflags & TOR_NONPERSIST) return STATE_TSPLIT_SERVER;  // begin of mail box
	if (!strlen(param)) return STATE_TSPLIT_READY;  // tsplit for a connection ("")
	return STATE_TSPLIT_CLIENT;   // else it must be the client tsplit
}

/* Bounded by the depth of torrent (how many time tsplit from). We can
 * also convert the following recursive to the stack based
 * non-recursive version. For now, just leave it. */
static void
rd_recover_state(struct rec_data_tor *rd)
{
	struct rec_data_tor *prd, *tmp = NULL;
	
	assert(rd && rd->p_tid >= 1 && rd->c_tid > 1);

	/* printc("calling recover!!!!!!\n"); */
	/* printc("thd %d restoring mbox for torrent id %d (parent id %d evt id %ld) \n", */
	/*        cos_get_thd_id(), rd->c_tid, rd->p_tid, rd->evtid); */
	
	/* If this is the server side final trelease, it is ok to just
	 * restore the "final" tid since only it is needed by the
	 * client to tsplit successfully */
	if (rd->p_tid > 1) {     // not tsplit from td_root
		assert((prd = map_rd_lookup(rd->p_tid)));
		prd->fcnt = fcounter;
		rd_recover_state(prd);
	}
	
	// has reached td_root, start rebuilding and no tracking...
	// tsplit returns the client id !!!!
	td_t tmp_tid = tsplit(cos_spd_id(), rd->p_tid, 
			      rd->param, rd->param_len, rd->tflags, rd->evtid);
	if (tmp_tid <= 1) return;
	
	assert((tmp = map_rd_lookup(tmp_tid)));
	rd->s_tid = tmp->s_tid;
	/* printc("got the new client side %d and its new server id %d\n",  */
	/*        tmp_tid, tmp->s_tid); */

        /* do not track the new tid for retsplitting.. (wish to avoid
	 * this) add this to ramfs as well */
	map_rd_delete(tmp_tid);  

	if (rd->evt_wait == TOR_WAIT) {
		/* printc("thd %d is waiting on event %ld \n", cos_get_thd_id(), rd->evtid); */
		evt_wait(cos_spd_id(), rd->evtid);
		/* printc("thd %d is back from evt_wait %ld \n", cos_get_thd_id(), rd->evtid); */
	}

	/* printc("....rd->state %d......\n", rd->state); */

	/* The data should be tracked for both direction, based on
	 * component id. TODO: small change to the uniq_map and use
	 * component id to save the data for different channel on the
	 * same mbox. For now, just simplify this-- only one direction
	 * in the test program. */
	if (rd->state == STATE_TSPLIT_CLIENT) {
		/* printc("....recover data now......\n"); */
		int tmp_sz, tmp_cbid, tmp_tot, tmp_i;
		struct tid_uniqid_data *idmapping = tiduniq_lookup(rd->s_tid);
		assert(idmapping);

		rd->being_recovered = 1; // will be reset after twritep

		tmp_tot = uniq_map_reflection(cos_spd_id(), idmapping->uniq_id, 0);
		/* printc("there are %d entries in total in uniq trie\n", tmp_tot); */
		/* Ignore the last one, since it is still on the stack */
		for (tmp_i = 1; tmp_i < tmp_tot; tmp_i++) {
			tmp_cbid = uniq_map_reflection(cos_spd_id(), 
						       idmapping->uniq_id, tmp_i);
			printc("cbid %d sz %d\n",
			       (tmp_cbid >> 16) & 0xFFFF, tmp_cbid & 0xFFFF);

			twritep(cos_spd_id(), rd->c_tid,
				(tmp_cbid >> 16) & 0xFFFF, tmp_cbid & 0xFFFF);
		}
	}

	return;
}

static struct rec_data_tor *
rd_update(td_t tid, int state)
{
        struct rec_data_tor *rd = NULL;

	/* in state machine, normally we do not track the td_root,
	 * however, the state of mail box case involves server and
	 * client. And protocol requires that server is presented
	 * before we can tsplit on the client side. For example,
	 * tsplit on client side faults and we need follow the state
	 * machine to rebuild server torrent first. Note: for torrent,
	 * tsplit/tread/twrite/trelease is always in the same
	 * component */

	// first tsplit 
	if (tid <= 1 && state == STATE_TSPLIT_PARENT) goto done;
        rd = map_rd_lookup(tid);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;

	rd->fcnt = fcounter;

	/* One issue: when recover both client/server, since the
	 * thread needs to be evt_wait as part of the protocol, do we
	 * need two threads to do the recovery?

	 * When replay (e.g, by upcall):	   
	 * Step 1: rebuild the protocol upon the fault (tsplits in
	 *         server and client respectively)	   
	 * Step 2: bring data back
	 * Step 3: resume the previous execution
	 */

	int server_spd, client_spd, release_state;
	
	/* STATE MACHINE */
	switch (state) {
	case STATE_TSPLIT_PARENT:
		/* we get here because the fault occurs during the
		 * tsplit. Then rd is referring to the parent torrent
		 * therefore we do not need update the state. We need
		 * start rebuilding parent's state */
		rd_recover_state(rd);
		break; 
	case STATE_TRELEASE:
		/* If we get here because the fault occurs while
		 * trelease, there are two situations: read more than
		 * write or write more than read. For example, t1
		 * writes many to RB and before t2 can read, t3
		 * crashes mail box server. In these cases, we need
		 * rebuild the mailbox in order to bring back the data
		 * and prepare for the late data accessing */
		release_state = rd->state;
		switch (release_state) {
		case STATE_TSPLIT_CLIENT:
			break; // this is the end of the current mbox, no need recovery !!!!!
		case STATE_TSPLIT_SERVER:
		case STATE_TSPLIT_READY:
			rd_recover_state(rd);
			break;
		default:
			assert(0);  // has to be one of above state		
		}
		break;
	case STATE_TREAD:
	case STATE_TWRITE:
		/* we need check protocol state since for
		 * treadp/twritep, we do not want to recreate protocol
		 * each time when we see the fault (reflect if parent
		 * torrent ids are presented in the mail box server,
		 * maybe after rebuilt due to a early fault. Then we
		 * do not need rebuild protocol). However, if the
		 * parent id is not in mail box server, we know they
		 * have not been rebuilt yet.
		 */
		rd_recover_state(rd);
		break;
	default:
		assert(0);
	}
	/* printc("thd %d restore mbox done!!!\n", cos_get_thd_id()); */
done:
	return rd;
}

static void
rd_cons(struct rec_data_tor *rd, td_t p_tid, td_t c_tid, td_t s_tid, 
	char *param, int len, tor_flags_t tflags, long evtid)
{
	/* printc("rd_cons: parent tid %d c_tid %d s_tid %d\n",  p_tid, c_tid, s_tid); */
	assert(rd);

	/* C_TAKE(cos_spd_id()); */

	rd->p_tid	 = p_tid;
	rd->c_tid 	 = c_tid;
	rd->s_tid 	 = s_tid;
	rd->param	 = param;
	rd->param_len	 = len;
	rd->tflags	 = tflags;
	rd->evtid	 = evtid;
	rd->fcnt	 = fcounter;
	// set object state to server/client/ready, for trelease
	rd->state = rd_get_state(param, tflags);
	
	rd->evt_wait	 = tflags & TOR_WAIT;  // add for now, wait after tsplit

	/* C_RELEASE(cos_spd_id()); */
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
	struct __sg_tsplit_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tsplit_data);

        struct rec_data_tor *rd = NULL;
        td_t		cli_tid	   = 0;
        td_t		ser_tid	   = 0;
	
	/* printc("\n[[cli: thd %d is calling tsplit from spd %ld (parent tid %d, evt %ld)]]\n",  */
	/*        cos_get_thd_id(), cos_spd_id(), parent_tid, evtid); */

	assert(parent_tid >= 1);
        assert(param && len >= 0);
        assert(param[len] == '\0');

        if (first == 0) {
		cos_map_init_static(&uniq_tids);
		first = 1;
	}
	
redo:
        /* printc("<<< In: call tsplit  (thread %d) >>>\n", cos_get_thd_id()); */
	rd = rd_update(parent_tid, STATE_TSPLIT_PARENT);
	/* assert(rd); */
	
	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;
        d->parent_tid = parent_tid;   
	d->tflags     = tflags;
	d->evtid      = evtid;
        d->len[0]     = 0;
        d->len[1]     = len;
	memcpy(&d->data[0], param, len + 1);

#ifdef BENCHMARK_MEAS_TSPLIT
	rdtscll(meas_end);
	printc("end measuring.....(thd %d)\n", cos_get_thd_id());
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu (by thd %d)\n", 
		       meas_end - meas_start, cos_get_thd_id());
	}
#endif		

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

        if (unlikely(fault)) {

#ifdef BENCHMARK_MEAS_TSPLIT
		meas_flag = 1;
		printc("start measuring.....(thd %d)\n", cos_get_thd_id());
		rdtscll(meas_start);
#endif		
		CSTUB_FAULT_UPDATE();
		memset(&d->data[0], 0, len);  // why?
		cbuf_free(cb);
		goto redo;
	}
        cbuf_free(cb);


	rdtscll(mbox_overhead_start);
	
	// ret is server side id
	ser_tid = ret;
	/* printc("cli: tsplit create a new rd %d in spd %ld\n", ser_tid, cos_spd_id()); */
	assert (ser_tid >= 1);
	
	struct uniqmap_data *dm = NULL;
	cbuf_t cb_p;
	int uniq_id;
	int tmp_sz = strlen(&d->data[0]);
	/* printc("sz of the passed in string path name is %d\n", sz); */
	if (tmp_sz > 0) {
		dm = cbuf_alloc(tmp_sz, &cb_p);
		assert(dm);
		dm->parent_tid = parent_tid;
		dm->server_tid = ser_tid;
		dm->sz         = tmp_sz;
		memcpy(dm->data, &d->data[0], tmp_sz);
	
		/* if does not exist, create it in uniqmap. Use the
		 * most recent server tid to find uniq_id in uniqmap
		 * when twritep/treadp. So we do not track here. */
		uniq_id = uniq_map_lookup(cos_spd_id(), cb_p, 
					(tmp_sz + sizeof(struct uniqmap_data)));
		assert(uniq_id);
		
		struct tid_uniqid_data *idmapping;
		if (!(idmapping = tiduniq_lookup(ser_tid))) {
			idmapping = tiduniq_alloc(ser_tid);
		}
		assert(idmapping);

		/* printc("server: check lookup uniqid for tid %d\n", ser_tid); */
		struct tid_uniqid_data *tmp = tiduniq_lookup(ser_tid);
		assert(tmp);		

		idmapping->uniq_id = uniq_id;
		
		cbuf_free(cb_p);
	} else {
                /* mbox protocol asks for "" as the last tsplit. So we
		 * only need the uniq id for parent */
		struct tid_uniqid_data *emptymapping, *tmp;
		if (!(emptymapping = tiduniq_lookup(ser_tid))) {
			emptymapping = tiduniq_alloc(ser_tid);
		}
		assert(emptymapping);
		/* printc("server: check lookup uniqid for tid %d\n", d->parent_tid); */
		tmp = tiduniq_lookup(d->parent_tid);
		assert(tmp);
		emptymapping->uniq_id = tmp->uniq_id;
	}
	
	char *l_param = ""; // len can be zero (this is not the case for mbox "")
	if (len > 0) { 
		l_param = param_save(param, len);
		assert(l_param);
	}

	cli_tid = map_rd_create();
	assert(cli_tid >= 2);

	rd = map_rd_lookup(cli_tid);
        assert(rd);

        rd_cons(rd, parent_tid, cli_tid, ser_tid, l_param, len, tflags, evtid);
	
	/* printc("tsplit done!!! ... ser %d cli %d\n\n", ret, cli_tid); */

	/* we return the entry's id allocated by map_rd_create, not
	 * server side id */

	rdtscll(mbox_overhead_end);
	printc("tsplit interface overhead %llu\n", 
	       mbox_overhead_end - mbox_overhead_start);		
	

	return cli_tid;
}

/* after the fault, recovery should replay the twrite, but without
 * tracking these data again !!!! */
CSTUB_FN(int, twritep)(struct usr_inv_cap *uc,
		       spdid_t spdid, td_t c_tid, int cbid, int sz)
{
	int ret;
	long fault = 0;

        struct rec_data_tor *rd = NULL;

	rdtscll(mbox_overhead_start);
redo:
        /* printc("<<< In: call twritep  (thread %d) >>>\n", cos_get_thd_id()); */
        rd = rd_update(c_tid, STATE_TWRITE);
	assert(rd);

	/* At here we know that the data(e.g, cbid is going to RB),
	 * and we track this info in the name server for data
	 * recovery) */
	if (!rd->being_recovered) {
		struct tid_uniqid_data *idmapping = tiduniq_lookup(rd->s_tid);
		assert(idmapping);
		/* printc("<<<unqi_id %d cbid %d sz %d>>>\n", idmapping->uniq_id, cbid, sz); */
		if (uniq_map_add(cos_spd_id(), idmapping->uniq_id, cbid, sz)) assert(0);
	}

	rdtscll(mbox_overhead_end);
	printc("twritep interface overhead %llu\n", 
	       mbox_overhead_end - mbox_overhead_start);		
		

#ifdef BENCHMARK_MEAS_TWRITEP
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, rd->s_tid, cbid, sz);
	if (unlikely (fault)){

#ifdef BENCHMARK_MEAS_TWRITEP
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	/* printc("<<< remove written cbuf id (thd %d)>>>\n", cos_get_thd_id()); */
	rd->being_recovered = 0;  // this is due to the current data is still on stack

	return ret;
}

CSTUB_FN(int, treadp)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t c_tid, int *off, int *sz)
{
	int ret = 0;
	long fault = 0;
        struct rec_data_tor *rd = NULL;
redo:
        /* printc("<<< In: call treadp  (thread %d) >>>\n", cos_get_thd_id()); */
        rd = rd_update(c_tid, STATE_TREAD);
	assert(rd);
        /* printc("<<< In: call treadp  (thread %d)... after rd_update >>>\n", cos_get_thd_id()); */

#ifdef BENCHMARK_MEAS_TREADP
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		

	CSTUB_INVOKE_3RETS(ret, fault, *off, *sz, uc, 2, spdid, rd->s_tid);
	if (unlikely(fault)){

#ifdef BENCHMARK_MEAS_TREADP
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		

		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	// ret is the cbufid for mbox
	if (ret > 0) {

		rdtscll(mbox_overhead_start);

		/* printc("server: treadp lookup uniqid for tid %d\n", rd->s_tid); */
		struct tid_uniqid_data *idmapping = tiduniq_lookup(rd->s_tid);
		assert(idmapping);
		/* printc("unqi_id %d\n", idmapping->uniq_id); */
		if (uniq_map_del(cos_spd_id(), idmapping->uniq_id, ret)) assert(0);

		rdtscll(mbox_overhead_end);
		printc("treadp interface overhead %llu\n", 
		       mbox_overhead_end - mbox_overhead_start);		
	
	}

	return ret;
}


CSTUB_FN(void, trelease)(struct usr_inv_cap *uc,
			 spdid_t spdid, td_t c_tid)
{
	int ret;
	long fault = 0;
        struct rec_data_tor *rd = NULL;

redo:
        /* printc("<<< In: call trelease  (thread %d) >>>\n", cos_get_thd_id()); */
        rd = rd_update(c_tid, STATE_TRELEASE);
	assert(rd);

#ifdef BENCHMARK_MEAS_TRELEASE
	rdtscll(meas_end);
	printc("end measuring.....\n");
	if (meas_flag) {
		meas_flag = 0;
		printc("recovery an event cost: %llu\n", meas_end - meas_start);
	}
#endif		

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_tid);
	if (unlikely(fault)){

#ifdef BENCHMARK_MEAS_TRELEASE
		meas_flag = 1;
		printc("start measuring.....\n");
		rdtscll(meas_start);
#endif		

		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	rdtscll(mbox_overhead_start);

	map_rd_delete(c_tid);
	// TODO: delete un uniq_map as well? keep it around for now

	rdtscll(mbox_overhead_end);
	printc("trelease interface overhead %llu\n", 
	       mbox_overhead_end - mbox_overhead_start);		

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
