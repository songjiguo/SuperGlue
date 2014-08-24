#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <evt.h>

#include <periodic_wake.h>
#include <timed_blk.h>


// test evt: 10 and 13, 11 and 12

//#define EXAMINE_LOCK
#define EXAMINE_EVT

#ifdef EXAMINE_LOCK
static int low = 13;
static int mid = 12;
static int hig = 11;

#include <cos_synchronization.h>
cos_lock_t t_lock;
#define LOCK_TAKE()    lock_take(&t_lock)
#define LOCK_RELEASE() lock_release(&t_lock)
#define LOCK_INIT()    lock_static_init(&t_lock);

volatile int spin = 1;

static void try_hp(void)
{
	printc("thread h (in spd %ld) : %d is doing something\n", 
	       cos_spd_id(), cos_get_thd_id());

	spin = 0;
	printc("thread h (in spd %ld) : %d try to take lock\n", 
	       cos_spd_id(), cos_get_thd_id());
	LOCK_TAKE();
	printc("thread h (in spd %ld) : %d has the lock\n", 
	       cos_spd_id(), cos_get_thd_id());
	LOCK_RELEASE();
	printc("thread h (in spd %ld) : %d released lock\n", 
	       cos_spd_id(), cos_get_thd_id());

	return;
}

static void try_mp(void)
{
	printc("thread m : %d try to take lock\n", cos_get_thd_id());
	LOCK_TAKE();
	printc("thread m : %d has the lock\n", cos_get_thd_id());
	LOCK_RELEASE();
	printc("thread m : %d released lock\n", cos_get_thd_id());

	return;
}


static void try_lp(void)
{
	printc("<<< thread l (in spd %ld) : %d is doing something \n", 
	       cos_spd_id(), cos_get_thd_id());
	printc("thread l (in spd %ld) : %d try to take lock\n", 
	       cos_spd_id(), cos_get_thd_id());
	LOCK_TAKE();
	printc("thread l (in spd %ld) : %d has the lock\n", 
	       cos_spd_id(), cos_get_thd_id());
	printc("thread l (in spd %ld) : %d spinning\n", 
	       cos_spd_id(), cos_get_thd_id());
	while (spin);
	printc("thread l (in spd %ld): %d is doing something\n", 
	       cos_spd_id(), cos_get_thd_id());
	printc("thread l (in spd %ld): %d try to release lock\n", 
	       cos_spd_id(), cos_get_thd_id());
	LOCK_RELEASE();

	return;
}

vaddr_t ec3_ser2_test(void)
{
	if (cos_get_thd_id() == hig) try_hp();
	if (cos_get_thd_id() == mid) try_mp();
	if (cos_get_thd_id() == low) try_lp();

	return 0;
}


void
cos_init(void)
{
	printc("thd %d is trying to init lock (in spd %ld)\n", cos_get_thd_id(), cos_spd_id());
	LOCK_INIT();
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
	printc("(ser 2) thread h : %d is creating evts\n", cos_get_thd_id());
	evt1 = evt_create(cos_spd_id());
	assert(evt1 > 0);	

	while(test_num1++ < 10) {
		printc("(ser 2) thread h : %d waiting for event %ld (%d time)\n", 
		       cos_get_thd_id(), evt1, test_num1);

		evt_wait(cos_spd_id(), evt1);
		printc("(ser 2) thread h : %d is processing event %ld (%d time)\n", 
		       cos_get_thd_id(), evt1, test_num1);
	}

	printc("(ser2) thread h : %d frees event %ld\n", cos_get_thd_id(), evt1);
	evt_free(cos_spd_id(), evt1);

	return;
}

static void try_mp(void)
{
	while(test_num2++ < 10 && test_num1 <= 10) {
		printc("(ser 2) thread h's test_num1 is already (%d time)\n", test_num1);
		
		printc("(ser 2) thread m : %d is triggering event %ld (%d time)\n", cos_get_thd_id(), evt1, test_num2);
		if (evt_trigger(cos_spd_id(), evt1)) assert(0);
	}

	test_num2 = 0;
	while(test_num2++ < 2) {
		printc("\n [[[ trying to trigger an event in a different spd.....]]]\n");
		printc("(ser 2) thread m : %d is triggering event %ld (%d time)\n", cos_get_thd_id(), evt1+1, test_num2);
		if (evt_trigger(cos_spd_id(), evt1+1)) assert(0);  //  +1 for test only
	}

	return;
}

vaddr_t ec3_ser2_test(void)
{

	if (cos_get_thd_id() == hig) {
		printc("\n<<< Ser2 event test start in spd %d ... >>>>\n\n", cos_get_thd_id());
		try_hp();
	}
	if (cos_get_thd_id() == mid) try_mp();

	if (cos_get_thd_id() == hig) {
		printc("\n<<< ... Ser2 event test done in spd %d >>>>\n\n", cos_get_thd_id());
	}

	return 0;
}

#endif


