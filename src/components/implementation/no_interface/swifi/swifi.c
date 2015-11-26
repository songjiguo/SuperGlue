/* It is strange that this spd has to be loaded early ???? in script*/

#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>

#include <mem_mgr_large.h>
#include <valloc.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <swifi.h>
#include <c3_test.h>

int high;
unsigned long counter = 0;

// 10 for non-MM
#define INJECTION_PERIOD 10
#define INJECTION_ITER 10000

/* // hard code all components */
/* enum SEL_SPD{ */
/* 	SEL_SH,    // scheduelr */
/* 	SEL_MM,    // mm */
/* 	SEL_FS,    // file system */
/* 	SEL_LK,    // lock */
/* 	SEL_TE,    // time management */
/* 	SEL_EV,    // event */
/* 	SEL_MB,    // mailbox */
/* 	MAX_C3_SPD */
/* }; */

/* // choose this for different injection */
/* #define TARGET_SPD SEL_FS */

/* #ifdef SWIFI_ON */
/* // need check this every time */
/* #define SH_SPD 2      // done */
/* #define MM_SPD 3      // done */
/* #define FS_SPD 15     // done */
/* #define LK_SPD 10     // done */
/* #define TE_SPD 11     // done      */
/* #define EV_SPD 12     // done */
/* #define MB_SPD 99     // where should this fit into webserver?  */
/* #endif */

/* #ifdef SWIFI_WEB */
/* // need check this every time */
/* #define SH_SPD 2      // done */
/* #define MM_SPD 3      // done */
/* #define FS_SPD 15     // done */
/* #define LK_SPD 10     // done */
/* #define TE_SPD 11     // done      */
/* #define EV_SPD 12     // done */
/* #endif */

/* int target_spd[MAX_C3_SPD]; */

#define IDLE_THD 4
static int entry_cnt = 0;

int fault_inject(int spd)
{
	int ret = 0;
	int tid, spdid;

	entry_cnt++;
	printc("\nthread %d in SWIFI %ld (%d) ... TARGET_COMPONENT %d\n",
	       cos_get_thd_id(), cos_spd_id(), entry_cnt, spd);
	
	if (spd == 0) return 0;
	
	struct cos_regs r;
	
	for (tid = 1; tid <= MAX_NUM_THREADS; tid++) {
		spdid = cos_thd_cntl(COS_THD_FIND_SPD_TO_FLIP, tid, spd, 0);
		if (tid == IDLE_THD || spdid == -1) continue;
		counter++;
		printc("<<flip counter %lu>> flip the register in spd %d (thd %d)!!!\n", 
		       counter, spd, tid);
		cos_regs_read(tid, spdid, &r);
		cos_regs_print(&r);
		flip_all_regs(&r);
		/* cos_regs_print(&r); */
	}
	return 0;
}


/* void cos_init(void) */
/* { */
/* 	static int first = 0; */
/* 	union sched_param sp; */
/* 	int rand; */

/* /\* #ifndef SWIFI_ON *\/ */
/* /\* 	return; *\/ */
/* /\* #else *\/ */
/* #ifndef SWIFI_WEB */
/* 	return; */
/* #else */
/* 	if(first == 0){ */
/* 		first = 1; */
/* 		sp.c.type = SCHEDP_PRIO; */
/* 		sp.c.value = 4; */
/* 		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0); */

/* 		int i; */
/* 		for(i = 0; i < MAX_C3_SPD; i++) { */
/* 			switch(i) { */
/* 			case SEL_SH: */
/* 				target_spd[i] = SH_SPD; */
/* 				break; */
/* 			case SEL_MM: */
/* 				target_spd[i] = MM_SPD; */
/* 				break; */
/* 			case SEL_FS: */
/* 				target_spd[i] = FS_SPD; */
/* 				break; */
/* 			case SEL_TE: */
/* 				target_spd[i] = TE_SPD; */
/* 				break; */
/* 			case SEL_EV: */
/* 				target_spd[i] = EV_SPD; */
/* 				break; */
/* 			case SEL_LK: */
/* 				target_spd[i] = LK_SPD; */
/* 				break; */
/* 			case SEL_MB: */
/* 				target_spd[i] = MB_SPD; */
/* 				break; */
/* 			default: */
/* 				break;				 */
/* 			} */
/* 		} */

/* 	} else { */
/* 		if (cos_get_thd_id() == high) { */
/* 			printc("\nfault injector %ld (high %d thd %d)\n",  */
/* 			       cos_spd_id(), high, cos_get_thd_id()); */
/* 			periodic_wake_create(cos_spd_id(), INJECTION_PERIOD); */
/* 			timed_event_block(cos_spd_id(), 1); */

/* #ifdef SWIFI_ON */
/* 			// this is for each service fault coverage test */
/* 			while(1) { */
/* 				recovery_upcall(cos_spd_id(), COS_UPCALL_SWIFI_BEFORE,  */
/* 						target_spd[TARGET_SPD], 0); */
/* 				// in 1 tick, hope some thread is spinning there! */
/* 				timed_event_block(cos_spd_id(), 1); */
				
/* 				fault_inject(target_spd[TARGET_SPD]); */
/* 				recovery_upcall(cos_spd_id(), COS_UPCALL_SWIFI_AFTER,  */
/* 						target_spd[TARGET_SPD], 0); */
/* 				periodic_wake_wait(cos_spd_id()); */
/* 			} */
/* #endif */
/* #ifdef SWIFI_WEB */
/* 			// this is for web server fault injection only */
/* 			while(1) { */
/* 				fault_inject(target_spd[TARGET_SPD]); */
/* 				periodic_wake_wait(cos_spd_id()); */
/* 			} */
/* #endif */
/* 		} */
/* 	} */
/* #endif */
	
/* } */

void cos_init(void)
{
	static int first = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 4;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (cos_get_thd_id() == high) {
			printc("\nfault injector %ld (high %d thd %d)\n",
			       cos_spd_id(), high, cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), INJECTION_PERIOD);
			timed_event_block(cos_spd_id(), 1);
#ifdef SWIFI_ON
			// this is for each service fault coverage test
			while(1) {
				recovery_upcall(cos_spd_id(), COS_UPCALL_SWIFI_BEFORE,
						SWIFI_SPD, 0);
				// in 1 tick, hope some thread is spinning there!
				timed_event_block(cos_spd_id(), 1);
				
				fault_inject(SWIFI_SPD);

				recovery_upcall(cos_spd_id(), COS_UPCALL_SWIFI_AFTER,
						SWIFI_SPD, 0);
				periodic_wake_wait(cos_spd_id());
			}
#endif
#ifdef SWIFI_WEB
			// this is for web server fault injection only
			while(1) {
				fault_inject(target_spd[TARGET_SPD]);
				periodic_wake_wait(cos_spd_id());
			}
#endif
		}
	}
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
