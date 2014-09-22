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

   Since we are using unique id from name server now, it should not be
   a problem to restore the data. However, mbox has 3 torrents so the
   tracking per mailbox data is ok. Ramfs can have multiple torrents
   so the tracking per file data is not realistic. Instead, for ramfs
   we need track per file data for a group of torrents. The point is
   the number of tracking data should be bounded by the number of
   object (mailbox or file), not by torrent.

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

static unsigned long fcounter = 0;

/* Linked list for tracking cbufp ids that are passed in twritep. Any
   cbufp on RB (not received yet and deref yet should be claimed)
*/
struct cb_node {
	int cbid;
	int sz;
	struct cb_node *next, *prev;
};

// tracking data structure
struct rec_data_tor {
	td_t	ptid;
        td_t	tid;

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;

	long		 evtid;

	unsigned int     evt_wait;  // for evt_wait after split	
	struct cb_node   clist_head;  // see above explanation

	int              state;
	unsigned long	 fcnt;
};

CSLAB_CREATE(cbn, sizeof(struct cb_node));

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

static struct rec_data_tor *
rd_lookup(td_t td)
{
	return cvect_lookup(&rec_vect, td); 
}

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
	if (cvect_del(&rec_vect, rd->tid)) BUG();
	cslab_free_rd(rd);
}

/* return the state of a torrent object, only called after tsplit when
 * creating mail box. This is specific for mail box protocol */
static int
rd_get_state(char *param, tor_flags_t tflags)
{
	if (tflags & TOR_NONPERSIST) return STATE_TSPLIT_SERVER;  // begin of mail box
	if (!strcmp(param, "")) return STATE_TSPLIT_READY;  // tsplit for a connection
	return STATE_TSPLIT_CLIENT;   // else it must be the client tsplit
}

static void
rd_cons(struct rec_data_tor *rd, td_t ptid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid)
{
	printc("rd_cons: tid %d (its parent tid %d)\n",  ptid, tid);
	assert(rd);

	C_TAKE(cos_spd_id());

	rd->ptid	 = ptid;
	rd->tid 	 = tid;
	rd->param	 = param;
	rd->param_len	 = len;
	rd->tflags	 = tflags;
	rd->evtid	 = evtid;
	rd->fcnt	 = fcounter;
	// set object state to server/client/ready, for trelease
	rd->state = rd_get_state(param, tflags);
	
	rd->evt_wait	 = tflags & TOR_WAIT;  // add for now, wait after tsplit
	rd->clist_head.cbid = 0;
	INIT_LIST(&rd->clist_head, next, prev);
	assert(EMPTY_LIST(&rd->clist_head, next, prev));
	
	C_RELEASE(cos_spd_id());
	return;
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
extern int lock_trigger_all(spdid_t spdid, int dest);  

static void
rd_reflection(int cap)
{
	assert(cap);

	C_TAKE(cos_spd_id());

	int count_obj = 0; // reflected objects
	int dest_spd = cap_to_dest(cap);
	
	/* // remove the mapped page for mailbox spd */
	/* vaddr_t addr; */
	/* count_obj = mman_reflect(cos_spd_id(), dest_spd, 1); */
	/* printc("mbox relfects on mmgr: %d objs\n", count_obj); */
	/* while (count_obj--) { */
	/* 	addr = mman_reflect(cos_spd_id(), dest_spd, 0); */
	/* 	/\* printc("evt mman_release: %p addr\n", (void *)addr); *\/ */
	/* 	mman_release_page(cos_spd_id(), addr, dest_spd); */
	/* } */

	/* see top comments */
	printc("in rd_reflection dest is %d\n", dest_spd);
	evt_trigger_all(cos_spd_id());  // due to protocol
	lock_trigger_all(cos_spd_id(), dest_spd);     // due to lock contention in mailbox

	C_RELEASE(cos_spd_id());
	printc("mailbox reflection done (thd %d)\n\n", cos_get_thd_id());
	return;
}
#endif

td_t tresplit(spdid_t spdid, td_t tid, char *param, 
	      int len, tor_flags_t tflags, long evtid, td_t old_tid);

/* Bounded by the depth of torrent (how many time tsplit from)*/
static void
rd_recover_state(struct rec_data_tor *rd)
{
	struct rec_data_tor *prd;
	
	assert(rd && rd->ptid >= 1 && rd->tid > 1);
	
	/* printc("thd %d restoring mbox for torrent id %d (parent id %d evt id %ld) \n",  */
	/*        cos_get_thd_id(), rd->tid, rd->ptid, rd->evtid); */

	if (rd->ptid > 1) {     // not tsplit from td_root
		assert((prd = rd_lookup(rd->ptid)));
		prd->fcnt = fcounter;
		rd_recover_state(prd);
	}
	
	// has reached td_root, start rebuilding and no tracking...
	td_t new_tid = tresplit(cos_spd_id(), rd->ptid, 
				rd->param, rd->param_len, rd->tflags, rd->evtid, rd->tid);
	/* assert(new_tid > 1); */
	if (new_tid <= 1) return;
	printc("got the new server side torrent %d \n", new_tid);

	if (rd->evt_wait == TOR_WAIT) {
		printc("thd %d is waiting on event %ld \n", cos_get_thd_id(), rd->evtid);
		evt_wait(cos_spd_id(), rd->evtid);
		printc("thd %d is back from evt_wait %ld \n", cos_get_thd_id(), rd->evtid);
	}

	return;
}

static struct rec_data_tor *
rd_update(td_t tid, int state)
{
        struct rec_data_tor *rd = NULL;

	/* C_TAKE(cos_spd_id()); */

	/* in state machine, normally we do not track the td_root,
	 * however, the state of mail box case involves server and
	 * client. And protocol requires that server is presented
	 * before we can tsplit on the client side. For example,
	 * tsplit on client side faults and we need follow the state
	 * machine to rebuild server torrent first. Note: for torrent,
	 * tsplit/tread/twrite/trelease is always in the same
	 * component */

	if (tid <= 1) goto done;   // both first tsplit and last trelease
        rd = rd_lookup(tid);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;

	rd->fcnt = fcounter;

	/* If the fault occurs at the tsplit/trelease, we need to do
	 * tsplit again (and its parent torrent if it has any ). This
	 * is different from event interface (EVT_CREATE) because
	 * recreating event does not require a parent event to be
	 * presented first, torrent does
	
	 * One issue: when recover both client/server, since the
	 * thread needs to be evt_wait as part of the protocol, do we
	 * need two threads to do the recovery?

	 * When replay (e.g, by upcall):	   
	 * Step 1: rebuild the protocol upon the fault (tsplits in
	 *         server and client respectively)	   
	 * Step 2: bring data back here too  
	 * Step 3: resume the previous execution
	 */

	int server_spd, client_spd, release_state;
	
	/* STATE MACHINE */
	switch (state) {
	case STATE_TSPLIT_PARENT:
		/* we get here because the fault occurs during the
		 * tsplit. Then rd is referring to the parent torrent
		 * . Therefore we do not need update the state. We
		 * need start rebuilding parent's state if the parent
		 * is not rd_root */
		rd_recover_state(rd);
		break; 
	case STATE_TRELEASE:
		/* If we get here because the fault occurs while
		 * trelease, there are two situations: read more than
		 * write or write more than read. For example, t1
		 * writes many to RB and before t2 can read, t3
		 * crashes mail box server. Another example is that
		 * what is the priority of the thread that tries to
		 * read/write is not always high/low. In these cases,
		 * we need rebuild the mailbox in order to bring back
		 * the data and prepare for the late data accessing */
		release_state = rd->state;
		switch (release_state) {
		case STATE_TSPLIT_SERVER:
		case STATE_TSPLIT_CLIENT:
			/* at the proper priority, reflection over
			 * event manager should alreay wake up server
			 * side evt_wait thread and "see" the fault
			 * when returns from event component */
		case STATE_TSPLIT_READY:
			/* Similar to above case */
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
		 * torrent ids are presented in the mial box server,
		 * maybe after rebuilt due to a early fault. Then we
		 * do not need rebuild protocol). However, if the
		 * parent id is not in mail box server, we know they
		 * have not been rebuilt yet.
		 */
		printc("check if the protocol has been rebuilt\n");
		if (treflection(cos_spd_id(), tid)) break;
		else rd_recover_state(rd);
		break;
	default:
		assert(0);
	}

	/* /\* rd's parent should not be a mailbox channel, so just check */
	/*  * this for rd and write cbid back to channel if found any *\/ */
	/* if (!EMPTY_LIST(&rd->clist_head, next, prev)) { */
	/* 	struct cb_node *cbn, *tmp; */
	/* 	cbn = LAST_LIST(&rd->clist_head, next, prev); */
	/* 	assert(cbn); */
	/* 	while (cbn != &rd->clist_head) { */
	/* 		/\* printc("cli: recovering twritep s_tid %d cbid %d sz %d\n", *\/ */
	/* 		/\*        rd->s_tid, cbn->cbid, cbn->sz); *\/ */
	/* 		twritep(cos_spd_id(), rd->s_tid, cbn->cbid, cbn->sz); // > 0? */
	/* 		tmp = cbn; */
	/* 		cbn = LAST_LIST(cbn, next, prev); */
	/* 		REM_LIST(tmp, next, prev); */
	/* 	} */
	/* } */	
	printc("thd %d restore mbox done!!!\n", cos_get_thd_id());
done:
	/* C_RELEASE(cos_spd_id()); */
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

        struct rec_data_tor *rd = NULL;
	printc("\n[[cli: thd %d is calling tsplit from spd %ld (parent tid %d, evt %ld)]]\n", 
	       cos_get_thd_id(), cos_spd_id(), tid, evtid);

	assert(tid >= 1);
        assert(param && len >= 0);
        assert(param[len] == '\0');
redo:
        printc("<<< In: call tsplit  (thread %d) >>>\n", cos_get_thd_id());
	rd = rd_update(tid, STATE_TSPLIT_PARENT);
	/* assert(rd); */
	
	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;
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
		memset(&d->data[0], 0, len);  // why?
		cbuf_free(cb);
		goto redo;
	}

	assert (ret >= 1);
	printc("cli: tsplit create a new rd in spd %ld\n", cos_spd_id());
        rd = rd_alloc(ret);
        assert(rd);
	char *l_param = param_save(param, len);
        rd_cons(rd, tid, ret, l_param, len, tflags, evtid);  // tid is parent tid

	printc("tsplit done!!!\n\n");
        cbuf_free(cb);

	return ret;
}

/* This function is used to create a new server side id for the old
 * torrent id, and do the cache in the server. Caching on the server
 * side can avoid tracking mapping between old and new id at client
 * interface (the mapping should be only valid between faults!!)

 * More generally if we keep the mapping between old id and new id in
 * the server (say, by caching), we need this function and we do not
 * need to track the mapping on the client side. However, if we keep
 * the mappings at client interface, we do not need this function
 * since the client can find the correct server id. 
 
 NO tracking here!!!! If failed, just replay. This function can return
 ERRON if it can not create a new torrent. For example, one side of
 mailbox has been tore down via trelease.
 */
struct __sg_tresplit_data {
	td_t tid;
	td_t old_tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};
CSTUB_FN(td_t, tresplit)(struct usr_inv_cap *uc,
			 spdid_t spdid, td_t tid, char * param,
			 int len, tor_flags_t tflags, long evtid, td_t old_tid)
{
	long fault = 0;
	unsigned long ret;

	struct __sg_tresplit_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tresplit_data);
	
        assert(param && len >= 0);
        assert(param[len] == '\0');
redo:
        printc("<<< In: call tresplit  (thread %d) >>>\n", cos_get_thd_id());
	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;
        d->tid    = tid;
	d->old_tid = old_tid;
	d->tflags = tflags;
	d->evtid  = evtid;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len + 1);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);
	if (unlikely (fault)){
		printc("re tsplit found a fault (thd %d)\n", cos_get_thd_id());		
		CSTUB_FAULT_UPDATE();
		cbuf_free(cb);
		goto redo;
	}

	cbuf_free(cb);
	return ret;
}


CSTUB_FN(int, twritep)(struct usr_inv_cap *uc,
		       spdid_t spdid, td_t tid, int cbid, int sz)
{
	int ret;
	long fault = 0;

        struct rec_data_tor *rd = NULL;
redo:
        printc("<<< In: call twritep  (thread %d) >>>\n", cos_get_thd_id());
        rd = rd_update(tid, STATE_TWRITE);
	assert(rd);

	/* struct cb_node *cn;	 */
	/* assert(rd); */
	/* cn = cslab_alloc_cbn(); */
	/* assert(cn); */
	/* cn->cbid = cbid; */
	/* cn->sz   = sz; */
	/* printc("<<< add written cbuf id (thd %d)>>>\n", cos_get_thd_id()); */
	/* ADD_LIST(&rd->clist_head, cn, next, prev); */

	/* Jiguo: Before return from twritep, make cbufp page read
	 * only and track in cbuf_manager. The purpose is to avoid
	 * corrupt the content in the shared pages before they are
	 * read (dequeued from mailbox) cbuf_c_claim(dest_spd, cbid);
	 * do this in server
	*/
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, tid, cbid, sz);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	/* printc("<<< remove written cbuf id (thd %d)>>>\n", cos_get_thd_id()); */
	/* REM_LIST(cn, next, prev); */

	return ret;
}

CSTUB_FN(int, treadp)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t tid, int *off, int *sz)
{
	int ret = 0;
	long fault = 0;
        struct rec_data_tor *rd = NULL;
redo:
        printc("<<< In: call treadp  (thread %d) >>>\n", cos_get_thd_id());
        rd = rd_update(tid, STATE_TREAD);
	assert(rd);

	CSTUB_INVOKE_3RETS(ret, fault, *off, *sz, uc, 2, spdid, tid);
	if (unlikely(fault)){
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
        struct rec_data_tor *rd = NULL;

redo:
        printc("<<< In: call trelease  (thread %d) >>>\n", cos_get_thd_id());
        rd = rd_update(tid, STATE_TRELEASE);
	assert(rd);

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, tid);
	if (unlikely(fault)){
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
