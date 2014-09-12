#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <evt.h>

#include <periodic_wake.h>
#include <timed_blk.h>

int ec3_ser3_pass(long id)
{
	printc("\n*** trigger *****\n");
	printc("(ser 3) thread %d is triggering event %ld\n\n", cos_get_thd_id(), id);
	evt_trigger(cos_spd_id(), id);

	return 0;
}


