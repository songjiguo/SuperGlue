/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub.h>  //this is using stkmgr
//#include <cos_asm_server_stub_simple_stack.h>	

.text	
// cos_asm_server_stub_spdid(tsplit)
// cos_asm_server_fn_stub_spdid(__tsplit, __sg_tsplit)
// cos_asm_server_fn_stub_spdid(tmerge, __sg_tmerge)
	
// cos_asm_server_stub_spdid(tread)
// cos_asm_server_stub_spdid(twrite)
// cos_asm_server_stub_spdid(twmeta)
// cos_asm_server_stub_spdid(trelease)
	
//cos_asm_server_fn_stub_spdid(tread, __sg_tread)
//cos_asm_server_fn_stub_spdid(twrite, __sg_twrite)

//cos_asm_server_fn_stub_spdid(twmeta, __sg_twmeta)
//cos_asm_server_fn_stub_spdid(tmerge, __sg_tmerge)

cos_asm_server_fn_stub_spdid(tsplit, __sg_tsplit)
cos_asm_server_fn_stub_spdid(tmerge, __sg_tmerge)
cos_asm_server_stub_spdid(trelease)
cos_asm_server_stub_spdid(tread)
cos_asm_server_stub_spdid(twrite)
cos_asm_server_fn_stub_spdid(treadp, __sg_treadp)
//cos_asm_server_stub_spdid(treadp)
cos_asm_server_stub_spdid(twritep)
cos_asm_server_fn_stub_spdid(trmeta, __sg_trmeta)
cos_asm_server_fn_stub_spdid(twmeta, __sg_twmeta)
