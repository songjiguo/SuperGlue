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

#define EXAMINE_MM
//#define EXAMINE_SCHED
//#define EXAMINE_TE
//#define EXAMINE_LOCK
//#define EXAMINE_EVT

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

vaddr_t ec3_ser1_test(vint low, int mid, int hig)
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

#endif

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

	return 0;
}

#endif

#ifdef EXAMINE_MM

#define PAGE_NUM 10
vaddr_t s_addr[PAGE_NUM];
vaddr_t d_addr[PAGE_NUM];

static void
test_mmpage()
{
	int i;
	for (i = 0; i<PAGE_NUM; i++) {
		s_addr[i] = (vaddr_t)cos_get_vas_page();
		d_addr[i] = ec3_ser2_test();
		printc("s_addr[%d] -> %p\n", i, s_addr[i]);
		printc("d_addr[%d] -> %p\n", i, d_addr[i]);
	}

	for (i = 0; i<PAGE_NUM; i++) {
		mman_get_page(cos_spd_id(), s_addr[i], 0);
		if (!mman_alias_page(cos_spd_id(), s_addr[i], 
				     cos_spd_id()+1, d_addr[i], MAPPING_RW)) assert(0);
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
		while(i++ < 2) { /* 80 x 10 x 4k  < 4M */
			printc("<<< MM RECOVERY TEST START (thd %d) >>>\n", cos_get_thd_id());
			test_mmpage();
			printc("<<< MM RECOVERY TEST DONE!! >>> (%d)\n\n\n", i);
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


