/* IDL generated code ver 0.1 ---  Tue Oct 27 01:42:29 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <ramfs.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct desc_track {
	spdid_t spdid;
	td_t parent_tid;
	char *param;
	int len;
	tor_flags_t tflags;
	long evtid;
	td_t tid;
	int offset;
	unsigned int state;
	unsigned int next_state;
	int server_tid;
	unsigned long long fault_cnt;
};

static volatile unsigned long global_fault_cnt = 0;

/* tracking thread state for data recovery */
//CVECT_CREATE_STATIC(rd_vect);
COS_MAP_CREATE_STATIC(ramfs_desc_maps);
CSLAB_CREATE(ramfs_slab, sizeof(struct desc_track));

// assumption: marshalled function is not same as the block/wakeup function
struct __ser_tsplit_marshalling {
	spdid_t spdid;
	td_t parent_tid;
	int len;
	tor_flags_t tflags;
	long evtid;
	char data[0];
};

enum state_codes { state_tsplit, state_trelease, state_treadp, state_twritep,
	    state_null };

static inline void block_cli_if_recover(int id);
static inline void block_cli_if_basic_id(int id);
static inline void call_restore_data(struct desc_track *desc);
static inline void call_save_data(int id, void *data);
static inline int block_cli_if_invoke_tsplit(spdid_t spdid, td_t parent_tid,
					     char *param, int len,
					     tor_flags_t tflags, long evtid,
					     int ret, long *fault,
					     struct usr_inv_cap *uc);
static inline int block_cli_if_marshalling_invoke_tsplit(spdid_t spdid,
							 td_t parent_tid,
							 char *param, int len,
							 tor_flags_t tflags,
							 long evtid, int ret,
							 long *fault,
							 struct usr_inv_cap *uc,
							 struct
							 __ser_tsplit_marshalling
							 *md, int sz,
							 cbuf_t cb);
static inline void block_cli_if_desc_update_tsplit();
static inline int block_cli_if_track_tsplit(int ret, spdid_t spdid,
					    td_t parent_tid, char *param,
					    int len, tor_flags_t tflags,
					    long evtid);
static inline int block_cli_if_invoke_treadp(spdid_t spdid, td_t tid,
					     int *_retval_cbuf_off,
					     int *_retval_sz, int ret,
					     long *fault,
					     struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_treadp(int id);
static inline int block_cli_if_track_treadp(int ret, spdid_t spdid, td_t tid,
					    int *_retval_cbuf_off,
					    int *_retval_sz);
static inline int block_cli_if_invoke_twritep(spdid_t spdid, td_t tid, int cbid,
					      int sz, int ret, long *fault,
					      struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_twritep(int id);
static inline int block_cli_if_track_twritep(int ret, spdid_t spdid, td_t tid,
					     int cbid, int sz);
static inline int block_cli_if_invoke_trelease(spdid_t spdid, td_t tid, int ret,
					       long *fault,
					       struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_trelease(int id);
static inline int block_cli_if_track_trelease(int ret, spdid_t spdid, td_t tid);

static inline struct desc_track *call_desc_lookup(int id)
{
	/* return (struct desc_track *)cvect_lookup(&rd_vect, id); */
	return (struct desc_track *)cos_map_lookup(&ramfs_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc()
{
	struct desc_track *desc = NULL;
	int map_id = 0;

	while (1) {
		desc = cslab_alloc_ramfs_slab();
		assert(desc);
		map_id = cos_map_add(&ramfs_desc_maps, desc);
		desc->tid = map_id;
		desc->server_tid = -1;	// reset to -1
		if (map_id >= 1)
			break;
	}
	assert(desc && desc->tid >= 1);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	int id = desc->tid;
	desc->server_tid = -1;	// reset to -1
	assert(desc);
	cslab_free_ramfs_slab(desc);
	cos_map_del(&ramfs_desc_maps, id);
	return;
}

static inline void call_desc_cons(struct desc_track *desc, int id,
				  spdid_t spdid, td_t parent_tid, char *param,
				  int len, tor_flags_t tflags, long evtid)
{
	assert(desc);

	desc->server_tid = id;
	desc->spdid = spdid;
	desc->parent_tid = parent_tid;
	desc->param = param;
	desc->len = len;
	desc->tflags = tflags;
	desc->evtid = evtid;

	return;
}

/* static inline struct desc_track *call_desc_alloc(int id) { */
/* 	struct desc_track *_desc_track; */

/* 	_desc_track = (struct desc_track *)cslab_alloc_rdservice(); */
/* 	assert(_desc_track); */
/* 	if (cvect_add(&rd_vect, _desc_track, id)) { */
/* 		assert(0); */
/* 	} */
/* 	_desc_track->tid = id; */
/* 	return _desc_track; */
/* } */

/* static inline void call_desc_dealloc(struct desc_track *desc) { */
/* 	assert(desc); */
/* 	assert(!cvect_del(&rd_vect, desc->tid)); */
/* 	cslab_free_rdservice(desc); */
/* } */

/* static inline void call_desc_cons(struct desc_track *desc, int id, spdid_t spdid, td_t parent_tid, char * param, int len, tor_flags_t tflags, long evtid) { */
/* 	assert(desc); */

/* 	desc->tid = id; */
/* 	desc->server_tid = id; */
/* 	desc->spdid=spdid;
 desc->parent_tid= parent_tid;
 desc->param= param;
 desc->len= len;
 desc->tflags= tflags;
 desc->evtid= evtid;
 */
/* 	return; */
/* } */

static inline struct desc_track *call_desc_update(int id, int next_state)
{
	struct desc_track *desc = NULL;
	unsigned int from_state = 0;
	unsigned int to_state = 0;

	desc = call_desc_lookup(id);
	if (unlikely(!desc))
		goto done;

	desc->next_state = next_state;

	if (likely(desc->fault_cnt == global_fault_cnt))
		goto done;
	desc->fault_cnt = global_fault_cnt;

	// State machine transition under the fault
	block_cli_if_recover(id);
	from_state = desc->state;
	to_state = next_state;

 done:
	return desc;
}

static inline void call_restore_data(struct desc_track *desc)
{
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
	assert(desc);
	call_restore_data(desc);
}

static inline void block_cli_if_basic_id(int id)
{
	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	int retval = 0;
	retval =
	    tsplit(desc->spdid, desc->parent_tid, desc->param, desc->len,
		   desc->tflags, desc->evtid);

	//TODO: define the error code for non-recovered parent
	if (retval == -99) {
		id = desc->parent_tid;
		//block_cli_if_recover(id);
		call_desc_update(id, state_tsplit);
	} else {
		assert(retval);
		desc->server_tid = retval;
		desc->state = state_tsplit;
	}

	block_cli_if_recover_data(desc);
}

static inline void block_cli_if_recover(int id)
{
	block_cli_if_basic_id(id);
}

static inline void call_save_data(int id, void *data)
{
}

static inline void block_cli_if_save_data(int id, void *data)
{
	call_save_data(id, data);
}

static inline int block_cli_if_track_tsplit(int ret, spdid_t spdid,
					    td_t parent_tid, char *param,
					    int len, tor_flags_t tflags,
					    long evtid)
{
	struct desc_track *desc = call_desc_alloc();
	assert(desc);
	call_desc_cons(desc, ret, spdid, parent_tid, param, len, tflags, evtid);
	desc->state = state_tsplit;

	return desc->tid;
}

static inline void block_cli_if_desc_update_tsplit()
{
}

static inline int block_cli_if_marshalling_invoke_tsplit(spdid_t spdid,
							 td_t parent_tid,
							 char *param, int len,
							 tor_flags_t tflags,
							 long evtid, int ret,
							 long *fault,
							 struct usr_inv_cap *uc,
							 struct
							 __ser_tsplit_marshalling
							 *md, int sz, cbuf_t cb)
{
	struct desc_track *parent_desc = NULL;
	if ((parent_desc = call_desc_lookup(parent_tid))) {
		parent_tid = parent_desc->server_tid;
	}

	md->spdid = spdid;
	md->parent_tid = parent_tid;
	md->len = len;
	md->tflags = tflags;
	md->evtid = evtid;

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spdid, cb, sz);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_trelease(int ret, spdid_t spdid, td_t tid)
{
	struct desc_track *desc = call_desc_lookup(tid);
	assert(desc);
	call_desc_dealloc(desc);

	return ret;
	// TODO: this needs to be changed
	/* struct desc_track *child_desc_list = desc->child_desc_list;   */
	/* if (EMPTY_LIST(child_desc_list)) { */
	/*      call_desc_dealloc(desc); */
	/* } */
}

static inline void block_cli_if_desc_update_trelease(int id)
{
	call_desc_update(id, state_trelease);
}

static inline int block_cli_if_invoke_trelease(spdid_t spdid, td_t tid, int ret,
					       long *fault,
					       struct usr_inv_cap *uc)
{
	struct desc_track *desc = call_desc_lookup(tid);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_tid);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_twritep(int ret, spdid_t spdid, td_t tid,
					     int cbid, int sz)
{
	struct desc_track *desc = call_desc_lookup(tid);
	assert(desc);

	return ret;
}

static inline void block_cli_if_desc_update_twritep(int id)
{
	call_desc_update(id, state_twritep);
}

static inline int block_cli_if_invoke_twritep(spdid_t spdid, td_t tid, int cbid,
					      int sz, int ret, long *fault,
					      struct usr_inv_cap *uc)
{
	struct desc_track *desc = call_desc_lookup(tid);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 4, spdid, desc->server_tid, cbid, sz);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_treadp(int ret, spdid_t spdid, td_t tid,
					    int *_retval_cbuf_off,
					    int *_retval_sz)
{
	struct desc_track *desc = call_desc_lookup(tid);
	assert(desc);

	return ret;
}

static inline void block_cli_if_desc_update_treadp(int id)
{
	call_desc_update(id, state_treadp);
}

static inline int block_cli_if_invoke_treadp(spdid_t spdid, td_t tid,
					     int *_retval_cbuf_off,
					     int *_retval_sz, int ret,
					     long *fault,
					     struct usr_inv_cap *uc)
{
	struct desc_track *desc = call_desc_lookup(tid);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 4, spdid, desc->server_tid,
		     _retval_cbuf_off, _retval_sz);
	*fault = __fault;

	return ret;
}

CSTUB_FN(td_t, tsplit) (struct usr_inv_cap * uc, spdid_t spdid, td_t parent_tid,
			char *param, int len, tor_flags_t tflags, long evtid) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!ramfs_desc_maps.data.depth)) {
		cos_map_init_static(&ramfs_desc_maps);
	}

	struct __ser_tsplit_marshalling *md = NULL;
	cbuf_t cb = 0;
	int sz = len + sizeof(struct __ser_tsplit_marshalling);
 redo:
	block_cli_if_desc_update_tsplit();

	md = (struct __ser_tsplit_marshalling *)cbuf_alloc(sz, &cb);
	assert(md);		// assume we always get cbuf for now

	ret =
	    block_cli_if_marshalling_invoke_tsplit(spdid, parent_tid, param,
						   len, tflags, evtid, ret,
						   &fault, uc, md, sz, cb);

	if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		cbuf_free(cb);
		goto redo;
	}
	cbuf_free(cb);

	ret =
	    block_cli_if_track_tsplit(ret, spdid, parent_tid, param, len,
				      tflags, evtid);

	return ret;
}

CSTUB_FN(int, trelease)(struct usr_inv_cap * uc, spdid_t spdid, td_t tid) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!ramfs_desc_maps.data.depth)) {
		cos_map_init_static(&ramfs_desc_maps);
	}

 redo:
	block_cli_if_desc_update_trelease(tid);
	ret = block_cli_if_invoke_trelease(spdid, tid, ret, &fault, uc);
	if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	ret = block_cli_if_track_trelease(ret, spdid, tid);

	return ret;
}

CSTUB_FN(int, twritep)(struct usr_inv_cap * uc, spdid_t spdid, td_t tid,
		       int cbid, int sz) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!ramfs_desc_maps.data.depth)) {
		cos_map_init_static(&ramfs_desc_maps);
	}

 redo:
	block_cli_if_desc_update_twritep(tid);
	ret =
	    block_cli_if_invoke_twritep(spdid, tid, cbid, sz, ret, &fault, uc);
	if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	ret = block_cli_if_track_twritep(ret, spdid, tid, cbid, sz);

	return ret;
}

CSTUB_FN(int, treadp)(struct usr_inv_cap * uc, spdid_t spdid, td_t tid,
		      int *_retval_cbuf_off, int *_retval_sz) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!ramfs_desc_maps.data.depth)) {
		cos_map_init_static(&ramfs_desc_maps);
	}

 redo:
	block_cli_if_desc_update_treadp(tid);
	ret =
	    block_cli_if_invoke_treadp(spdid, tid, _retval_cbuf_off, _retval_sz,
				       ret, &fault, uc);
	if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	ret =
	    block_cli_if_track_treadp(ret, spdid, tid, _retval_cbuf_off,
				      _retval_sz);

	return ret;
}
