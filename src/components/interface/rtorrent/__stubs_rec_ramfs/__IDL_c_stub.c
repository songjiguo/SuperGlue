/* IDL generated code ver 0.1 ---  Fri Nov 27 10:24:09 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <torrent.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

static char *param_save(char *param, int param_len)
{
	char *l_param;

	if (param_len <= 0)
		return NULL;
	assert(param);
	l_param = malloc(param_len);
	assert(l_param);
	strncpy(l_param, param, param_len);
	l_param[param_len] = '\0';	// zero out any thing left after the end                                                                                                                  
	return l_param;
}

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
	unsigned long long fault_cnt;
	int server_tid;

};

static volatile unsigned long global_fault_cnt = 0;
static int first_map_init = 0;

COS_MAP_CREATE_STATIC(ramfs_desc_maps);
CSLAB_CREATE(ramfs_slab, sizeof(struct desc_track));

static inline struct desc_track *call_desc_lookup(td_t id)
{
	return (struct desc_track *)cos_map_lookup(&ramfs_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc()
{
	struct desc_track *desc = NULL;
	td_t map_id = 0;

	while (1) {
		desc = cslab_alloc_ramfs_slab();
		assert(desc);
		map_id = cos_map_add(&ramfs_desc_maps, desc);
		desc->tid = map_id;
		desc->server_tid = -1;	// reset to -1
		if (map_id >= 2)
			break;
	}
	assert(desc && desc->tid >= 1);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	td_t id = desc->tid;
	desc->server_tid = -1;	// reset to -1
	assert(desc);
	cslab_free_ramfs_slab(desc);
	cos_map_del(&ramfs_desc_maps, id);
	return;
}

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

static inline struct desc_track *call_desc_update(td_t id, int next_state);
static inline void call_map_init();
static inline void block_cli_if_recover(td_t id);
static inline void block_cli_if_basic_id(td_t id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void call_save_data(td_t id, void *data);
static inline void block_cli_if_upcall_creator(td_t id);
static inline int block_cli_if_invoke_tsplit(spdid_t spdid, td_t parent_tid,
					     char *param, int len,
					     tor_flags_t tflags, long evtid,
					     int ret, long *fault,
					     struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_tsplit();
static inline int block_cli_if_track_tsplit(int ret, spdid_t spdid,
					    td_t parent_tid, char *param,
					    int len, tor_flags_t tflags,
					    long evtid);
static inline void block_cli_if_desc_update_tsplit(spdid_t spdid,
						   td_t parent_tid, char *param,
						   int len, tor_flags_t tflags,
						   long evtid);
static inline void call_desc_cons(struct desc_track *desc, td_t id,
				  spdid_t spdid, td_t parent_tid, char *param,
				  int len, tor_flags_t tflags, long evtid);
static inline int block_cli_if_invoke_treadp(spdid_t spdid, td_t tid, int len,
					     int *_retval_cbuf_off,
					     int *_retval_sz, int ret,
					     long *fault,
					     struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_treadp();
static inline int block_cli_if_track_treadp(int ret, spdid_t spdid, td_t tid,
					    int len, int *_retval_cbuf_off,
					    int *_retval_sz);
static inline void block_cli_if_desc_update_treadp(spdid_t spdid, td_t tid,
						   int len,
						   int *_retval_cbuf_off,
						   int *_retval_sz);
static inline int block_cli_if_invoke_twritep(spdid_t spdid, td_t tid, int cbid,
					      int sz, int ret, long *fault,
					      struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_twritep();
static inline int block_cli_if_track_twritep(int ret, spdid_t spdid, td_t tid,
					     int cbid, int sz);
static inline void block_cli_if_desc_update_twritep(spdid_t spdid, td_t tid,
						    int cbid, int sz);
static inline int block_cli_if_invoke_trelease(spdid_t spdid, td_t tid, int ret,
					       long *fault,
					       struct usr_inv_cap *uc);
static inline void block_cli_if_recover_upcall_subtree(td_t id);
static inline int block_cli_if_desc_update_post_fault_trelease();
static inline int block_cli_if_track_trelease(int ret, spdid_t spdid, td_t tid);
static inline void block_cli_if_desc_update_trelease(spdid_t spdid, td_t tid);

static inline void call_map_init()
{
	if (unlikely(!first_map_init)) {
		first_map_init = 1;
		cos_map_init_static(&ramfs_desc_maps);
	}
	return;
}

static inline void block_cli_if_recover(td_t id)
{
	block_cli_if_basic_id(id);
}

static inline void call_desc_cons(struct desc_track *desc, td_t id,
				  spdid_t spdid, td_t parent_tid, char *param,
				  int len, tor_flags_t tflags, long evtid)
{
	assert(desc);

	desc->server_tid = id;

	desc->spdid = spdid;
	desc->parent_tid = parent_tid;
	desc->param = param_save(param, len);;
	desc->len = len;
	desc->tflags = tflags;
	desc->evtid = evtid;

	desc->fault_cnt = global_fault_cnt;

	return;
}

static inline void block_cli_if_basic_id(td_t id)
{
	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	int retval = 0;
 again:
	retval =
	    tsplit(desc->spdid, desc->parent_tid, desc->param, desc->len,
		   desc->tflags, desc->evtid);
	//TODO: define the error code for non-recovered parent
	// thinking...1111111
	if (retval == -EINVAL) {
		id = desc->parent_tid;
		call_desc_update(id, state_tsplit);
		goto again;
	}

	assert(retval);
	desc->state = state_tsplit;	// set the state to the initial state
	desc->fault_cnt = global_fault_cnt;	// set the fault counter to the global
	block_cli_if_recover_data(desc);
	return;
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
	assert(desc);
}

static inline void block_cli_if_upcall_creator(td_t id)
{
}

static inline void call_save_data(td_t id, void *data)
{
}

static inline void block_cli_if_save_data(td_t id, void *data)
{
	call_save_data(id, data);
}

static inline struct desc_track *call_desc_update(td_t id, int next_state)
{
	struct desc_track *desc = NULL;
	unsigned int from_state = 0;
	unsigned int to_state = 0;

	if (id == 1)
		return NULL;

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

static inline int block_cli_if_track_tsplit(int ret, spdid_t spdid,
					    td_t parent_tid, char *param,
					    int len, tor_flags_t tflags,
					    long evtid)
{
	// if ret does not exist, just return as it is, thinking....
	if (ret == -EINVAL)
		return ret;

	struct desc_track *desc = call_desc_alloc();
	assert(desc);
	call_desc_cons(desc, ret, spdid, parent_tid, param, len, tflags, evtid);
	desc->state = state_tsplit;

	return desc->tid;
}

static inline void block_cli_if_desc_update_tsplit(spdid_t spdid,
						   td_t parent_tid, char *param,
						   int len, tor_flags_t tflags,
						   long evtid)
{
	call_desc_update(parent_tid, state_tsplit);
}

static inline int block_cli_if_desc_update_post_fault_tsplit()
{
	return 1;
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
	// thinking....
	if ((parent_tid > 1) && (parent_desc = call_desc_lookup(parent_tid))) {
		parent_tid = parent_desc->server_tid;
	}

	md->spdid = spdid;
	md->parent_tid = parent_tid;
	md->len = len;
	md->tflags = tflags;
	md->evtid = evtid;
	memcpy(&md->data[0], param, len + 1);

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spdid, cb, sz);
	*fault = __fault;

	return ret;
}

static inline void block_cli_if_desc_update_trelease(spdid_t spdid, td_t tid)
{
	call_desc_update(tid, state_trelease);
}

static inline void block_cli_if_recover_upcall_subtree(td_t id)
{
}

static inline int block_cli_if_desc_update_post_fault_trelease()
{
	return 1;
}

static inline int block_cli_if_invoke_trelease(spdid_t spdid, td_t tid, int ret,
					       long *fault,
					       struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(tid);

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_tid);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_trelease(int ret, spdid_t spdid, td_t tid)
{
	struct desc_track *desc = call_desc_lookup(tid);
	if (desc)
		call_desc_dealloc(desc);

	return ret;
	// TODO: this needs to be changed
	/* struct desc_track *child_desc_list = desc->child_desc_list;   */
	/* if (EMPTY_LIST(child_desc_list)) { */
	/*      call_desc_dealloc(desc); */
	/* } */
}

static inline int block_cli_if_track_twritep(int ret, spdid_t spdid, td_t tid,
					     int cbid, int sz)
{
	struct desc_track *desc = call_desc_lookup(tid);
	if (desc) {
	}

	if (ret == -EINVAL) {
		block_cli_if_basic_id(tid);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_twritep(spdid_t spdid, td_t tid,
						    int cbid, int sz)
{
	call_desc_update(tid, state_twritep);
}

static inline int block_cli_if_desc_update_post_fault_twritep()
{
	return 1;
}

static inline int block_cli_if_invoke_twritep(spdid_t spdid, td_t tid, int cbid,
					      int sz, int ret, long *fault,
					      struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(tid);

	CSTUB_INVOKE(ret, __fault, uc, 4, spdid, desc->server_tid, cbid, sz);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_treadp(int ret, spdid_t spdid, td_t tid,
					    int len, int *_retval_cbuf_off,
					    int *_retval_sz)
{
	struct desc_track *desc = call_desc_lookup(tid);
	if (desc) {
	}

	if (ret == -EINVAL) {
		block_cli_if_basic_id(tid);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_treadp(spdid_t spdid, td_t tid,
						   int len,
						   int *_retval_cbuf_off,
						   int *_retval_sz)
{
	call_desc_update(tid, state_treadp);
}

static inline int block_cli_if_desc_update_post_fault_treadp()
{
	return 1;
}

static inline int block_cli_if_invoke_treadp(spdid_t spdid, td_t tid, int len,
					     int *_retval_cbuf_off,
					     int *_retval_sz, int ret,
					     long *fault,
					     struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(tid);

	CSTUB_INVOKE_3RETS(ret, __fault, *_retval_cbuf_off, *_retval_sz, uc, 3,
			   spdid, desc->server_tid, len);
	*fault = __fault;

	return ret;
}

CSTUB_FN(td_t, tsplit) (struct usr_inv_cap * uc, spdid_t spdid, td_t parent_tid,
			char *param, int len, tor_flags_t tflags, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

	struct __ser_tsplit_marshalling *md = NULL;
	cbuf_t cb = 0;
	int sz = len + sizeof(struct __ser_tsplit_marshalling);

 redo:
	block_cli_if_desc_update_tsplit(spdid, parent_tid, param, len, tflags,
					evtid);

	md = (struct __ser_tsplit_marshalling *)cbuf_alloc(sz, &cb);
	assert(md);		// assume we always get cbuf for now

	ret =
	    block_cli_if_marshalling_invoke_tsplit(spdid, parent_tid, param,
						   len, tflags, evtid, ret,
						   &fault, uc, md, sz, cb);

	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		cbuf_free(cb);
		if (block_cli_if_desc_update_post_fault_tsplit()) {
			goto redo;
		}
	}
	cbuf_free(cb);

	ret =
	    block_cli_if_track_tsplit(ret, spdid, parent_tid, param, len,
				      tflags, evtid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, trelease)(struct usr_inv_cap * uc, spdid_t spdid, td_t tid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_trelease(spdid, tid);

	ret = block_cli_if_invoke_trelease(spdid, tid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_trelease()) {
			goto redo;
		}
	}

	ret = block_cli_if_track_trelease(ret, spdid, tid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, twritep)(struct usr_inv_cap * uc, spdid_t spdid, td_t tid,
		       int cbid, int sz) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_twritep(spdid, tid, cbid, sz);

	ret =
	    block_cli_if_invoke_twritep(spdid, tid, cbid, sz, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_twritep()) {
			goto redo;
		}
	}

	ret = block_cli_if_track_twritep(ret, spdid, tid, cbid, sz);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, treadp)(struct usr_inv_cap * uc, spdid_t spdid, td_t tid, int len,
		      int *_retval_cbuf_off, int *_retval_sz) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_treadp(spdid, tid, len, _retval_cbuf_off,
					_retval_sz);

	ret =
	    block_cli_if_invoke_treadp(spdid, tid, len, _retval_cbuf_off,
				       _retval_sz, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_treadp()) {
			goto redo;
		}
	}

	ret =
	    block_cli_if_track_treadp(ret, spdid, tid, len, _retval_cbuf_off,
				      _retval_sz);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}
