#include <cos_component.h>
#include <sched.h>
#include <print.h>

/* if we can tell this happened due to a failure in scheduler,
 * we can simply switch to the blocked thread (will catch the
 * fault when context switch), and let the failure notifier to
 * replay block  */

/* extern int sched_wakeup_helper(spdid_t spdid, unsigned short int thd_id); */
/* extern int sched_block_helper(spdid_t spdid, unsigned short int dependency_thd); */

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	/* if (unlikely(crash_flag)) { */
	/* 	/\* printc("in scheduler server wakeup interface thd_id %d crash_flag %d\n", thd_id, crash_flag); *\/ */
	/* 	return sched_wakeup_helper(spdid, thd_id); */
	/* } */
	return sched_wakeup(spdid, thd_id);
}


int __sg_sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	/* printc("in scheduler server block interface\n"); */
	/* if (unlikely(crash_flag)) { */
	/* 	return sched_block_helper(spdid, dependency_thd); */
	/* } */
	return sched_block(spdid, dependency_thd);
}
