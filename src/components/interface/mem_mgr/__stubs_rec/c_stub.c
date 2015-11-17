/* C3 - Memory manager recovery: client stub interface -- Jiguo */

/* Issue: 1. Booter: booter calls mman_alias. however, in
   boot_spd_map_memory the init thread will call cos_get_vas_page
   which can change the heap pointer. If we keep tracking every alias
   from booter using cslab, which will allocate the new page when the
   number is greater than 4096 (see cslab.h). For simplicity, we
   assume the fault will happen after booter initialize all
   components. So we do not track alias from booter.
  */

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <valloc.h>

#include <cstub.h>

static int first = 0;  // for cvect

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

//extern int valloc_upcall(spdid_t spdid, vaddr_t addr);   // Jiguo: upcall to rebuild alias

#define vaddr2id(addr) (addr >> PAGE_SHIFT) & 0xFFF

/* global fault counter, only increase, never decrease */
static unsigned long global_fault_cnt;

/* recovery data structure for each created alias */
struct rec_data_mm {
	vaddr_t		d_addr;	// this is the tracking id
	vaddr_t		s_addr;	// this is the parent
	spdid_t		s_spd;  // this should be just cos_spd_id()
	unsigned int	d_spd_flags;
	
	unsigned int    is_parent;

	unsigned int	state;
	unsigned long	fcnt;
	
	struct rec_data_mm *next, *prev;
};

/* the state of a page object */
enum {
	PAGE_STATE_GET,  
	PAGE_STATE_ALIAS,
	PAGE_STATE_REVOKE
};

/**********************************************/
/* slab allocator and cvect for tracking pages */
/**********************************************/
CVECT_CREATE_STATIC(rec_mm_vect);
CSLAB_CREATE(rdmm, sizeof(struct rec_data_mm));

static struct rec_data_mm *
rdmm_lookup(vaddr_t addr)
{
	/* printc("cli: rdmm_lookup cvect_lookup...(addr %p)id %d\n",  */
	/*        addr, vaddr2id(addr)); */
	return cvect_lookup(&rec_mm_vect, vaddr2id(addr));
}

static struct rec_data_mm *
rdmm_alloc(vaddr_t addr)
{
	struct rec_data_mm *rd;

	/* printc("cli: rdmm_alloc cslab_alloc_rdmm....1\n"); */
	rd = cslab_alloc_rdmm();
	/* printc("cli: rdmm_alloc cslab_alloc_rdmm....2\n"); */
	assert(rd);
	
	/* printc("cli: rdmm_alloc cvect_add...(addr %p)id %d\n",  */
	/*        addr, vaddr2id(addr)); */
	if (cvect_add(&rec_mm_vect, rd, vaddr2id(addr))) assert(0);

	return rd;
}

static void
rdmm_dealloc(struct rec_data_mm *rd)
{
	assert(rd);
	
	/* printc("cli: rdmm_dealloc cvect_del...(addr %p)id %d\n",  */
	/*        rd->d_addr, vaddr2id(rd->d_addr)); */
	if (cvect_del(&rec_mm_vect, vaddr2id(rd->d_addr))) assert(0);

	cslab_free_rdmm(rd);
	
	return;
}

static void
print_rd_info(struct rec_data_mm *rd)
{
	assert(rd);
	
	printc("rd->s_spd %d\n",rd->s_spd);
	printc("rd->s_addr %p\n",rd->s_addr);
	printc("rd->d_spd_flags %p\n",rd->d_spd_flags);
	printc("rd->d_addr %p\n",rd->d_addr);
	printc("rd->fcnt %ld\n",rd->fcnt);
	printc("global_fault_cnt %ld \n",global_fault_cnt);
	
	return;
}

static void
rd_cons(struct rec_data_mm *rd, spdid_t s_spd, vaddr_t s_addr, 
	unsigned int d_spd_flags, vaddr_t d_addr, int state)
{
	assert(rd);

	rd->d_addr	= d_addr;  // this is the id for the look up
	rd->s_addr	= s_addr;  // this is the parent

	rd->s_spd	= s_spd;
	rd->d_spd_flags	= d_spd_flags;

	rd->is_parent   = 0;
	rd->state	= state;
	rd->fcnt	= global_fault_cnt;

	INIT_LIST(rd, next, prev);

	struct rec_data_mm *parent_rd = NULL;

	parent_rd  = rdmm_lookup(s_addr);
	if (!parent_rd) {
		parent_rd = rdmm_alloc(s_addr);
		assert(parent_rd);
		INIT_LIST(parent_rd, next, prev);

		parent_rd->d_addr	= d_addr;
		parent_rd->s_addr	= s_addr;  // this is the id for the look up
		parent_rd->s_spd	= s_spd;
		parent_rd->d_spd_flags	= d_spd_flags;
		parent_rd->state	= state;
		parent_rd->fcnt	        = global_fault_cnt;
	}
	
	/* printc("in spd %ld a rd is added to the list of (s_addr %p)\n",  */
	/*        cos_spd_id(), s_addr); */
	/* printc("parent rd -- \n"); */
	/* print_rd_info(parent_rd); */
	/* printc("child rd -- \n"); */
	/* print_rd_info(rd); */

	parent_rd->is_parent    = 1;
	ADD_LIST(parent_rd, rd, next, prev);

	return;
}

/* static void */
/* rd_remove(vaddr_t addr) */
/* { */
/* 	struct rec_data_mm *alias_rd = NULL; */
/* 	struct rec_data_mm *parent_rd = NULL; */

/* 	assert(addr); */
	
/* 	if ((parent_rd = rdmm_lookup(addr))) {  // there is alias from this addr */
/* 		while (!EMPTY_LIST(parent_rd, next, prev)) { */
/* 			alias_rd = FIRST_LIST(parent_rd, next, prev); */
/* 			assert(alias_rd); */
/* 			/\* printc("cli: remove alias %p\n", alias_rd->d_addr); *\/ */
/* 			REM_LIST(alias_rd, next, prev); */
/* 		} */
/* 		REM_LIST(parent_rd, next, prev);  // remove this when no alias anymore */
/* 	} */
/* 	return; */
/* } */

static struct rec_data_mm *
rd_update(vaddr_t addr, int state)
{
        struct rec_data_mm *rd = NULL;
	long ret = 0;
	
	/* printc("cli: rd_update %p\n", addr); */
	if (unlikely(!(rd = rdmm_lookup(addr)))) goto done;  // local root
	/* printc("check fcnt: rd->fcnt %d global_fault_cnt %d\n",  */
	/*        rd->fcnt, global_fault_cnt); */
	if (likely(rd->fcnt == global_fault_cnt)) goto done;
	rd->fcnt = global_fault_cnt;

	/* printc("cli: thd %d ready to replay......in spd spd %ld\n",  */
	/*        cos_get_thd_id(), cos_spd_id()); */
	/* print_rd_info(rd); */

	ret = __mman_alias_page_exist(rd->s_spd, rd->s_addr, rd->d_spd_flags, rd->d_addr);

	if (ret > 0 && ret != (long)rd->d_addr) assert(0);

	/* printc("thd %d: replay done......in spd spd %ld\n\n",  */
	/*        cos_get_thd_id(), cos_spd_id()); */

done:
	return rd;
}

static void
rd_remove(vaddr_t addr, int state)
{
	struct rec_data_mm *alias_rd;
	struct rec_data_mm *rd = NULL;
	vaddr_t s_addr;
	spdid_t d_spd;
	long ret;

	assert(addr);
	rd = rdmm_lookup(addr);
	if (!rd) return;
	if (!rd->is_parent) return;

	/* remove */
	/* printc("thd %d in spd %ld found parent addr %p (removing....)\n",  */
	/*        cos_get_thd_id(), cos_spd_id(), addr); */

	while (!EMPTY_LIST(rd, next, prev)) {
		alias_rd = FIRST_LIST(rd, next, prev);
		assert(alias_rd);
		
		/* printc("thd %d in spd %ld found child addr %p (removing.....)\n",  */
		/*        cos_get_thd_id(), cos_spd_id(), alias_rd->d_addr); */
		
		d_spd = alias_rd->d_spd_flags >> 16;
		if (d_spd != cos_spd_id()) {
			valloc_upcall(d_spd, alias_rd->d_addr, COS_UPCALL_REMOVE_SUBTREE);
		} else {
			rd_remove(alias_rd->d_addr, PAGE_STATE_ALIAS);
		}

		REM_LIST(alias_rd, next, prev);
		rdmm_dealloc(alias_rd);
	}

	/* printc("thd %d: remove subtree done......in spd spd %ld\n",  */
	/*        cos_get_thd_id(), cos_spd_id()); */

	return;
}

static void
rd_update_subtree(vaddr_t addr, int state)
{
	struct rec_data_mm *alias_rd;
	struct rec_data_mm *rd = NULL;
	vaddr_t s_addr;
	spdid_t d_spd;
	long ret;

	assert(addr);
	rd = rdmm_lookup(addr);
	if (!rd || !rd->is_parent) return;

	/* replay */
	/* printc("thd %d in spd %ld found parent addr %p\n",  */
	/*        cos_get_thd_id(), cos_spd_id(), addr); */
		
	for (alias_rd = FIRST_LIST(rd, next, prev) ;
	     alias_rd != rd;
	     alias_rd = FIRST_LIST(alias_rd, next, prev)) {

		/* printc("thd %d in spd %ld found child addr %p\n",  */
		/*        cos_get_thd_id(), cos_spd_id(), alias_rd->d_addr); */

		/* if (alias_rd->d_addr == addr) break; */
		
		ret = __mman_alias_page_exist(alias_rd->s_spd, alias_rd->s_addr, 
					      alias_rd->d_spd_flags, alias_rd->d_addr);
		if (ret > 0 && ret != (long)alias_rd->d_addr) assert(0);
		
		d_spd = alias_rd->d_spd_flags >> 16;
		if (d_spd != cos_spd_id()) {
			/* printc("thd %d in spd %ld valloc_upcall for addr %p\n",  */
			/*        cos_get_thd_id(), cos_spd_id(), alias_rd->d_addr); */
			valloc_upcall(d_spd, alias_rd->d_addr, COS_UPCALL_RECOVERY_SUBTREE);
			/* printc("thd %d in spd %ld valloc_upcall for addr %p...back\n",  */
			/*        cos_get_thd_id(), cos_spd_id(), alias_rd->d_addr); */
		} else {
			/* printc("calling rd_update_subtree again....\n"); */
			rd_update_subtree(alias_rd->d_addr, PAGE_STATE_ALIAS);
			/* printc("calling rd_update_subtree again....back\n"); */
		}

	}

	/* printc("thd %d: replay subtree done......in spd spd %ld\n",  */
	/*        cos_get_thd_id(), cos_spd_id()); */

	return;
}

void 
mm_cli_if_recover_upcall_entry(vaddr_t addr)
{
	struct rec_data_mm *rdmm = NULL;
	vaddr_t ret;
	/* printc("now we are going to recover addr %p (parent type)\n", addr); */
	assert(addr);
	rdmm = rdmm_lookup(addr);
	/* if (!(rdmm = rdmm_lookup(addr))) { */
	/* 	assert(0); */
	/* 	/\* This function is going to recreate the mapping for */
	/* 	 * the root page. Note that the root page must be in */
	/* 	 * the page table already (spdid, flags etc can be */
	/* 	 * introspected from the kernel on the server side, if */
	/* 	 * necessary, though it does not seem to need)*\/ */
	/* 	ret = mman_get_page_exist(cos_spd_id(), addr, 0); */
	/* 	if (ret != addr) assert(0); */
	/* } else { */
	/* printc("cli: found record for addr %p\n", addr); */
	assert(rdmm);
	ret = __mman_alias_page_exist(rdmm->s_spd, rdmm->s_addr, 
				      rdmm->d_spd_flags, rdmm->d_addr);
	assert(ret == rdmm->d_addr);
	/* } */
/* done: */
	return;
}


void 
mm_cli_if_recover_subtree_upcall_entry(vaddr_t addr)
{
	/* printc("now we are going to recover addr %p (subtree type)\n", addr); */
	return rd_update_subtree(addr, PAGE_STATE_ALIAS);
}

void 
mm_cli_if_remove_subtree_upcall_entry(vaddr_t addr)
{
	/* printc("now we are going to remove addr %p (subtree type)\n", addr); */
	return rd_remove(addr, PAGE_STATE_ALIAS);
}

/* this is expensive and see the comment for trade-off */
void 
mm_cli_if_recover_all_alias_upcall_entry(vaddr_t addr)
{
	/* printc("now we are going to recover addr %p (all alias type, in spd %ld)\n",  */
	/*        addr, cos_spd_id()); */
	rd_update_subtree(addr, PAGE_STATE_ALIAS);
}
/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN(vaddr_t, mman_get_page) (struct usr_inv_cap *uc,
				  spdid_t spdid, vaddr_t addr, int flags)
{
	long fault = 0;
	long ret;

        struct rec_data_mm *rd;

        if (first == 0) {
		cvect_init_static(&rec_mm_vect);
		first = 1;
	}

redo:
	/* if (cos_spd_id() != 5) { */
	/* 	printc("cli: mman_get_page spd %d addr %p\n", spdid, addr); */
	/* } */
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, addr, flags);
        if (unlikely (fault)){
		/* printc("found a fault in mman_get_page!!!!!\n"); */
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
done:
	return ret;
}

static int aaaaa= 0;
CSTUB_FN(vaddr_t, __mman_alias_page) (struct usr_inv_cap *uc,
				      spdid_t s_spd, vaddr_t s_addr, 
				      unsigned int d_spd_flags, vaddr_t d_addr)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
        struct rec_data_mm *rd, *dest;

        if (first == 0) {
		cvect_init_static(&rec_mm_vect);
		first = 1;
	}

redo:
	if (cos_spd_id() == 5) goto con;
	
	/* rd = rd_update(s_addr, PAGE_STATE_ALIAS); */
	
	/* printc("cli: mman_alias_page s_spd %d s_addr %p d_spd %d d_addr %p\n", */
	/*        s_spd, s_addr, d_spd_flags >> 16,  d_addr); */
con:	
	CSTUB_INVOKE(ret, fault, uc, 4,  s_spd, s_addr, d_spd_flags, d_addr);
        if (unlikely (fault)){
		/* printc("found a fault in mman_alias_page!!!!\n"); */
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (cos_spd_id() == 5) goto done;

	if (ret == -EINVAL) {
		valloc_upcall(s_spd, s_addr, COS_UPCALL_RECOVERY);
		goto redo;
	} else if (ret == -ECHILD) {  // s_addr is the root page
		/* printc("cli:this is funny..........in alias\n"); */
		assert(s_spd ==  cos_spd_id());
		struct rec_data_mm *rdmm = NULL;
		rdmm = rdmm_lookup(s_addr);
		assert(rdmm);
		vaddr_t tmp;
		tmp = mman_get_page_exist(cos_spd_id(), s_addr, rdmm->d_spd_flags & 0xFFFF);
		if (tmp != s_addr) assert(0);
		goto redo;
	}
	
	/* if (ret == -EINVAL) { */
	/* 	valloc_upcall(s_spd, s_addr, COS_UPCALL_RECOVERY); */
	/* 	goto redo; */
	/* } */

	
	/* printc("cli: mman_alias_page 2 (in spd %ld, ret %p)\n", cos_spd_id(), ret); */
	
	assert(ret == d_addr);
	rd = rdmm_lookup(d_addr);
	if (likely(!rd)) rd = rdmm_alloc(d_addr);
	assert(rd);
	/* printc("ABC\n"); */
	/* rd->s_spd = 0; // test */
	/* printc("XYZ\n"); */
	rd_cons(rd, s_spd, s_addr, d_spd_flags, d_addr, PAGE_STATE_ALIAS);
	/* printc("cli: mman_alias_page 3 (rd_cons)\n"); */
	/* print_rd_info(rd); */
done:	
	return ret;
}

static int bbbbb= 0;
CSTUB_FN(vaddr_t, mman_revoke_page) (struct usr_inv_cap *uc,
				     spdid_t spdid, vaddr_t addr, int flags)
{
	long fault = 0;
	long ret;
	
	struct rec_data_mm *rd = NULL;

        if (first == 0) {
		cvect_init_static(&rec_mm_vect);
		first = 1;
	}

	/* if (cos_spd_id() == 5) goto con; */
	
redo:

	/* printc("cli: mman_revoke_page 1\n"); */
	rd_update_subtree(addr, PAGE_STATE_REVOKE);
	
con:
	/* printc("cli: mman_revoke_page 2\n"); */
	CSTUB_INVOKE(ret, fault, uc, 3,  spdid, addr, flags);
        if (unlikely (fault)){
		/* printc("found a fault in mman_revoke_page!!!!\n"); */
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	/* if (cos_spd_id() == 5) goto done; */

	/* if (ret == -EINVAL) { */
	/* 	assert(0); */
	/* 	if (bbbbb++ > 5) assert(0); */
	/* 	rd_update_subtree(addr, PAGE_STATE_REVOKE); */
	/* 	goto redo; */
	/* } */


	/* here is one issue: if we always remove all tracking
	 * descriptors here, a page that was aliased (and tracked) in
	 * another component might not be removed explicitly through
	 * the revoke functoin. So we do non remove the descriptors*/

	/* printc("\ncli: mman_revoke_page %p\n", addr);	 */
	rd_remove(addr, PAGE_STATE_REVOKE);
	/* if ((rd = rdmm_lookup(addr))) rdmm_dealloc(rd); */
done:
	/* printc("cli: mman_revoke_page return %d\n", ret); */
	return ret;
}


CSTUB_FN(vaddr_t, __mman_alias_page_exist) (struct usr_inv_cap *uc,
					    spdid_t s_spd, vaddr_t s_addr,
					    unsigned int d_spd_flags, vaddr_t d_addr)
{
	long fault = 0;
	long ret;

        if (first == 0) {
		cvect_init_static(&rec_mm_vect);
		first = 1;
	}

redo:
	
	/* rd_update(s_addr, PAGE_STATE_REVOKE); */
	
	CSTUB_INVOKE(ret, fault, uc, 4,  s_spd, s_addr, d_spd_flags, d_addr);
        if (unlikely (fault)){
		/* printc("found a fault in mman_alias_page_exist!!!!\n"); */
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (ret == -EINVAL) {
		valloc_upcall(s_spd, s_addr, COS_UPCALL_RECOVERY);
		goto redo;
	} else if (ret == -ECHILD) {  // s_addr is the root page
		/* printc("this is funny..........in alias_exist\n"); */
		assert(s_spd ==  cos_spd_id());
		struct rec_data_mm *rdmm = NULL;
		rdmm = rdmm_lookup(s_addr);
		assert(rdmm);
		vaddr_t tmp;
		tmp = mman_get_page_exist(cos_spd_id(), s_addr, rdmm->d_spd_flags & 0xFFFF);
		if (tmp != s_addr) assert(0);
		goto redo;
	}


	return ret;
}
