/* /\*  */
/*  *  Jiguo Song:  */
/*  *\/ */

/* #include <mbtorrent.h> */
/* /\* #include <torrent.h> *\/ */
/* #include <uniq_map.h> */
/* #include <cbuf.h> */

/* /\* when select these, do not forget to change c_stub.c *\/ */
/* /\* use ramfs_rec.sh *\/ */
/* /\* ramfs_trest1 nad ramfs_test2 in implementation/test *\/ */

/* /\* create an inconsistency list, so we can tell later when read/write */
/*  * on an un-recovered file *\/ */

/* struct tor_list { */
/* 	int fid; */
/* 	int tid; */
/* #if (!LAZY_RECOVERY) */
/* 	int has_rebuilt; */
/* #endif */
	
/* 	struct tor_list *next, *prev; */
/* }; */

/* struct tor_list *all_tor_list = NULL; */

/* static int __twmeta(spdid_t spdid, td_t td, int cbid, int sz, int offset, int flag); */

/* static td_t */
/* tid_lookup(int fid) */
/* { */
/* 	td_t ret = -1; */
/* 	struct tor_list *item; */
/* 	assert(all_tor_list); */
	
/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (fid == item->fid) { */
/* 			ret = item->tid; */
/* 			break; */
/* 		} */
/* 	} */

/* 	return ret; */
/* } */

/* static int */
/* fid_lookup(int tid) */
/* { */
/* 	int ret = -1; */
/* 	struct tor_list *item; */
/* 	if (!all_tor_list) return ret; */
/* 	/\* assert(all_tor_list); *\/ */
	
/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (tid == item->tid) { */
/* 			ret = item->fid; */
/* 			break; */
/* 		} */
/* 	} */
/* 	return ret; */
/* } */


/* static void */
/* fid_find_del(int fid) */
/* { */
/* 	struct tor_list *item; */
/* 	assert(all_tor_list); */
	
/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (fid == item->fid) { */
/* 			REM_LIST(item, next, prev); */
/* 			break; */
/* 		} */
/* 	} */

/* 	return; */
/* } */

/* static int */
/* fid_update_tid(int fid, int tid) */
/* { */
/* 	int ret = 0; */
/* 	struct tor_list *item; */
/* 	assert(all_tor_list); */
	
/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (fid == item->fid) { */
/* #if (!LAZY_RECOVERY) */
/* 			item->has_rebuilt = 1; */
/* #endif */
/* 			item->tid = tid; */
/* 			ret = 1; */
/* 		} */
/* 	} */
	
/* 	return ret; */
/* } */

/* static void */
/* tid_update_fid(int fid, int tid) */
/* { */
/* 	struct tor_list *item; */
/* 	assert(all_tor_list); */
	
/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (tid == item->tid) { */
/* #if (!LAZY_RECOVERY) */
/* 			item->has_rebuilt = 1; */
/* #endif */
/* 			item->fid = fid; */
/* 		} */
/* 	} */
	
/* 	return; */
/* } */

/* static int */
/* fid_lookup_rebuilt(int fid) */
/* { */
/* 	struct tor_list *item; */
/* 	int ret = 0; */
/* 	assert(all_tor_list); */

/* #if (!LAZY_RECOVERY)	 */
/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (fid == item->fid) { */
/* 			return item->has_rebuilt; */
/* 		} */
/* 	} */
/* #endif */
/* 	return ret; */
/* } */

/* static int */
/* restore_tor(int fid, td_t tid) */
/* { */
/* 	int ret = 0; */
/* 	int cbid = 0, sz = 0, offset = 0; */
/* 	int nth_cb = 0, total_cbs = 0; */

/* 	struct torrent *tor; */
/* 	tor  = tor_lookup(tid); */
/* 	assert(tor); */
/* 	/\* printc("before meta done: tor->offset %d\n", tor->offset); *\/ */

/* 	/\* printc("restore_tor:::fid is %d tid %d\n", fid, tid); *\/ */
/* 	if (fid_lookup_rebuilt(fid)) goto done; */

/* 	total_cbs = cbuf_c_introspect(cos_spd_id(), fid, 0, CBUF_INTRO_TOT); */
/* 	/\* printc("total cbs %d\n", total_cbs); *\/ */

/* 	while(total_cbs--) { */
/* 		nth_cb++; */
/* 		cbid   = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_CBID); */
/* 		if (!cbid) goto no_found; */
/* 		sz     = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_SIZE); */
/* 		if (!sz) goto no_found; */
/* 		offset = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_OFFSET); */
	
/* 		/\* printc("found cbid %d size %d offset %d\n", cbid, sz, offset); *\/ */
/* 		/\* printc("meta write:: tor id %d for fid %d\n", tid, fid); *\/ */
/* 		__twmeta(cos_spd_id(), tid, cbid, sz, offset, 1);	/\* 1 for recovery now *\/ */
/* 	} */

/* 	/\* printc("after meta done: tor->offset %d\n", tor->offset); *\/ */
/* 	tor->offset = 0; */
/* 	/\* fid_find_del(fid); *\/ */
/* 	fid_update_tid(fid, tid);   /\*  tid is 0 when tor_list was built, so needs to be updated here *\/ */

/* 	/\* printc("[restore meta done]\n"); *\/ */
/* done: */
/* 	return ret; */
/* no_found:  */
/* 	/\* printc("Not found the record\n"); *\/ */
/* 	ret = -1; */
/* 	goto done; */
/* } */



/* static void */
/* find_restore(int tid) */
/* { */
/* 	struct tor_list *item; */
/* 	assert(all_tor_list); */

/* 	for (item = FIRST_LIST(all_tor_list, next, prev); */
/* 	     item != all_tor_list; */
/* 	     item = FIRST_LIST(item, next, prev)){ */
/* 		if (tid == item->tid) { */
/* 			restore_tor(item->fid, tid); /\* tid should be always unique *\/ */
/* 			return; */
/* 		} */
/* 	} */
/* 	return; */
/* } */

/* /\* static void *\/ */
/* /\* find_update(int fid, int val) *\/ */
/* /\* { *\/ */
/* /\* 	struct tor_list *item; *\/ */
/* /\* 	assert(all_tor_list); *\/ */
	
/* /\* 	for (item = FIRST_LIST(all_tor_list, next, prev); *\/ */
/* /\* 	     item != all_tor_list; *\/ */
/* /\* 	     item = FIRST_LIST(item, next, prev)){ *\/ */
/* /\* 		if (fid == item->fid) { *\/ */
/* /\* 			item->tid = val; *\/ */
/* /\* 		} *\/ */
/* /\* 	} *\/ */

/* /\* 	return; *\/ */
/* /\* } *\/ */


/* static char * */
/* get_unique_path(struct fsobj *fsp) */
/* { */
/* 	struct fsobj *item, *fso, *test; */
/* 	struct torrent *p, *t; */
/* 	char *name = ""; */
/* 	unsigned int end_l; */
	
/* 	assert(fsp->parent);  	/\* non root *\/ */

/* 	/\* printc("unique path does not exist...\n"); *\/ */
/* 	item = fsp->parent; */
/* 	if (!item->parent) { */
/* 		name = strcpy(name, fsp->name);	/\* top *\/ */
/* 	} else { */
/* 		name = strcpy(name, item->unique_path); */
/* 		name = strcat(name, "/"); */
/* 		name = strcat(name, fsp->name); */
/* 	} */

/* 	fsp->unique_path = (char *)malloc(strlen(name)); */
/* 	memcpy(fsp->unique_path, name, strlen(name)); */
	
/* 	return fsp->unique_path; */
/* } */


/* /\*  */
/*  * find the full path as the unique map index, e.g, "foo/bar/who" is */
/*  * different from "foo/boo/who" / */
/* *\/ */

/* static char * */
/* find_fullpath(td_t td) */
/* { */
/* 	struct fsobj *fsp; */
/* 	struct torrent *p; */
/* 	char *name; */

/* 	p  = tor_lookup(td); */
/* 	assert(p); */
/* 	fsp = p->data; */
/* 	assert(fsp); */

/* 	if (unlikely(!fsp->unique_path)) { */
/* 		name = get_unique_path(fsp); */
/* 	} else { */
/* 		name = fsp->unique_path; */
/* 	} */

/* 	/\* printc("unique path %s in fn with size %d\n", name, strlen(name)); *\/ */

/* 	return name; */
/* } */


/* static void */
/* add_tor_list(int fid, int tid) */
/* { */
/* 	struct tor_list *item; */

/* 	if (!all_tor_list) { /\* initialize the head *\/ */
/* 		all_tor_list = (struct tor_list *)malloc(sizeof(struct tor_list)); */
/* 		INIT_LIST(all_tor_list, next, prev); */
/* 	} */

/* 	assert(all_tor_list); */
/* 	if (!fid_update_tid(fid, tid)) { */

/* 		item  = (struct tor_list *)malloc(sizeof(struct tor_list)); */
/* 		if (!item) assert(0); */
		
/* 		item->fid = fid; */
/* 		item->tid = tid; */
/* #if (!LAZY_RECOVERY) */
/* 		item->has_rebuilt = 0; */
/* #endif */
/* 		INIT_LIST(item, next, prev); */
		
/* 		ADD_LIST(all_tor_list, item, next, prev); */
/* 	} */
/* 	/\* printc("FID: %d\n", fid); *\/ */
/* 	return; */
/* } */

/* static int */
/* reserve_cbuf_fid(cbuf_t cb, td_t td, u32_t offset, int len, int fid) */
/* { */
/* 	int ret = 0, sz_p = 0; */
/* 	cbuf_t cb_p; */
/* 	char *d, *path; */

/* 	/\* /\\* update the cbuf owner to ramfs *\\/ *\/ */
/* 	/\* /\\* only the owner can free/revoke the cbuf *\\/ *\/ */
/* 	/\* if (cbuf_claim(cb)) { *\/ */
/* 	/\* 	printc("failed to claim the ownership\n"); *\/ */
/* 	/\* 	BUG(); *\/ */
/* 	/\* } *\/ */

/* 	/\* pass the FT relevant info to cbuf manager  *\/ */
/* 	if (cbuf_add_record(cb, len, offset, fid)) { */
/* 		printc("failed to record cbuf\n"); */
/* 		ret = -1; */
/* 	} */
	
/* 	add_tor_list(fid, td);  */

/* 	return ret; */
/* } */

