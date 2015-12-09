/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#include <cos_asm_server_stub.h>  this is using stkmgr
#include <cos_asm_server_stub_simple_stack.h>	
	
.text	
cos_asm_server_stub_spdid(evt_create)
cos_asm_server_fn_stub_spdid(evt_split, __sg_evt_split)
cos_asm_server_fn_stub_spdid(evt_split_exist, __sg_evt_split_exist)
cos_asm_server_stub_spdid(evt_free)
//cos_asm_server_stub_spdid(evt_wait)
cos_asm_server_stub_spdid(evt_grp_wait)
cos_asm_server_stub_spdid(evt_grp_mult_wait)
cos_asm_server_stub_spdid(evt_trigger)
cos_asm_server_stub_spdid(evt_set_prio)
cos_asm_server_stub_spdid(evt_stats)
cos_asm_server_stub_spdid(evt_stats_len)

cos_asm_server_fn_stub_spdid(evt_wait, __sg_evt_wait)

cos_asm_server_fn_stub(evt_upcall_creator, __sg_evt_upcall_creator)
cos_asm_server_fn_stub_spdid(evt_client_fault_notification,__ser_evt_client_fault_notification)
