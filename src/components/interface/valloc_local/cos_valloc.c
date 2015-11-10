/*
 * Local valloc (for components which do not depend on valloc)
 *
*/

// for now, only 1 page at a time

#include <cos_component.h>
#include <print.h>
#include <valloc_config.h>

#ifdef USE_VALLOC_LOCAL

/* // can not use this lock since cos_sched_lock_take/release will call context_switch */
/* #include "../../implementation/sched/cos_sched_sync.h" */
/* /\* synchronization... *\/ */
/* #define LOCK()   if (cos_sched_lock_take())    BUG(); */
/* #define UNLOCK() if (cos_sched_lock_release()) BUG(); */

void *
valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages)
{
	char *addr;
	vaddr_t ret = 0;
	
	/* LOCK(); */

	printc("local valloc (in spd %ld, thd %d)\n", cos_spd_id(), cos_get_thd_id());

	addr = cos_get_heap_ptr();
	if (!addr) {
		printc("fail to allocate a page from the heap\n");
		goto done;
	}
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));

done:
	/* UNLOCK(); */
	return addr;
}


int 
valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages)
{
	return 0;
}

/* Jiguo: for recovering alias */ 
int
valloc_upcall(spdid_t spdid, vaddr_t addr)
{
	printc("cos_valloc: valloc_upcall called from spd %d\n", spdid);
	return 0;
}

#else
extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
extern int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages);
extern int valloc_upcall(spdid_t spdid, vaddr_t addr);
#endif
