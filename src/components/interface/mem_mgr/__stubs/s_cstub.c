/* 
   Jiguo: add the reflection for the case that the client calls malloc
   and crashes. The mapping DS maintained in mm is thought valid,
   which should be released
*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <cos_alloc.h>

#include <mem_mgr.h>
#include <objtype.h>

#include "../../implementation/sched/cos_sched_sync.h"

/*  Jiguo: Same way/data structures that memory manager maintains its
 *  own pages */

struct mapping;
/* A tagged union, where the tag holds the number of maps: */
struct frame {
	int nmaps;
	union {
		struct mapping *m;  /* nmaps > 0 : root of all mappings */
		vaddr_t addr;	    /* nmaps = -1: local mapping */
		struct frame *free; /* nmaps = 0 : no mapping */
	} c;
} frames[COS_MAX_MEMORY];
struct frame *freelist;

static inline int  frame_index(struct frame *f) { return f-frames; }
static inline void frame_ref(struct frame *f)   { f->nmaps++; }

static inline struct frame *
frame_alloc(void)
{
	struct frame *f = freelist;

	if (!f) return NULL;
	freelist = f->c.free;
	f->nmaps = 0;
	f->c.m   = NULL;

	return f;
}

static inline struct frame *frame_alloc(void);
static inline int frame_index(struct frame *f);
static inline void *
__page_get(void)
{
	void *hp = cos_get_vas_page();

	struct frame *f = frame_alloc();

	assert(hp && f);
	frame_ref(f);
	f->nmaps  = -1; 	 /* belongs to us... */
	f->c.addr = (vaddr_t)hp; /* ...at this address */
	if (cos_mmap_cntl(COS_MMAP_GRANT, MAPPING_RW, cos_spd_id(), (vaddr_t)hp, frame_index(f))) {
		printc("mm interface: grant @ %p for frame %d\n", hp, frame_index(f));
		BUG();
	}
	/* printc("grant @ %p for frame %d\n", hp, frame_index(f)); */
	return hp;
}

#define CPAGE_ALLOC() __page_get()
#include <cpage_alloc.h>

#define CSLAB_ALLOC(sz)   cpage_alloc()
#define CSLAB_FREE(x, sz) cpage_free(x)
#include <cslab.h>

#define CVECT_ALLOC() cpage_alloc()
#define CVECT_FREE(x) cpage_free(x)
#include <cvect.h>

struct rec_data_page {
	vaddr_t addr;
	struct rec_data_page *next, *prev;
};

struct rec_data_spd {
	spdid_t       spdid;
	struct rec_data_page pages;
        /* every time during reflection for a fault, we switch to a
	   different list */
	/* struct rec_data_page pages2;   */
};

// slab allocator for different spds
CVECT_CREATE_STATIC(rec_spd_vect);
CSLAB_CREATE(rdspd, sizeof(struct rec_data_spd));

static struct rec_data_spd *
rdspd_lookup(int id)
{ 
	return cvect_lookup(&rec_spd_vect, id); 
}

static struct rec_data_spd *
rdspd_alloc(int spdid)
{
	struct rec_data_spd *rd;

	rd = cslab_alloc_rdspd();
	assert(rd);
	cvect_add(&rec_spd_vect, rd, spdid);
	return rd;
}

static void
rdspd_dealloc(struct rec_data_spd *rd)
{
	assert(rd);
	cslab_free_rdspd(rd);
}

// slab allocator for pages in a spd
/* CVECT_CREATE_STATIC(rec_page_vect); */
CSLAB_CREATE(rdpage, sizeof(struct rec_data_page));

/* static struct rec_data_page * */
/* rdpage_lookup(vaddr_t addr) */
/* {  */
/* 	return cvect_lookup(&rec_page_vect, addr >> PAGE_SHIFT);  */
/* } */

static struct rec_data_page *
rdpage_alloc(struct rec_data_spd *rd, vaddr_t addr)
{
	struct rec_data_page *rd_page = NULL;

	assert(rd);

	rd_page = cslab_alloc_rdpage();
	if (!rd_page) goto done;
	rd_page->addr = addr;
	INIT_LIST(rd_page, next, prev);
	ADD_END_LIST(&rd->pages, rd_page, next, prev);
	
	/* printc("tracking....page %p (spd %d, by thd %d)\n", */
	/*        (void *)addr, rd->spdid, cos_get_thd_id()); */

	/* /\* cvect_add(&rec_page_vect, rd_page, addr >> PAGE_SHIFT); *\/ */
done:
	return rd_page;
}

static void
rdpage_dealloc(struct rec_data_page *rd_page)
{
	assert(rd_page);
	cslab_free_rdpage(rd_page);
	
	return;
}

// arg is the src_spd
int __sg_mman_release_page(spdid_t spd, vaddr_t addr, int arg)
{
	int ret = 0;
	
	ret = mman_release_page(spd, addr, arg);

	/* printc("mem_normal: ser side release_page %p\n", (void *)addr); */

#ifdef REFLECTION
	cos_sched_lock_take();

	struct rec_data_spd *rd;
	rd = rdspd_lookup(arg);
	assert(rd);

	struct rec_data_page *rd_page;
	/* rd_page = rdpage_lookup(addr); */
	/* assert(rd_page); */
	// remove from list and from cvect
	/* REM_LIST(rd_page, next, prev); */
	/* rdpage_dealloc(rd_page); */

	rd_page = FIRST_LIST(&rd->pages, next, prev);
	assert(rd_page);
	REM_LIST(rd_page, next, prev);
	/* printc("mem_normal release: pages %p @ %p\n", (void *)rd_page->addr, (void *)rd_page); */
	/* printc("mem_normal release: ser side removing from list\n"); */

	cos_sched_lock_release();
#endif
	return ret;
}

vaddr_t __sg_mman_get_page(spdid_t spdid, vaddr_t addr, int flags)
{
	vaddr_t ret = 0;

	struct rec_data_spd *rd = NULL;

	/* printc("mem_normal: ser side get_page\n"); */

#ifdef REFLECTION
	ret = mman_get_page(spdid, addr, flags);
	assert(ret > 0);

	cos_sched_lock_take();

	// track the allocated page
	rd = rdspd_lookup(spdid);
	if (unlikely(!rd)) {
		rd = rdspd_alloc(spdid);
		assert(rd);
		INIT_LIST(&rd->pages, next, prev);
		/* INIT_LIST(&rd->pages2, next, prev); */
		rd->spdid = spdid;
	} 
	assert(rd && rd->spdid == spdid);
	assert(rdpage_alloc(rd, addr));
	cos_sched_lock_release();
#else
	ret = mman_get_page(spdid, addr, flags);
	assert(ret > 0);
#endif

	return ret;
}

/* This is memory manger server interface function for client to
   reflect the allocated pages  */
vaddr_t __sg_mman_reflect(spdid_t spd, int src_spd, int cnt)
{
        struct rec_data_spd *rd = NULL;
        struct rec_data_page *rd_page = NULL;
        struct rec_data_page *rd_page_list = NULL;
	int ret = 0;

	assert(src_spd);
	cos_sched_lock_take();
	/* printc("mm server side stub (thd %d)\n", cos_get_thd_id()); */
	/* printc("passed reflection: spd %d src_spd %d\n", spd, src_spd); */

	rd = rdspd_lookup(src_spd);
	if (!rd) goto done;  // no pages allocated yet for src_spd

	if (cnt) {
		rd_page_list = &rd->pages;
		for (rd_page = FIRST_LIST(rd_page_list, next, prev);
		     rd_page != rd_page_list;
		     rd_page = FIRST_LIST(rd_page, next, prev)){
			/* printc("(cnt)saved pages %p @ %p (thd %d)\n", (void *)rd_page->addr, (void *)rd_page, cos_get_thd_id()); */
			ret++;
		}
	} else {
		rd_page_list = &rd->pages;
		/* for (rd_page = FIRST_LIST(rd_page_list, next, prev); */
		/*      rd_page != rd_page_list; */
		/*      rd_page = FIRST_LIST(rd_page, next, prev)){ */
		/* 	printc("(before)saved pages %p @ %p\n", (void *)rd_page->addr, (void *)rd_page); */
		/* } */

		rd_page = FIRST_LIST(rd_page_list, next, prev);
		/* printc("mem_normal reflection: pages %p @ %p\n", (void *)rd_page->addr, (void *)rd_page); */
		ret = rd_page->addr;
	}
done:
	cos_sched_lock_release();
	return ret;
}
