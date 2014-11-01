#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <evt.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <ec3_ser2.h>
#include <ec3_ser3.h>


volatile unsigned long long overhead_start, overhead_end;

// test evt: 10 and 13, 11 and 12

static int low = 13;
static int mid = 12;
static int hig = 11;

//#define EXAMINE_TE
//#define EXAMINE_LOCK
#define EXAMINE_EVT

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

static void try_hp(void)
{
	int i = 0;
	while(i++ < 100) {
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
	while(i++ < 100) {
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
	while(i++ < 100) {
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

vaddr_t ec3_ser1_test(void)
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

	rdtscll(overhead_start);
	LOCK_INIT();
	rdtscll(overhead_end);
	printc("lock init overhead %llu\n",
	       overhead_end - overhead_start);

	rdtscll(overhead_start);
	LOCK2_INIT();
	rdtscll(overhead_end);
	printc("lock2 init overhead %llu\n",
	       overhead_end - overhead_start);
	
	printc("after init LOCK\n");
}

#endif


#ifdef EXAMINE_EVT

// evt id is 2

long evt1;
static int test_num1 = 0;
static int test_num2 = 0;
static void try_hp(void)
{
	while(test_num1++ < 10) {
		printc("\n**** split (%d) ****\n", test_num1);
		printc("(ser1) thread h : %d is creating evts\n", cos_get_thd_id());
		rdtscll(overhead_start);
		evt1 = evt_split(cos_spd_id(), 0, 0);
		assert(evt1 > 0);
		rdtscll(overhead_end);
		printc("evt_split...overhead %llu\n", overhead_end - overhead_start);

		rdtscll(overhead_start);
		evt_wait(cos_spd_id(), evt1);
		rdtscll(overhead_end);
		printc("evt_wait...evt_trigger...back to wait...overhead %llu\n",
		       overhead_end - overhead_start);

		/* ec3_ser2_pass(evt1);  // go to evt_wait */
		printc("\n**** free (%d) ****\n", test_num1);
		printc("(ser1) thd h : %d frees event %ld\n", cos_get_thd_id(), evt1);
		rdtscll(overhead_start);
		evt_free(cos_spd_id(), evt1);
		rdtscll(overhead_end);
		printc("evt_free...overhead %llu\n", overhead_end - overhead_start);

	}
	return;
}

static void try_mp(void)
{
	while(test_num2++ < 10) {
		ec3_ser3_pass(evt1);  // go to evt_trigger
	}
	return;
}

vaddr_t ec3_ser1_test(void)
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

#endif


#ifdef EXAMINE_TE

static void try_hp(void)
{
	rdtscll(overhead_start);
	periodic_wake_create(cos_spd_id(), 50);
	rdtscll(overhead_end);
	printc("pte_h create overhead %llu\n", overhead_end - overhead_start);

	while(1) {
		printc("period blocked (50 ticks) thd %d\n", cos_get_thd_id());
		/* rdtscll(overhead_start); */
		periodic_wake_wait(cos_spd_id());
		/* rdtscll(overhead_end); */
		/* printc("pte_h wake from wait overhead %llu\n", overhead_end - overhead_start); */
	}

	return;
}

static void try_mp(void)
{
	timed_event_block(cos_spd_id(), 2);

	rdtscll(overhead_start);
	periodic_wake_create(cos_spd_id(), 35);
	rdtscll(overhead_end);
	printc("pte_m create overhead %llu\n", overhead_end - overhead_start);

	while(1) {
		printc("period blocked (35 ticks) thd %d\n", cos_get_thd_id());
		/* rdtscll(overhead_start); */
		periodic_wake_wait(cos_spd_id());
		/* rdtscll(overhead_end); */
		/* printc("pte_m wake from wait overhead %llu\n", overhead_end - overhead_start); */
	}

	return;
}

vaddr_t ec3_ser1_test(void)
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

#endif


