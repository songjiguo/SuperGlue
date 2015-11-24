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

#include <mem_mgr_large.h>
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
	vaddr_t d_addr;      // this is the tracking id
	vaddr_t s_addr;      // this is the parent

	spdid_t s_spd;      // this should be just cos_spd_id()
	unsigned int d_spd_flags;

	unsigned int  state;
	unsigned long fcnt;
	
	struct rec_data_mm *next, *prev;
};

/* recovery data structure for subtree tracking of a parent */
struct parent_rec_data_mm {
	vaddr_t addr;
	struct rec_data_mm *head;
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
CVECT_CREATE_STATIC(parent_rec_mm_vect);
/* cvect_t *parent_rec_mm_vect; */
CSLAB_CREATE(parent_rdmm, sizeof(struct parent_rec_data_mm));

static struct parent_rec_data_mm *
parent_rdmm_lookup(vaddr_t addr)
{
	return cvect_lookup(&parent_rec_mm_vect, vaddr2id(addr));
}

static struct parent_rec_data_mm *
parent_rdmm_alloc(vaddr_t addr)
{
	struct parent_rec_data_mm *rd;

	printc("cli: parent_rdmm_alloc cslab_alloc_parent_rdmm...1\n");
	rd = cslab_alloc_parent_rdmm();
	printc("cli: parent_rdmm_alloc cslab_alloc_parent_rdmm...2\n");
	assert(rd);
	rd->addr = addr;

	if (cvect_add(&parent_rec_mm_vect, rd, vaddr2id(addr))) assert(0);

	return rd;
}

static void
parent_rdmm_dealloc(struct parent_rec_data_mm *rd)
{
	assert(rd);

	if (cvect_del(&parent_rec_mm_vect, vaddr2id(rd->addr))) assert(0);
	cslab_free_parent_rdmm(rd);
	
	return;
}

/* cvect_t *rec_mm_vect; */
CVECT_CREATE_STATIC(rec_mm_vect);
CSLAB_CREATE(rdmm, sizeof(struct rec_data_mm));

static struct rec_data_mm *
rdmm_lookup(vaddr_t addr)
{
	return cvect_lookup(&rec_mm_vect, vaddr2id(addr));
}

static struct rec_data_mm *
rdmm_alloc(vaddr_t addr)
{
	struct rec_data_mm *rd;

	printc("cli: rdmm_alloc cslab_alloc_rdmm....1\n");
	rd = cslab_alloc_rdmm();
	printc("cli: rdmm_alloc cslab_alloc_rdmm....2\n");
	assert(rd);

	if (cvect_add(&rec_mm_vect, rd, vaddr2id(addr))) assert(0);

	return rd;
}

static void
rdmm_dealloc(struct rec_data_mm *rd)
{
	assert(rd);

	if (cvect_del(&rec_mm_vect, vaddr2id(rd->d_addr))) assert(0);
	cslab_free_rdmm(rd);
	
	return;
}

/* void */
/* print_rd_info(struct rec_data_mm *rd) */
/* { */
/* 	assert(rd); */
	
/* 	printc("rd->s_spd %d\n",rd->s_spd); */
/* 	printc("rd->s_addr %d\n",rd->s_addr); */
/* 	printc("rd->d_spd_flags %p\n",rd->d_spd_flags); */
/* 	printc("rd->d_addr %d\n",rd->d_addr); */
/* 	printc("rd->fcnt %ld\n",rd->fcnt); */
/* 	printc("global_fault_cnt %ld \n",global_fault_cnt); */
	
/* 	return; */
/* } */

static void
rd_cons(struct rec_data_mm *rd, spdid_t s_spd, vaddr_t s_addr, 
	unsigned int d_spd_flags, vaddr_t d_addr, int state)
{
	assert(rd);

	rd->d_addr	= d_addr;  // this is the id for the look up
	rd->s_addr	= s_addr;  // this is the parent

	rd->s_spd	= s_spd;
	rd->d_spd_flags	= d_spd_flags;

	rd->state	= state;
	rd->fcnt	= global_fault_cnt;

	INIT_LIST(rd, next, prev);

	return;
}

static void
parent_rd_cons(struct rec_data_mm *rd, vaddr_t s_addr)
{
	assert(rd && s_addr);

	struct parent_rec_data_mm *parent_rd = NULL;
	parent_rd  = parent_rdmm_lookup(s_addr);
	if (!parent_rd) {
		parent_rd = parent_rdmm_alloc(s_addr);
		assert(parent_rd);
		parent_rd->head = rd;
	} else {
		assert(parent_rd && parent_rd->head);
		ADD_LIST(parent_rd->head, rd, next, prev);
	}

	return;
}

static struct rec_data_mm *
rd_update(vaddr_t addr, int state)
{
        struct rec_data_mm *rd = NULL;
	long ret = 0;

	if (unlikely(!(rd = rdmm_lookup(addr)))) goto done;  // local root
	if (likely(rd->fcnt == global_fault_cnt)) goto done;
	rd->fcnt = global_fault_cnt;

	printc("thd %d: ready to replay......in spd spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());
	
again:	
	ret = __mman_alias_page_exist(rd->s_spd, rd->s_addr, rd->d_spd_flags, rd->d_addr);
	if (ret == -EINVAL) {
		rd_update(rd->s_addr, PAGE_STATE_ALIAS);
		goto again;
	} else if (ret > 0 && ret != (long)rd->d_addr) assert(0);

	printc("thd %d: replay done......in spd spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

done:
	return rd;
}

static int
rd_recover_child(struct rec_data_mm *rd)
{
	spdid_t d_spd;
	int flags;
	long ret;

	assert(rd && rd->s_spd == cos_spd_id());

	ret = __mman_alias_page_exist(rd->s_spd, rd->s_addr, rd->d_spd_flags, rd->d_addr);
	if (ret > 0 && ret != (long)rd->d_addr) assert(0);
	
	d_spd = rd->d_spd_flags >> 16;
	if (d_spd != cos_spd_id()) {
		valloc_upcall(d_spd, rd->d_addr);
	}
	
	return 0;
}

static void
rd_update_subtree(vaddr_t addr, int state)
{
	struct rec_data_mm *head, *alias_rd;
	struct parent_rec_data_mm *parent_rd;
	vaddr_t s_addr;
	
	printc("thd %d: ready to replay subtree......in spd spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	/* There is no need to remove the entries from the list here,
	 * since the revoke function on the normal path will remove
	 * them anyway */
	parent_rd = parent_rdmm_lookup(addr);
	if (parent_rd && (head = parent_rd->head)) {  // there is alias from this addr
		rd_recover_child(head);
		for (alias_rd = FIRST_LIST(head, next, prev) ; 
		     alias_rd != head ; 
		     alias_rd = FIRST_LIST(alias_rd, next, prev)) {
			printc("interface recovery: replay alias alias_rd->d_addr %p\n",
			       (void *)alias_rd->d_addr);
			rd_recover_child(alias_rd);
		}
	}

	printc("thd %d: replay subtree done......in spd spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	return;
}

static void
rd_remove(vaddr_t addr)
{
	struct rec_data_mm *head = NULL, *alias_rd = NULL;
	struct parent_rec_data_mm *parent_rd = NULL;

	assert(addr);
	
	parent_rd = parent_rdmm_lookup(addr);
	if (parent_rd && (head = parent_rd->head)) {  // there is alias from this addr
		while (!EMPTY_LIST(head, next, prev)) {
			alias_rd = FIRST_LIST(head, next, prev);
			assert(alias_rd);
			/* printc("cli: remove alias %p\n", alias_rd->d_addr); */
			REM_LIST(alias_rd, next, prev);
		}
	}
	return;
}

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN(vaddr_t, mman_get_page) (struct usr_inv_cap *uc,
				  spdid_t spdid, vaddr_t addr, int flags)
{
	long fault = 0;
	long ret;

        if (first == 0 && cos_spd_id() != 5) {
		cvect_init_static(&rec_mm_vect);
		cvect_init_static(&parent_rec_mm_vect);
		first = 1;
	}

	unsigned long long start, end;
redo:
	if (cos_spd_id() != 5) {
		printc("cli: (spd %ld) call mman_get_page addr %p\n", 
		       cos_spd_id(), addr);
	}
	
	CSTUB_INVOKE(ret, fault, uc, 3,  spdid,addr, flags);
        if (unlikely (fault)){
		printc("found a fault in mman_get_page!!!!!\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	if (cos_spd_id() != 5) {
		printc("cli: (in spd %ld) mman_get_page return ret %p\n",
		       cos_spd_id(), ret);
	}
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
        struct rec_data_mm *rd, *dest, *parent_rd;

        if (first == 0 && cos_spd_id() != 5) {
		cvect_init_static(&rec_mm_vect);
		cvect_init_static(&parent_rec_mm_vect);
		first = 1;
	}

redo:
	if (cos_spd_id() == 5) goto con;
	
	rd = rd_update(s_addr, PAGE_STATE_REVOKE);

	if (cos_spd_id() != 5) {
		printc("cli: mman_alias_page s_spd %d s_addr %p d_spd %d d_addr %p\n",
		       s_spd, s_addr, d_spd_flags >> 16,  d_addr);
	}
con:	
	CSTUB_INVOKE(ret, fault, uc, 4,  s_spd, s_addr, d_spd_flags, d_addr);
        if (unlikely (fault)){
		printc("found a fault in mman_alias_page!!!!\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (ret == -EINVAL) {
		if (aaaaa++ > 5) assert(0);
		/*if we can not find the creator, it must be the root
		 * page since the root page comes from the
		 * mman_get_page function, not by alias (so it is not
		 * tracked in valloc)*/
		if (valloc_upcall(s_spd, s_addr)) {
			/* cos_introspect....rootpage */
		}
		goto redo;
	}

	if (cos_spd_id() == 5) goto done;
	
	printc("cli: mman_alias_page 2 (in spd %ld, ret %p)\n", cos_spd_id(), ret);

	rd = rdmm_alloc(d_addr);
	assert(rd);
	rd_cons(rd, s_spd, s_addr, d_spd_flags, d_addr, PAGE_STATE_ALIAS);

	if (cos_spd_id() != 5) {
		printc("cli: mman_alias_page 3\n");
	}
	/* track sub tree here */
	parent_rd_cons(rd, s_addr);

done:	
	return ret;
}

CSTUB_FN(vaddr_t, mman_revoke_page) (struct usr_inv_cap *uc,
				     spdid_t spdid, vaddr_t addr, int flags)
{
	long fault = 0;
	long ret;
	
	struct rec_data_mm *rd = NULL;

        if (first == 0 && cos_spd_id() != 5) {
		cvect_init_static(&rec_mm_vect);
		cvect_init_static(&parent_rec_mm_vect);
		first = 1;
	}

	if (cos_spd_id() == 5) goto con;
	
redo:

	rd_update(addr, PAGE_STATE_REVOKE);
	
	printc("cli: mman_revoke_page 1\n");
con:
	CSTUB_INVOKE(ret, fault, uc, 3,  spdid, addr, flags);
        if (unlikely (fault)){
		printc("found a fault in mman_revoke_page!!!!\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (cos_spd_id() == 5) goto done;

	if (ret == -EINVAL) {
		rd_update_subtree(addr, PAGE_STATE_REVOKE);
		goto redo;
	}


	/* here is one issue: if we always remove all tracking
	 * descriptors here, a page that was aliased (and tracked) in
	 * another component might not be removed explicitly through
	 * the revoke functoin. So we do non remove the descriptors*/
	
	/* rd_remove(addr);*/

	/* revoke does not reomve itself, only subtree. So the created
	 * rd is not reomved here. release_page does.*/
	/* rdmm_dealloc(rd); */
done:
	printc("cli: mman_revoke_page return %d\n", ret);
	return ret;
}

