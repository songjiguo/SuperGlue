#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>
#include <periodic_wake.h>
#include <timed_blk.h>

#include <c3_test.h>

// test evt: 10 and 13, 11 and 12

long passed_evtid;

#ifdef EXAMINE_LOCK
static int low = 13;
static int mid = 12;
static int hig = 11;

#include <cos_synchronization.h>
cos_lock_t t_lock;
#define THIS_LOCK_TAKE()    lock_take(&t_lock)
#define THIS_LOCK_RELEASE() lock_release(&t_lock)
#define THIS_LOCK_INIT()    lock_static_init(&t_lock);

volatile int spin = 1;

static void try_hp(void)
{
	/* printc("thread h (in spd %ld) : %d is doing something\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */

	spin = 0;
	/* printc("thread h (in spd %ld) : %d try to take lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	THIS_LOCK_TAKE();
	/* printc("thread h (in spd %ld) : %d has the lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	THIS_LOCK_RELEASE();
	/* printc("thread h (in spd %ld) : %d released lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */

	return;
}

static void try_mp(void)
{
	/* printc("thread m : %d try to take lock\n", cos_get_thd_id()); */
	THIS_LOCK_TAKE();
	/* printc("thread m : %d has the lock\n", cos_get_thd_id()); */
	THIS_LOCK_RELEASE();
	/* printc("thread m : %d released lock\n", cos_get_thd_id()); */

	return;
}


static void try_lp(void)
{
	/* printc("<<< thread l (in spd %ld) : %d is doing something \n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	/* printc("thread l (in spd %ld) : %d try to take lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	THIS_LOCK_TAKE();
	/* printc("thread l (in spd %ld) : %d has the lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	/* printc("thread l (in spd %ld) : %d spinning\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	while (spin);
	/* printc("thread l (in spd %ld): %d is doing something\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	/* printc("thread l (in spd %ld): %d try to release lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	THIS_LOCK_RELEASE();

	return;
}

vaddr_t ec3_ser2_test(void)
{
	if (cos_get_thd_id() == hig) try_hp();
	if (cos_get_thd_id() == mid) try_mp();
	if (cos_get_thd_id() == low) try_lp();

	return 0;
}

int ec3_ser2_pass(long id)
{
	return 0;
}


void
cos_init(void)
{
	printc("ser2 :thd %d is trying to init lock (in spd %ld)\n", cos_get_thd_id(), cos_spd_id());
	THIS_LOCK_INIT();
	printc("after init LOCK\n");
}

#endif


#ifdef EXAMINE_EVT

// evt id is 1

static int mid = 13;
static int hig = 10;

long evt1;
static int test_num1 = 0;
static int test_num2 = 0;

static void try_hp(void)
{
	/* printc("(ser 2) thread h : %d is triggering event %ld (%d time)\n",  */
	/*        cos_get_thd_id(), passed_evtid, test_num2); */
	/* if (evt_trigger(cos_spd_id(), passed_evtid)) assert(0);	 */
	
	return;
}

static void try_mp(void)
{
	return;
}

vaddr_t ec3_ser2_test(void)
{
	/* if (cos_get_thd_id() == hig) { */
	/* 	printc("\n<<< Ser2 event test start in spd %d ... >>>>\n\n",  */
	/* 	       cos_spd_id(), cos_get_thd_id()); */
	/* 	try_hp(); */
	/* 	printc("\n<<< ... Ser2 event test done in spd %d >>>>\n\n",  */
	/* 	       cos_spd_id(), cos_get_thd_id()); */
	/* } */
	return 0;
}


// not using this anymore, since evt_create has to be in the same spd as evt_wait
int ec3_ser2_pass(long id)
{
	printc("\n**** wait ****\n");
	printc("(ser 2) thd %d waiting for event %ld\n\n", cos_get_thd_id(), id);
	evt_wait(cos_spd_id(), id);
	return 0;
}

#endif


#ifdef EXAMINE_MM

#define PAGE_NUM 10
vaddr_t addr[PAGE_NUM];

static int first = 1;
static int index = 0;

vaddr_t ec3_ser2_test(void)
{
	vaddr_t ret;
	int idx = 0;

	/* printc("\n<<< ... Ser2 MM test in spd %d >>>>\n\n", */
	/*        cos_spd_id(), cos_get_thd_id()); */
	
	if (first) {
		first = 0;
		for (idx = 0; idx < PAGE_NUM; idx++) {
			addr[idx]  = (vaddr_t)cos_get_vas_page();
		}
	}
	ret = addr[index%PAGE_NUM];
	index++;
	/* printc("kevin: addr %p\n", ret); */
	return ret;
}


// not using this anymore, since evt_create has to be in the same spd as evt_wait
int ec3_ser2_pass(long id)
{
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


// this is client side, open later
#ifdef EXAMINE_MBOX

volatile unsigned long long overhead_start, overhead_end;
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
	td_t t1, serv;
	long evt;
	char *params1 = "foo", *params2 = "", *d;
	char *data = "1234567890";
	int period, num, ret, sz, i, j;
	u64_t start = 0, end = 0, re_cbuf;
	cbufp_t cb1;

	evt = evt_split(cos_spd_id(), 0, 0);
	assert(evt > 0);
	printc("mb client: tsplit by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	serv = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_RW | TOR_WAIT, evt);
	if (serv < 1) {
		printc("UNIT TEST FAILED (3): split1 failed %d\n", serv); 
		assert(0);
	}

	printc("mb client: thd %d 1\n", cos_get_thd_id());
	evt_wait(cos_spd_id(), evt);
	printc("mb client: thd %d 2\n", cos_get_thd_id());

	printc("client split successfully\n");
	sz = 4096;
	j = 1000*ITER;
	j = 40;
	rdtscll(start);
	for (i=1; i<=j; i++) {
		if (i == j) rdtscll(end);
		d = cbufp_alloc(sz, &cb1);
		if (!d) {
			printc("can not get a cbufp (thd %d)\n", cos_get_thd_id());
			assert(0);
		}
		cbufp_send(cb1);
		rdtscll(end);
		((u64_t *)d)[0] = end;
		printc("cli:passed out data is %lld\n", ((u64_t *)d)[0]);
		printc("cli:passed out data in cbuf %d\n", cb1);
		ret = twritep(cos_spd_id(), serv, cb1, sz);

		cbufp_deref(cb1); 
	}

	printc("mb client: finally trelease by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	evt_wait(cos_spd_id(), evt);  // test only
	rdtscll(overhead_start);
	trelease(cos_spd_id(), serv);
	rdtscll(overhead_end);
	printc("mbox client trelease overhead %llu\n", overhead_end - overhead_start);

	return;
}

vaddr_t ec3_ser2_test(void)
{
	printc("<<< MBOX CLIENT RECOVERY TEST START (thd %d) >>>\n", cos_get_thd_id());
	mbox_test();
	printc("<<< MBOX CLIENT RECOVERY TEST DONE!! >>>\n\n\n");
	return 0;
}

int ec3_ser2_pass(long id)
{
	return 0;
}

#endif

#ifdef EXAMINE_RAMFS
#include <mem_mgr_large.h>
#include <valloc.h>

vaddr_t ec3_ser2_test(void)
{
	return 0;
}

int ec3_ser2_pass(long id)
{
	return 0;
}

#endif



#ifdef EXAMINE_TE

vaddr_t ec3_ser2_test(void)
{
	return 0;
}

int ec3_ser2_pass(long id)
{
	return 0;
}

#endif


#ifdef EXAMINE_SCHED

vaddr_t ec3_ser2_test(void)
{
	return 0;
}

int ec3_ser2_pass(long id)
{
	return 0;
}

#endif



#ifdef NO_EXAMINE

vaddr_t ec3_ser2_test(void)
{
	return 0;
}

int ec3_ser2_pass(long id)
{
	return 0;
}

#endif


