#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <swifi.h>

int high;
unsigned long counter = 0;

#define SWIFI_ENABLE 0
#define INJECTION_PERIOD 10

// hard code all components
enum SEL_SPD{
	SEL_SH,
	SEL_MM,
	SEL_FS,
	SEL_LK,
	SEL_TE,
	SEL_EV,
	SEL_MB,
	MAX_C3_SPD
};
#define TARGET_SPD SEL_EV

#define SH_SPD 2
#define MM_SPD 3
#define FS_SPD 23
#define LK_SPD 15
#define TE_SPD 16
#define EV_SPD 18
#define MB_SPD 99   // where should this fit into webserver?

int target_spd[MAX_C3_SPD];
#define IDLE_THD 4
static int entry_cnt = 0;
int fault_inject(int spd)
{
	int ret = 0;
	int tid, spdid;

	entry_cnt++;
	/* printc("\nthread %d in SWIFI %ld (%d) ... TARGET_COMPONENT %d\n",  */
	/*        cos_get_thd_id(), cos_spd_id(), entry_cnt, spd); */

	if (spd == 0) return 0;
	
	struct cos_regs r;
	
	for (tid = 1; tid <= MAX_NUM_THREADS; tid++) {
		spdid = cos_thd_cntl(COS_THD_FIND_SPD_TO_FLIP, tid, spd, 0);
		if (tid == IDLE_THD || spdid == -1) continue;
		counter++;
		printc("<<%lu>> flip the register in component %d (tid %d)!!!\n", 
		       counter, spd, tid);
		cos_regs_read(tid, spdid, &r);
		cos_regs_print(&r);
		flip_all_regs(&r);
		/* cos_regs_print(&r); */
	}
	return 0;
}


void cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int rand;
	int num = 0;

#if (SWIFI_ENABLE == 0)
	return;
#else
	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 4;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		int i;
		for(i = 0; i < MAX_C3_SPD; i++) {
			switch(i) {
			case SEL_SH:
				target_spd[i] = SH_SPD;
				break;
			case SEL_MM:
				target_spd[i] = MM_SPD;
				break;
			case SEL_FS:
				target_spd[i] = FS_SPD;
				break;
			case SEL_TE:
				target_spd[i] = TE_SPD;
				break;
			case SEL_EV:
				target_spd[i] = EV_SPD;
				break;
			case SEL_LK:
				target_spd[i] = LK_SPD;
				break;
			case SEL_MB:
				target_spd[i] = MB_SPD;
				break;
			default:
				break;				
			}
		}

	} else {
		if (cos_get_thd_id() == high) {
			printc("\nfault injector %ld (high %d thd %d)\n", 
			       cos_spd_id(), high, cos_get_thd_id());
			timed_event_block(cos_spd_id(), 30);
			periodic_wake_create(cos_spd_id(), INJECTION_PERIOD);
			while(num < 500) {
				/*  run this first to update the
				 *  wakeup time */
				periodic_wake_wait(cos_spd_id());
				fault_inject(target_spd[TARGET_SPD]);
				num++;
			}
		}
	}
#endif

}
