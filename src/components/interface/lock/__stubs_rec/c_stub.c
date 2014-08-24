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
different) 2) when recovery (in update_rd), we need just a new server
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

/* recovery data structure lock service */
struct blocked_thd {
	int id;
	struct blocked_thd *next, *prev;
};

struct rec_data_lk {
	spdid_t       spdid;
	int           owner_thd;  // for sanity check
	unsigned long c_lkid;
	unsigned long s_lkid;

	unsigned long fcnt;
	struct blocked_thd blkthd;
};

/**********************************************/
/* slab allocalk and cvect for tracking lock */
/**********************************************/

CVECT_CREATE_STATIC(rec_lk_vect);
CSLAB_CREATE(rdlk, sizeof(struct rec_data_lk));

static struct rec_data_lk *
rdlk_lookup(int id)
{ 
	return cvect_lookup(&rec_lk_vect, id); 
}

static struct rec_data_lk *
rdlk_alloc(int lkid)
{
	struct rec_data_lk *rd;

	rd = cslab_alloc_rdlk();
	assert(rd);
	cvect_add(&rec_lk_vect, rd, lkid);
	return rd;
}

static void
rdlk_dealloc(struct rec_data_lk *rd)
{
	assert(rd);
	cslab_free_rdlk(rd);
}

static void
rdlk_addblk(struct rec_data_lk *rd, struct blocked_thd *ptr_blkthd)
{
	assert(rd && ptr_blkthd);

	ptr_blkthd->id = cos_get_thd_id();
	INIT_LIST(ptr_blkthd, next, prev);
	ADD_LIST(&rd->blkthd, ptr_blkthd, next, prev);
       
	return;
}

static int
get_unique(void)
{
	unsigned int i;
	cvect_t *v;

	v = &rec_lk_vect;
	for(i = 1 ; i <= CVECT_MAX_ID ; i++) {
		if (!cvect_lookup(v, i)) return i;
	}
	/* if (!cvect_lookup(v, CVECT_MAX_ID)) return CVECT_MAX_ID; */

	return -1;
}

static void 
rd_cons(struct rec_data_lk *rd, spdid_t spdid, unsigned long cli_lkid, unsigned long ser_lkid, int owner_thd)
{
	assert(rd);

	rd->spdid	 = spdid;
	rd->s_lkid	 = ser_lkid;
	rd->c_lkid	 = cli_lkid;
	rd->owner_thd    = owner_thd;
	rd->fcnt	 = fcounter;

	return;
}

#ifdef REFLECTION
static int
cap_to_dest(int cap)
{
	int dest = 0;
	assert(cap > MAX_NUM_SPDS);
	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap)) <= 0) assert(0);
	return dest;
}

extern int sched_reflect(spdid_t spdid, int src_spd, int cnt);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
extern vaddr_t mman_reflect(spdid_t spd, int src_spd, int cnt);
extern int mman_release_page(spdid_t spd, vaddr_t addr, int flags); 

static void
rd_reflection(int cap)
{
	assert(cap);

	TAKE(cos_spd_id());

	int count_obj = 0; // reflected objects
	int dest_spd = cap_to_dest(cap);
	
	// remove the mapped page for lock spd
	vaddr_t addr;
	count_obj = mman_reflect(cos_spd_id(), dest_spd, 1);
	printc("evt relfects on mmgr: %d objs\n", count_obj);
	while (count_obj--) {
		addr = mman_reflect(cos_spd_id(), dest_spd, 0);
		printc("evt mman_release: %p addr\n", (void *)addr);
		mman_release_page(cos_spd_id(), addr, dest_spd);
	}

	// to reflect all threads blocked from lock component
	int wake_thd;
	count_obj = sched_reflect(cos_spd_id(), dest_spd, 1);
	printc("evt relfects on sched: %d objs\n", count_obj);
	while (count_obj--) {
		wake_thd = sched_reflect(cos_spd_id(), dest_spd, 0);
		printc("wake_thd %d\n", wake_thd);
		sched_wakeup(cos_spd_id(), wake_thd);
	}
	printc("evt reflection done (thd %d)\n\n", cos_get_thd_id());

	RELEASE(cos_spd_id());

	return;
}
#else
static void
rd_reflection(int cap)
{
	return;
}
#endif

static struct rec_data_lk *
update_rd(int lkid, int cap)
{
        struct rec_data_lk *rd = NULL;
	struct blocked_thd *blk_thd;

        rd = rdlk_lookup(lkid);
	if (unlikely(!rd)) goto done;
	printc("update_rd: rd->fcnt %ld fcounter %ld (thd %d)\n", 
	       rd->fcnt, fcounter, cos_get_thd_id());
	if (likely(rd->fcnt == fcounter)) goto done;
	rd->fcnt = fcounter;  // update fcounter first before wake up any other threads

	// update server side id, but keep client side id unchanged
	printc("thd %d is creating a new server side lock id\n", cos_get_thd_id());
	rd->s_lkid = __lock_component_alloc(cos_spd_id());
done:
	return rd;
}

CSTUB_FN(unsigned long, __lock_component_alloc) (struct usr_inv_cap *uc,
						 spdid_t spdid)
{
	long fault = 0;
	unsigned long ret;

redo:

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);		
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	return ret;
}


/************************************/
/******  client stub functions ******/
/************************************/

/* 
   lock_component_alloc will get a server side lock id every time
   client side id should always be uniquex 
*/
CSTUB_FN(unsigned long, lock_component_alloc) (struct usr_inv_cap *uc,
					       spdid_t spdid)
{
	long fault;
	unsigned long ret;

        struct rec_data_lk *rd = NULL;
	unsigned long ser_lkid, cli_lkid;
redo:
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	if ((ser_lkid = ret) > 0) {
		// if does exist, we need an unique client id. Otherwise, create it
		if (unlikely(rdlk_lookup(ser_lkid))) {
			cli_lkid = get_unique();
			assert(cli_lkid > 0 && cli_lkid != ser_lkid);
		} else {
			cli_lkid = ser_lkid;
		}
		// always ensure that cli_lkid is unique
		rd = rdlk_alloc(cli_lkid);
		assert(rd);
		
		rd_cons(rd, cos_spd_id(), cli_lkid, ser_lkid, 0);
		INIT_LIST(&rd->blkthd, next, prev);
		ret = cli_lkid;
	}
	
	return ret;
}


// usually the lock is not freed once created
CSTUB_FN(int, lock_component_free) (struct usr_inv_cap *uc,
				    spdid_t spdid, unsigned long lock_id)
{
	long fault = 0;
	int ret;

        struct rec_data_lk *rd = NULL;
        rd = update_rd(lock_id, uc->cap_no);
	if (!rd) {
		printc("try to free a non-tracking lock\n");
		return -1;
	}
	
	CSTUB_INVOKE(ret, fault, uc, 2 , spdid, rd->s_lkid);
	
	if (unlikely(fault)) {
		CSTUB_FAULT_UPDATE();
	}

	return ret;
}

/* pretake still needs trying to contend the lock, so use goto redo */

CSTUB_FN(int, lock_component_pretake) (struct usr_inv_cap *uc,
				       spdid_t spdid, unsigned long lock_id, 
				       unsigned short int thd)
{
	long fault = 0;
	int ret;
	
        struct rec_data_lk *rd = NULL;
redo:
	printc("lock cli: pretake calling update_rd %d\n", cos_get_thd_id());
        rd = update_rd(lock_id, uc->cap_no);
	if (!rd) {
		printc("try to pretake a non-tracking lock\n");
		return -1;
	}
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, rd->s_lkid, thd);
	
	if (unlikely(fault)){
		printc("lock cli pretak: found fault \n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	return ret;
}

/* track the block thread on each thread stack. No need to goto redo
 * since ret = 0 will force it to contend again with other threads
 */

CSTUB_FN(int, lock_component_take) (struct usr_inv_cap *uc,
				    spdid_t spdid, 
				    unsigned long lock_id, unsigned short int thd)
{
	long fault = 0;
	int ret;

	struct blocked_thd blk_thd;
	struct rec_data_lk *rd = NULL;
	
        rd = update_rd(lock_id, uc->cap_no);   // call call booter instead
	if (!rd) {
		printc("try to take a non-tracking lock\n");
		return -1;
	}
	rdlk_addblk(rd, &blk_thd);       
	
	CSTUB_INVOKE(ret, fault, uc, 3, spdid, rd->s_lkid, thd);
	
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		/* this will force contending the lock again */
		ret = 0; 
	}
	/*  remove current thd from blocking list since the
	    fault occurs either before the current thd blocked
	    (invoke scheduler) or after woken up (return from
	    scheduler). So it should not staty on the blocking
	    list after fault occurs */
	REM_LIST(&blk_thd, next, prev);
	
	return ret;
}

/* There is no need to goto redo since reflection will wake up all threads from lock spd */
CSTUB_FN(int, lock_component_release) (struct usr_inv_cap *uc,
				       spdid_t spdid, unsigned long lock_id)
{
	long fault = 0;
	int ret;

        struct rec_data_lk *rd = NULL;

        rd = update_rd(lock_id, uc->cap_no);
	if (!rd) {
		printc("try to release a non-tracking lock\n");
		return -1;
	}

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, rd->s_lkid);

	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
	}
	
	return ret;
}
