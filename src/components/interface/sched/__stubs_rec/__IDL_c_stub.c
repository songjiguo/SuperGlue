/* IDL generated code ver 0.1 ---  Mon Nov 23 20:12:02 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <sched.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct desc_track {
	int thdid;
	unsigned dependency_thd;
	unsigned int state;
	unsigned int next_state;
	unsigned long long fault_cnt;
};

static volatile unsigned long global_fault_cnt = 0;
static volatile unsigned long last_system_ticks = 0;
static int first_map_init = 0;

CVECT_CREATE_STATIC(sched_desc_maps);
CSLAB_CREATE(sched_slab, sizeof(struct desc_track));

static inline struct desc_track *call_desc_lookup(int id)
{
	return (struct desc_track *)cvect_lookup(&sched_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc(int id)
{
	struct desc_track *desc = NULL;
	desc = cslab_alloc_sched_slab();
	assert(desc);
	desc->thdid = id;
	if (cvect_add(&sched_desc_maps, desc, id))
		assert(0);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	if (cvect_del(&sched_desc_maps, desc->thdid))
		assert(0);
	cslab_free_sched_slab(desc);
	return;
}

enum state_codes { state_sched_create_thd, state_sched_block,
	    state_sched_wakeup, state_sched_component_take,
	    state_sched_component_release, state_null };

static inline struct desc_track *call_desc_update(int id, int next_state);
static inline void call_map_init();
static inline void block_cli_if_recover(int id);
static inline void block_cli_if_basic_id(int id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void block_cli_if_upcall_creator(int id);
void sched_cli_if_recover_upcall_entry(int id);
static inline int block_cli_if_invoke_sched_create_thd(spdid_t spdid,
						       u32_t sched_param0,
						       u32_t sched_param1,
						       u32_t sched_param2,
						       int ret, long *fault,
						       struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_sched_create_thd();
static inline int block_cli_if_track_sched_create_thd(int ret, spdid_t spdid,
						      u32_t sched_param0,
						      u32_t sched_param1,
						      u32_t sched_param2);
static inline void block_cli_if_desc_update_sched_create_thd(spdid_t spdid,
							     u32_t sched_param0,
							     u32_t sched_param1,
							     u32_t
							     sched_param2);
static inline void call_desc_cons(struct desc_track *desc, int id,
				  spdid_t spdid, u32_t sched_param0,
				  u32_t sched_param1, u32_t sched_param2);
static inline int block_cli_if_invoke_sched_block(spdid_t spdid,
						  unsigned dependency_thd,
						  int ret, long *fault,
						  struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_sched_block();
static inline int block_cli_if_track_sched_block(int ret, spdid_t spdid,
						 unsigned dependency_thd);
static inline void block_cli_if_desc_update_sched_block(spdid_t spdid,
							unsigned
							dependency_thd);
static inline int block_cli_if_invoke_sched_wakeup(spdid_t spdid,
						   unsigned thdid, int ret,
						   long *fault,
						   struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_sched_wakeup();
static inline int block_cli_if_track_sched_wakeup(int ret, spdid_t spdid,
						  unsigned thdid);
static inline void block_cli_if_desc_update_sched_wakeup(spdid_t spdid,
							 unsigned thdid);
static inline int block_cli_if_invoke_sched_component_take(spdid_t spdid,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc);
static inline int block_cli_if_desc_update_post_fault_sched_component_take();
static inline int block_cli_if_track_sched_component_take(int ret,
							  spdid_t spdid);
static inline void block_cli_if_desc_update_sched_component_take(spdid_t spdid);
static inline int block_cli_if_invoke_sched_component_release(spdid_t spdid,
							      int ret,
							      long *fault,
							      struct usr_inv_cap
							      *uc);
static inline int block_cli_if_desc_update_post_fault_sched_component_release();
static inline int block_cli_if_track_sched_component_release(int ret,
							     spdid_t spdid);
static inline void block_cli_if_desc_update_sched_component_release(spdid_t
								    spdid);
static inline int block_cli_if_invoke_sched_timeout(spdid_t spdid,
						    unsigned amnt, int ret,
						    long *fault,
						    struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_sched_timeout();
static inline int block_cli_if_track_sched_timeout(int ret, spdid_t spdid,
						   unsigned amnt);
static inline void block_cli_if_desc_update_sched_timeout(spdid_t spdid,
							  unsigned amnt);
static inline int block_cli_if_invoke_sched_timestamp(int ret, long *fault,
						      struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_sched_timestamp();
static inline int block_cli_if_track_sched_timestamp(int ret);
static inline void block_cli_if_desc_update_sched_timestamp();

static inline void call_map_init()
{
	if (unlikely(!first_map_init)) {
		first_map_init = 1;
		cvect_init_static(&sched_desc_maps);
	}
	return;
}

static inline void block_cli_if_recover(int id)
{
	/* spdid_t creater_component; */

	/* assert(id); */
	/* creater_component = call_introspect_creator(id); */
	/* assert(creater_component); */

	/* if (creater_component != cos_spd_id()) { */
	/*      call_recover_call_nameserver(creater_component, id, type); */
	/* } else { */
	/*      block_cli_if_basic_id(id); */
	/* } */
	block_cli_if_basic_id(id);
}

static inline void call_desc_cons(struct desc_track *desc, int id,
				  spdid_t spdid, u32_t sched_param0,
				  u32_t sched_param1, u32_t sched_param2)
{
}

static inline void block_cli_if_basic_id(int id)
{
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
	assert(desc);
}

static inline void block_cli_if_upcall_creator(int id)
{
}

void sched_cli_if_recover_upcall_entry(int id)
{
}

static inline struct desc_track *call_desc_update(int id, int next_state)
{
	struct desc_track *desc = NULL;
	unsigned int from_state = 0;
	unsigned int to_state = 0;

	desc = call_desc_lookup(cos_get_thd_id());
	if (unlikely(!desc)) {
		desc = call_desc_alloc(cos_get_thd_id());
		desc->fault_cnt = global_fault_cnt;
	}

	desc->next_state = next_state;

	if (likely(desc->fault_cnt == global_fault_cnt))
		goto done;
	desc->fault_cnt = global_fault_cnt;
 done:
	return desc;
}

static inline int block_cli_if_track_sched_timestamp(int ret)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(cos_get_thd_id(), 0);
		ret = -ELOOP;
	}

	if (last_system_ticks > ret) {
		sched_restore_ticks(last_system_ticks);
		ret = last_system_ticks;
	} else
		last_system_ticks = ret;

	return ret;
}

static inline void block_cli_if_desc_update_sched_timestamp()
{
	call_desc_update(cos_get_thd_id(), 0);
}

static inline int block_cli_if_desc_update_post_fault_sched_timestamp()
{

	return 1;
}

static inline int block_cli_if_invoke_sched_timestamp(int ret, long *fault,
						      struct usr_inv_cap *uc)
{
	long __fault = 0;

	CSTUB_INVOKE_NULL(ret, __fault, uc);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_sched_block(int ret, spdid_t spdid,
						 unsigned dependency_thd)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(cos_get_thd_id(), state_sched_block);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_sched_block(spdid_t spdid,
							unsigned dependency_thd)
{
	call_desc_update(cos_get_thd_id(), state_sched_block);
}

static inline int block_cli_if_desc_update_post_fault_sched_block()
{

	return 1;
}

static inline int block_cli_if_invoke_sched_block(spdid_t spdid,
						  unsigned dependency_thd,
						  int ret, long *fault,
						  struct usr_inv_cap *uc)
{
	long __fault = 0;

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, dependency_thd);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_sched_timeout(int ret, spdid_t spdid,
						   unsigned amnt)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(cos_get_thd_id(), 0);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_sched_timeout(spdid_t spdid,
							  unsigned amnt)
{
	call_desc_update(cos_get_thd_id(), 0);
}

static inline int block_cli_if_desc_update_post_fault_sched_timeout()
{
	sched_restore_ticks(last_system_ticks);
	return 1;
}

static inline int block_cli_if_invoke_sched_timeout(spdid_t spdid,
						    unsigned amnt, int ret,
						    long *fault,
						    struct usr_inv_cap *uc)
{
	long __fault = 0;

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, amnt);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_sched_create_thd(int ret, spdid_t spdid,
						      u32_t sched_param0,
						      u32_t sched_param1,
						      u32_t sched_param2)
{
	// if ret does not exist, just return as it is, thinking....
	if (ret == -EINVAL)
		return ret;

	struct desc_track *desc = call_desc_alloc(ret);
	assert(desc);
	call_desc_cons(desc, ret, spdid, sched_param0, sched_param1,
		       sched_param2);
	desc->state = state_sched_create_thd;

	return desc->thdid;
}

static inline void block_cli_if_desc_update_sched_create_thd(spdid_t spdid,
							     u32_t sched_param0,
							     u32_t sched_param1,
							     u32_t sched_param2)
{
}

static inline int block_cli_if_desc_update_post_fault_sched_create_thd()
{

	return 1;
}

static inline int block_cli_if_invoke_sched_create_thd(spdid_t spdid,
						       u32_t sched_param0,
						       u32_t sched_param1,
						       u32_t sched_param2,
						       int ret, long *fault,
						       struct usr_inv_cap *uc)
{
	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 4, spdid, sched_param0, sched_param1,
		     sched_param2);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_sched_component_release(int ret,
							     spdid_t spdid)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(cos_get_thd_id(),
				 state_sched_component_release);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_sched_component_release(spdid_t
								    spdid)
{
	call_desc_update(cos_get_thd_id(), state_sched_component_release);
}

static inline int block_cli_if_desc_update_post_fault_sched_component_release()
{

	return 1;
}

static inline int block_cli_if_invoke_sched_component_release(spdid_t spdid,
							      int ret,
							      long *fault,
							      struct usr_inv_cap
							      *uc)
{
	long __fault = 0;

	CSTUB_INVOKE(ret, __fault, uc, 1, spdid);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_sched_component_take(int ret,
							  spdid_t spdid)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(cos_get_thd_id(), state_sched_component_take);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_sched_component_take(spdid_t spdid)
{
	call_desc_update(cos_get_thd_id(), state_sched_component_take);
}

static inline int block_cli_if_desc_update_post_fault_sched_component_take()
{

	return 1;
}

static inline int block_cli_if_invoke_sched_component_take(spdid_t spdid,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc)
{
	long __fault = 0;

	CSTUB_INVOKE(ret, __fault, uc, 1, spdid);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_sched_wakeup(int ret, spdid_t spdid,
						  unsigned thdid)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(cos_get_thd_id(), state_sched_wakeup);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_sched_wakeup(spdid_t spdid,
							 unsigned thdid)
{
	call_desc_update(cos_get_thd_id(), state_sched_wakeup);
}

static inline int block_cli_if_desc_update_post_fault_sched_wakeup()
{
	return 1;
}

static inline int block_cli_if_invoke_sched_wakeup(spdid_t spdid,
						   unsigned thdid, int ret,
						   long *fault,
						   struct usr_inv_cap *uc)
{
	long __fault = 0;

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, thdid);
	*fault = __fault;
	return ret;
}

CSTUB_FN(unsigned long, sched_timestamp)(struct usr_inv_cap * uc) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_timestamp();

	ret = block_cli_if_invoke_sched_timestamp(ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_timestamp()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_sched_timestamp(ret);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, sched_block)(struct usr_inv_cap * uc, spdid_t spdid,
			   unsigned dependency_thd) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_block(spdid, dependency_thd);

	ret =
	    block_cli_if_invoke_sched_block(spdid, dependency_thd, ret, &fault,
					    uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_block()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_sched_block(ret, spdid, dependency_thd);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, sched_timeout)(struct usr_inv_cap * uc, spdid_t spdid,
			     unsigned amnt) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_timeout(spdid, amnt);

	ret = block_cli_if_invoke_sched_timeout(spdid, amnt, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_timeout()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_sched_timeout(ret, spdid, amnt);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, sched_create_thd)(struct usr_inv_cap * uc, spdid_t spdid,
				u32_t sched_param0, u32_t sched_param1,
				u32_t sched_param2) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_create_thd(spdid, sched_param0,
						  sched_param1, sched_param2);

	ret =
	    block_cli_if_invoke_sched_create_thd(spdid, sched_param0,
						 sched_param1, sched_param2,
						 ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_create_thd()) {
			goto redo;
		}
	}
	ret =
	    block_cli_if_track_sched_create_thd(ret, spdid, sched_param0,
						sched_param1, sched_param2);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, sched_component_release)(struct usr_inv_cap * uc, spdid_t spdid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_component_release(spdid);

	ret =
	    block_cli_if_invoke_sched_component_release(spdid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_component_release
		    ()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_sched_component_release(ret, spdid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, sched_component_take)(struct usr_inv_cap * uc, spdid_t spdid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_component_take(spdid);

	ret = block_cli_if_invoke_sched_component_take(spdid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_component_take()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_sched_component_take(ret, spdid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, sched_wakeup)(struct usr_inv_cap * uc, spdid_t spdid,
			    unsigned thdid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_sched_wakeup(spdid, thdid);

	ret = block_cli_if_invoke_sched_wakeup(spdid, thdid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_sched_wakeup()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_sched_wakeup(ret, spdid, thdid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}
