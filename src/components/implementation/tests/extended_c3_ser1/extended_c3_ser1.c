#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <evt.h>
#include <cbuf.h>
#include <cbufp.h>
#include <torrent.h>
#include <periodic_wake.h>
#include <timed_blk.h>

#include <mem_mgr.h>

#include <c3_test.h>
#include <ec3_ser2.h>
/* #include <ec3_ser3.h> */

volatile unsigned long long overhead_start, overhead_end;

/****************************
 _            _    
| | ___   ___| | __
| |/ _ \ / __| |/ /
| | (_) | (__|   < 
|_|\___/ \___|_|\_\
****************************/

#ifdef EXAMINE_LOCK
#include <cos_synchronization.h>
cos_lock_t t_lock;
#define LOCK_TAKE()    lock_take(&t_lock)
#define LOCK_RELEASE() lock_release(&t_lock)
#define LOCK_INIT()    lock_static_init(&t_lock);

cos_lock_t t_lock2;
#define LOCK2_TAKE()    lock_take(&t_lock2)
#define LOCK2_RELEASE() lock_release(&t_lock2)
#define LOCK2_INIT()    lock_static_init(&t_lock2);

volatile int spin = 1;

#define LOCK_NUM 10000

static void try_hp(void)
{
	int i = 0;
	while(1) {
		/* printc("thread h : %d is doing something\n", cos_get_thd_id()); */
		/* printc("thread h : %d is trying to take another lock...\n", cos_get_thd_id()); */
		/* ec3_ser2_test(); */
		
		spin = 0;
		/* printc("thread h : %d try to take lock\n", cos_get_thd_id()); */
		LOCK_TAKE();
		
		/* printc("thread h : %d has the lock\n", cos_get_thd_id()); */
		LOCK_RELEASE();
		
		/* printc("thread h : %d released lock\n", cos_get_thd_id()); */

		spin = 1;
		timed_event_block(cos_spd_id(), 1);
	}
	
	return;
}

static void try_mp(void)
{
	int i = 0;
	while(1) {
		/* printc("thread m : %d try to take lock\n", cos_get_thd_id()); */

		LOCK_TAKE();
		/* printc("thread m : %d has the lock\n", cos_get_thd_id()); */
		LOCK_RELEASE();
		/* printc("thread m : %d released lock\n", cos_get_thd_id()); */
		
		timed_event_block(cos_spd_id(), 1);
	}
	return;
}
	

static void try_lp(void)
{
	int i = 0;
	while(1) {
		/* printc("<<< thread l : %d is doing something \n", cos_get_thd_id()); */
		/* printc("thread l : %d try to take lock\n", cos_get_thd_id()); */
		LOCK_TAKE();
		/* printc("thread l : %d has the lock\n", cos_get_thd_id()); */
		
		/* printc("thread l : %d is trying to take another lock...\n", cos_get_thd_id()); */
		/* ec3_ser2_test(); */
		
		/* printc("thread l : %d spinning\n", cos_get_thd_id()); */
		while (spin);
		/* printc("thread l : %d is doing something\n", cos_get_thd_id()); */
		/* printc("thread l : %d try to release lock\n", cos_get_thd_id()); */
		LOCK_RELEASE();
	}
	
	return;
}

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{

	if (cos_get_thd_id() == hig) try_hp();
	if (cos_get_thd_id() == mid) try_mp();
	if (cos_get_thd_id() == low) try_lp();

	return 0;
}


void
cos_init(void)
{
	printc("thd %d is trying to init lock\n", cos_get_thd_id());

	LOCK_INIT();
	LOCK2_INIT();
	
	printc("after init LOCK\n");

}

#endif

/****************************
            _   
  _____   _| |_ 
 / _ \ \ / / __|
|  __/\ V /| |_ 
 \___| \_/  \__|
****************************/

#ifdef EXAMINE_EVT

// evt id is 2

long evt1;
static int test_num1 = 0;
static int test_num2 = 0;
static void try_hp(void)
{
	long wait_ret = 0;
	evt1 = evt_split(cos_spd_id(), 0, 0);
	assert(evt1 > 0);

	while(1) {
		/* printc("\n**** split (%d) ****\n", test_num1); */
		/* printc("(ser1) thread h : %d is creating evts\n", cos_get_thd_id()); */
		/* rdtscll(overhead_start); */
		/* rdtscll(overhead_end); */
		/* printc("evt_split...overhead %llu\n", overhead_end - overhead_start); */

		/* rdtscll(overhead_start); */
	wait:
		wait_ret = evt_wait(cos_spd_id(), evt1);
		if (unlikely(wait_ret < 0)) goto wait;

		/* rdtscll(overhead_end); */
		/* printc("evt_wait...evt_trigger...back to wait..\n"); */

		/* ec3_ser2_pass(evt1);  // go to evt_wait */
		/* printc("\n**** free (%d) ****\n", test_num1); */
		/* printc("(ser1) thd h : %d frees event %ld\n", cos_get_thd_id(), evt1); */
		/* rdtscll(overhead_start); */
		/* rdtscll(overhead_end); */
		/* printc("evt_free...overhead %llu\n", overhead_end - overhead_start); */

	}
	evt_free(cos_spd_id(), evt1);

	return;
}

static void try_mp(void)
{
	while(1) {
		ec3_ser3_pass(evt1);  // go to evt_trigger
	}
	return;
}

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	if (cos_get_thd_id() == hig) {
		printc("\n<< Test start in spd %d ... >>>\n\n", cos_get_thd_id());
		try_hp();
		printc("\n<< ... Test done in spd %d >>>\n\n", cos_get_thd_id());
	}

	if (cos_get_thd_id() == mid) try_mp();

	while(1);  // quick fix the thread termination issue ???
	return 0;
}

#ifdef EVT_C3
void events_replay_all();
#endif

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	/* printc("upcall type %d, core %ld, thd %d, args %p %p %p\n", */
	/*        t, cos_cpuid(), cos_get_thd_id(), arg1, arg2, arg3); */
	
	switch (t) {
	case COS_UPCALL_THD_CREATE:
	{
		return;
	}
	case COS_UPCALL_RECEVT:
		/* printc("test_ser1: upcall to recover the event (thd %d, spd %ld)\n", */
		/*        cos_get_thd_id(), cos_spd_id()); */
#ifdef EVT_C3
		events_replay_all();
#endif
		break;
	default:
		return;
	}
	return;
}

#endif

/****************************
 _       
| |_ ___ 
| __/ _ \
| ||  __/
 \__\___|
****************************/

#ifdef EXAMINE_TE

static void try_hp(void)
{
	periodic_wake_create(cos_spd_id(), 2);

	while(1) {
		/* printc("period blocked (50 ticks) thd %d\n", cos_get_thd_id()); */
		/* rdtscll(overhead_start); */
		periodic_wake_wait(cos_spd_id());
		/* rdtscll(overhead_end); */
		/* printc("pte_h wake from wait overhead %llu\n", overhead_end - overhead_start); */
	}

	return;
}

static void try_mp(void)
{
	/* timed_event_block(cos_spd_id(), 2); */

	periodic_wake_create(cos_spd_id(), 3);

	while(1) {
		/* printc("period blocked (35 ticks) thd %d\n", cos_get_thd_id()); */
		/* rdtscll(overhead_start); */
		periodic_wake_wait(cos_spd_id());
		/* rdtscll(overhead_end); */
		/* printc("pte_m wake from wait overhead %llu\n", overhead_end - overhead_start); */
	}

	return;
}

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	if (cos_get_thd_id() == hig) {
		printc("\n<< Test start in spd %d ... >>>\n\n", cos_get_thd_id());
		try_hp();
		printc("\n<< ... Test done in spd %d >>>\n\n", cos_get_thd_id());
	}
	
	/* if (!low && !mid && !hig) { */
	/* 	printc("[Other test thread %d ... ]\n", cos_get_thd_id()); */
	/* 	periodic_wake_create(cos_spd_id(), 4); */
	/* 	while(1) periodic_wake_wait(cos_spd_id()); */
	/* } */
	
	/* For benchmark, we use only one thread, so comment this for that */
	if (cos_get_thd_id() == mid) try_mp();

	try_mp();   // for fault coverage test

	while(1);  // quick fix the thread termination issue ???
	return 0;
}

#endif

/****************************
          _              _ 
 ___  ___| |__   ___  __| |
/ __|/ __| '_ \ / _ \/ _` |
\__ \ (__| | | |  __/ (_| |
|___/\___|_| |_|\___|\__,_|
                           
****************************/

#ifdef EXAMINE_SCHED

#define LOCK()   if (sched_component_take(cos_spd_id())) assert(0);
#define UNLOCK() if (sched_component_release(cos_spd_id())) assert(0);

#define ITER_SCHED 10

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	if (cos_get_thd_id() == hig) {
		int test_sched_lock = 0;
		while(test_sched_lock++ < 10) {
			LOCK();
			UNLOCK();
		}

		int i = 0;
		while(i++ < ITER_SCHED) {
			printc("\n<< high thd %d is blocking on mid thd %d in spd %d (iter %d) >>>\n", cos_get_thd_id(), mid, cos_spd_id(), i);
			sched_block(cos_spd_id(), mid);
		}
	}
	
	if (cos_get_thd_id() == mid) {
		int j = 0;
		while(j++ < ITER_SCHED) {
			printc("\n<< mid thd %d is waking up high thd %d in spd %d (iter %d) >>>\n", cos_get_thd_id(), hig, cos_spd_id(), j);
			sched_wakeup(cos_spd_id(), hig);
		}
	}

	if (cos_get_thd_id() == low) {
		int j = 0;
		if (periodic_wake_create(cos_spd_id(), 100) < 0) BUG();
		while (1) {
			periodic_wake_wait(cos_spd_id());
			printc("thd periodic wakeup %d\n", cos_get_thd_id());
		}
	}

	return 0;
}

#endif

/****************************
 _ __ ___  _ __ ___  
| '_ ` _ \| '_ ` _ \ 
| | | | | | | | | | |
|_| |_| |_|_| |_| |_|
****************************/

#ifdef EXAMINE_MM

#define PAGE_NUM 10
vaddr_t s_addr[PAGE_NUM];
vaddr_t d_addr[PAGE_NUM];

static void
test_mmpage()
{
	int i;
	unsigned long long loopi = 0;
	for (i = 0; i<PAGE_NUM; i++) {
		s_addr[i] = (vaddr_t)cos_get_vas_page();
		d_addr[i] = ec3_ser2_test();
		/* printc("s_addr[%d] -> %p\n", i, s_addr[i]); */
		/* printc("d_addr[%d] -> %p\n", i, d_addr[i]); */
	}


	// For fault coverage test, this has to be a loop See revoke (small change for this)
	for (i = 0; i<PAGE_NUM; i++) {
		printc("thread %d calls get_page in spd %ld\n",
		       cos_get_thd_id(), cos_spd_id());

		mman_get_page(cos_spd_id(), s_addr[i], 0);
		
		printc("thread %d calls alias_page in spd %ld\n",
		       cos_get_thd_id(), cos_spd_id());
		
		if (!mman_alias_page(cos_spd_id(), s_addr[i], 
				     cos_spd_id()+1, d_addr[i], MAPPING_RW)) assert(0);

		printc("thread %d calls revoke_page in spd %ld\n",
		       cos_get_thd_id(), cos_spd_id());
		
		mman_revoke_page(cos_spd_id(), s_addr[i], 0);
	}
	
	return;
}

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	if (cos_get_thd_id() == hig) {
		printc("\n<< high thd %d is in MM testing... >>>\n",
		       cos_get_thd_id());
		
		int i = 0;
		/* while(i++ < 1000) { /\* 80 x 10 x 4k  < 4M *\/ */
		while(i++ < 80) { /* 80 x 10 x 4k  < 4M */
			test_mmpage();
		}
		
	}
	
	return 0;
}

#ifdef MM_C3
void alias_replay(vaddr_t s_addr);
#endif
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_REBOOT:
	{
		printc("thread %d passing arg1 %p here (t %d)\n", 
		       cos_get_thd_id(), arg1, t);
#ifdef MM_C3
		alias_replay((vaddr_t)arg1);
#endif
			
		return;
	}
	default:
		return;
	}
	return;
}


#endif

/****************************
                      __     
 _ __ __ _ _ __ ___  / _|___ 
| '__/ _` | '_ ` _ \| |_/ __|
| | | (_| | | | | | |  _\__ \
|_|  \__,_|_| |_| |_|_| |___/
****************************/

#ifdef EXAMINE_RAMFS
#include <mem_mgr_large.h>
#include <valloc.h>

char buffer[1024];

static void
ramfs_test(void)
{
	td_t t1, t2, t3;
	long evt1, evt2, evt3;
	char *params1 = "bar";
	char *params2 = "foo/";
	char *params3 = "foo/bar";
	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890";
	unsigned int ret1, ret2;

	evt1 = evt_split(cos_spd_id(), 0, 0);
	evt2 = evt_split(cos_spd_id(), 0, 0);
	/* evt3 = evt_create(cos_spd_id()); */
	assert(evt1 > 0 && evt2 > 0);
	
	printc("\nRAMFS Testing Starting (in ser1 %d).....(thd %d)\n\n", 
	       cos_spd_id(), cos_get_thd_id());

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t1);
		return;
	}
	trelease(cos_spd_id(), t1);
	
	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("UNIT TEST FAILED: split3 failed %d\n", t2); return;
	}

#ifdef TEST_RAMFS_C3
	ret1 = twritep_pack(cos_spd_id(), t1, data1, strlen(data1));
	ret2 = twritep_pack(cos_spd_id(), t2, data2, strlen(data2));
#else 
	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
#endif
	printc("write %d & %d, ret %d & %d\n", strlen(data1), strlen(data2), ret1, ret2);

	/* This is important!!!! release in the opposite order */
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t1);

	int max_test;
	
	// need test for max number of allowed faults (ureboot)
	for (max_test = 0; max_test < 400; max_test++) {
		printc("\n>>>>>>ramfs test phase 3 start .... (iter %d)\n", max_test);
		t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
		/* printc("\n[[[[[[.... 2nd tsplit\n"); */
		t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
		if (t1 < 1 || t2 < 1) {
			printc("UNIT TEST FAILED: later splits failed\n");
			return;
		}

		/* printc("\n[[[[[[.... 1st tread\n"); */
#ifdef TEST_RAMFS_C3
		ret1 = treadp_pack(cos_spd_id(), t1, buffer, 1023);
#else
		ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
#endif
		if (ret1 > 0) buffer[ret1] = '\0';
		assert(!strcmp(buffer, data1));
		// treadp does not return length, instead return cbid
		/* assert(ret1 == strlen(data1)); */
		printc("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1);
		buffer[0] = '\0';

#ifdef TEST_RAMFS_C3
		ret1 = treadp_pack(cos_spd_id(), t2, buffer, 1023);
#else
		ret1 = tread_pack(cos_spd_id(), t2, buffer, 1023);
#endif
		if (ret1 > 0) buffer[ret1] = '\0';
		assert(!strcmp(buffer, data2));
		/* assert(ret1 == strlen(data2)); */
		printc("read %d (%d): %s (%s)\n", ret1, strlen(data2), buffer, data2);
		buffer[0] = '\0';

		trelease(cos_spd_id(), t2);
		trelease(cos_spd_id(), t1);
	}

	printc("\nRAMFS Testing Done.....\n\n");
	return;
}

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	if (cos_get_thd_id() == hig) ramfs_test();
	return 0;
}

#endif

/********************************
                 _ _ _               
 _ __ ___   __ _(_) | |__   _____  __
| '_ ` _ \ / _` | | | '_ \ / _ \ \/ /
| | | | | | (_| | | | |_) | (_) >  < 
|_| |_| |_|\__,_|_|_|_.__/ \___/_/\_\
**********************************/

// this is server side, open first
#ifdef EXAMINE_MBOX

#define  ITER 10
static void parse_args(int *p, int *n)
{
	char *c;
	int i = 0, s = 0;
	c = cos_init_args();
	while (c[i] != ' ') {
		s = 10*s+c[i]-'0';
		i++;
	}
	*p = s;
	s = 0;
	i++;
	while (c[i] != '\0') {
		s = 10*s+c[i]-'0';
		i++;
	}
	*n = s;
	return ;
}
static void mbox_test()
{
	td_t t1 = td_root, cli;
	long evt1, evt2;
	char *params1 = "foo", *params2 = "", *buf;
	int period, num, sz, off, i, j;
	cbufp_t cb1;
	u64_t start = 0, end = 0, re_mbox;
	union sched_param sp;
	static int first = 1;
	
	evt1 = evt_split(cos_spd_id(), 0, 0);
	assert(evt1 > 0);
	evt2 = evt_split(cos_spd_id(), 0, 0);
	assert(evt2 > 0);
	printc("mb server: 1st tsplit by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL | TOR_NONPERSIST | TOR_WAIT, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED (1): split failed %d\n", t1);
		assert(0);
	}
	printc("mb server: thd %d (waiting on event %ld)\n", cos_get_thd_id(), evt1);
	evt_wait(cos_spd_id(), evt1);
	printc("mb server: params2 length %d\n", strlen(params2));
	printc("mb server: thd %d (back from evt_wait %ld)\n", cos_get_thd_id(), evt1);

	cli = tsplit(cos_spd_id(), t1, params2, strlen(params2), TOR_RW, evt2);
	if (cli < 1) {
		printc("UNIT TEST FAILED (2): split1 failed %d\n", cli);
		assert(0);
	}

	j = 1000*ITER;
	j = 10;
	rdtscll(start);
	for (i=0; i<j; i++) {
		while (1) {
			// not sure why we need off return? not used when read cbufp2buf
			printc("before treadp\n");
			cb1 = treadp(cos_spd_id(), cli, 0, &off, &sz);
			printc("after treadp\n");
			if ((int)cb1<0) evt_wait(cos_spd_id(), evt2);
			else            break;
		}
		printc("server treadp: off %d sz %d (return cbuf %d)\n", off, sz, cb1);
		buf = cbufp2buf(cb1,sz);
		printc("ser:received in data is %lld (sz %d)\n", ((u64_t *)buf)[0], sz);
		cbufp_deref(cb1);
	}

	printc("mb server: 1st trelease by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	rdtscll(overhead_start);
	trelease(cos_spd_id(), cli);
	rdtscll(overhead_end);
	printc("mbox server 1st trelease overhead %llu\n", overhead_end - overhead_start);

	printc("mb server: 2nd trelease by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	evt_wait(cos_spd_id(), evt1);  // test only
	rdtscll(overhead_start);
	trelease(cos_spd_id(), t1);
	rdtscll(overhead_end);
	printc("mbox server 2nd trelease overhead %llu\n", overhead_end - overhead_start);
	
	return;
}

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	printc("<<< MBOX SERVER RECOVERY TEST START (thd %d) >>>\n", cos_get_thd_id());
	mbox_test();
	printc("<<< MBOX SERVER  RECOVERY TEST DONE!! >>>\n\n\n");
	return 0;
}

#endif



#ifdef NO_EXAMINE

vaddr_t ec3_ser1_test(int low, int mid, int hig)
{
	return 0;
}

#endif


