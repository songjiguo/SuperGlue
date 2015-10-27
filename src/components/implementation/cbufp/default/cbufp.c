/*
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#include <cos_component.h>
#include <cbuf_meta.h>
#include <mem_mgr_large.h>
#include <cbuf.h>
#include <cbufp.h>
#include <cbuf_c.h>
#include <cos_synchronization.h>
#include <valloc.h>
#include <cos_alloc.h>
#include <cmap.h>
#include <cos_list.h>


/***********************************/
/* tracking uniq_id between faults */
/***********************************/
#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct cbid_fid_data {
	int cbid;
	int fid;
	int len;
	int offset;
	struct cbid_fid_data *next, *prev;
};

CVECT_CREATE_STATIC(cfidmapping_vect);
CSLAB_CREATE(cbidfid, sizeof(struct cbid_fid_data));

static struct cbid_fid_data *
cbidfid_lookup(int fid)
{
	return cvect_lookup(&cfidmapping_vect, fid);
}

static struct cbid_fid_data *
cbidfid_alloc(int fid)
{
	struct cbid_fid_data *head;

	head = cslab_alloc_cbidfid();
	assert(head);
	if (cvect_add(&cfidmapping_vect, head, fid)) {
		printc("can not add into cvect\n");
		BUG();
	}
	INIT_LIST(head, next, prev);
	return head;
}

static void
cbidfid_dealloc(struct cbid_fid_data *idmapping)
{
	assert(idmapping);
	if (cvect_del(&cfidmapping_vect, idmapping->fid)) BUG();
	cslab_free_cbidfid(idmapping);
}

/** 
 * The main data-structures tracked in this component.
 * 
 * cbufp_comp_info is the per-component data-structure that tracks the
 * page shared with the component to return garbage-collected cbufs, the
 * cbufs allocated to the component, and the data-structures for
 * tracking where the cbuf_metas are associated with the cbufs.
 * 
 * cbuf_meta_range is a simple linked list to track the metas for
 * given cbuf id ranges.
 *
 * cbufp_info is the per-cbuf structure that tracks the cbid, size,
 * and contains a linked list of all of the mappings for that cbuf.
 *
 * See the following diagram:

  cbufp_comp_info                 cbuf_meta_range
  +------------------------+	  +---------+	+---------+
  | spdid     	           |  +-->| daddr   +--->         |
  | +--------------------+ |  |	  | cbid    |	|         |
  | | size = X, c-+      | |  |	  | meta    <---+       | |
  | --------------|------- |  |	  +-|-------+	+-------|-+
  | | size = ...  |      | |  |     |  +-----------+	+-->
  | +-------------|------+ |  |	    +->|           |
  |          cbuf_metas-------+	       +-----^-----+ cbuf_meta
  +---------------|--------+	+------------|---+
		  |   		| cbid, size |   |
		  |     	| +----------|-+ | +------------+
		  +------------>| | spdid,.. | |<->| .., addr   |
		   		| +------------+ | +------------+
				+----------------+     cbufp_maps
                                cbufp_info
*/

/* Per-cbuf information */
struct cbufp_maps {
	spdid_t spdid;
	vaddr_t addr;
	struct cbuf_meta *m;
	struct cbufp_maps *next, *prev;
};

struct cbufp_info {
	u32_t cbid;
	int size;
	char *mem;
	struct cbufp_maps owner;
	struct cbufp_info *next, *prev;
};

/* Per-component information */
struct cbufp_meta_range {
	struct cbuf_meta *m;
	vaddr_t dest;
	u32_t low_id;
	struct cbufp_meta_range *next, *prev;
};
#define CBUFP_META_RANGE_HIGH(cmr) (cmr->low_id + (PAGE_SIZE/sizeof(struct cbuf_meta)))

struct cbufp_bin {
	int size;
	struct cbufp_info *c;
};

struct cbufp_comp_info {
	spdid_t spdid;
	struct cbufp_shared_page *csp;
	vaddr_t dest_csp;
	int nbin;
	struct cbufp_bin cbufs[CBUFP_MAX_NSZ];
	struct cbufp_meta_range *cbuf_metas;
};

#define printl(s) //printc(s)
cos_lock_t cbufp_lock;
#define CBUFP_LOCK_INIT() lock_static_init(&cbufp_lock);
#define CBUFP_TAKE()      do { if (lock_take(&cbufp_lock))    BUG(); } while(0)
#define CBUFP_RELEASE()   do { if (lock_release(&cbufp_lock)) BUG(); } while(0)
CVECT_CREATE_STATIC(components);
CMAP_CREATE_STATIC(cbufs);

static struct cbufp_meta_range *
cbufp_meta_lookup_cmr(struct cbufp_comp_info *comp, u32_t cbid)
{
	struct cbufp_meta_range *cmr;
	assert(comp);

	cmr = comp->cbuf_metas;
	if (!cmr) return NULL;
	do {
		if (cmr->low_id >= cbid || CBUFP_META_RANGE_HIGH(cmr) > cbid) {
			return cmr;
		}
		cmr = FIRST_LIST(cmr, next, prev);
	} while (cmr != comp->cbuf_metas);

	return NULL;
}

static struct cbuf_meta *
cbufp_meta_lookup(struct cbufp_comp_info *comp, u32_t cbid)
{
	struct cbufp_meta_range *cmr;

	cmr = cbufp_meta_lookup_cmr(comp, cbid);
	if (!cmr) return NULL;
	return &cmr->m[cbid - cmr->low_id];
}

static struct cbufp_meta_range *
cbufp_meta_add(struct cbufp_comp_info *comp, u32_t cbid, struct cbuf_meta *m, vaddr_t dest)
{
	struct cbufp_meta_range *cmr;

	if (cbufp_meta_lookup(comp, cbid)) return NULL;
	cmr = malloc(sizeof(struct cbufp_meta_range));
	if (!cmr) return NULL;
	INIT_LIST(cmr, next, prev);
	cmr->m      = m;
	cmr->dest   = dest;
	/* must be power of 2: */
	cmr->low_id = (cbid & ~((PAGE_SIZE/sizeof(struct cbuf_meta))-1));

	if (comp->cbuf_metas) ADD_LIST(comp->cbuf_metas, cmr, next, prev);
	else                  comp->cbuf_metas = cmr;

	return cmr;
}

static void
cbufp_comp_info_init(spdid_t spdid, struct cbufp_comp_info *cci)
{
	memset(cci, 0, sizeof(*cci));
	cci->spdid = spdid;
	cvect_add(&components, cci, spdid);
}

static struct cbufp_comp_info *
cbufp_comp_info_get(spdid_t spdid)
{
	struct cbufp_comp_info *cci;

	cci = cvect_lookup(&components, spdid);
	if (!cci) {
		cci = malloc(sizeof(*cci));
		if (!cci) return NULL;
		cbufp_comp_info_init(spdid, cci);
	}
	return cci;
}

static struct cbufp_bin *
cbufp_comp_info_bin_get(struct cbufp_comp_info *cci, int sz)
{
	int i;

	assert(sz);
	for (i = 0 ; i < cci->nbin ; i++) {
		if (sz == cci->cbufs[i].size) return &cci->cbufs[i];
	}
	return NULL;
}

static struct cbufp_bin *
cbufp_comp_info_bin_add(struct cbufp_comp_info *cci, int sz)
{
	if (sz == CBUFP_MAX_NSZ) return NULL;
	cci->cbufs[cci->nbin].size = sz;
	cci->nbin++;

	return &cci->cbufs[cci->nbin-1];
}

static int
cbufp_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size)
{
	void *p;
	vaddr_t dest;
	int off;

	assert(size == (int)round_to_page(size));
	p = page_alloc(size/PAGE_SIZE);
	assert(p);
	memset(p, 0, size);

	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	assert(dest);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		vaddr_t d = dest + off;
		if (d != 
		    (mman_alias_page(cos_spd_id(), ((vaddr_t)p) + off, spdid, d, MAPPING_RW))) {
			assert(0);
			/* TODO: roll back the aliases, etc... */
			valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
		}
	}
	*page  = p;
	*daddr = dest;

	return 0;
}

/* Do any components have a reference to the cbuf? */
static int
cbufp_referenced(struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;
	int sent, recvd;

	sent = recvd = 0;
	do {
		struct cbuf_meta *meta = m->m;

		if (meta) {
			if (meta->nfo.c.refcnt) return 1;
			sent  += meta->owner_nfo.c.nsent;
			recvd += meta->owner_nfo.c.nrecvd;
		}

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	if (sent != recvd) return 1;
	
	return 0;
}

static void
cbufp_references_clear(struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;

	do {
		struct cbuf_meta *meta = m->m;

		if (meta) {
			meta->owner_nfo.c.nsent = meta->owner_nfo.c.nrecvd = 0;
		}
		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	return;
}

static void
cbufp_free_unmap(spdid_t spdid, struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;
	void *ptr = cbi->mem;
	int off;

	if (cbufp_referenced(cbi)) return;
	cbufp_references_clear(cbi);
	do {
		assert(m->m);
		assert(!m->m->nfo.c.refcnt);
		/* TODO: fix race here with atomic instruction */
		memset(m->m, 0, sizeof(struct cbuf_meta));

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	/* Unmap all of the pages from the clients */
	for (off = 0 ; off < cbi->size ; off += PAGE_SIZE) {
		mman_revoke_page(cos_spd_id(), (vaddr_t)ptr + off, 0);
	}

	/* 
	 * Deallocate the virtual address in the client, and cleanup
	 * the memory in this component
	 */
	m = &cbi->owner;
	do {
		struct cbufp_maps *next;

		next = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);
		if (m != &cbi->owner) free(m);
		m = next;
	} while (m != &cbi->owner);

	/* deallocate/unlink our data-structures */
	page_free(ptr, cbi->size/PAGE_SIZE);
	cmap_del(&cbufs, cbi->cbid);
	free(cbi);
}

int
cbufp_create(spdid_t spdid, int size, long cbid)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	struct cbuf_meta *meta;
	int ret = 0;

	printl("cbufp_create\n");
	if (unlikely(cbid < 0)) return 0;
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		struct cbufp_bin *bin;

 		cbi = malloc(sizeof(struct cbufp_info));
		if (!cbi) goto done;

		/* Allocate and map in the cbuf. */
		cbid = cmap_add(&cbufs, cbi);
		cbi->cbid        = cbid;
		size             = round_up_to_page(size);
		cbi->size        = size;
		cbi->owner.m     = NULL;
		cbi->owner.spdid = spdid;
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);

		bin = cbufp_comp_info_bin_get(cci, size);
		if (!bin) bin = cbufp_comp_info_bin_add(cci, size);
		if (!bin) goto free;

		if (cbufp_alloc_map(spdid, &(cbi->owner.addr), 
				    (void**)&(cbi->mem), size)) goto free;
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
		cbi = cmap_lookup(&cbufs, cbid);
		if (!cbi) goto done;
		if (cbi->owner.spdid != spdid) goto done;
	}
	meta = cbufp_meta_lookup(cci, cbid);
	/* We need to map in the meta for this cbid.  Tell the client. */
	if (!meta) {
		ret = cbid * -1;
		goto done;
	}
	cbi->owner.m = meta;

	/* 
	 * Now we know we have a cbid, a backing structure for it, a
	 * component structure, and the meta mapped in for the cbuf.
	 * Update the meta with the correct addresses and flags!
	 */
	memset(meta, 0, sizeof(struct cbuf_meta));
	meta->nfo.c.flags |= CBUFM_TOUCHED | 
		             CBUFM_OWNER  | CBUFM_WRITABLE;
	meta->nfo.c.ptr    = cbi->owner.addr >> PAGE_ORDER;
	meta->sz           = cbi->size >> PAGE_ORDER;
	if (meta->nfo.c.refcnt == CBUFP_REFCNT_MAX) assert(0);
	meta->nfo.c.refcnt++;
	ret = cbid;
done:
	CBUFP_RELEASE();

	return ret;
free:
	cmap_del(&cbufs, cbid);
	free(cbi);
	goto done;
}

/*
 * Allocate and map the garbage-collection list used for cbufp_collect()
 */
vaddr_t
cbufp_map_collect(spdid_t spdid)
{
	struct cbufp_comp_info *cci;
	vaddr_t ret = (vaddr_t)NULL;

	printl("cbufp_map_collect\n");

	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;

	/* if the mapped page exists already, just return it. */
	if (cci->dest_csp) {
		ret = cci->dest_csp;
		goto done;
	}

	assert(sizeof(struct cbufp_shared_page) <= PAGE_SIZE);
	/* alloc/map is leaked. Where should it be freed/unmapped? */
	if (cbufp_alloc_map(spdid, &cci->dest_csp, (void**)&cci->csp, PAGE_SIZE)) goto done;
	ret = cci->dest_csp;

	/* initialize a continuous ck ring */
	assert(cci->csp->ring.size == 0);
	CK_RING_INIT(cbufp_ring, &cci->csp->ring, NULL, CSP_BUFFER_SIZE);

done:
	CBUFP_RELEASE();
	return ret;
}

/*
 * For a certain principal, collect any unreferenced persistent cbufs
 * so that they can be reused.  This is the garbage-collection
 * mechanism.
 *
 * Collect cbufps and add them onto the component's freelist.
 *
 * This function is semantically complicated.  It can block if no
 * cbufps are available, and the component is not supposed to allocate
 * any more.  It can return no cbufps even if they are available to
 * force the pool of cbufps to be expanded (the client will call
 * cbufp_create in this case).  Or, the common case: it can return a
 * number of available cbufs.
 */
int
cbufp_collect(spdid_t spdid, int size)
{
	struct cbufp_info *cbi;
	struct cbufp_comp_info *cci;
	struct cbufp_shared_page *csp;
	struct cbufp_bin *bin;
	int ret = 0;

	printl("cbufp_collect\n");

	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (unlikely(!cci)) ERR_THROW(-ENOMEM, done);
	csp = cci->csp;
	if (unlikely(!csp)) ERR_THROW(-EINVAL, done);

	assert(csp->ring.size == CSP_BUFFER_SIZE);

	/* 
	 * Go through all cbufs we own, and report all of them that
	 * have no current references to them.  Unfortunately, this is
	 * O(N*M), N = min(num cbufs, PAGE_SIZE/sizeof(int)), and M =
	 * num components.
	 */
	bin = cbufp_comp_info_bin_get(cci, round_up_to_page(size));
	if (!bin) ERR_THROW(0, done);
	cbi = bin->c;
	do {
		if (!cbi) break;
		if (!cbufp_referenced(cbi)) {
			struct cbufp_ring_element el = { .cbid = cbi->cbid };
			cbufp_references_clear(cbi);
			if (!CK_RING_ENQUEUE_SPSC(cbufp_ring, &csp->ring, &el)) break;
			if (++ret == CSP_BUFFER_SIZE) break;
		}
		cbi = FIRST_LIST(cbi, next, prev);
	} while (cbi != bin->c);
done:
	CBUFP_RELEASE();
	return ret;
}

/* 
 * Called by cbufp_deref.
 */
int
cbufp_delete(spdid_t spdid, int cbid)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	int ret = -EINVAL;

	printl("cbufp_delete\n");
	assert(0);
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	
	cbufp_free_unmap(spdid, cbi);
	ret = 0;
done:
	CBUFP_RELEASE();
	return ret;
}

/* 
 * Called by cbufp2buf to retrieve a given cbid.
 */
int
cbufp_retrieve(spdid_t spdid, int cbid, int size)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	struct cbuf_meta *meta;
	struct cbufp_maps *map;
	vaddr_t dest;
	void *page;
	int ret = -EINVAL, off;

	printl("cbufp_retrieve\n");

	CBUFP_TAKE();
	cci        = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) goto done;
	meta       = cbufp_meta_lookup(cci, cbid);
	if (!meta) goto done;

	map        = malloc(sizeof(struct cbufp_maps));
	if (!map) ERR_THROW(-ENOMEM, done);
	if (size > cbi->size) goto done;
	assert((int)round_to_page(cbi->size) == cbi->size);
	size       = cbi->size;
	dest       = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	if (!dest) goto free;

	map->spdid = spdid;
	map->m     = meta;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);

	page = cbi->mem;
	assert(page);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		if (dest+off != 
		    (mman_alias_page(cos_spd_id(), ((vaddr_t)page)+off, spdid, dest+off, MAPPING_READ))) {
			assert(0);
			valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
		}
	}

	meta->nfo.c.flags |= CBUFM_TOUCHED;
	meta->nfo.c.ptr    = map->addr >> PAGE_ORDER;
	meta->sz           = cbi->size >> PAGE_ORDER;
	ret                = 0;
done:
	CBUFP_RELEASE();

	return ret;
free:
	free(map);
	goto done;
}

vaddr_t
cbufp_register(spdid_t spdid, long cbid)
{
	struct cbufp_comp_info  *cci;
	struct cbufp_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

	printl("cbufp_register\n");
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cmr = cbufp_meta_lookup_cmr(cci, cbid);
	if (cmr) ERR_THROW(cmr->dest, done);

	/* Create the mapping into the client */
	if (cbufp_alloc_map(spdid, &dest, &p, PAGE_SIZE)) goto done;
	assert((u32_t)p == round_to_page(p));
	cmr = cbufp_meta_add(cci, cbid, p, dest);
	assert(cmr);
	ret = cmr->dest;
done:
	CBUFP_RELEASE();
	return ret;
}


/* FIXME: the writing mode is not defined yet. Now if close a file
 * ,and reopen it, the old contents is overwritten. In the future, for
 * ramFS, other mode should be supported, like append writing.... */
/* 
   cbid  :  the one that carries the data we want to back up
   len   :  actual written size
   offset:  offset before writing (offset to offset+len will have the data )
   fid   :  unique file id (each unique path has its fid)
 */
int
cbufp_record(int cbid, int len, int offset, int fid)
{
	struct cbufp_info *cbi;
	struct cbid_fid_data *head = NULL;
	struct cbid_fid_data *cfd = NULL;

	vaddr_t dest;
	void *page;
	int ret = -EINVAL, off;
	
	printc("cbufp_record\n");
	printc("passed in para: len %d offset %d cbid %d fid %d\n", 
	       len, offset, cbid, fid);

	CBUFP_TAKE();

	
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	if (unlikely(!(head = cbidfid_lookup(fid)))) {
		printc("cbufp_record: fid %d\n", fid);	
		head = cbidfid_alloc(fid);
	}
	assert(head);
	cfd = cslab_alloc_cbidfid();
	if (!cfd) goto done;
	
	cfd->fid    = fid;
	cfd->cbid   = cbid;
	cfd->len    = len;
	cfd->offset = offset;
	ADD_LIST(head, cfd, next, prev);

	/* Addition to tracking the cbufs for a fid, also make a cbuf
	 * read only */
	/* printc("record: cbid %ld d->addr %p (d's spd %d, and addr is %p)\n",  */
	/*        cbid, d->addr, d->owner.spd, d->owner.addr); */
	vaddr_t vaddr = cbi->owner.addr;
	printc("vaddr %p\n", (void *)vaddr);
	if (cos_mmap_cntl(COS_MMAP_SETRW, MAPPING_READ, cbi->owner.spdid, vaddr, 0)) {
		printc("set page to be read only failed\n");
		BUG();
	}
	
	ret = 0;
done:
	CBUFP_RELEASE();

	return ret;
}


/* return the state of tracked cbufps, such as 
   total number of cbufps (query 0) 
   old_offset             (query 1) 
   actual bytes           (query 2) 
   cbuf                   (query 3) 
*/
int
cbufp_reflect(int fid, int nth, int type)
{
	struct cbid_fid_data *head = NULL;
	struct cbid_fid_data *cfd = NULL;

	vaddr_t dest;
	void *page;
	int ret = -1, off;
	int total_num = 0;
	int index = 0;
	
	printc("cbufp_reflect fid %d\n", fid);
	
	CBUFP_TAKE();
	
	head = cbidfid_lookup(fid);
	if (!head) goto done;
	if (EMPTY_LIST(head, next, prev)) goto done;
	
	switch (type) {
	case 0:    // return the total number of cbufps
		printc("try to get the total number of cbufps\n");
		for(cfd = FIRST_LIST(head, next, prev);
		    cfd != head;
		    cfd = FIRST_LIST(cfd, next, prev)){
			printc("cbid %d\n", cfd->cbid);
			printc("len %d\n", cfd->len);
			printc("offset %d\n", cfd->offset);
			total_num++;
		}
		ret = total_num;
		break;
	case 1:
	case 2:
	case 3:
		printc("try to get the other cbupf info\n");
		for(cfd = FIRST_LIST(head, next, prev);
		    cfd != head;
		    cfd = FIRST_LIST(cfd, next, prev)){
			if (index == nth) break;
			index++;
		}
		if (type == 1) ret = cfd->offset;
		if (type == 2) ret = cfd->len;
		if (type == 3) ret = cfd->cbid;
		break;
	default:
		break;
	}
done:
	CBUFP_RELEASE();
	return ret;
}

int
cbufp_introspect(spdid_t spdid, int iter)
{
	return 0;
/* 	struct spd_tmem_info *sti; */
/* 	spdid_t s_spdid; */
/* 	struct cos_cbuf_item *cci = NULL, *list; */
	
/* 	int counter = 0; */

/* 	TAKE(); */

/* 	sti = get_spd_info(spdid); */
/* 	assert(sti); */
/* 	s_spdid = sti->spdid; */
/* 	list = &spd_tmem_info_list[s_spdid].tmem_list; */
/* 	printc("try to find cbuf for this spd 1\n"); */
/* 	if (iter == -1){ */
/* 		for (cci = FIRST_LIST(list, next, prev) ; */
/* 		     cci != list; */
/* 		     cci = FIRST_LIST(cci, next, prev)) { */
/* 			printc("try to find cbuf for this spd 2\n"); */
/* 			struct cbuf_meta cm; */
/* 			cm.nfo.v = cci->desc.owner.meta->nfo.v; */
/* 			if (CBUF_OWNER(cm.nfo.c.flags) &&  */
/* 			    cm.nfo.c.refcnt) counter++; */
/* 		} */
/* 		RELEASE(); */
/* 		return counter; */
/* 	} else { */
/* 		for (cci = FIRST_LIST(list, next, prev) ; */
/* 		     cci != list; */
/* 		     cci = FIRST_LIST(cci, next, prev)) { */
/* 			struct cbuf_meta cm; */
/* 			cm.nfo.v = cci->desc.owner.meta->nfo.v; */
/* 			if (CBUF_OWNER(cm.nfo.c.flags) &&  */
/*                             cm.nfo.c.refcnt && */
/*                             !(--iter)) goto found; */
/* 		} */
/* 	} */
	
/* found: */
/* 	RELEASE(); */
/* 	return cci->desc.cbid; */
}


/* /\*  */
/*  * Exchange the cbuf descriptor (flags of ownership) of old spd and */
/*  * requested spd */
/*  *\/ */
/* static int  */
/* mgr_update_owner(spdid_t new_spdid, long cbid) */
/* { */
/* 	struct spd_tmem_info *old_sti, *new_sti; */
/* 	struct cb_desc *d; */
/* 	struct cos_cbuf_item *cbi; */
/* 	struct cb_mapping *old_owner, *new_owner, tmp; */
/* 	struct cbuf_meta *old_mc, *new_mc; */
/* 	vaddr_t mgr_addr; */
/* 	int ret = 0; */

/* 	cbi = cos_map_lookup(&cb_ids, cbid); */
/* 	if (!cbi) goto err; */
/* 	d = &cbi->desc; */
/* 	old_owner = &d->owner; */
/* 	old_sti = get_spd_info(old_owner->spd); */
/* 	assert(SPD_IS_MANAGED(old_sti)); */

/* 	old_mc = __spd_cbvect_lookup_range(old_sti, cbid); */
/* 	if (!old_mc) goto err; */
/* 	if (!CBUF_OWNER(old_mc->nfo.c.flags)) goto err; */
/* 	for (new_owner = FIRST_LIST(old_owner, next, prev) ;  */
/* 	     new_owner != old_owner;  */
/* 	     new_owner = FIRST_LIST(new_owner, next, prev)) { */
/* 		if (new_owner->spd == new_spdid) break; */
/* 	} */

/* 	if (new_owner == old_owner) goto err; */
/* 	new_sti = get_spd_info(new_owner->spd); */
/* 	assert(SPD_IS_MANAGED(new_sti)); */

/*         // this returns the whole page for the range */
/* 	mgr_addr = __spd_cbvect_retrieve_page(old_sti, cbid);  */
/* 	assert(mgr_addr); */
/* 	__spd_cbvect_add_range(new_sti, cbid, mgr_addr); */

/* 	new_mc = __spd_cbvect_lookup_range(new_sti, cbid); */
/* 	if(!new_mc) goto err; */
/* 	new_mc->nfo.c.flags |= CBUFM_OWNER; */
/* 	old_mc->nfo.c.flags &= ~CBUFM_OWNER;	 */

/* 	// exchange the spd and addr in cbuf_mapping */
/* 	tmp.spd = old_owner->spd; */
/* 	old_owner->spd = new_owner->spd; */
/* 	new_owner->spd = tmp.spd; */

/* 	tmp.addr = old_owner->addr; */
/* 	old_owner->addr = new_owner->addr; */
/* 	new_owner->addr = tmp.addr; */
/* done: */
/* 	return ret; */
/* err: */
/* 	ret = -1; */
/* 	goto done; */
/* } */

/* Jiguo: This is called when the component checks if it still owns
 * the cbuf or wants to hold a cbuf, if it is not the creater, the
 * ownership should be re-granted to it from the original owner. For
 * example, when the ramfs server is called and the server wants to
 * keep the cbuf longer before restore.(need remember which cbufs for
 * that tid??)
 *
 * r_spdid is the requested spd
 */
int   
cbufp_claim(spdid_t r_spdid, int cbid)
{
	int ret = 0;
/* 	spdid_t o_spdid; */
/* 	struct cb_desc *d; */
/* 	struct cos_cbuf_item *cbi; */

/* 	assert(cbid >= 0); */

/* 	TAKE(); */
/* 	cbi = cos_map_lookup(&cb_ids, cbid); */
/* 	if (!cbi) {  */
/* 		ret = -1;  */
/* 		goto done; */
/* 	} */
/* 	d = &cbi->desc; */
	
/* 	o_spdid = d->owner.spd; */
/* 	if (o_spdid == r_spdid) goto done; */

/* 	/\* ret = mgr_update_owner(r_spdid, cbid); // -1 fail, 0 success *\/ */
/* done: */
/* 	RELEASE(); */
	return ret;   
}


void
cos_init(void)
{
	long cbid;
	CBUFP_LOCK_INIT();
	cmap_init_static(&cbufs);
	cbid = cmap_add(&cbufs, NULL);
}
