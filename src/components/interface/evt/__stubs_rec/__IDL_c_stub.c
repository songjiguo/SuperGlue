/* IDL generated code ver 0.1 ---  Thu Nov  5 12:52:22 2015 */

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

volatile unsigned long long ubenchmark_start, ubenchmark_end;

struct desc_track {
	spdid_t spdid;
	long parent_evtid;
	int grp;
	long evtid;
	unsigned int state;
	unsigned int next_state;
	int server_evtid;
	unsigned long long fault_cnt;
};

static volatile unsigned long global_fault_cnt = 0;

static int first_map_init = 0;

CVECT_CREATE_STATIC(evt_desc_maps);
CSLAB_CREATE(evt_slab, sizeof(struct desc_track));

static inline struct desc_track *call_desc_lookup(int id)
{
	return (struct desc_track *)cvect_lookup(&evt_desc_maps, id);
}

static inline struct desc_track *call_desc_alloc(int id)
{
	struct desc_track *desc = NULL;
	desc = cslab_alloc_evt_slab();
	assert(desc);
	desc->evtid = id;
	cvect_add(&evt_desc_maps, desc, id);
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

static inline void call_map_init();
static inline void block_cli_if_upcall_creator(int id);
static inline void block_cli_if_recover(int id);
static inline void block_cli_if_basic_id(int id);
static inline void block_cli_if_recover_upcall(int id);
void evt_cli_if_recover_upcall_entry(int id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void call_save_data(int id, void *data);
static inline int block_cli_if_invoke_evt_split(spdid_t spdid,
						long parent_evtid, int grp,
						int ret, long *fault,
						struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_pre_evt_split(int id);
static inline int block_cli_if_desc_update_post_fault_evt_split(int id);
static inline int block_cli_if_track_evt_split(int ret, spdid_t spdid,
					       long parent_evtid, int grp);
static inline int block_cli_if_invoke_evt_wait(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_pre_evt_wait();
static inline int block_cli_if_desc_update_post_fault_evt_wait(int id);
static inline int block_cli_if_track_evt_wait(int ret, spdid_t spdid,
					      long evtid);
static inline int block_cli_if_invoke_evt_trigger(spdid_t spdid, long evtid,
						  int ret, long *fault,
						  struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_pre_evt_trigger();
static inline int block_cli_if_desc_update_post_fault_evt_trigger(int id);
static inline int block_cli_if_track_evt_trigger(int ret, spdid_t spdid,
						 long evtid);
static inline int block_cli_if_invoke_evt_free(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc);

static inline void block_cli_if_desc_update_pre_evt_free(int id);
static inline int block_cli_if_desc_update_post_fault_evt_free(int id);
static inline void block_cli_if_recover_subtree(int id);
static inline int block_cli_if_track_evt_free(int ret, spdid_t spdid,
					      long evtid);

static inline void call_desc_cons(struct desc_track *desc, int id,
				  spdid_t spdid, long parent_evtid, int grp)
{
	assert(desc);

	desc->server_evtid = id;

	desc->spdid = spdid;
	desc->parent_evtid = parent_evtid;
	desc->grp = grp;

	desc->fault_cnt = global_fault_cnt;

	return;
}

static inline struct desc_track *call_desc_update(int id, int next_state)
{
	struct desc_track *desc = NULL;
	unsigned int from_state = 0;
	unsigned int to_state = 0;

	if (id == 0)
		return NULL;	/* root id */

	desc = call_desc_lookup(id);
	if (unlikely(!desc)) {
		printc("cli: call_desc_update calling upcall creator\n");
		block_cli_if_upcall_creator(id);
		goto done;
	}

	desc->next_state = next_state;

	if (likely(desc->fault_cnt == global_fault_cnt))
		goto done;
	/* desc->fault_cnt = global_fault_cnt; */

	// State machine transition under the fault
	block_cli_if_recover(id);
	from_state = desc->state;
	to_state = next_state;

 done:
	return desc;
}

static inline void call_map_init()
{
	if (unlikely(!first_map_init)) {
		first_map_init = 1;
		cvect_init_static(&evt_desc_maps);
	}
	return;
}

static inline void block_cli_if_recover_upcall(int id)
{
	assert(id);
	block_cli_if_recover(id);
	block_cli_if_recover_subtree(id);
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
}

static inline void block_cli_if_basic_id(int id)
{
	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	int retval = 0;
 again:
	retval =
	    evt_split_exist(desc->spdid, desc->parent_evtid, desc->grp,
			    desc->server_evtid);
	//TODO: define the error code for non-recovered parent
	// thinking...2222
	if (retval == -EINVAL) {
		id = desc->parent_evtid;
		printc("1\n");
		call_desc_update(id, state_evt_split);
		goto again;
	}

	assert(retval);
	desc->state = state_evt_split;	// set the state to the initial state
	desc->fault_cnt = global_fault_cnt;	// set the fault counter to the global
	block_cli_if_recover_data(desc);
}

static inline void block_cli_if_recover(int id)
{
	/* spdid_t creater_component; */

	/* assert(id); */
	/* creater_component = call_introspect_creator(id); */
	/* assert(creater_component); */

	/* if (creater_component != cos_spd_id()) { */
	/*      call_recover_upcall(creater_component, id); */
	/* } else { */
	/*      block_cli_if_basic_id(id); */
	/* } */
	block_cli_if_basic_id(id);
}

static inline void block_cli_if_upcall_creator(int id)
{
	evt_upcall_creator(cos_spd_id(), id);
}

void evt_cli_if_recover_upcall_entry(int id)
{
	block_cli_if_recover_upcall(id);
}

static inline void call_save_data(int id, void *data)
{
}

static inline void block_cli_if_save_data(int id, void *data)
{
	call_save_data(id, data);
}

static inline void block_cli_if_desc_update_pre_evt_wait()
{
}

static inline int block_cli_if_track_evt_wait(int ret, spdid_t spdid,
					      long evtid)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	if (desc) {
	}

	return ret;
}

static inline int block_cli_if_desc_update_post_fault_evt_wait(int id)
{
	printc("3\n");
	call_desc_update(id, state_evt_wait);
	return 0;
}

static inline int block_cli_if_invoke_evt_wait(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	long __fault = 0;
	if (desc) {		// might be created in the same component
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_evtid);
	} else {		// might be created in different component
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
		if (ret == -1) {	// desc not exist  TODO: change to error code
			block_cli_if_recover(evtid);	// need upcall
			assert((desc = call_desc_lookup(evtid)));
			CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
		}
	}
	*fault = __fault;

	return ret;
}

static inline void block_cli_if_desc_update_pre_evt_split(int id)
{
	call_desc_update(id, state_evt_split);
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

static inline int block_cli_if_desc_update_post_fault_evt_split(int id)
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
		parent_evtid = parent_desc->server_evtid;
	}

	/* else {         // td_root, or in a different component */
	/*     parent_evtid = parent_evtid; */
	/* } */
	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spdid, parent_evtid, grp);
	*fault = __fault;

	return ret;
}

static inline void block_cli_if_desc_update_pre_evt_free(int id)
{
	call_desc_update(id, state_evt_free);
}

static inline void block_cli_if_recover_subtree(int id)
{
}

static inline int block_cli_if_desc_update_post_fault_evt_free(int id)
{
	return 1;
}

static inline int block_cli_if_invoke_evt_free(spdid_t spdid, long evtid,
					       int ret, long *fault,
					       struct usr_inv_cap *uc)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	long __fault = 0;
	if (desc) {		// might be created in the same component
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_evtid);
	} else {		// might be created in different component
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
		if (ret == -1) {	// desc not exist  TODO: change to error code
			block_cli_if_recover(evtid);	// need upcall
			assert((desc = call_desc_lookup(evtid)));
			CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
		}
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

static inline void block_cli_if_desc_update_pre_evt_trigger()
{
}

static inline int block_cli_if_track_evt_trigger(int ret, spdid_t spdid,
						 long evtid)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	if (desc) {
	}

	if (ret == -EINVAL) {
		printc("this is a test\n");
		call_desc_update(evtid, state_evt_trigger);
		ret = -ELOOP;
	}

	return ret;
}

static inline int block_cli_if_desc_update_post_fault_evt_trigger(int id)
{
	printc("2\n");
	call_desc_update(id, state_evt_trigger);
	return 0;
}

static inline int block_cli_if_invoke_evt_trigger(spdid_t spdid, long evtid,
						  int ret, long *fault,
						  struct usr_inv_cap *uc)
{
	struct desc_track *desc = call_desc_lookup(evtid);
	long __fault = 0;
	if (desc) {		// might be created in the same component
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, desc->server_evtid);
	} else {		// might be created in different component
		CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
		if (ret == -1) {	// desc not exist  TODO: change to error code
			block_cli_if_recover(evtid);	// need upcall
			assert((desc = call_desc_lookup(evtid)));
			CSTUB_INVOKE(ret, __fault, uc, 2, spdid, evtid);
		}
	}
	*fault = __fault;

	return ret;
}

static int evt_wait_ubenchmark_flag;
CSTUB_FN(long, evt_wait) (struct usr_inv_cap * uc, spdid_t spdid, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_pre_evt_wait(evtid);

	rdtscll(ubenchmark_end);
	if (evt_wait_ubenchmark_flag) {
		evt_wait_ubenchmark_flag = 0;
		printc("evt_wait:recover per object end-end cost: %llu\n",
		       ubenchmark_end - ubenchmark_start);
	}

	ret = block_cli_if_invoke_evt_wait(spdid, evtid, ret, &fault, uc);
	if (unlikely(fault)) {

		evt_wait_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_wait(evtid)) {
			goto redo;
		}
	}
	ret = block_cli_if_track_evt_wait(ret, spdid, evtid);

	return ret;
}

static int evt_split_ubenchmark_flag;
CSTUB_FN(long, evt_split) (struct usr_inv_cap * uc, spdid_t spdid,
			   long parent_evtid, int grp) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_pre_evt_split(parent_evtid);

	rdtscll(ubenchmark_end);
	if (evt_split_ubenchmark_flag) {
		evt_split_ubenchmark_flag = 0;
		printc("evt_split:recover per object end-end cost: %llu\n",
		       ubenchmark_end - ubenchmark_start);
	}

	ret =
	    block_cli_if_invoke_evt_split(spdid, parent_evtid, grp, ret, &fault,
					  uc);
	if (unlikely(fault)) {

		evt_split_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_split(parent_evtid)) {
			goto redo;
		}
	}
	ret = block_cli_if_track_evt_split(ret, spdid, parent_evtid, grp);

	return ret;
}

static int evt_free_ubenchmark_flag;
CSTUB_FN(int, evt_free) (struct usr_inv_cap * uc, spdid_t spdid, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_pre_evt_free(evtid);

	rdtscll(ubenchmark_end);
	if (evt_free_ubenchmark_flag) {
		evt_free_ubenchmark_flag = 0;
		printc("evt_free:recover per object end-end cost: %llu\n",
		       ubenchmark_end - ubenchmark_start);
	}

	ret = block_cli_if_invoke_evt_free(spdid, evtid, ret, &fault, uc);
	if (unlikely(fault)) {

		evt_free_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_free(evtid)) {
			goto redo;
		}
	}
	ret = block_cli_if_track_evt_free(ret, spdid, evtid);

	return ret;
}

static int evt_trigger_ubenchmark_flag;
CSTUB_FN(int, evt_trigger) (struct usr_inv_cap * uc, spdid_t spdid, long evtid) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_pre_evt_trigger(evtid);

	rdtscll(ubenchmark_end);
	if (evt_trigger_ubenchmark_flag) {
		evt_trigger_ubenchmark_flag = 0;
		printc("evt_trigger:recover per object end-end cost: %llu\n",
		       ubenchmark_end - ubenchmark_start);
	}

	ret = block_cli_if_invoke_evt_trigger(spdid, evtid, ret, &fault, uc);
	if (unlikely(fault)) {

		evt_trigger_ubenchmark_flag = 1;
		rdtscll(ubenchmark_start);

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_evt_trigger(evtid)) {
			goto redo;
		}
	}
	ret = block_cli_if_track_evt_trigger(ret, spdid, evtid);

	if (unlikely(ret == -ELOOP)) {
		printc("ELOOP..... go to do it again\n");
		goto redo;
	}

	return ret;
}
