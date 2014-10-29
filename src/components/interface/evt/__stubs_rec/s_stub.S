/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#include <cos_asm_server_stub.h>  this is using stkmgr
#include <cos_asm_server_stub_simple_stack.h>	
	
.text	
//cos_asm_server_stub_spdid(evt_create)
//cos_asm_server_stub_spdid(evt_split)
//cos_asm_server_stub_spdid(evt_free)
//cos_asm_server_stub_spdid(evt_wait)
cos_asm_server_stub_spdid(evt_grp_wait)
cos_asm_server_stub_spdid(evt_grp_mult_wait)
//cos_asm_server_stub_spdid(evt_trigger)
cos_asm_server_stub_spdid(evt_set_prio)

cos_asm_server_stub_spdid(evt_stats)
cos_asm_server_stub_spdid(evt_stats_len)

cos_asm_server_fn_stub_spdid(evt_create, __sg_evt_create)
cos_asm_server_fn_stub_spdid(evt_split, __sg_evt_split)
cos_asm_server_stub_spdid(c3_evt_split)
cos_asm_server_fn_stub_spdid(evt_free, __sg_evt_free)
cos_asm_server_fn_stub_spdid(evt_wait, __sg_evt_wait)
cos_asm_server_fn_stub_spdid(evt_trigger, __sg_evt_trigger)

cos_asm_server_stub_spdid(evt_reflection)
