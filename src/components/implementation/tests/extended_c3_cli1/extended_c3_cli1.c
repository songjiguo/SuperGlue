#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <ec3_ser1.h>
#include <ec3_ser2.h>

int high, low, med;
int warm;

#define ITER 5

//#define EXAMINE_LOCK
#define EXAMINE_EVT

#define US_PER_TICK 10000

void
cos_init(void)
{
	static int first = 0;
	static int second = 0;
	union sched_param sp;
	int i, j, k;

	printc("c3 cli test (thd %d in spd %ld)\n", cos_get_thd_id(), cos_spd_id());
	
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		warm = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((warm = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a warm thd %d\n",
		       cos_get_thd_id(), warm);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((high = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a high thd %d\n",
		       cos_get_thd_id(), high);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 15;
		med = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((med = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a med thd %d\n",
		       cos_get_thd_id(), med);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((low = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a low thd %d\n",
		       cos_get_thd_id(), low);

	} else {
#ifdef EXAMINE_LOCK
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 5);
			ec3_ser1_test();
		}

		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 2);
			ec3_ser1_test();
		}

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test();
		}
#endif

#ifdef EXAMINE_EVT
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test();
		}
			
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test();
		}
			
		/* if (cos_get_thd_id() == warm) { */
		/* 	printc("<<<warm thd %d>>>\n", cos_get_thd_id()); */
		/* 	ec3_ser2_test(); */
		/* } */
			
		/* if (cos_get_thd_id() == low) { */
		/* 	printc("<<<low thd %d>>>\n", cos_get_thd_id()); */
		/* 	ec3_ser2_test(); */
		/* } */
#endif
	}

	return;
}


void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_THD_CREATE:
	/* New thread creation method passes in this type. */
	{
		if (arg1 == 0) {
			cos_init();
		}
		printc("thread %d passing arg1 %p here (t %d)\n", 
		       cos_get_thd_id(), arg1, t);
		return;
	}
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	return;
}
