#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

vaddr_t mm_test4()
{
	vaddr_t addr;
	addr  = (vaddr_t)cos_get_vas_page();
	if (!addr) BUG();
	return addr;
}

/* #ifndef MM_RECOVERY */
/* void alias_replay(vaddr_t s_addr, int flag) { return; } */
/* #endif */

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
		printc("UNIT_MMREC 4 upcall: thread %d\n", cos_get_thd_id());
		/* alias_replay((vaddr_t)arg3, 0); */
		break;
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}

	return;
}

