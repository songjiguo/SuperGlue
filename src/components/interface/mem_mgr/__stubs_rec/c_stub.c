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

#include <mem_mgr.h>
#include <cstub.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

struct rec_data_mm_list;
/* recovery data structure for each alias */
struct rec_data_mm {
	int     id;         // id in cvect is (s_addr >> PAGE_SHIFT) & 0xFFF

	vaddr_t s_addr;        
	vaddr_t d_addr;
	spdid_t s_spd;      // this should be just cos_spd_id()
	unsigned int d_spd_flags;

	unsigned int  state;
	unsigned long fcnt;
	
	struct rec_data_mm *next, *prev;  // the list of aliased pages from s_addr
};


/* the state of a page object */
enum {
	PAGE_STATE_GET,  
	PAGE_STATE_ALIAS,
	PAGE_STATE_REVOKE
};

void print_rdmm_info(struct rec_data_mm *rdmm);
/**********************************************/
/* slab allocator and cvect for tracking pages */
/**********************************************/
CVECT_CREATE_STATIC(rec_mm_vect);
CSLAB_CREATE(rdmm, sizeof(struct rec_data_mm));

static struct rec_data_mm *
rdmm_lookup(vaddr_t s_addr)
{
	return cvect_lookup(&rec_mm_vect, (s_addr >> PAGE_SHIFT) & 0xFFF);
}

static struct rec_data_mm *
rdmm_alloc(vaddr_t s_addr)
{
	struct rec_data_mm *rd;

	rd = cslab_alloc_rdmm();
	assert(rd);

	INIT_LIST(rd, next, prev);
	rd->id = (s_addr >> PAGE_SHIFT) & 0xFFF;

	return rd;
}

static void
rdmm_dealloc(struct rec_data_mm *rd)
{
	assert(rd);

	REM_LIST(rd, next, prev);
	if (cvect_del(&rec_mm_vect, rd->id)) assert(0);
	cslab_free_rdmm(rd);
	
	return;
}

/*****************/
/* Debug helper  */
/*****************/
void
print_rd_info(struct rec_data_mm *rd)
{
	assert(rd);
	
	printc("rd->idx %d\n",rd->id);
	printc("rd->s_spd %d\n",rd->s_spd);
	printc("rd->s_addr %d\n",rd->s_addr);
	printc("rd->d_spd_flags %p\n",rd->d_spd_flags);
	printc("rd->d_addr %d\n",rd->d_addr);
	printc("rd->fcnt %ld\n",rd->fcnt);
	printc("fcounter %ld \n",fcounter);
	
	return;
}

static void
rd_cons(struct rec_data_mm *rd, int id, 
	spdid_t s_spd, vaddr_t s_addr, unsigned int d_spd_flags, vaddr_t d_addr, int state)
{
	assert(rd);
	
	rd->id		= id;
	rd->s_addr	= s_addr;
	rd->s_spd	= s_spd;
	rd->d_addr	= d_addr;
	rd->d_spd_flags	= d_spd_flags;

	rd->state	= state;
	rd->fcnt	= fcounter;

	
	printc("rd_cons print rd --- \n");
	print_rd_info(rd);
	return;
}

static void
rd_recover(struct rec_data_mm *rd)
{
	struct rec_data_mm *tmp;
	vaddr_t s_addr;

	printc("spd %ld: ready to replay now...thd %d\n", cos_spd_id(), cos_get_thd_id());

	assert(rd);
	rd->fcnt = fcounter;
	
	struct rec_data_mm *next, *alias_rd, *new;
	
	printc("interface recovery: replay alias alias_rd->d_addr %p\n",
	       rd->d_addr);
	print_rd_info(rd);
	if (rd->d_addr != __mman_alias_page(rd->s_spd, rd->s_addr, 
					    rd->d_spd_flags, rd->d_addr)) BUG();
	new = FIRST_LIST(rd, next, prev);
	REM_LIST(new, next, prev);
	cslab_free_rdmm(new);
	
	alias_rd = FIRST_LIST(rd, next, prev);
	while (!EMPTY_LIST(alias_rd, next, prev)) {
		next = FIRST_LIST(alias_rd, next, prev);
		assert(alias_rd->s_spd == cos_spd_id());
		printc("interface recovery: replay alias alias_rd->d_addr %p\n",
		       alias_rd->d_addr);
		print_rd_info(alias_rd);
		if (alias_rd->d_addr != 
		    __mman_alias_page(alias_rd->s_spd, alias_rd->s_addr, 
				      alias_rd->d_spd_flags, alias_rd->d_addr)) BUG();
		/* removed the new created record for the same
		 * mapping. ADD_LIST will put the "new" at the
		 * beginning. So we remove it before proceed to the
		 * next */
		new = FIRST_LIST(rd, next, prev);
		REM_LIST(new, next, prev);
		cslab_free_rdmm(new);
		
		alias_rd = next;
	}

	printc("spd %ld: replay done...thd %d\n", cos_spd_id(), cos_get_thd_id());
	return;
}

/* reconstruct all aliased pages from a root page. This function
 * should be called by the recovery thread (upcalled into this
 * component and do the replay) */
void 
alias_replay(vaddr_t s_addr)
{
        struct rec_data_mm *rd = rdmm_lookup(s_addr);
	assert(rd);
	rd_recover(rd);
	return;
}


static struct rec_data_mm *
rd_update(vaddr_t s_addr, int state)
{
        struct rec_data_mm *rd = NULL;

	/* in state machine, we track/update the page's alias
	 * state. The actual re-construction of all aliased pages from
	 * s_addr is done by recovery thread and its upcall into each
	 * spd (see above alias_replay) */
	if (unlikely(!(rd = rdmm_lookup(s_addr)))) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;
	rd->fcnt = fcounter;

	printc("State Machine mm %p -- ", s_addr);
	/* STATE MACHINE */
	switch (state) {
	case PAGE_STATE_GET:
		/* Should not get here. We only create/track the state
		 * from alias */
		assert(0);
		break;
	case PAGE_STATE_ALIAS:
		/* Just re-exe the current on the stack */
		break;
	case PAGE_STATE_REVOKE:
		/* Just re-exe the current on the stack */
		break;
	default:
		assert(0);
	}
	printc("thd %d restore mm done!!!\n", cos_get_thd_id());
done:
	return rd;
}

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN(vaddr_t, mman_get_page) (struct usr_inv_cap *uc,
				  spdid_t spdid, vaddr_t addr, int flags)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
redo:
	if (cos_spd_id() != 5) {
		printc("mm cli: call mman_get_page addr %p\n", addr);
	}
	
	CSTUB_INVOKE(ret, fault, uc, 3,  spdid,addr, flags);
        if (unlikely (fault)){
		printc("found a fault in mman_get_page!!!!!\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}

CSTUB_FN(vaddr_t, __mman_alias_page) (struct usr_inv_cap *uc,
				      spdid_t s_spd, vaddr_t s_addr, 
				      unsigned int d_spd_flags, vaddr_t d_addr)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
        struct rec_data_mm *rd, *dest;

	unsigned int test  = d_spd_flags;
	
redo:
	if (cos_spd_id() != 5) {
		printc("mm cli: d_spd_flags %p\n", d_spd_flags);
		printc("mm cli: __mman_alias_page spd %d s_addr %p and (PAGE_SHIFT %p) dest %d d_addr %p\n",
		       cos_spd_id(), s_addr, (s_addr >> PAGE_SHIFT) & 0xFFF, 
		       test >> 16, d_addr);
	}
	
	CSTUB_INVOKE(ret, fault, uc, 4,  s_spd, s_addr, d_spd_flags, d_addr);
        if (unlikely (fault)){
		printc("found a fault in mman_alias_page!!!!!\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (cos_spd_id() == 5) goto done;   // do not track booter for now
	
	rd = rdmm_lookup(s_addr);
	if (!rd) {
		rd = rdmm_alloc(s_addr);
		assert(rd);
		if (cvect_add(&rec_mm_vect, rd, rd->id)) assert(0);
		/* printc("<< 1 >> mm cli: track spd %d s_addr %p dest %d d_addr %p\n", */
		/*        cos_spd_id(), s_addr,  d_spd_flags >> 16, d_addr); */
		rd_cons(rd, rd->id, s_spd, s_addr, d_spd_flags, d_addr, PAGE_STATE_ALIAS);
	} else {
		dest = rdmm_alloc(s_addr);
		/* printc("<< 2 >> mm cli: track spd %d s_addr %p dest %d d_addr %p\n", */
		/*        cos_spd_id(), s_addr,  d_spd_flags >> 16, d_addr); */
		rd_cons(dest, dest->id, s_spd, s_addr, d_spd_flags, d_addr, PAGE_STATE_ALIAS);
		ADD_LIST(rd, dest, next, prev);  // this should be removed when revoke
	}

done:	
	return ret;
}

CSTUB_FN(vaddr_t, mman_revoke_page) (struct usr_inv_cap *uc,
				     spdid_t spdid, vaddr_t addr, int flags)
{
	long fault = 0;
	long ret;
	
	unsigned long long start, end;
	struct rec_data_mm *rd;
	
redo:
	printc("mm cli: call mman_revoke_page addr %p\n", addr);
	
	if (cos_spd_id() == 5) goto cont;   // do not track booter for now
	rd = rd_update(addr, PAGE_STATE_REVOKE);
	assert(rd);
cont:
	CSTUB_INVOKE(ret, fault, uc, 3,  spdid, addr, flags);
        if (unlikely (fault)){
		printc("found a fault in mman_revoke_page!!!!\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (cos_spd_id() == 5) goto done;   // do not track booter for now
	
	struct rec_data_mm *next, *alias_rd;
	alias_rd = FIRST_LIST(rd, next, prev);
	while (!EMPTY_LIST(alias_rd, next, prev)) {
		next = FIRST_LIST(alias_rd, next, prev);
		REM_LIST(alias_rd, next, prev);
		alias_rd = next;
	}
	rdmm_dealloc(rd);
done:
	return ret;
}

