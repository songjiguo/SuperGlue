/* IDL generated code ver 0.1 ---  Fri Nov 27 10:24:09 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <evt.h>

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
	long parent_evtid;
	int grp;
	long evtid;
	unsigned int state;
	unsigned int next_state;
	unsigned long long fault_cnt;
};

static volatile unsigned long global_fault_cnt = 0;
static int first_map_init = 0;

CVECT_CREATE_STATIC(evt_desc_maps);
CSLAB_CREATE(evt_slab, sizeof(struct desc_track));

static inline struct desc_track *call_desc_lookup(long id)
{
	return (struct desc_track *)cvect_lookup(&evt_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc(long id)
{
	struct desc_track *desc = NULL;
	desc = cslab_alloc_evt_slab();
	assert(desc);
	desc->evtid = id;
	if (cvect_add(&evt_desc_maps, desc, id))
		assert(0);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	if (cvect_del(&evt_desc_maps, desc->evtid))
		assert(0);
	cslab_free_evt_slab(desc);
	return;
}

enum state_codes { state_evt_split, state_evt_free, state_evt_wait,
	    state_evt_trigger, state_null };

static inline struct desc_track *call_desc_update(long id, int next_state);
static inline void call_map_init();
static inline void block_cli_if_recover(long id);
static inline void block_cli_if_basic_id(long id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void call_save_data(long id, void *data);
static inline void block_cli_if_upcall_creator(long id);
static inline void block_cli_if_recover_upcall(long id);
void evt_cli_if_recover_upcall_entry(long id);
static inline int block_cli_if_invoke_evt_split(spdid_t spdid,
						long parent_evtid, int grp,
						int ret, long *fault,
						struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_evt_split();
static inline int block_cli_if_track_evt_split(int ret, spdid_t spdid,
					       long parent_evtid, int grp);
static inline void block_cli_if_desc_update_evt_split(spdid_t spdid,
						      long parent_evtid,
						      int grp);
static inline void call_desc_cons(struct desc_track *desc, long id,
				  spdid_t spdid, long parent_evtid, int grp);
static inline int block_cli_if_invoke_evt_wait(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_evt_wait();
static inline int block_cli_if_track_evt_wait(int ret, spdid_t spdid,
					      long evtid);
static inline void block_cli_if_desc_update_evt_wait(spdid_t spdid, long evtid);
static inline int block_cli_if_invoke_evt_trigger(spdid_t spdid, long evtid,
						  int ret, long *fault,
						  struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_evt_trigger();
static inline int block_cli_if_desc_update_post_fault_evt_trigger();
static inline int block_cli_if_track_evt_trigger(int ret, spdid_t spdid,
						 long evtid);
static inline void block_cli_if_desc_update_evt_trigger(spdid_t spdid,
							long evtid);
static inline int block_cli_if_invoke_evt_free(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc);
static inline void block_cli_if_recover_upcall_subtree(long id);
static inline int block_cli_if_desc_update_post_fault_evt_free();
static inline int block_cli_if_track_evt_free(int ret, spdid_t spdid,
					      long evtid);
static inline void block_cli_if_desc_update_evt_free(spdid_t spdid, long evtid);

static inline void call_map_init()
{
	if (unlikely(!first_map_init)) {
		first_map_init = 1;
		cvect_init_static(&evt_desc_maps);
	}
	return;
}

static inline void block_cli_if_recover_upcall(long id)
{
	assert(id);
	block_cli_if_recover(id);
}

static inline void block_cli_if_recover(long id)
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

static inline void call_desc_cons(struct desc_track *desc, long id,
				  spdid_t spdid, long parent_evtid, int grp)
{
	assert(desc);

	desc->spdid = spdid;
	desc->parent_evtid = parent_evtid;
	desc->grp = grp;

	desc->fault_cnt = global_fault_cnt;

	return;
}

static inline void block_cli_if_basic_id(long id)
{
	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	int retval = 0;
 again:
	retval =
	    evt_split_exist(desc->spdid, desc->parent_evtid, desc->grp, id);
	if (retval == -EINVAL) {
		id = desc->parent_evtid;
		call_desc_update(id, state_evt_split);
		goto again;
	}

	assert(retval);
	desc->state = state_evt_split;	// set the state to the initial state
	desc->fault_cnt = global_fault_cnt;	// set the fault counter to the global
	block_cli_if_recover_data(desc);
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
}

static inline void block_cli_if_upcall_creator(long id)
{
	evt_upcall_creator(cos_spd_id(), id);
}

void evt_cli_if_recover_upcall_entry(long id)
{
	block_cli_if_recover_upcall(id);
}

static inline void call_save_data(long id, void *data)
{
}

static inline void block_cli_if_save_data(long id, void *data)
{
	call_save_data(id, data);
}

static inline struct desc_track *call_desc_update(long id, int next_state)
{
	struct desc_track *desc = NULL;
	unsigned int from_state = 0;
	unsigned int to_state = 0;

	/* reach the root id */
	if (id == 0) {
		return NULL;
	}

	desc = call_desc_lookup(id);
	if (unlikely(!desc)) {
		block_cli_if_upcall_creator(id);
		goto done;
	}

	desc->next_state = next_state;

	if (likely(desc->fault_cnt == global_fault_cnt))
		goto done;
	desc->fault_cnt = global_fault_cnt;

	// State machine transition under the fault
	block_cli_if_recover(id);
	from_state = desc->state;
	to_state = next_state;

	if ((from_state == state_evt_split) && (to_state == state_evt_trigger)) {
		evt_wait(desc->spdid, desc->evtid);

		goto done;
	}

 done:
	return desc;
}

static inline int block_cli_if_track_evt_wait(int ret, spdid_t spdid,
					      long evtid)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(evtid, state_evt_wait);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_evt_wait(spdid_t spdid, long evtid)
{

}

static inline int block_cli_if_desc_update_post_fault_evt_wait()
{
	return 1;
}

static inline int block_cli_if_invoke_evt_wait(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(evtid);

	//if (desc) {  // might be created in the same component
	//      CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	//} else {    // might be created in different component

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	if (ret == -1) {	// desc not exist  TODO: change to error code
		block_cli_if_recover(evtid);	// need upcall
		assert((desc = call_desc_lookup(evtid)));
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	}

	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_evt_split(int ret, spdid_t spdid,
					       long parent_evtid, int grp)
{
	// if ret does not exist, just return as it is, thinking....
	if (ret == -EINVAL)
		return ret;

	struct desc_track *desc = call_desc_alloc(ret);
	assert(desc);
	call_desc_cons(desc, ret, spdid, parent_evtid, grp);
	desc->state = state_evt_split;

	return desc->evtid;
}

static inline void block_cli_if_desc_update_evt_split(spdid_t spdid,
						      long parent_evtid,
						      int grp)
{
	call_desc_update(parent_evtid, state_evt_split);
}

static inline int block_cli_if_desc_update_post_fault_evt_split()
{
	return 1;
}

static inline int block_cli_if_invoke_evt_split(spdid_t spdid,
						long parent_evtid, int grp,
						int ret, long *fault,
						struct usr_inv_cap *uc)
{
	struct desc_track *parent_desc = NULL;
	if ((parent_evtid > 1)
	    && (parent_desc = call_desc_lookup(parent_evtid))) {
		parent_evtid = parent_desc->evtid;
	}

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spdid, parent_evtid, grp);
	*fault = __fault;

	return ret;
}

static inline void block_cli_if_desc_update_evt_free(spdid_t spdid, long evtid)
{
	block_cli_if_recover_upcall_subtree(evtid);
}

static inline void block_cli_if_recover_upcall_subtree(long id)
{
}

static inline int block_cli_if_desc_update_post_fault_evt_free()
{
	return 1;
}

static inline int block_cli_if_invoke_evt_free(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(evtid);

	//if (desc) {  // might be created in the same component
	//      CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	//} else {    // might be created in different component

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	if (ret == -1) {	// desc not exist  TODO: change to error code
		block_cli_if_recover(evtid);	// need upcall
		assert((desc = call_desc_lookup(evtid)));
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	}

	*fault = __fault;

	return ret;
}

static inline int block_cli_if_track_evt_free(int ret, spdid_t spdid,
					      long evtid)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	if (desc)
		call_desc_dealloc(desc);

	return ret;
}

static inline int block_cli_if_track_evt_trigger(int ret, spdid_t spdid,
						 long evtid)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	if (desc) {
	}
	//testsetset
	if (ret == -EINVAL) {
		call_desc_update(evtid, state_evt_trigger);
		ret = -ELOOP;
	}

	return ret;
}

static inline void block_cli_if_desc_update_evt_trigger(spdid_t spdid,
							long evtid)
{

}

static inline int block_cli_if_desc_update_post_fault_evt_trigger()
{
	return 1;
}

static inline int block_cli_if_invoke_evt_trigger(spdid_t spdid, long evtid,
						  int ret, long *fault,
						  struct usr_inv_cap *uc)
{
	long __fault = 0;
	struct desc_track *desc = call_desc_lookup(evtid);

	//if (desc) {  // might be created in the same component
	//      CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	//} else {    // might be created in different component

	CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	if (ret == -1) {	// desc not exist  TODO: change to error code
		block_cli_if_recover(evtid);	// need upcall
		assert((desc = call_desc_lookup(evtid)));
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
	}

	*fault = __fault;

	return ret;
}

CSTUB_FN(long, evt_wait)(struct usr_inv_cap * uc, spdid_t spdid, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_evt_wait(spdid, evtid);

	ret = block_cli_if_invoke_evt_wait(spdid, evtid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_wait()) {
			goto redo;
		}
	}

	ret = block_cli_if_track_evt_wait(ret, spdid, evtid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(long, evt_split)(struct usr_inv_cap * uc, spdid_t spdid,
			  long parent_evtid, int grp) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_evt_split(spdid, parent_evtid, grp);

	ret =
	    block_cli_if_invoke_evt_split(spdid, parent_evtid, grp, ret, &fault,
					  uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_split()) {
			goto redo;
		}
	}

	ret = block_cli_if_track_evt_split(ret, spdid, parent_evtid, grp);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, evt_free)(struct usr_inv_cap * uc, spdid_t spdid, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_evt_free(spdid, evtid);

	ret = block_cli_if_invoke_evt_free(spdid, evtid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_free()) {
			goto redo;
		}
	}

	ret = block_cli_if_track_evt_free(ret, spdid, evtid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(int, evt_trigger)(struct usr_inv_cap * uc, spdid_t spdid, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_evt_trigger(spdid, evtid);

	ret = block_cli_if_invoke_evt_trigger(spdid, evtid, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_trigger()) {
			goto redo;
		}
	}

	ret = block_cli_if_track_evt_trigger(ret, spdid, evtid);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}
