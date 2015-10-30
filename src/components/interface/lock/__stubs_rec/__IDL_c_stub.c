/* IDL generated code ver 0.1 ---  Thu Oct 29 18:17:32 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <lock.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

volatile unsigned long long ubenchmark_start, ubenchmark_end;

struct desc_track {
	spdid_t spdid;
	ul_t lock_id;
	u32_t thd_id;
	unsigned int state;
	unsigned int next_state;
	int server_lock_id;
	unsigned long long fault_cnt;
};

static volatile unsigned long global_fault_cnt = 0;

/* tracking thread state for data recovery */
//CVECT_CREATE_STATIC(rd_vect);
COS_MAP_CREATE_STATIC(lock_desc_maps);
CSLAB_CREATE(lock_slab, sizeof(struct desc_track));

enum state_codes { state_lock_component_alloc, state_lock_component_free,
	    state_lock_component_take, state_lock_component_pretake,
	    state_lock_component_release, state_null };

static inline void block_cli_if_recover(int id);
static inline void block_cli_if_basic_id(int id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void block_cli_if_save_data(int id, void *data);
static inline int block_cli_if_invoke_lock_component_alloc(spdid_t spdid,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc);

static inline void block_cli_if_desc_update_lock_component_alloc();
static inline int block_cli_if_track_lock_component_alloc(int ret,
							  spdid_t spdid);
static inline int block_cli_if_invoke_lock_component_take(spdid_t spdid,
							  ul_t lock_id,
							  u32_t thd_id, int ret,
							  long *fault,
							  struct usr_inv_cap
							  *uc);

static inline void block_cli_if_desc_update_lock_component_take(int id);
static inline int block_cli_if_track_lock_component_take(int ret, spdid_t spdid,
							 ul_t lock_id,
							 u32_t thd_id);
static inline int block_cli_if_invoke_lock_component_pretake(spdid_t spdid,
							     ul_t lock_id,
							     u32_t thd_id,
							     int ret,
							     long *fault,
							     struct usr_inv_cap
							     *uc);

static inline void block_cli_if_desc_update_lock_component_pretake(int id);
static inline int block_cli_if_track_lock_component_pretake(int ret,
							    spdid_t spdid,
							    ul_t lock_id,
							    u32_t thd_id);
static inline int block_cli_if_invoke_lock_component_release(spdid_t spdid,
							     ul_t lock_id,
							     int ret,
							     long *fault,
							     struct usr_inv_cap
							     *uc);

static inline void block_cli_if_desc_update_lock_component_release(int id);
static inline int block_cli_if_track_lock_component_release(int ret,
							    spdid_t spdid,
							    ul_t lock_id);
static inline int block_cli_if_invoke_lock_component_free(spdid_t spdid,
							  ul_t lock_id, int ret,
							  long *fault,
							  struct usr_inv_cap
							  *uc);

static inline void block_cli_if_desc_update_lock_component_free(int id);
static inline int block_cli_if_track_lock_component_free(int ret, spdid_t spdid,
							 ul_t lock_id);

static inline struct desc_track *call_desc_lookup(int id)
{
	/* return (struct desc_track *)cvect_lookup(&rd_vect, id); */
	return (struct desc_track *)cos_map_lookup(&lock_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc()
{
	struct desc_track *desc = NULL;
	int map_id = 0;

	while (1) {
		desc = cslab_alloc_lock_slab();
		assert(desc);
		map_id = cos_map_add(&lock_desc_maps, desc);
		desc->lock_id = map_id;
		desc->server_lock_id = -1;	// reset to -1
		if (map_id >= 2)
			break;
	}
	assert(desc && desc->lock_id >= 1);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	int id = desc->lock_id;
	desc->server_lock_id = -1;	// reset to -1
	assert(desc);
	cslab_free_lock_slab(desc);
	cos_map_del(&lock_desc_maps, id);
	return;
}

static inline void call_desc_cons(struct desc_track *desc, int id,
				  spdid_t spdid)
{
	assert(desc);

	desc->server_lock_id = id;

	desc->spdid = spdid;

	desc->fault_cnt = global_fault_cnt;

	return;
}

/* static inline struct desc_track *call_desc_alloc(int id) { */
/* 	struct desc_track *_desc_track; */

/* 	_desc_track = (struct desc_track *)cslab_alloc_rdservice(); */
/* 	assert(_desc_track); */
/* 	if (cvect_add(&rd_vect, _desc_track, id)) { */
/* 		assert(0); */
/* 	} */
/* 	_desc_track->lock_id = id; */
/* 	return _desc_track; */
/* } */

/* static inline void call_desc_dealloc(struct desc_track *desc) { */
/* 	assert(desc); */
/* 	assert(!cvect_del(&rd_vect, desc->lock_id)); */
/* 	cslab_free_rdservice(desc); */
/* } */

/* static inline void call_desc_cons(struct desc_track *desc, int id, spdid_t spdid) { */
/* 	assert(desc); */

/* 	desc->lock_id = id; */
/* 	desc->server_lock_id = id; */
/* 	desc->spdid=spdid;
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

	if ((from_state == state_lock_component_alloc)
	    && (to_state == state_lock_component_take)) {
		lock_component_pretake(desc->spdid, desc->lock_id,
				       desc->thd_id);

		goto done;
	}

 done:
	return desc;
}

static inline void block_cli_if_basic_id(int id)
{

	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	int retval = 0;
	retval = lock_component_alloc(desc->spdid);
	assert(retval);

	struct desc_track *new_desc = call_desc_lookup(retval);
	assert(new_desc);

	desc->server_lock_id = new_desc->server_lock_id;
	desc->state = state_lock_component_alloc;
	call_desc_dealloc(new_desc);

	block_cli_if_recover_data(desc);
}

static inline void block_cli_if_recover(int id)
{
	block_cli_if_basic_id(id);
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
}

static inline void block_cli_if_save_data(int id, void *data)
{
}

static inline int block_cli_if_track_lock_component_pretake(int ret,
							    spdid_t spdid,
							    ul_t lock_id,
							    u32_t thd_id)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);

	return ret;
}

static inline void block_cli_if_desc_update_lock_component_pretake(int id)
{
	call_desc_update(id, state_lock_component_pretake);
}

static inline int block_cli_if_invoke_lock_component_pretake(spdid_t spdid,
							     ul_t lock_id,
							     u32_t thd_id,
							     int ret,
							     long *fault,
							     struct usr_inv_cap
							     *uc)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spdid, desc->server_lock_id, thd_id);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_lock_component_release(int ret,
							    spdid_t spdid,
							    ul_t lock_id)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);

	return ret;
}

static inline void block_cli_if_desc_update_lock_component_release(int id)
{
	call_desc_update(id, state_lock_component_release);
}

static inline int block_cli_if_invoke_lock_component_release(spdid_t spdid,
							     ul_t lock_id,
							     int ret,
							     long *fault,
							     struct usr_inv_cap
							     *uc)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_lock_id);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_lock_component_take(int ret, spdid_t spdid,
							 ul_t lock_id,
							 u32_t thd_id)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);

	return ret;
}

static inline void block_cli_if_desc_update_lock_component_take(int id)
{
	call_desc_update(id, state_lock_component_take);
}

static inline int block_cli_if_invoke_lock_component_take(spdid_t spdid,
							  ul_t lock_id,
							  u32_t thd_id, int ret,
							  long *fault,
							  struct usr_inv_cap
							  *uc)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spdid, desc->server_lock_id, thd_id);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_lock_component_alloc(int ret,
							  spdid_t spdid)
{
	// if ret does not exist, just return as it is, thinking....
	if (ret == -EINVAL)
		return ret;

	struct desc_track *desc = call_desc_alloc();
	assert(desc);
	call_desc_cons(desc, ret, spdid);
	desc->state = state_lock_component_alloc;

	return desc->lock_id;
}

static inline void block_cli_if_desc_update_lock_component_alloc()
{
}

static inline int block_cli_if_invoke_lock_component_alloc(spdid_t spdid,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc)
{
	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 1, spdid);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_lock_component_free(int ret, spdid_t spdid,
							 ul_t lock_id)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);
	call_desc_dealloc(desc);

	return ret;
}

static inline void block_cli_if_desc_update_lock_component_free(int id)
{
	call_desc_update(id, state_lock_component_free);
}

static inline int block_cli_if_invoke_lock_component_free(spdid_t spdid,
							  ul_t lock_id, int ret,
							  long *fault,
							  struct usr_inv_cap
							  *uc)
{
	struct desc_track *desc = call_desc_lookup(lock_id);
	assert(desc);		// must be created in the same component

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_lock_id);
	*fault = __fault;

	return ret;
}

static int lock_component_pretake_ubenchmark_flag;
CSTUB_FN(int, lock_component_pretake) (struct usr_inv_cap * uc, spdid_t spdid,
				       ul_t lock_id, u32_t thd_id) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!lock_desc_maps.data.depth)) {
		cos_map_init_static(&lock_desc_maps);
	}

 redo:
	block_cli_if_desc_update_lock_component_pretake(lock_id);

	rdtscll(ubenchmark_end);
	if (lock_component_pretake_ubenchmark_flag) {
		lock_component_pretake_ubenchmark_flag = 0;
		printc
		    ("lock_component_pretake:recover per object end-end cost: %llu\n",
		     ubenchmark_end - ubenchmark_start);
	}

	ret =
	    block_cli_if_invoke_lock_component_pretake(spdid, lock_id, thd_id,
						       ret, &fault, uc);
	if (unlikely(fault)) {

		lock_component_pretake_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	ret =
	    block_cli_if_track_lock_component_pretake(ret, spdid, lock_id,
						      thd_id);

	return ret;
}

static int lock_component_release_ubenchmark_flag;
CSTUB_FN(int, lock_component_release) (struct usr_inv_cap * uc, spdid_t spdid,
				       ul_t lock_id) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!lock_desc_maps.data.depth)) {
		cos_map_init_static(&lock_desc_maps);
	}

	block_cli_if_desc_update_lock_component_release(lock_id);

	ret =
	    block_cli_if_invoke_lock_component_release(spdid, lock_id, ret,
						       &fault, uc);
	if (unlikely(fault)) {

		lock_component_release_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		block_cli_if_desc_update_lock_component_release(lock_id);

		rdtscll(ubenchmark_end);
		if (lock_component_release_ubenchmark_flag) {
			lock_component_release_ubenchmark_flag = 0;
			printc
			    ("lock_component_release:recover per object end-end cost: %llu\n",
			     ubenchmark_end - ubenchmark_start);
		}

	}
	ret = block_cli_if_track_lock_component_release(ret, spdid, lock_id);

	return ret;
}

static int lock_component_take_ubenchmark_flag;
CSTUB_FN(int, lock_component_take) (struct usr_inv_cap * uc, spdid_t spdid,
				    ul_t lock_id, u32_t thd_id) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!lock_desc_maps.data.depth)) {
		cos_map_init_static(&lock_desc_maps);
	}

	block_cli_if_desc_update_lock_component_take(lock_id);

	ret =
	    block_cli_if_invoke_lock_component_take(spdid, lock_id, thd_id, ret,
						    &fault, uc);

	if (unlikely(fault)) {

		lock_component_take_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		block_cli_if_desc_update_lock_component_take(lock_id);

		rdtscll(ubenchmark_end);
		if (lock_component_take_ubenchmark_flag) {
			lock_component_take_ubenchmark_flag = 0;
			printc
			    ("lock_component_take:recover per object end-end cost: %llu\n",
			     ubenchmark_end - ubenchmark_start);
		}

	}
	ret =
	    block_cli_if_track_lock_component_take(ret, spdid, lock_id, thd_id);

	return ret;
}

static int lock_component_alloc_ubenchmark_flag;
CSTUB_FN(ul_t, lock_component_alloc) (struct usr_inv_cap * uc, spdid_t spdid) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!lock_desc_maps.data.depth)) {
		cos_map_init_static(&lock_desc_maps);
	}

 redo:
	block_cli_if_desc_update_lock_component_alloc();

	rdtscll(ubenchmark_end);
	if (lock_component_alloc_ubenchmark_flag) {
		lock_component_alloc_ubenchmark_flag = 0;
		printc
		    ("lock_component_alloc:recover per object end-end cost: %llu\n",
		     ubenchmark_end - ubenchmark_start);
	}

	ret = block_cli_if_invoke_lock_component_alloc(spdid, ret, &fault, uc);
	if (unlikely(fault)) {

		lock_component_alloc_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	ret = block_cli_if_track_lock_component_alloc(ret, spdid);

	return ret;
}

static int lock_component_free_ubenchmark_flag;
CSTUB_FN(int, lock_component_free) (struct usr_inv_cap * uc, spdid_t spdid,
				    ul_t lock_id) {
	long fault = 0;
	int ret = 0;

	if (unlikely(!lock_desc_maps.data.depth)) {
		cos_map_init_static(&lock_desc_maps);
	}

 redo:
	block_cli_if_desc_update_lock_component_free(lock_id);

	rdtscll(ubenchmark_end);
	if (lock_component_free_ubenchmark_flag) {
		lock_component_free_ubenchmark_flag = 0;
		printc
		    ("lock_component_free:recover per object end-end cost: %llu\n",
		     ubenchmark_end - ubenchmark_start);
	}

	ret =
	    block_cli_if_invoke_lock_component_free(spdid, lock_id, ret, &fault,
						    uc);
	if (unlikely(fault)) {

		lock_component_free_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	ret = block_cli_if_track_lock_component_free(ret, spdid, lock_id);

	return ret;
}
