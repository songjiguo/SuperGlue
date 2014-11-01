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

long passed_evtid;

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
	/* printc("thread h (in spd %ld) : %d is doing something\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */

	spin = 0;
	/* printc("thread h (in spd %ld) : %d try to take lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	LOCK_TAKE();
	/* printc("thread h (in spd %ld) : %d has the lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	LOCK_RELEASE();
	/* printc("thread h (in spd %ld) : %d released lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */

	return;
}

static void try_mp(void)
{
	/* printc("thread m : %d try to take lock\n", cos_get_thd_id()); */
	LOCK_TAKE();
	/* printc("thread m : %d has the lock\n", cos_get_thd_id()); */
	LOCK_RELEASE();
	/* printc("thread m : %d released lock\n", cos_get_thd_id()); */

	return;
}


static void try_lp(void)
{
	/* printc("<<< thread l (in spd %ld) : %d is doing something \n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	/* printc("thread l (in spd %ld) : %d try to take lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	LOCK_TAKE();
	/* printc("thread l (in spd %ld) : %d has the lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	/* printc("thread l (in spd %ld) : %d spinning\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	while (spin);
	/* printc("thread l (in spd %ld): %d is doing something\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
	/* printc("thread l (in spd %ld): %d try to release lock\n",  */
	/*        cos_spd_id(), cos_get_thd_id()); */
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

int ec3_ser2_pass(long id)
{
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


