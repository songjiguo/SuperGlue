/*
fault tolerance conscious interface code for periodic api

Notes:

1. After the fault, the c^3 version periodic_wake_create is called and
   both period and when (in ticks) the pte thread is created at the
   first place are passed as parameter. So the te->expiration_time is
   rounded up to the closest release time

2. Now we only focus on the period_wake...interface APIs, however,
   addition to period_wake_create and period_wake_wait, which are used
   by normal pte threads, the timer thread is also affected by the
   fault. So be careful.

3. reflection needs to be done on event manager and mmgr, since now
   pte threads block via evt_wait, not sched_block anymore

4. the ticks is unsigned long long, but for now I assume this can be
   hold by the register and return the ticks at which a pte thread is
   created. C3 needs to remember when the pte thread is created and
   synchronize the thread with the original period. Assume all pte
   threads are created in the first 2^32-1 ticks

*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>


#include <periodic_wake.h>
#include <cstub.h>

extern unsigned long sched_timestamp(void);
extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
#define PTE_TAKE(spdid) 	do { if (sched_component_take(spdid))    return 0; } while (0)
#define PTE_RELEASE(spdid)	do { if (sched_component_release(spdid)) return 0; } while (0)

static unsigned long fcounter = 0;

struct rec_data_pte {
	unsigned int tid;   // thread id. time event server is not a id_based server
	unsigned int period;
	unsigned int creation_ticks;   // when this pte object is created
	
	unsigned int  state;
	unsigned long fcnt;
};

/* the state of a pte thread */
enum {
	PTE_CREATED,
	PTE_WAIT
};

/**********************************************/
/* slab allocaevt and cvect for tracking pte  */
/**********************************************/

CVECT_CREATE_STATIC(rec_pte_vect);
CSLAB_CREATE(rdpte, sizeof(struct rec_data_pte));

static struct rec_data_pte *
rdpte_lookup(int id)
{ 
	return cvect_lookup(&rec_pte_vect, id); 
}

static struct rec_data_pte *
rdpte_alloc(int id)
{
	struct rec_data_pte *rd;
	rd = cslab_alloc_rdpte();
	assert(rd);
	if (cvect_add(&rec_pte_vect, rd, id)) {
		printc("can not add into cvect\n");
		BUG();
	}
	rd->tid = id;   // do this only for pte thread
	return rd;
}

static void
rdpte_dealloc(struct rec_data_pte *rd)
{
	assert(rd);
	if (cvect_del(&rec_pte_vect, rd->tid)) BUG();
	cslab_free_rdpte(rd);
}


static void 
rd_cons(struct rec_data_pte *rd, unsigned int period, int state, unsigned int creation_ticks)
{
	assert(rd);

	assert(rd->tid = cos_get_thd_id());
	rd->period	   = period;
	rd->creation_ticks = creation_ticks;
	rd->state	   = state;
	rd->fcnt	   = fcounter;
	
	return;
}

static struct rec_data_pte *
rd_update(unsigned int id, int state)
{
        struct rec_data_pte *rd;

        rd = rdpte_lookup(id);
	if (unlikely(!rd)) goto done;
	if (likely(rd->fcnt == fcounter)) goto done;
	rd->fcnt	 = fcounter;

	/* STATE MACHINE*/
	switch(state) {
	case PTE_CREATED:
                /* for create, if failed, just redo, should not be here */
		assert(0);  
		break;
	case PTE_WAIT:
		printc("in rd_update (state %d) needs recreate pte thread by thd %d\n", 
		       state, cos_get_thd_id());
		printc("rd->period %d rd->creation_ticks %d\n", 
		       rd->period, rd->creation_ticks);
		assert(rd->state == PTE_CREATED);
		c3_periodic_wake_create(cos_spd_id(), rd->period, rd->creation_ticks);
		break;
	default:
		// now only allow above two state
		assert(0);
		break;
	}

done:
	return rd;
}

static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}

/* extern int sched_reflect(spdid_t spdid, int src_spd, int cnt); */
/* extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id); */
/* extern vaddr_t mman_reflect(spdid_t spd, int src_spd, int cnt); */
/* extern int mman_release_page(spdid_t spd, vaddr_t addr, int flags);  */
/* extern int evt_trigger_all(spdid_t spdid); */
/* extern int lock_trigger_all(spdid_t spdid, int dest);   */
extern int sched_component_release(spdid_t spdid);

/**********************************/
/*          API functions         */
/**********************************/

CSTUB_FN(int, c3_periodic_wake_create)(struct usr_inv_cap *uc,
				       spdid_t spdid, unsigned int period, unsigned int ticks)
{
	int ret;
	long fault = 0;
redo:
	printc("cli: __periodic_wake_create (thd %d ticks %d period %d) for recovery\n",
	       cos_get_thd_id(), ticks, period);
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, period, ticks);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		sched_component_release(cap_to_dest(uc->cap_no));// release the lock
		goto redo;
	}

	assert(ret >= 0);
	printc("cli: __periodic_wake_create -- in spd %ld period %d \n", cos_spd_id(), period);
	
	return ret;
}

CSTUB_FN(int, periodic_wake_create)(struct usr_inv_cap *uc,
				    spdid_t spdid, unsigned int period)
{
	int ret;
	long fault = 0;
	struct rec_data_pte *rd = NULL;
redo:
	printc("cli: periodic_wake_create (thd %d period %d)\n",
	       cos_get_thd_id(), period);

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, period);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		sched_component_release(cap_to_dest(uc->cap_no));// release the lock
		goto redo;
	}

	assert(ret > 0);
	printc("cli: periodic_wake_create -- in spd %ld ticks %d period %d \n", 
	       cos_spd_id(), ret, period);

	/* /\* Even assume a thread only is made as periodic thread once, */
	/*  * the reuse of a dead thread still can happen. TODO: remove */
	/*  * the entry in the cvect when the pte thread dies. For now, */
	/*  * just reuse the same entry if this happens. Also this is */
	/*  * used to update the recreate pte thread (in this case, the */
	/*  * record is expected to exist and state is not changed) *\/ */
	if (likely(!(rd = rdpte_lookup(cos_get_thd_id())))) {
		rd = rdpte_alloc(cos_get_thd_id());
		assert(rd);
		rd_cons(rd, period, PTE_CREATED, ret);
	}

	return ret;
}

CSTUB_FN(int, periodic_wake_wait)(struct usr_inv_cap *uc,
				  spdid_t spdid)
{
	int ret;
	long fault = 0;
	struct rec_data_pte *rd = NULL;
redo:
	printc("cli: periodic_wake_wait (thd %d)\n", cos_get_thd_id());
        rd = rd_update(cos_get_thd_id(), PTE_WAIT);
	assert(rd);

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		sched_component_release(cap_to_dest(uc->cap_no));// release the lock
		goto redo;
	}

	return ret;
}


