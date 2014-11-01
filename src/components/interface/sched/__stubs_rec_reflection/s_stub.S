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
	
cos_asm_server_fn_stub_spdid(sched_block, __sg_sched_block)	
cos_asm_server_fn_stub_spdid(sched_reflect, __sg_sched_reflect)

cos_asm_server_stub_spdid(sched_component_take)
//cos_asm_server_stub_spdid(sched_component_release)

/* After the fault, a thread might call sched to take the lock again,
   it will check if the lock has been taken and release the lock if so
   before it can take the lock again. This check is done on the
   recovery path which is on the client interface. We need pass the
   correct component id to the scheduler to release the lock. So I
   change this interface function to "not" use _stub_spid which will
   enfore only the caller spd can release the lock. We do not need
   change this for sched_component_take -- Jiguo */
cos_asm_server_stub(sched_component_release)
/* Jiguo: this is used to reflect the owner of a critical section. We
   do not need this on the noraml path. */
cos_asm_server_stub(sched_reflection_component_owner)
