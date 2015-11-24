#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>
#include <periodic_wake.h>
#include <timed_blk.h>

#include <c3_test.h>

#include <ec3_ser1.h>
#include <ec3_ser2.h>

int high, low, med, warm;
#define ITER 5
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

		printc("going to create a thd _PRIO 11....\n");
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((high = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a high thd %d\n",
		       cos_get_thd_id(), high);

		printc("going to create a thd _PRIO 15....\n");
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 15;
		med = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((med = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a med thd %d\n",
		       cos_get_thd_id(), med);

		printc("going to create a thd _PRIO 20....\n");
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		/* if ((low = cos_thd_create(cos_init, NULL, sp.v, 0, 0) <= 0)) assert(0); */
		printc("c3 cli thd %d is creating a low thd %d\n",
		       cos_get_thd_id(), low);

	} else {
#ifdef EXAMINE_MM
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
#endif
#ifdef EXAMINE_SCHED
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			/* timed_event_block(cos_spd_id(), 1); */
			ec3_ser1_test(low, med, high);
		}
		
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			/* timed_event_block(cos_spd_id(), 1); */
			ec3_ser1_test(low, med, high);
		}

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
#endif
#ifdef EXAMINE_RAMFS
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
		
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
#endif
#ifdef EXAMINE_MBOX
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
		
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			ec3_ser2_test(low, med, high);
		}
#endif
#ifdef EXAMINE_LOCK
		if (cos_get_thd_id() == high) {
			printc("\n\n<<high thd %d LOCK test>>>\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 5);
			ec3_ser1_test(low, med, high);
		}

		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d LOCK test>>>\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 2);
			ec3_ser1_test(low, med, high);
		}

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d LOCK test>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
#endif

#ifdef EXAMINE_EVT
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
			
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
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

#ifdef EXAMINE_TE
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		} 

		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}
		
		else if (cos_get_thd_id() > med) {
			printc("<<<other thd %d>>>\n", cos_get_thd_id());
			ec3_ser1_test(low, med, high);
		}

		
#endif
	}

	return;
}


void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	printc("thread %d passing arg1 %p here (type %d spd %ld)\n",
	       cos_get_thd_id(), arg1, t, cos_spd_id());

	switch (t) {
	case COS_UPCALL_THD_CREATE:
	/* New thread creation method passes in this type. */
	{
		if (arg1 == 0) {
			cos_init();
		}
		return;
	}
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	return;
}
