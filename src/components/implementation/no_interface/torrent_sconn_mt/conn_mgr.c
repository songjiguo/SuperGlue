/**
 * Copyright 2012 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Adapted from the connection manager (no_interface/conn_mgr/) and
 * the file descriptor api (fd/api).
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <cvect.h>
#include <print.h>
#include <errno.h>
#include <cos_synchronization.h>
#include <sys/socket.h>
#include <stdio.h>
#include <torrent.h>

#include <c3_test.h>

extern td_t server_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void server_trelease(spdid_t spdid, td_t tid);
extern int server_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int server_twrite(spdid_t spdid, td_t td, int cbid, int sz);
#include <sched.h>


#include <periodic_wake.h>   // for debug only
static volatile unsigned long connmgr_from_tsplit_cnt = 0;
static volatile unsigned long connmgr_from_tread_cnt = 0;
static volatile unsigned long connmgr_from_twrite_cnt = 0;
static volatile unsigned long connmgr_tsplit_cnt = 0;
static volatile unsigned long connmgr_tread_cnt = 0;
static volatile unsigned long connmgr_twrite_cnt = 0;
static volatile unsigned long amnt_0_break_cnt = 0;
static volatile unsigned long num_connection = 0;

static volatile int debug_thd = 0;

static cos_lock_t sc_lock;
cos_lock_t cbuf_lock;
#define LOCK() if (lock_take(&sc_lock)) BUG();
#define UNLOCK() if (lock_release(&sc_lock)) BUG();


#define BUFF_SZ 2048//1401 //(COS_MAX_ARG_SZ/2)

CVECT_CREATE_STATIC(tor_from);
CVECT_CREATE_STATIC(tor_to);

static inline int 
tor_get_to(int from, long *teid) 
{ 
	int val = (int)cvect_lookup(&tor_from, from);
	*teid = val >> 16;
	return val & ((1<<16)-1); 
}

static inline int 
tor_get_from(int to, long *feid) 
{ 
	int val = (int)cvect_lookup(&tor_to, to);
	*feid = val >> 16;
	return val & ((1<<16)-1); 
}

static inline void 
tor_add_pair(int from, int to, long feid, long teid)
{
#define MAXVAL (1<<16)
	assert(from < MAXVAL);
	assert(to   < MAXVAL);
	assert(feid < MAXVAL);
	assert(teid < MAXVAL);
	if (cvect_add(&tor_from, (void*)((teid << 16) | to), from) < 0) BUG();
	if (cvect_add(&tor_to, (void*)((feid << 16) | from), to) < 0) BUG();
}

static inline void
tor_del_pair(int from, int to)
{
	cvect_del(&tor_from, from);
	cvect_del(&tor_to, to);
}

CVECT_CREATE_STATIC(evts);
#define EVT_CACHE_SZ 0
int evt_cache[EVT_CACHE_SZ];
int ncached = 0;

long evt_all[MAX_NUM_THREADS] = {0,};

static inline long
evt_wait_all(void) { return evt_wait(cos_spd_id(), evt_all[cos_get_thd_id()]); }

/* 
 * tor > 0 == event is "from"
 * tor < 0 == event is "to"
 */
static inline long
evt_get_thdid(int thdid)
{
	long eid;
	
	if (!evt_all[thdid]) {
		evt_all[thdid] = evt_split(cos_spd_id(), 0, 1);
	}
	assert(evt_all[thdid]);

	/* /\* if we do not cache, it is faster? (8000 reqs/sec) *\/ */
	/* eid = evt_split(cos_spd_id(), evt_all[thdid], 0); */

	if (ncached == 0) {
		eid = evt_split(cos_spd_id(), evt_all[thdid], 0);
		/* printc("conn_mgr: tsplit a new evt id %d for thd %d (parent %d, group 0)\n", */
		/*        eid, thdid, evt_all[thdid]); */
	} else {
		eid = evt_cache[--ncached];
		/* printc("conn_mgr: tsplit a cached evt id %d for thd %d (parent %d, group 0)\n", eid, thdid, evt_all[thdid]); */
	}
	assert(eid > 0);

	/* eid = (ncached == 0) ? */
	/* 	evt_split(cos_spd_id(), evt_all[thdid], 0) : */
	/* 	evt_cache[--ncached]; */
	/* assert(eid > 0); */

	return eid;
}

static inline long
evt_get() { return evt_get_thdid(cos_get_thd_id()); }

static inline void
evt_put(long evtid)
{
	// if we do not cache, it is faster? (8000 reqs/sec)
	/* evt_free(cos_spd_id(), evtid);  // test */
	
	if (ncached >= EVT_CACHE_SZ) {
		evt_free(cos_spd_id(), evtid);
	} else {
		evt_cache[ncached++] = evtid;
	}
}

/* positive return value == "from", negative == "to" */
static inline int
evt_torrent(long evtid) { return (int)cvect_lookup(&evts, evtid); }

static inline void
evt_add(int tid, long evtid) { cvect_add(&evts, (void*)tid, evtid); }

struct tor_conn {
	int  from, to;
	long feid, teid;
};

static inline void 
mapping_add(int from, int to, long feid, long teid)
{
	long tf, tt;

	LOCK();
	tor_add_pair(from, to, feid, teid);

	if (cvect_lookup(&evts, feid)) {
		printc("thd %d adds id %ld in spd %ld, already exist(mapping_add feid)\n", 
		       cos_get_thd_id(), feid, cos_spd_id());
	}

	evt_add(from,    feid);

	if (cvect_lookup(&evts, teid)) {
		printc("thd %d adds id %ld in spd %ld, already exist(mapping_add teid)\n", 
		       cos_get_thd_id(), teid, cos_spd_id());
	}

	evt_add(to * -1, teid);
	assert(tor_get_to(from, &tt) == to);
	assert(tor_get_from(to, &tf) == from);
	assert(evt_torrent(feid) == from);
	assert(evt_torrent(teid) == (-1*to));
	assert(tt == teid);
	assert(tf == feid);
	UNLOCK();
}

static inline void
mapping_remove(int from, int to, long feid, long teid)
{
	LOCK();
	tor_del_pair(from, to);
	cvect_del(&evts, feid);
	cvect_del(&evts, teid);
	UNLOCK();
}

static int debug_first_accept = 0;
static int debug_teid = 0;
static cbuf_t debug_to; 

static int print_once = 0;

static void 
accept_new(int accept_fd, int test)
{
	int from, to, feid, teid;

	while (1) {
		feid = evt_get();
		
		/* printc("conn_mgr: tsplit new feid %d for thd %d\n", feid, cos_get_thd_id()); */
		assert(feid > 0);
		from = server_tsplit(cos_spd_id(), accept_fd, "", 0, TOR_RW, feid);
		connmgr_from_tsplit_cnt++;
		num_connection++;
		if (!print_once && num_connection > 2) {
			print_once = 1;
			/* printc("<<<num conn %ld>>>\n", num_connection); */
		}
		assert(from != accept_fd);
		if (-EAGAIN == from) {
			evt_put(feid);
			return;
		} else if (from < 0) {
			/* printc("from torrent returned %d\n", from); */
			BUG();
			return;
		}

		teid = evt_get();

		/* printc("conn_mgr: tsplit new teid %d for thd %d (fs)\n",  */
		/*        teid, cos_get_thd_id()); */
		assert(teid > 0);
		to = tsplit(cos_spd_id(), td_root, "", 0, TOR_RW, teid);
		if (to < 0) {
			/* printc("torrent split returned %d", to); */
			BUG();
		}
		connmgr_tsplit_cnt++;
		mapping_add(from, to, feid, teid);
	}
}

static int
from_data_new(struct tor_conn *tc, int test)
{
	int from, to, amnt;
	char *buf;
	cbuf_t cb;

	int ret = 0;

	from = tc->from;
	to   = tc->to;
	while (1) {
		int ret;

		buf = cbuf_alloc(BUFF_SZ, &cb);
		assert(buf);
		/* printc("connmgr reads net (thd %d)\n", cos_get_thd_id()); */
		amnt = server_tread(cos_spd_id(), from, cb, BUFF_SZ-1);
		connmgr_from_tread_cnt++;
		/* if (test < 0) printc("from data new reads net amnt %d\n", amnt); */
		if (test < 0 && amnt == 0) {
			mapping_remove(from, to, tc->feid, tc->teid);
			server_trelease(cos_spd_id(), from);
			trelease(cos_spd_id(), to);
			goto done; // Jiguo: after a fault and pretend
		}
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			/* printc("read from fd %d produced %d.\n", from, amnt); */
			BUG();
		}
		
		assert(amnt <= BUFF_SZ);
		if (amnt != (ret = twrite(cos_spd_id(), to, cb, amnt))) {
			printc("conn_mgr: write failed w/ %d on fd %d\n", ret, to);
			goto close;
		}
		connmgr_twrite_cnt++;

		cbuf_free(cb);
	}
done:
	cbuf_free(cb);
	return ret;
close:
	mapping_remove(from, to, tc->feid, tc->teid);
	/* printc("net_close from_data_new (in trelease) (thd %d, from %d)\n", */
	/*        cos_get_thd_id(), from); */
	server_trelease(cos_spd_id(), from);
	num_connection--;
	trelease(cos_spd_id(), to);
	assert(tc->feid && tc->teid);
	/* printc("(1) evt_put: thd %d evt %d\n", cos_get_thd_id(), tc->feid); */
	evt_put(tc->feid);
	/* printc("(2) evt_put: thd %d evt %d\n", cos_get_thd_id(), tc->teid); */
	evt_put(tc->teid);
	goto done;
}

static int debug_first_to = 0;
char *debug_buf_to = NULL;
static int debug_amnt_to = 0;
static cbuf_t debug_cb_to; 

static int
to_data_new(struct tor_conn *tc, int test)
{
	int from, to, amnt;
	char *buf;
	cbuf_t cb;

	int ret = 0;

	from = tc->from;
	to   = tc->to;

	while (1) {
		int ret;

		if (!(buf = cbuf_alloc(BUFF_SZ, &cb))) BUG();
		/* printc("connmgr reads https\n"); */
		amnt = tread(cos_spd_id(), to, cb, BUFF_SZ-1);
		connmgr_tread_cnt++;
		/* if (test < 0) printc("to data new reads amnt %d\n", amnt); */
		/* should not delete event !!!! see explanation in loop */
		if (test < 0 && amnt == 0) {
			mapping_remove(from, to, tc->feid, tc->teid);
			server_trelease(cos_spd_id(), from);
			trelease(cos_spd_id(), to);
			goto done; // Jiguo: after a fault and pretend
		}
		if (0 == amnt) {
			amnt_0_break_cnt++;
			break;
		} else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		assert(amnt <= BUFF_SZ);

		/* printc("connmgr writes to net\n"); */
		if (amnt != (ret = server_twrite(cos_spd_id(), from, cb, amnt))) {
			printc("conn_mgr: write failed w/ %d of %d on fd %d\n", 
			       ret, amnt, to);
			goto close;
		}
		connmgr_from_twrite_cnt++;
		cbuf_free(cb);
	}

done:
	cbuf_free(cb);
	return ret;
close:
	mapping_remove(from, to, tc->feid, tc->teid);
	/* printc("net_close to_data_new (in trelease) (thd %d, from %d)\n",  */
	/*        cos_get_thd_id(), from); */
	server_trelease(cos_spd_id(), from);
	num_connection--;
	trelease(cos_spd_id(), to);
	assert(tc->feid && tc->teid);
	/* printc("(3)evt_put: thd %d evt %d\n", cos_get_thd_id(), tc->feid); */
	evt_put(tc->feid);
	/* printc("(4)evt_put: thd %d evt %d\n", cos_get_thd_id(), tc->feid); */
	evt_put(tc->teid);
	goto done;
}

char *create_str;
int   __port, __prio, hpthd;

static void init(char *init_str)
{
	int nthds;

	cvect_init_static(&evts);
	cvect_init_static(&tor_from);
	cvect_init_static(&tor_to);
	lock_static_init(&sc_lock);
	
	lock_static_init(&cbuf_lock); // Jiguo: test
		
	sscanf(init_str, "%d:%d:%d", &nthds, &__prio, &__port);
	printc("nthds:%d, prio:%d, port %d\n", nthds, __prio, __port);
	create_str = strstr(init_str, "/");
	assert(create_str);

	for (; nthds > 0 ; nthds--) {
		union sched_param sp;
		int thdid;
		
		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = __prio++;
		thdid = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		if (!hpthd) hpthd = thdid;
	}
}

u64_t meas, avg, total = 0, vartot;
int meascnt = 0, varcnt;

void
meas_record(u64_t meas)
{
	if (cos_get_thd_id() != hpthd) return;
	total += meas;
	meascnt++;
	assert(meascnt > 0);
	avg = total/meascnt;
	if (meas > avg) {
		vartot += meas-avg;
		varcnt++;
	}
}

extern cvect_t rec_evt_map;
extern struct rec_data_evt *evts_grp_list_head;

/* recovery data structure for evt */
struct rec_data_evt {
	spdid_t       spdid;
	long          c_evtid;
	long          s_evtid;

	long          p_evtid;  // parent event id (eg needs this)
	int           grp;      // same as above

	struct rec_data_evt *next, *prev;  // track all init events in a group
	struct rec_data_evt *p_next, *p_prev;  // link all parent event

	unsigned int  state;
	unsigned long fcnt;
};
static struct rec_data_evt *
rdevt_lookup(cvect_t *vect, int id)
{ 
	return (struct rec_data_evt *)cvect_lookup(vect, id); 
}

static void
print_rde_info(struct rec_data_evt *rde)
{
	assert(rde);

	printc("rde->spdid %d\n", rde->spdid);
	printc("rde->c_evtid %ld\n", rde->c_evtid);
	printc("rde->s_evtid %ld\n", rde->s_evtid);
	printc("rde->p_evtid %ld\n", rde->p_evtid);
	printc("rde->grp %d\n", rde->grp);

	return;
}

void
cos_init(void *arg)
{
	int c, accept_fd, ret;
	long eid;
	char *init_str = cos_init_args();
	char __create_str[128];
	static volatile int first = 1, off = 0;
	int port;
	u64_t start, end;

	union sched_param sp;

	if (first) {
		first = 0;
		init(init_str);
		return;
	}

	printc("Thread %d, port %d\n", cos_get_thd_id(), __port+off);	
	port = off++;
	port += __port;
	eid = evt_get();
	printc("conn_mgr: tsplit init evtid %ld for thd %d\n", eid, cos_get_thd_id());
	if (snprintf(__create_str, 128, create_str, port) < 0) BUG();
	ret = c = server_tsplit(cos_spd_id(), td_root, __create_str, strlen(__create_str), TOR_ALL, eid);
	if (ret <= td_root) BUG();
	accept_fd = c;
	evt_add(c, eid);

	rdtscll(start);
	int i, tmp_t;
	static int tmp_first = 0;
	/* event loop... */
	/* int last_evt = 0; */
	while (1) {
		struct tor_conn tc;
		int t;
		long evt;
		long tmp_evt = 0;

		memset(&tc, 0, sizeof(struct tor_conn));
		rdtscll(end);
		meas_record(end-start);
		/* printc("thd %d calling evt_wait all\n", cos_get_thd_id()); */
		/* printc("\n---->conn: thd %d evt_wail_all\n", cos_get_thd_id()); */
		tmp_evt = evt_wait_all();
		/* printc("---->conn: thd %d event comes (returned evt %d)\n", */
		/*        cos_get_thd_id(), tmp_evt); */
		if (unlikely(tmp_evt < 0)) evt = -tmp_evt;
		else evt = tmp_evt;
		
		rdtscll(start);
		t   = evt_torrent(evt);
		if (!t) {
			/* printc("---->conn: thd %d event comes (returned evt %d)\n", */
			/*        cos_get_thd_id(), tmp_evt); */

			/* if (last_evt != evt) { */
			/* 	printc("thd %d find t %d for event %d\n",  */
			/* 	       cos_get_thd_id(), t, evt); */
			/* 	last_evt = evt; */
			/* } */

			/* When even t manager fails, the mapping
			 * between event and torrent might have not
			 * been set or has been removed. For example,
			 * current thread has created a new event
			 * (evt_get()), but before evt_put is called
			 * in from_data or to_data, the fault occurs
			 * when network thread calls
			 * event_trigger. When upcall recovery thread
			 * finishes rebuilding events, and "pretended
			 * triggered". If we detect this is pretended
			 * trigger, we will return and evt_wait
			 * again. Now when the event is truly
			 * triggered, we do not have a torrent
			 * associated since it has been treleased.
			 * ....but thread resume and remove event
			 * anyway? So we do not evt_put here?
			 *
			 *
			 */
			/* evt_put(evt); */
			continue;
		}
		
		if (t > 0) {
			tc.feid = evt;
			tc.from = t;
			if (t == accept_fd) {
				tc.to = 0;
				/* printc("[[[conn_mgr:accept_new(thd %d)]]]\n", */
				/*        cos_get_thd_id()); */
				accept_new(accept_fd, tmp_evt);
			} else {
				tc.to = tor_get_to(t, &tc.teid);
				/* if (unlikely(tmp_evt < 0)) goto redo; */
				assert(tc.to > 0);
				/* printc("[[[conn_mgr:from_data_new(thd %d)]]]\n", */
				/*        cos_get_thd_id()); */
				from_data_new(&tc, tmp_evt);
			}
		} else {
			t *= -1;
			tc.teid = evt;
			tc.to   = t;
			tc.from = tor_get_from(t, &tc.feid);
			/* if (unlikely(tmp_evt < 0)) goto redo; */
			assert(tc.from > 0);
			/* printc("[[[conn_mgr:to_data_new(thd %d)]]]\n", */
			/*         cos_get_thd_id()); */
			to_data_new(&tc, tmp_evt);
		}
		cos_mpd_update();
	}
}

int
periodic_wake_get_misses(unsigned short int tid)
{
	return 0;
}

int
periodic_wake_get_deadlines(unsigned short int tid) 
{
	return 0;
}

long
periodic_wake_get_lateness(unsigned short int tid)
{
	return 0;
}

long
periodic_wake_get_miss_lateness(unsigned short int tid)
{
	long long avg;

	if (varcnt == 0) return 0;
	avg = vartot/varcnt;
	/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */	
///	avg = (avg >> 20) + ! ((avg & 1048575) == 0);
	avg = (avg >> 8) + 1;//! ((avg & 1048575) == 0);
	vartot = 0;
	varcnt = 0;

	return avg;
}

int
periodic_wake_get_period(unsigned short int tid)
{
	if (tid == hpthd) return 1;
	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	/* printc("upcall type %d, core %ld, thd %d, args %p %p %p\n", */
	/*        t, cos_cpuid(), cos_get_thd_id(), arg1, arg2, arg3); */
	
	switch (t) {
	case COS_UPCALL_THD_CREATE:
	{
		printc("thread %d passing arg1 %p here (t %d)\n", 
		       cos_get_thd_id(), arg1, t);
		
		if (arg1 == 0) {
			cos_init(arg1);
		}
		printc("thread %d passing arg1 %p here (t %d)\n", 
		       cos_get_thd_id(), arg1, t);
		return;
	}
	case COS_UPCALL_RECEVT:
		printc("conn_mgr: upcall to recover the event (thd %d, spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
#ifdef EVT_C3
		/* events_replay_all((int)arg1); */
		evt_cli_if_recover_upcall_entry((int)arg1);
#endif
		break;
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	return;
}
