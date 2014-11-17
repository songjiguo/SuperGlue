#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <unit_mmrec3.h>
#include <unit_mmrec4.h>

#define PAGE_NUM 10

vaddr_t addr[PAGE_NUM];
static int idx = 0;

vaddr_t mm_test2(void)
{
	vaddr_t ret;
	addr[idx]  = (vaddr_t)cos_get_vas_page();
	/* printc("kevin: addr %p\n", addr[0]); */
	ret = addr[idx];
	idx++;
	return ret;
}

void mm_test2_34()
{
	vaddr_t d_addr, test;
	vaddr_t s_addr;
	int i;

	printc("in mm_test2_34\n");

	for (i = 0; i< PAGE_NUM; i++) {
		if (i < PAGE_NUM/2) {
#ifdef ONE2FIVE
			s_addr = addr[0];
#else
			s_addr = addr[i];
#endif
			/* printc("andy: addr %p\n", addr[0]); */
			d_addr  = mm_test3();
			if (!d_addr) BUG();
			if (d_addr != mman_alias_page(cos_spd_id(), s_addr, cos_spd_id()+1, d_addr, MAPPING_RW)) BUG();
		} else {
#ifdef ONE2FIVE
			s_addr = addr[1];
#else
			s_addr = addr[i];
#endif
			/* printc("andy: addr %p\n", addr[0]); */
			d_addr  = mm_test4();
			if (!d_addr) BUG();
			if (d_addr != mman_alias_page(cos_spd_id(), s_addr, cos_spd_id()+2, d_addr, MAPPING_RW)) BUG();
		}
	}

	idx = 0;

	return;
}

/* #ifndef MM_RECOVERY */
/* void alias_replay(vaddr_t s_addr , int flag) { return; } */
/* #endif */

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
		printc("UNIT_MMREC 2 upcall: thread %d\n", cos_get_thd_id());
		/* alias_replay((vaddr_t)arg3, 0); */
		break;
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}

	return;
}
