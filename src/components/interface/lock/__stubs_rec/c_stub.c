/* lock recovery client stub interface : Jiguo 

 When the lock service is faulty, there are two options to wake up the
 blocked threads in scheduler (they have no way to know the lock's
 status) -- Add the current thread onto blocked list 1) Track the
 blocked threads based on per lock over an interface 2) Wake up all
 blked threads on the lock if any blocked thread returns 3) Remove the
 items from list when the lock is released 

 See the reflection on scheduler server interface Question 1: When
 should we wake up all threads that invoke sched_block from lock
 component? When to wake the threads per lock from lock component?
 Question 2: Should we make the reflection from the client or the
 rebooted faulty component?  If do so from client, there is one issue
 :all clients need depend on the bottom component, which seems fine
 since there is no circular dependency created.

When get new lock id (alloc), there are two circumstances 1) on the
normal path (no fault or after the fault), we need get a new unique
client id (server id could be the same as client id, or totally
different) 2) when recovery (in rd_update), we need just a new server
id and unchanged client id

ps: in most cases the lock is not freed once created. so rd_dealloc is
not used

ps: not use ipc_fault_update on invocation path in the kernel anymore,
only when the client detects such fault and update fault counter on
capability over that interface 

*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <objtype.h>
#include <cos_map.h>

#include <sched.h>
#include <lock.h>
#include <cstub.h>

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
#define TAKE(spdid) 	do { if (sched_component_take(spdid))    return; } while (0)
#define RELEASE(spdid)	do { if (sched_component_release(spdid)) return; } while (0)

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

struct rec_data_lk {
	spdid_t       spdid;
	unsigned long c_lkid;
	unsigned long s_lkid;

	int state;
	unsigned long fcnt;
};

/* the state of an lock object */
enum {
	LOCK_ALLOC,
	LOCK_PRETAKE,
	LOCK_TAKE,
	LOCK_RELEASE
};

/**********************************************/
/* slab allocalk and cvect for tracking lock */
/**********************************************/

COS_MAP_CREATE_STATIC(uniq_lkids);
CSLAB_CREATE(rdlk, sizeof(struct rec_data_lk));

static struct rec_data_lk *
rdlk_lookup(int id)
{
	return (struct rec_data_lk *)cos_map_lookup(&uniq_lkids, id); 
}

static int
rdlk_alloc()
{
	struct rec_data_lk *rd = NULL;
	int map_id = 0;
	// lock return lock id from 1, and cos_map starts from 0
	// here want cos_map to return some ids at least from 1 and later
	while(1) {
		rd = cslab_alloc_rdlk();
		assert(rd);	
		map_id = cos_map_add(&uniq_lkids, rd);
		if (map_id >= 1) break;
		rd->s_lkid = -1;  // -1 means that this is a dummy record
	}
	assert(map_id >= 1);
	return map_id;	
}

static void
rdlk_dealloc(int id)
{
	assert(id >= 0);
	struct rec_data_lk *rd;
	rd = rdlk_lookup(id);
	assert(rd);
	cslab_free_rdlk(rd);
	cos_map_del(&uniq_lkids, id);
	return;
}

static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}

static void 
rd_cons(struct rec_data_lk *rd, spdid_t spdid, unsigned long c_lkid, 
	unsigned long s_lkid, int state)
{
	assert(rd);

	rd->spdid	 = spdid;
	rd->c_lkid	 = c_lkid;
	rd->s_lkid	 = s_lkid;
	rd->state	 = state;
	rd->fcnt	 = fcounter;

	return;
}

static void
rd_recover_state(struct rec_data_lk *rd)
{
	assert(rd && rd->c_lkid);
	printc("thd %d is creating a new server side lock id\n", cos_get_thd_id());

	struct rec_data_lk *tmp;
	int tmp_lkid = lock_component_alloc(cos_spd_id());
	assert(tmp_lkid);
	
	assert((tmp = rdlk_lookup(tmp_lkid)));
	rd->s_lkid = tmp->s_lkid;
	rdlk_dealloc(tmp_lkid);
	
	return;
}

static struct rec_data_lk *
rd_update(int lkid, int state)
{
        struct rec_data_lk *rd = NULL;

        rd = rdlk_lookup(lkid);
	if (unlikely(!rd)) goto done;
	/* printc("rd_update: rd->fcnt %ld fcounter %ld (thd %d)\n",  */
	/*        rd->fcnt, fcounter, cos_get_thd_id()); */
	if (likely(rd->fcnt == fcounter)) goto done;

	rd->fcnt = fcounter;

	/* Jiguo: rebuild the lock state using the following state
	 * machine */

	/* STATE MACHINE */
	switch (state) {
	case LOCK_ALLOC:
		/* lock_component_alloc will get a server side lock id
		 * every time client side id should always be
		 * unique */
	case LOCK_PRETAKE:
		/* pretake still needs trying to contend the lock
		 * since the client has not changed the owner yet. So
		 * use goto redo */
	case LOCK_TAKE:
		/* track the block thread on each thread stack. No
		 * need to goto redo since ret = 0 will force it to
		 * contend again with other threads */
	case LOCK_RELEASE:
		/* There is no need to goto redo since reflection will
		 * wake up all threads from lock spd and client has
		 * set owner to be 0 */
		rd_recover_state(rd);
		break;
	default:
		assert(0);
		break;
	}
done:
	return rd;
}

/************************************/
/******  client stub functions ******/
/************************************/
extern int sched_reflection_component_owner(spdid_t spdid);
static int first = 0;

CSTUB_FN(unsigned long, lock_component_alloc) (struct usr_inv_cap *uc,
					       spdid_t spdid)
{
	long fault;
	unsigned long ret;

        struct rec_data_lk *rd = NULL;
	unsigned long ser_lkid, cli_lkid;

        if (first == 0) {
		cos_map_init_static(&uniq_lkids);
		first = 1;
	}

redo:
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		int dest = cap_to_dest(uc->cap_no);
		int tmp_owner = sched_reflection_component_owner(dest);
		if (tmp_owner == cos_get_thd_id()) {
			sched_component_release(cap_to_dest(uc->cap_no));
		}
		goto redo;
	}
	
	assert(ret > 0);

	cli_lkid = rdlk_alloc();
	assert(cli_lkid >= 1);
	rd = rdlk_lookup(cli_lkid);
        assert(rd);
	
	rd_cons(rd, cos_spd_id(), cli_lkid, ret, LOCK_ALLOC);
	ret = cli_lkid;

	return ret;
}


CSTUB_FN(int, lock_component_pretake) (struct usr_inv_cap *uc,
				       spdid_t spdid, unsigned long lock_id, 
				       unsigned short int thd)
{
	long fault = 0;
	int ret;
	
        struct rec_data_lk *rd = NULL;
redo:
	printc("lock cli: pretake calling rd_update %d\n", cos_get_thd_id());
        rd = rd_update(lock_id, LOCK_PRETAKE);
	if (!rd) {
		printc("try to pretake a non-tracking lock\n");
		return -1;
	}
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, rd->s_lkid, thd);
	
	if (unlikely(fault)){
		CSTUB_FAULT_UPDATE();
		int dest = cap_to_dest(uc->cap_no);
		int tmp_owner = sched_reflection_component_owner(dest);
		if (tmp_owner == cos_get_thd_id()) {
			sched_component_release(cap_to_dest(uc->cap_no));
		}
		goto redo;  // update the generation number
	}
	
	return ret;
}

CSTUB_FN(int, lock_component_take) (struct usr_inv_cap *uc,
				    spdid_t spdid, 
				    unsigned long lock_id, unsigned short int thd)
{
	long fault = 0;
	int ret;

	struct rec_data_lk *rd = NULL;
	
        rd = rd_update(lock_id, LOCK_TAKE);
	if (!rd) {
		printc("try to take a non-tracking lock\n");
		return -1;
	}
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, rd->s_lkid, thd);
	
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		int dest = cap_to_dest(uc->cap_no);
		int tmp_owner = sched_reflection_component_owner(dest);
		if (tmp_owner == cos_get_thd_id()) {
			sched_component_release(cap_to_dest(uc->cap_no));
		}
		/* this will force contending the lock again */
		ret = 0; 
	}

	return ret;
}

/* Here we have reset the owner to be 0 on the client side, so we do
 * not goto redo */
CSTUB_FN(int, lock_component_release) (struct usr_inv_cap *uc,
				       spdid_t spdid, unsigned long lock_id)
{
	long fault = 0;
	int ret;

        struct rec_data_lk *rd = NULL;

        rd = rd_update(lock_id, LOCK_RELEASE);
	if (!rd) {
		printc("try to release a non-tracking lock\n");
		return -1;
	}

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_lkid);

	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		int dest = cap_to_dest(uc->cap_no);
		int tmp_owner = sched_reflection_component_owner(dest);
		if (tmp_owner == cos_get_thd_id()) {
			sched_component_release(cap_to_dest(uc->cap_no));
		}
	}
	
	return ret;
}
