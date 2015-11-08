#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <evt.h>

#include <periodic_wake.h>
#include <timed_blk.h>

int ec3_ser3_pass(long id)
{
	/* printc("\n*** trigger *****\n"); */
	/* printc("\n(ser 3) thread %d is triggering event %ld\n", cos_get_thd_id(), id); */
	evt_trigger(cos_spd_id(), id);

	return 0;
}

vaddr_t ec3_ser3_test(void)
{
	vaddr_t ret = (vaddr_t)cos_get_vas_page();
	return ret;
}


