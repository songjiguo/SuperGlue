/* IDL generated code ver 0.1 ---  Wed Nov 25 18:10:57 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <mem_mgr.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct desc_track {
	spdid_t spd;
	vaddr_t s_addr;
	u32_t d_spd_flags;
	vaddr_t addr;
	unsigned int state;
	unsigned int next_state;
	unsigned long long fault_cnt;

	unsigned int is_parent;
	struct desc_track *next, *prev;

};

static volatile unsigned long global_fault_cnt = 0;
static int first_map_init = 0;

CVECT_CREATE_STATIC(mem_mgr_desc_maps);
CSLAB_CREATE(mem_mgr_slab, sizeof(struct desc_track));

static inline struct desc_track *call_desc_lookup(vaddr_t id)
{
	return (struct desc_track *)cvect_lookup(&mem_mgr_desc_maps,
						 (id >> PAGE_SHIFT) & 0xFFF);
}

static inline struct desc_track *call_desc_alloc(vaddr_t id)
{
	struct desc_track *desc = NULL;
	desc = cslab_alloc_mem_mgr_slab();
	assert(desc);
	desc->addr = id;
	if (cvect_add(&mem_mgr_desc_maps, desc, (id >> PAGE_SHIFT) & 0xFFF))
		assert(0);
	return desc;
}

static inline void call_desc_dealloc(struct desc_track *desc)
{
	assert(desc);
	if (cvect_del(&mem_mgr_desc_maps, (desc->addr >> PAGE_SHIFT) & 0xFFF))
		assert(0);
	cslab_free_mem_mgr_slab(desc);
	return;
}

enum state_codes { state_mman_get_page, state_mman_revoke_page,
	    state___mman_alias_page, state_null };

static inline struct desc_track *call_desc_update(vaddr_t id, int next_state);
static inline void call_map_init();
static inline void block_cli_if_recover(vaddr_t id);
static inline void block_cli_if_basic_id(vaddr_t id);
void mem_mgr_cli_if_remove_upcall_subtree_entry(vaddr_t id);
static inline void block_cli_if_recover_data(struct desc_track *desc);
static inline void call_save_data(vaddr_t id, void *data);
static inline void block_cli_if_upcall_creator(vaddr_t id);
void mem_mgr_cli_if_recover_upcall_subtree_entry(vaddr_t id);
static inline void block_cli_if_recover_upcall(vaddr_t id);
void mem_mgr_cli_if_recover_upcall_entry(vaddr_t id);
static inline void call_recover_call_nameserver(int dest_spd, vaddr_t id,
						int type);
static inline int block_cli_if_invoke_mman_get_page(spdid_t spd, vaddr_t s_addr,
						    int flags, int ret,
						    long *fault,
						    struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault_mman_get_page();
static inline int block_cli_if_track_mman_get_page(int ret, spdid_t spd,
						   vaddr_t s_addr, int flags);
static inline void block_cli_if_desc_update_mman_get_page(spdid_t spd,
							  vaddr_t s_addr,
							  int flags);
static inline int block_cli_if_invoke___mman_alias_page(spdid_t spd,
							vaddr_t s_addr,
							u32_t d_spd_flags,
							vaddr_t addr, int ret,
							long *fault,
							struct usr_inv_cap *uc);
static inline int block_cli_if_desc_update_post_fault___mman_alias_page();
static inline int block_cli_if_track___mman_alias_page(int ret, spdid_t spd,
						       vaddr_t s_addr,
						       u32_t d_spd_flags,
						       vaddr_t addr);
static inline void block_cli_if_desc_update___mman_alias_page(spdid_t spd,
							      vaddr_t s_addr,
							      u32_t d_spd_flags,
							      vaddr_t addr);
static inline void call_desc_cons(struct desc_track *desc, vaddr_t id,
				  spdid_t spd, vaddr_t s_addr,
				  u32_t d_spd_flags, vaddr_t addr);
static inline int block_cli_if_invoke_mman_revoke_page(spdid_t spd,
						       vaddr_t addr, int flags,
						       int ret, long *fault,
						       struct usr_inv_cap *uc);
static inline void block_cli_if_remove_desc(vaddr_t addr);
static inline void block_cli_if_recover_upcall_subtree(vaddr_t id);
static inline int block_cli_if_desc_update_post_fault_mman_revoke_page();
static inline int block_cli_if_track_mman_revoke_page(int ret, spdid_t spd,
						      vaddr_t addr, int flags);
static inline int block_cli_if_track_mman_revoke_page(int ret, spdid_t spd,
						      vaddr_t addr, int flags);
static inline void block_cli_if_desc_update_mman_revoke_page(spdid_t spd,
							     vaddr_t addr,
							     int flags);

static inline void call_map_init()
{
	if (unlikely(!first_map_init)) {
		first_map_init = 1;
		cvect_init_static(&mem_mgr_desc_maps);
	}
	return;
}

static inline void block_cli_if_recover_upcall(vaddr_t id)
{
	assert(id);
	block_cli_if_recover(id);
}

static inline void block_cli_if_recover(vaddr_t id)
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

static inline void call_desc_cons(struct desc_track *desc, vaddr_t id,
				  spdid_t spd, vaddr_t s_addr,
				  u32_t d_spd_flags, vaddr_t addr)
{

	struct desc_track *parent_desc = NULL;
	assert(desc);

	desc->spd = spd;
	desc->s_addr = s_addr;
	desc->d_spd_flags = d_spd_flags;
	desc->addr = addr;

	desc->fault_cnt = global_fault_cnt;

	/* for close subtree */
	desc->is_parent = 0;

	INIT_LIST(desc, next, prev);

	parent_desc = call_desc_lookup(s_addr);
	if (!parent_desc) {
		parent_desc = call_desc_alloc(s_addr);
		assert(parent_desc);
		INIT_LIST(parent_desc, next, prev);

		parent_desc->spd = spd;
		parent_desc->s_addr = s_addr;
		parent_desc->d_spd_flags = d_spd_flags;
		parent_desc->addr = addr;

		parent_desc->fault_cnt = global_fault_cnt;
	}

	parent_desc->is_parent = 1;
	ADD_LIST(parent_desc, desc, next, prev);

	return;
}

static inline void block_cli_if_basic_id(vaddr_t id)
{
	assert(id);
	struct desc_track *desc = call_desc_lookup(id);
	assert(desc);

	int retval = 0;
	retval =
	    __mman_alias_page(desc->spd, desc->s_addr, desc->d_spd_flags,
			      desc->addr);
	//assert(retval);

	desc->state = state_mman_get_page;	// set the state to the initial state
	desc->fault_cnt = global_fault_cnt;	// set the fault counter to the global
	block_cli_if_recover_data(desc);
}

static inline void block_cli_if_recover_data(struct desc_track *desc)
{
	assert(desc);
}

static inline void call_recover_call_nameserver(int dest_spd, vaddr_t id,
						int type)
{
	valloc_upcall(dest_spd, id, type);
}

static inline void block_cli_if_upcall_creator(vaddr_t id)
{
}

void mem_mgr_cli_if_recover_upcall_entry(vaddr_t id)
{
	block_cli_if_recover_upcall(id);
}

void mem_mgr_cli_if_recover_upcall_subtree_entry(vaddr_t id)
{
	block_cli_if_recover_upcall_subtree(id);
}

static inline void call_save_data(vaddr_t id, void *data)
{
}

static inline void block_cli_if_save_data(vaddr_t id, void *data)
{
	call_save_data(id, data);
}

static inline struct desc_track *call_desc_update(vaddr_t id, int next_state)
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

 done:
	return desc;
}

void mem_mgr_cli_if_remove_upcall_subtree_entry(vaddr_t id)
{
	block_cli_if_remove_desc(id);
}

static inline void block_cli_if_desc_update_mman_revoke_page(spdid_t spd,
							     vaddr_t addr,
							     int flags)
{
	call_desc_update(addr, state_mman_revoke_page);
	block_cli_if_recover_upcall_subtree(addr);
}

static inline void block_cli_if_recover_upcall_subtree(vaddr_t id)
{
	struct desc_track *desc = NULL;
	struct desc_track *desc_child = NULL;

	assert(id);
	desc = call_desc_lookup(id);
	if (!desc || !desc->is_parent)
		return;

	for (desc_child = FIRST_LIST(desc, next, prev);
	     desc_child != desc;
	     desc_child = FIRST_LIST(desc_child, next, prev)) {
		block_cli_if_basic_id(desc_child->addr);

		// hard-code this
		if ((desc_child->d_spd_flags >> 16) != cos_spd_id()) {
			call_recover_call_nameserver((desc_child->
						      d_spd_flags >> 16),
						     desc_child->addr,
						     COS_UPCALL_RECOVERY_SUBTREE);
		} else {
			id = desc_child->addr;
			block_cli_if_recover_upcall_subtree(id);
		}
	}
	return;
}

static inline void block_cli_if_remove_desc(vaddr_t addr)
{
	int dest_spd;
	struct desc_track *parent_desc = call_desc_lookup(addr);
	if (!parent_desc)
		goto done;
	if (!parent_desc->is_parent)
		goto done;

	struct desc_track *desc = NULL;
	while (!EMPTY_LIST(parent_desc, next, prev)) {
		desc = FIRST_LIST(parent_desc, next, prev);
		assert(desc);

		// hard-code ">> 16" now for mem_mgr
		if ((desc->d_spd_flags >> 16) != cos_spd_id()) {
			call_recover_call_nameserver((desc->d_spd_flags >> 16),
						     desc->addr,
						     COS_UPCALL_REMOVE_SUBTREE);
		} else {
			block_cli_if_remove_desc(desc->addr);
		}

		REM_LIST(desc, next, prev);
		call_desc_dealloc(desc);
	}
 done:
	return;
}

static inline int block_cli_if_desc_update_post_fault_mman_revoke_page()
{
	return 1;
}

static inline int block_cli_if_invoke_mman_revoke_page(spdid_t spd,
						       vaddr_t addr, int flags,
						       int ret, long *fault,
						       struct usr_inv_cap *uc)
{
	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spd, addr, flags);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_mman_revoke_page(int ret, spdid_t spd,
						      vaddr_t addr, int flags)
{
	block_cli_if_remove_desc(addr);
	return ret;
}

static inline int block_cli_if_track___mman_alias_page(int ret, spdid_t spd,
						       vaddr_t s_addr,
						       u32_t d_spd_flags,
						       vaddr_t addr)
{

	struct desc_track *desc = NULL;
	if (ret == -EINVAL) {
		call_recover_call_nameserver(spd, s_addr, COS_UPCALL_RECOVERY);
		ret = -ELOOP;
	} else if (ret == -ECHILD) {
		//assert(spd ==  cos_spd_id());
		desc = call_desc_lookup(s_addr);
		assert(desc);
		int tmp;
		tmp = mman_get_page_exist(spd, s_addr, 0, s_addr);
		if (tmp != s_addr)
			assert(0);
		desc->fault_cnt = global_fault_cnt;
		ret = -ELOOP;
	}

	if (!ret)
		return ret;

	desc = call_desc_lookup(addr);
	if (likely(!desc)) {
		desc = call_desc_alloc(addr);
		assert(desc);
		call_desc_cons(desc, addr, spd, s_addr, d_spd_flags, addr);
	}

	return ret;
}

static inline void block_cli_if_desc_update___mman_alias_page(spdid_t spd,
							      vaddr_t s_addr,
							      u32_t d_spd_flags,
							      vaddr_t addr)
{
}

static inline int block_cli_if_desc_update_post_fault___mman_alias_page()
{
	return 1;
}

static inline int block_cli_if_invoke___mman_alias_page(spdid_t spd,
							vaddr_t s_addr,
							u32_t d_spd_flags,
							vaddr_t addr, int ret,
							long *fault,
							struct usr_inv_cap *uc)
{
	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 4, spd, s_addr, d_spd_flags, addr);
	*fault = __fault;
	return ret;
}

static inline int block_cli_if_track_mman_get_page(int ret, spdid_t spd,
						   vaddr_t s_addr, int flags)
{
	return ret;
}

static inline void block_cli_if_desc_update_mman_get_page(spdid_t spd,
							  vaddr_t s_addr,
							  int flags)
{
	call_desc_update(s_addr, state_mman_get_page);
}

static inline int block_cli_if_desc_update_post_fault_mman_get_page()
{
	return 1;
}

static inline int block_cli_if_invoke_mman_get_page(spdid_t spd, vaddr_t s_addr,
						    int flags, int ret,
						    long *fault,
						    struct usr_inv_cap *uc)
{

	long __fault = 0;
	CSTUB_INVOKE(ret, __fault, uc, 3, spd, s_addr, flags);
	*fault = __fault;

	return ret;
}

CSTUB_FN(int, mman_revoke_page)(struct usr_inv_cap * uc, spdid_t spd,
				vaddr_t addr, int flags) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_mman_revoke_page(spd, addr, flags);

	ret =
	    block_cli_if_invoke_mman_revoke_page(spd, addr, flags, ret, &fault,
						 uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_mman_revoke_page()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_mman_revoke_page(ret, spd, addr, flags);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}

CSTUB_FN(vaddr_t, __mman_alias_page) (struct usr_inv_cap * uc, spdid_t spd,
				      vaddr_t s_addr, u32_t d_spd_flags,
				      vaddr_t addr) {
	long fault = 0;
	int ret = 0;

	call_map_init();

	if (cos_spd_id() == 5) goto con;

 redo:
	block_cli_if_desc_update___mman_alias_page(spd, s_addr, d_spd_flags,
						   addr);

con:
	ret =
	    block_cli_if_invoke___mman_alias_page(spd, s_addr, d_spd_flags,
						  addr, ret, &fault, uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault___mman_alias_page()) {
			goto redo;
		}
	}

	if (cos_spd_id() == 5) goto done;

	ret =
	    block_cli_if_track___mman_alias_page(ret, spd, s_addr, d_spd_flags,
						 addr);

	if (unlikely(ret == -ELOOP))
		goto redo;

done:
	return ret;
}

CSTUB_FN(vaddr_t, mman_get_page) (struct usr_inv_cap * uc, spdid_t spd,
				  vaddr_t s_addr, int flags) {
	long fault = 0;
	int ret = 0;

	call_map_init();

 redo:
	block_cli_if_desc_update_mman_get_page(spd, s_addr, flags);

	ret =
	    block_cli_if_invoke_mman_get_page(spd, s_addr, flags, ret, &fault,
					      uc);
	if (unlikely(fault)) {

		CSTUB_FAULT_UPDATE();
		if (block_cli_if_desc_update_post_fault_mman_get_page()) {
			goto redo;
		}
	}
	ret = block_cli_if_track_mman_get_page(ret, spd, s_addr, flags);

	if (unlikely(ret == -ELOOP))
		goto redo;

	return ret;
}
