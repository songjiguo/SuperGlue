/* IDL generated code ver 0.1 ---  Mon Nov 23 20:13:14 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <periodic_wake.h>

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
	unsigned period;
	int thdid;
	unsigned int state;
	unsigned int next_state;
	unsigned long long fault_cnt;
};

static volatile unsigned long global_fault_cnt = 0;
static volatile unsigned long last_system_ticks = 0;
static int first_map_init = 0;

CVECT_CREATE_STATIC(periodic_wake_desc_maps);
CSLAB_CREATE(periodic_wake_slab, sizeof(struct desc_track));

static inline struct desc_track *call_desc_lookup(int id)
{
	return (struct desc_track *)cvect_lookup(&periodic_wake_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc(int id)
{
	struct desc_track *desc = NULL;
	desc = cslab_alloc_periodic_wake_slab();
	assert(desc);
	desc->thdid = id;
	if (cvect_add(&periodic_wake_desc_maps, desc, id))
		assert(0);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	if (cvect_del(&periodic_wake_desc_maps, desc->thdid))
		assert(0);
	cslab_free_periodic_wake_slab(desc);
	return;
}

enum state_codes { state_periodic_wake_create, state_periodic_wake_remove,
	    state_periodic_wake_wait, state_null };

static inline struct desc_track *call_desc_update(int id, int next_state);
static inline void call_map_init();
static inline void block_cli_if_recover(int id);
static inline void block_cli_if_basic_id(int id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void call_save_data(int id, void *data);
static inline void block_cli_if_upcall_creator(int id);
static inline int block_cli_if_invoke_periodic_wake_create(spdid_t spdid,
							   unsigned period,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc);
static inline int block_cli_if_desc_update_post_fault_periodic_wake_create();
static inline int block_cli_if_track_periodic_wake_create(int ret,
							  spdid_t spdid,
							  unsigned period);
static inline void block_cli_if_desc_update_periodic_wake_create(spdid_t spdid,
								 unsigned
								 period);
static inline void call_desc_cons(struct desc_track *desc, int id,
				  spdid_t spdid, unsigned period);
static inline int block_cli_if_invoke_periodic_wake_wait(spdid_t spdid, int ret,
							 long *fault,
							 struct usr_inv_cap
							 *uc);
static inline int block_cli_if_desc_update_post_fault_periodic_wake_wait();
static inline int block_cli_if_track_periodic_wake_wait(int ret, spdid_t spdid);
static inline void block_cli_if_desc_update_periodic_wake_wait(spdid_t spdid);
static inline int block_cli_if_invoke_periodic_wake_remove(spdid_t spdid,
							   unsigned thdid,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc);
static inline void block_cli_if_recover_upcall_subtree(int id);
static inline int block_cli_if_desc_update_post_fault_periodic_wake_remove();
static inline int block_cli_if_track_periodic_wake_remove(int ret,
							  spdid_t spdid,
							  unsigned thdid);
static inline void block_cli_if_desc_update_periodic_wake_remove(spdid_t spdid,
								 unsigned
								 thdid);

static inline void call_map_init()
{
	if (unlikely(!first_map_init)) {
		first_map_init = 1;
		cvect_init_static(&periodic_wake_desc_maps);
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
				  spdid_t spdid, unsigned period)
{
	assert(desc);

	desc->spdid = spdid;
	desc->period = period;

	desc->fault_cnt = global_fault_cnt;

	return;
}

static inline void block_cli_if_basic_id(int id)
{

	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	periodic_wake_create_exist(desc->spdid, desc->period);

	desc->state = state_periodic_wake_create;	// set the state to the initial state
	desc->fault_cnt = global_fault_cnt;	// set the fault counter to the global

	block_cli_if_recover_data(desc);
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
}

static inline void block_cli_if_upcall_creator(int id)
{
}

static inline void call_save_data(int id, void *data)
{
}

static inline void block_cli_if_save_data(int id, void *data)
{
	call_save_data(id, data);
}

static inline struct desc_track *call_desc_update(int id, int next_state)
{
	struct desc_track *desc = NULL;
	unsigned int from_state = 0;
	unsigned int to_state = 0;

	if (id == 0)
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

static inline int block_cli_if_track_periodic_wake_wait(int ret, spdid_t spdid)
{
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());
	if (desc) {
	}
	return ret;
}

static inline void block_cli_if_desc_update_periodic_wake_wait(spdid_t spdid)
{
	call_desc_update(cos_get_thd_id(), state_periodic_wake_wait);
}

static inline int block_cli_if_desc_update_post_fault_periodic_wake_wait()
{

	return 1;
}

static inline int block_cli_if_invoke_periodic_wake_wait(spdid_t spdid, int ret,
							 long *fault,
							 struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());

	CSTUB_INVOKE(ret, __fault, uc, 1, spdid);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_periodic_wake_create(int ret,
							  spdid_t spdid,
							  unsigned period)
{
	if (ret == -EINVAL)
		return ret;

	struct desc_track *desc = call_desc_alloc(cos_get_thd_id());
	assert(desc);
	call_desc_cons(desc, cos_get_thd_id(), spdid, period);
	desc->state = state_periodic_wake_create;

	return ret;
}

static inline void block_cli_if_desc_update_periodic_wake_create(spdid_t spdid,
								 unsigned
								 period)
{
}

static inline int block_cli_if_desc_update_post_fault_periodic_wake_create()
{

	return 1;
}

static inline int block_cli_if_invoke_periodic_wake_create(spdid_t spdid,
							   unsigned period,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc)
{
	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, period);
	*fault = __fault;
	return ret;
}

static inline void block_cli_if_desc_update_periodic_wake_remove(spdid_t spdid,
								 unsigned thdid)
{
	call_desc_update(cos_get_thd_id(), state_periodic_wake_remove);
}

static inline void block_cli_if_recover_upcall_subtree(int id)
{
}

static inline int block_cli_if_desc_update_post_fault_periodic_wake_remove()
{

	return 1;
}

static inline int block_cli_if_invoke_periodic_wake_remove(spdid_t spdid,
							   unsigned thdid,
							   int ret, long *fault,
							   struct usr_inv_cap
							   *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(cos_get_thd_id());

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, thdid);
	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_periodic_wake_remove(int ret,
							  spdid_t spdid,
							  unsigned thdid)
{
	struct desc_track *desc = call_desc_lookup(thdid);
	if (desc)
		call_desc_dealloc(desc);

	return ret;
}

CSTUB_FN(int, periodic_wake_wait)(struct usr_inv_cap * uc, spdid_t spdid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_periodic_wake_wait(spdid);

	ret = block_cli_if_invoke_periodic_wake_wait(spdid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_periodic_wake_wait()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_periodic_wake_wait(ret, spdid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, periodic_wake_create)(struct usr_inv_cap * uc, spdid_t spdid,
				    unsigned period) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_periodic_wake_create(spdid, period);

	ret =
	    block_cli_if_invoke_periodic_wake_create(spdid, period, ret, &fault,
						     uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_periodic_wake_create()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_periodic_wake_create(ret, spdid, period);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, periodic_wake_remove)(struct usr_inv_cap * uc, spdid_t spdid,
				    unsigned thdid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_periodic_wake_remove(spdid, thdid);

	ret =
	    block_cli_if_invoke_periodic_wake_remove(spdid, thdid, ret, &fault,
						     uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_periodic_wake_remove()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_periodic_wake_remove(ret, spdid, thdid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}
