/**************************/
/*   C3 --- Scheduelr     */
/**************************/
/* C3 -- scheduler interface Scheduler recovery: client stub interface
   recovery data structure, mainly for block/wakeup. Issue: The
   threads created in scheduler (timer/IPI/idle/init) are already
   taken cared during the reboot and reinitialize in the scheduler
   (sched_reboot(), and kernel introspection)

   -- some threads are created within scheduler (sched_init)
   -- Over this interface, only threads created from remote spds are tracked
   -- created thread is tracked here but the "deletion" is not executed
      here. The thread is killed by upcall to its base scheduler
     (destroy). So basically over the interface we track per thread
      status. Also a thread can be reused (from graveyard) */

#include <cos_component.h>
#include <cos_debug.h>

#include <sched.h>
#include <cstub.h>

#include <print.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

static volatile unsigned long last_ticks = 0;

static unsigned long long meas_start, meas_end;
static int meas_flag = 0;

/* global fault counter, only increase, never decrease */
static unsigned long global_fault_cnt;

static unsigned int timer_thread;

/* recovery data structure for threads */
struct rec_data_thd {
	unsigned int thd;
	unsigned int dep_thd;

	unsigned int lock_state;    // if this thread is holding a scheduler lock
	
	unsigned int  state;
	unsigned long fcnt;
};

/* the state of a thread object */
enum {
	THD_STATE_CREATE,           // other remote components call this
	THD_STATE_CREATE_DEFAULT,   // now, only booter thread does this
	THD_STATE_WAKEUP,
	THD_STATE_BLOCK,
	THD_STATE_RUNNING,
	THD_STATE_LOCK_TAKE,      // a thread might be associated with sched lock
	THD_STATE_LOCK_RELEASE
};

/*******************************************/
/* tracking thread state for data recovery */
/*******************************************/
CVECT_CREATE_STATIC(thd_vect);
CSLAB_CREATE(thdrdt, sizeof(struct rec_data_thd));

static struct rec_data_thd *
thdrdt_lookup(int thd)
{
	return cvect_lookup(&thd_vect, thd);
}

static struct rec_data_thd *
thdrdt_alloc(int thd)
{
	struct rec_data_thd *thd_track;

	thd_track = cslab_alloc_thdrdt();
	assert(thd_track);
	if (cvect_add(&thd_vect, thd_track, thd)) {
		printc("can not add into cvect\n");
		BUG();
	}
	thd_track->thd = thd;
	return thd_track;
}

static void
thdrdt_dealloc(struct rec_data_thd *thd_track)
{
	assert(thd_track);
	if (cvect_del(&thd_vect, thd_track->thd)) BUG();
	cslab_free_thdrdt(thd_track);
}

static void
rd_cons(struct rec_data_thd *rd, int thd, int dep_thd, int state)
{
	assert(rd);

	rd->thd		= thd;
	rd->dep_thd	= dep_thd;
	rd->lock_state	= 0;
	rd->state	= state;
	rd->fcnt	= global_fault_cnt;

	return;
}

static struct rec_data_thd *
rd_update(int thd, int target_thd, int state)
{
        struct rec_data_thd *rd = NULL;

	/* target_thd is dep_thd for wakeup, is thd_id for block  */

	/* in state machine, we track/update the thread's state and
	 * the associated locks. We can not track exactly the state of
	 * a thread since the preemption (thread switching) does not
	 * occur through the interface. But we need track the
	 * sched_component_take/release. For example, the fault occurs
	 * while a thread is holding such lokc, and later on when it
	 * releases the lock, we need know the lock status (e.g,
	 * boother thread fail during sched_create_thread_default) */

	if (unlikely(!(rd = thdrdt_lookup(thd)))) {
		rd = thdrdt_alloc(cos_get_thd_id());
		rd_cons(rd, thd, target_thd, state);
		rd->state = state;
		rd->fcnt = global_fault_cnt;
	}
	if (likely(rd->fcnt == global_fault_cnt)) goto done;
	rd->fcnt = global_fault_cnt;
	
	/* if (cos_get_thd_id() == timer_thread) sched_timeout_thd(cos_spd_id()); */

	/* printc("State Machine thd %d in spd %ld -- ", cos_get_thd_id(), cos_spd_id()); */
	/* /\* STATE MACHINE *\/ */
	/* switch (state) { */
	/* case THD_STATE_CREATE: */
	/* 	/\* The re-creation of these threads should be taken */
	/* 	 * cared by the scheduler sched_reboot + kernel */
	/* 	 * reflection already. See scheduler recovery code *\/ */
	/* 	printc("rd_update: THD_STATE_CREATE\n"); */
	/* 	break; */
	/* case THD_STATE_CREATE_DEFAULT: */
	/* 	printc("rd_update: THD_STATE_CREATE_DEFAULT\n"); */
	/* 	/\* if (rd->lock_state) sched_component_take(cos_spd_id()); *\/ */
	/* 	break; */
	/* case THD_STATE_WAKEUP: */
	/* 	printc("rd_update: THD_STATE_WAKEUP (thd %d)\n", rd->dep_thd); */
	/* 	/\* cos_sched_cntl(COS_SCHED_BREAK_PREEMPTION_CHAIN, 0, 0); *\/ */
	/* 	break; */
	/* case THD_STATE_BLOCK: */
	/* 	printc("rd_update: THD_STATE_BLOCK\n"); */
	/* 	/\* rd->dep_thd = target_thd; *\/ */
	/* 	break; */
	/* case THD_STATE_LOCK_TAKE: */
	/* 	printc("rd_update: THD_STATE_LOCK_TAKE\n"); */
	/* 	break; */
	/* case THD_STATE_LOCK_RELEASE: */
	/* 	printc("rd_update: THD_STATE_LOCK_RELEASE\n"); */
	/* 	/\* if (rd->lock_state) sched_component_take(cos_spd_id()); *\/ */
	/* 	break; */
	/* case THD_STATE_RUNNING: */
	/* 	printc("rd_update: THD_STATE_LOCK_RUNNING\n"); */
	/* 	assert(0); */
	/* 	break; */
	/* default: */
	/* 	assert(0); */
	/* } */
	/* printc("thd %d restore scheduler done!!!\n", cos_get_thd_id()); */
done:
	return rd;
}

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN(int, sched_create_thd) (struct usr_inv_cap *uc,
				 spdid_t spdid, u32_t sched_param0,
				 u32_t sched_param1, u32_t sched_param2)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
        struct rec_data_thd *rd = NULL;

redo:
	rd = rd_update(cos_get_thd_id(), 0, THD_STATE_CREATE);
	assert(rd);

	/* printc("thread %d calls << sched_create_thd >>\n", cos_get_thd_id()); */

#ifdef MEASU_SCHED_INTERFACE_CREATE
	rdtscll(start);
#endif
	
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, sched_param0, sched_param1, sched_param2);
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	assert(ret > 0);
	/* printc("sched_create_thd done!!! ... new thread %d is created\n\n", ret); */

	assert(rd);
	rd->state = THD_STATE_RUNNING;

	return ret;
}


/* CSTUB_FN(int, sched_create_thread_default) (struct usr_inv_cap *uc, */
/* 					    spdid_t spdid, u32_t sched_param0, */
/* 					    u32_t sched_param1, u32_t sched_param2) */
/* { */
/* 	long fault = 0; */
/* 	long ret; */

/* 	unsigned long long start, end; */
/*         struct rec_data_thd *rd = NULL; */

/* redo: */
/* 	rd = rd_update(cos_get_thd_id(), 0, THD_STATE_CREATE_DEFAULT); */
/* 	assert(rd); */

/* 	/\* printc("thread %d calls << sched_create_thd_default >>\n", cos_get_thd_id()); *\/ */

/* 	CSTUB_INVOKE(ret, fault, uc, 4, spdid, sched_param0, sched_param1, sched_param2); */
/*         if (unlikely (fault)){ */
/* 		CSTUB_FAULT_UPDATE(); */
/* 		goto redo; */
/* 	} */

/* 	/\* printc("sched_create_thread_default ret %d\n", ret); *\/ */
/* 	/\* assert(ret > 0); *\/   // Jiguo: change this back later */

/* 	printc("cli:sched_create_thd_default done!!! ... new thread %d created\n\n", ret); */

/* 	assert(rd); */
/* 	rd->state = THD_STATE_RUNNING; */

/* 	return ret; */
/* } */

CSTUB_FN(int, sched_wakeup) (struct usr_inv_cap *uc,
			     spdid_t spdid, unsigned short int dep_thd)
{
	long fault = 0;
	long ret;

        unsigned long long start, end;
        struct rec_data_thd *rd = NULL;
redo:

	rd = rd_update(cos_get_thd_id(), dep_thd, THD_STATE_WAKEUP);
	assert(rd);
	
	printc("cli:thread %d calls << sched_wakeup thd %d>>\n",
	       cos_get_thd_id(), dep_thd);

#ifdef MEASU_SCHED_INTERFACE_WAKEUP
	rdtscll(start);
#endif
	
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, dep_thd);
	if (unlikely (fault)){
		printc("see a fault during sched_wakeup (thd %d in spd %ld dep thd %d)\n",
		       cos_get_thd_id(), cos_spd_id(), dep_thd);
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

#ifdef MEASU_SCHED_INTERFACE_WAKEUP
	rdtscll(end);
	printc("<<< entire cost (sched_wakeup): %llu >>>>\n", (end-start));
#endif

	assert(rd);
	rd->state = THD_STATE_RUNNING;

	return ret;
}

CSTUB_FN(int, sched_block) (struct usr_inv_cap *uc,
			    spdid_t spdid, unsigned short int thd_id)
{
	long fault = 0;
	long ret;

        unsigned long long start, end;
        struct rec_data_thd *rd = NULL;

redo:
        rd = rd_update(cos_get_thd_id(), thd_id, THD_STATE_BLOCK);
	assert(rd);

	printc("cli: thread %d calls from spd %d << sched_block thd %d>>\n",
	       cos_get_thd_id(), spdid, thd_id);
	
#ifdef MEASU_SCHED_INTERFACE_BLOCK
	rdtscll(start);
#endif
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, thd_id);
	if (unlikely (fault)){
		printc("see a fault during sched_block (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

#ifdef MEASU_SCHED_INTERFACE_BLOCK
	rdtscll(end);
	printc("<<< entire cost (sched_block): %llu >>>>\n", (end-start));
#endif

	assert(rd);
	rd->state = THD_STATE_RUNNING;
	       
	return ret;
}


CSTUB_FN(int, sched_component_take) (struct usr_inv_cap *uc,
				     spdid_t spdid)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
        struct rec_data_thd *rd = NULL;

redo:
        rd = rd_update(cos_get_thd_id(), 0, THD_STATE_LOCK_TAKE);
	assert(rd);

	/* printc("thread %d calls << sched_component_take >>\n",cos_get_thd_id()); */

#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
	rdtscll(start);
#endif

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		printc("see a fault during sched_component_take (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
	rdtscll(end);
	printc("<<< entire cost (sched_component_take): %llu >>>>\n", (end-start));
#endif

	assert(rd && rd->thd == cos_get_thd_id());
	rd->lock_state = 1;
	rd->state = THD_STATE_RUNNING;
	
	return ret;
}

CSTUB_FN(int, sched_component_release) (struct usr_inv_cap *uc,
					spdid_t spdid)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
        struct rec_data_thd *rd = NULL;

redo:
        rd = rd_update(cos_get_thd_id(), 0, THD_STATE_LOCK_RELEASE);
	assert(rd);

	/* printc("thread %d calls << sched_component_release >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
	rdtscll(start);
#endif
      
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		printc("see a fault during sched_component_release (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		/* goto redo; */
	}
	
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
	rdtscll(end);
	printc("<<< entire cost (sched_component_release): %llu >>>>\n", (end-start));
#endif

	assert(rd && rd->thd == cos_get_thd_id());
	rd->state = THD_STATE_RUNNING;
	rd->lock_state = 0;
	
	return ret;
}

/* only used by timed_evt */
CSTUB_FN(int, sched_timeout_thd) (struct usr_inv_cap *uc,
				  spdid_t spdid)
{
	long fault = 0;
	long ret;

redo:
	/* printc("<< cli: sched_timeout_thd >>>\n"); */

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		printc("see a fault during sched_timeout_thd (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	/* timer_thread = cos_get_thd_id(); */

	return ret;
}

CSTUB_FN(int, sched_timeout) (struct usr_inv_cap *uc,
			      spdid_t spdid, unsigned long amnt)
{
	long fault = 0;
	long ret;
redo:
	/* printc("<< cli: sched_timeout >>>\n"); */

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, amnt);
	if (unlikely (fault)){
		printc("see a fault during sched_timeout (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		sched_restore_ticks(last_ticks);
		goto redo;
	}
	
	return ret;
}

CSTUB_FN(unsigned long, sched_timestamp) (struct usr_inv_cap *uc)
{
	long fault = 0;
	long ret;

redo:
	/* printc("<< cli: sched_timestamp >>>\n"); */

	CSTUB_INVOKE_NULL(ret, fault, uc);
	if (unlikely (fault)){
		printc("see a fault during sched_timestamp (thd %d in spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	if (last_ticks > ret) {
		sched_restore_ticks(last_ticks);
		ret = last_ticks;
	} else last_ticks = ret;
	
	return ret;
}

CSTUB_FN(int, sched_create_net_acap) (struct usr_inv_cap *uc,
				      spdid_t spdid, int acap_id, unsigned short int port)
{
	long fault = 0;
	long ret;
	
redo:
	printc("<< cli: sched_create_net_acap >>>\n");

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, acap_id, port);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}
