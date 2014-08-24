/*
  Jiguo: an event can be triggered from a different component. This
  interface will track the mapping of client and server id between
  faults. The purpose is to trigger/pass the correct server side id,
  even from a different client. 

  This is not needed for sched/mm/fs/lock since in most cases, the
  object state is updated from the same client. So tracking over the
  client interface is enough in these cases.

  Issue: cvect needs to be initialized every time after a fault in evt_wait

*/

#include <cos_component.h>
#include <evt.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

struct csmapping_evt {
	unsigned long cid;
	unsigned long sid;
};

/**********************************************/
/* slab allocaevt and cvect for tracking evts */
/**********************************************/

static int first = 0;
CSLAB_CREATE(csmapping, sizeof(struct csmapping_evt));
CVECT_CREATE_STATIC(csmapping_vect);

static struct csmapping_evt *
csmapping_lookup(int id)
{ 
	return cvect_lookup(&csmapping_vect, id); 
}

static struct csmapping_evt *
csmapping_alloc(int evtid)
{
	struct csmapping_evt *csmap;

	csmap = cslab_alloc_csmapping();
	if (!csmap) return NULL;
	printc("(evt %d)adding into csmapping cvect 1!!!!!!\n", evtid);
	cvect_add(&csmapping_vect, csmap, evtid);
	printc("(evt %d)adding into csmapping cvect 2!!!!!!\n", evtid);
	return csmap;
}

static void
csmapping_dealloc(struct csmapping_evt *csmap)
{
	assert(csmap);
	cslab_free_csmapping(csmap);
}

long __sg_evt_split(spdid_t spdid, long parent_evt, int grp)
{
	return evt_split(spdid, parent_evt, grp);
}

long __sg_evt_create(spdid_t spdid)
{
	return evt_create(spdid);
}

int __sg_evt_free(spdid_t spdid, long extern_evt)
{
	struct csmapping_evt *csmap;
	assert(extern_evt);	

	// mapping might be already gone due to fault
	csmap = csmapping_lookup(extern_evt);
	if (csmap) {
		evt_free(spdid, csmap->sid);
		csmapping_dealloc(csmap);
	}
		
	return 0;
}

long __sg_evt_wait(spdid_t spdid, long extern_evt)
{
	long cid, sid;
	struct csmapping_evt *csmap;
	
	assert(extern_evt);
	cid = extern_evt >> 16;
	sid = extern_evt & 0xFFFF;
	assert(cid && sid);
	printc("\n\nevt ser: evt_wait %d (combined id %p)\n", cos_get_thd_id(), extern_evt);
	printc("evt ser (spd %d): cid %ld sid %ld\n", cos_spd_id(), cid, sid);
	printc("from spd %d\n", spdid);
	
	csmap = csmapping_lookup(cid);
	if (!csmap) {
		csmap = csmapping_alloc(cid);
		assert(csmap);
		csmap->cid = cid;
		csmap->sid = sid;
		printc("evt ser: create... track for cid %ld (ser sid %ld)\n", cid, sid);
		printc("csmap@%p\n", csmap);
		printc("csmapping_vect@%p\n", &csmapping_vect);

		int i;
		if (!first) {
			printc("first time init csmapping vect\n");
			first = 1;
			for (i = 0 ; i < (int)CVECT_BASE ; i++) {
				if (i != cid) 
					__cvect_set(&csmapping_vect, i, (void*)CVECT_INIT_VAL);
			}
		}
	} else {
		printc("evt ser: update... track for cid %ld (ser sid %ld)\n", cid, sid);
		printc("csmap@%p\n", csmap);
		printc("csmapping_vect@%p\n", &csmapping_vect);
		csmap->cid = cid;
		assert(csmap->sid = sid);
	}

	return evt_wait(spdid, csmap->sid);
}

int __sg_evt_trigger(spdid_t spdid, long extern_evt)
{
	struct csmapping_evt *csmap;

	assert(extern_evt);	
	csmap = csmapping_lookup(extern_evt);
	assert(csmap);
	
	return evt_trigger(spdid, csmap->sid);
}
