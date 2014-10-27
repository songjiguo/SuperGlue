/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>
.text	

cos_asm_server_stub_spdid(sched_wakeup)
//cos_asm_server_stub_spdid(sched_block)
cos_asm_server_stub_spdid(sched_timeout)
cos_asm_server_stub_spdid(sched_timeout_thd)
cos_asm_server_stub_spdid(sched_create_net_acap)
cos_asm_server_stub(sched_create_thread)
cos_asm_server_stub(sched_create_thd)
cos_asm_server_stub(sched_create_thread_default)
cos_asm_server_stub_spdid(sched_thread_params)
cos_asm_server_stub(sched_tick_freq)
cos_asm_server_stub(sched_cyc_per_tick)
cos_asm_server_stub(sched_timestamp)
cos_asm_server_stub(sched_timer_stopclock)
cos_asm_server_stub(sched_priority)
	
//cos_asm_server_stub_spdid(sched_component_take)
//cos_asm_server_stub_spdid(sched_component_release)

/* with _spdid will enforce the same component to call these APIs,
   however, for reflection in c^3, sometimes we need do the reflection
   from the client component which is dofferent from the component
   that makes these calls earlier. So change this to non _spdid version
	-- Jiguo
*/
cos_asm_server_stub(sched_component_take)
cos_asm_server_stub(sched_component_release)


cos_asm_server_fn_stub_spdid(sched_block, __sg_sched_block)	
cos_asm_server_fn_stub_spdid(sched_reflect, __sg_sched_reflect)
