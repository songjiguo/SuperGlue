/***************************************************************
  Jiguo: This is the header file to help the kernel keep clean 
         for recovery related changes
         depends on gcc will optimize away for i = 0 case
***************************************************************/

#ifndef RECOVERY_H
#define RECOVERY_H

#include "spd.h"
#include "thread.h"

//#define MEASURE_COST

#ifdef MEASURE_COST
#define MEAS_INV_FAULT_DETECT   /* measure the fault detection cost for invocation */
#define MEAS_RET_FAULT_DETECT   /* measure the fault detection cost for return */
#define MEAS_TCS_FAULT_DETECT	/* measure the fault detection cost for thread context switch */
#define MEAS_INT_FAULT_DETECT   /* measure the fault detection cost for interrupt */
#endif

/* type indicates it is HRT(0) or BEST(1) */

/*---------Threads that created by scheduler--------*/
static int
sched_thread_lookup(struct spd *spd, int thd_id, int operation, int hrt)
{
	struct thread *thd;
	struct thd_sched_info *tsi;
	struct spd *cur_spd = spd;
	int ret = 0;
	
	thd = thd_get_by_id(thd_id);
	if (!thd) {
		printk("cos: thd id %d invalid (when record dest)\n", (unsigned int)thd_id);
		return -1;
	}
		
	if  (!cur_spd->sched_depth) cur_spd = spd_get_by_index(2);   // change scheduler
	
	tsi = thd_get_sched_info(thd, cur_spd->sched_depth);
	if (tsi->scheduler != cur_spd) {
		printk("cos: spd %d not the scheduler of %d\n",
		       spd_get_index(cur_spd), (unsigned int)thd_id);
		return -1;
	}

	if (spd_is_scheduler(spd) && !spd_is_root_sched(spd)){
		/* printk("cos: look up thread %d info\n", thd_id); */
		switch (operation) {
		case COS_SCHED_INTRO_THD_DEST:
			return thd->sched_info[cur_spd->sched_depth].thread_dest;
		case COS_SCHED_INTRO_THD_METRIC:
			return thd->sched_info[cur_spd->sched_depth].thread_metric;
		case COS_SCHED_INTRO_THD_FN:
			return thd->sched_info[cur_spd->sched_depth].thread_fn;
		case COS_SCHED_INTRO_THD_D:
			return thd->sched_info[cur_spd->sched_depth].thread_d;
		case COS_SCHED_INTRO_THD_PARAM:
			return thd->sched_info[cur_spd->sched_depth].thread_param;
			break;
		default:
			assert(0);
		}
	}
	return 0;
}

static int
sched_thread_info(struct spd *spd, int thd_id, int operation, int hrt)
{
	struct thread *thd;
	struct thd_sched_info *tsi;
	int ret = 0;
	int i;
		
	if (hrt) {
		for (i = thd_id  ; i < MAX_NUM_THREADS ; i++) {
			thd = &threads[i];
			if (thd->sched_info[spd->sched_depth].thread_dest > 0) {
				if (operation == COS_SCHED_INTRO_THD_FIRST) {
					return thd->thread_id;  // first tracked thd id
				}
				ret++;
			}
		}
	}
	else {
		/* if (!spd->scheduler_bes_threads) goto done; */
		/* ret = spd->scheduler_bes_threads->thd_cnts; */
	}
done:
	return ret;
}

static int
sched_thread_add(struct spd *spd, int thd_id, int option, int operation, int hrt)
{
	struct thread *thd;
	struct thd_sched_info *tsi;
		
	thd = thd_get_by_id(thd_id);
	if (!thd) {
		printk("cos: thd id %d invalid (when record dest)\n", (unsigned int)thd_id);
		return -1;
	}
		
	tsi = thd_get_sched_info(thd, spd->sched_depth);
	if (tsi->scheduler != spd) {
		printk("cos: spd %d not the scheduler of %d\n",
		       spd_get_index(spd), (unsigned int)thd_id);
		return -1;
	}

	if (spd_is_scheduler(spd) && !spd_is_root_sched(spd)){
		/* printk("cos: add thread %d onto spd %d list (operation %d, val %d)\n",  */
		/*        thd_id, spd_get_index(spd), operation, option); */
		switch (operation) {
		case COS_SCHED_THD_DEST:
			thd->sched_info[spd->sched_depth].thread_dest = option;
			break;
		case COS_SCHED_THD_METRIC:
			thd->sched_info[spd->sched_depth].thread_metric = option;
			break;
		case COS_SCHED_THD_FN:
			thd->sched_info[spd->sched_depth].thread_fn = option;
			break;
		case COS_SCHED_THD_D:
			thd->sched_info[spd->sched_depth].thread_d = option;
			break;
		case COS_SCHED_THD_PARAM:
			thd->sched_info[spd->sched_depth].thread_param = option;
			break;
		default:
			assert(0);
		}
		thd->sched_info[spd->sched_depth].thread_hrt = hrt;
	}
	
	return 0;
}

static int
sched_thread_remove(struct spd *spd, int thd_id)
{
	struct thread *thd;
	struct thd_sched_info *tsi;
	
	thd = thd_get_by_id(thd_id);
	if (!thd) {
		printk("cos: thd id %d invalid (when record dest)\n", (unsigned int)thd_id);
		return -1;
	}
	
	tsi = thd_get_sched_info(thd, spd->sched_depth);
	if (tsi->scheduler != spd) {
		printk("cos: spd %d not the scheduler of %d\n",
		       spd_get_index(spd), (unsigned int)thd_id);
		return -1;
	}

	if (spd_is_scheduler(spd) && !spd_is_root_sched(spd)){
		/* printk("cos: remove all tracking info for thread %d\n", thd_id); */
		thd->sched_info[spd->sched_depth].thread_dest	= 0;
		thd->sched_info[spd->sched_depth].thread_metric = 0;
		thd->sched_info[spd->sched_depth].thread_fn	= 0;
		thd->sched_info[spd->sched_depth].thread_d	= 0;
		thd->sched_info[spd->sched_depth].thread_param	= 0;
		thd->sched_info[spd->sched_depth].thread_hrt	= 0;
	}
	
	return 0;
}

#if (RECOVERY_ENABLE == 1)
//*************************************************
/* enable recovery */
//*************************************************

/*---------Fault Notification Operations--------*/
static inline int
init_spd_fault_cnt(struct spd *spd)
{
	spd->fault.cnt = 0;
	return 0;
}

static inline int
init_spd_reflect_cnt(struct spd *spd)
{
	spd->reflection.cnt = 0;
	return 0;
}

static inline int
init_cap_fault_cnt(struct invocation_cap *cap)
{
	cap->fault.cnt = 0;
	return 0;
}

static inline int
init_invframe_fault_cnt(struct thd_invocation_frame *inv_frame)
{
	inv_frame->fault.cnt = 0;
	return 0;
}

/**************  detect ************/
static inline int
ipc_fault_detect(struct invocation_cap *cap_entry, struct spd *dest_spd)
{
	/* // init thread does not need recover when exit */
	/* if (thd_get_id(core_get_curr_thd_id(get_cpuid_fast())) == 3) return 0; */

	if (cap_entry->fault.cnt < dest_spd->fault.cnt) {
		/* printk("cap_entry fault cnt %lu\n", cap_entry->fault.cnt); */
		/* printk("dest spd %d fault cnt %lu\n", spd_get_index(dest_spd), dest_spd->fault.cnt); */
		/* inv_frame->fault.cnt = spd->fault.cnt; */   // we update cap
		return 1;
	}
	else return 0;
}

extern void print_regs(struct pt_regs *regs);

static inline int
pop_fault_detect(struct thd_invocation_frame *frame)
{
	if (spd_get_index(frame->spd) == 21 ||
	    spd_get_index(frame->spd) == 1) return 0;  // test pgfault spd (or deps)

	/* if (spd_get_index(frame->spd) == 18) { */
	/* 	struct thread *curr = core_get_curr_thd_id(get_cpuid_fast()); */
	/* 	printk("cos:pop_frame spd %d spd fault cnt %d frame fault cnt %d (thd %d)\n",  */
	/* 	       spd_get_index(frame->spd), frame->spd->fault.cnt, */
	/* 	       frame->fault.cnt, thd_get_id(curr)); */
	/* 	/\* print_regs(&curr->regs); *\/ */
	/* } */
	if (frame->fault.cnt < frame->spd->fault.cnt) {
		printk("cos: pop_frame spd %d spd fault cnt %d frame fault cnt %d\n", 
		       spd_get_index(frame->spd), frame->spd->fault.cnt,
		       frame->fault.cnt);
		frame->fault.cnt = frame->spd->fault.cnt;
		return 1;
	}
	else return 0;
}

static inline int
switch_thd_fault_detect(struct thread *next)
{
	struct spd *n_spd;
	struct thd_invocation_frame *tif;
	
	tif    = thd_invstk_top(next);
	assert(tif);
	n_spd  = tif->spd;
	
	/* if (thd_get_id(next) == 10) return 0;  // network thread has no sched interface */
	
	if (tif->fault.cnt < n_spd->fault.cnt) 
	{
		printk("thread %d fault cnt %d\n", thd_get_id(next), tif->fault.cnt);
		printk("spd %d fault cnt %d\n", spd_get_index(n_spd), n_spd->fault.cnt);
		/* struct thd_invocation_frame *tmp = thd_invstk_base(next); */
		/* printk("home spd %d\n", spd_get_index(tmp->spd)); */
		/* if (tmp == tif) return 0;  // home spd */
		tif->fault.cnt = n_spd->fault.cnt;
		return 1;
	}
	else return 0;
}

static inline int
interrupt_fault_detect(struct thread *next)
{
	struct spd *n_spd;
	struct thd_invocation_frame *tif;

	tif    = thd_invstk_top(next);
	assert(tif);
	n_spd  = tif->spd;
	assert(n_spd);
	
	if (tif->fault.cnt < n_spd->fault.cnt) 
	{
		printk("int: thread %d fault cnt %d\n", thd_get_id(next), tif->fault.cnt);
		printk("spd %d fault cnt %d\n", spd_get_index(n_spd), n_spd->fault.cnt);
		tif->fault.cnt = n_spd->fault.cnt;
		return 1;
	}
	else return 0;
}
/**************  update ************/
static inline int
inv_frame_fault_cnt_update(struct thd_invocation_frame *inv_frame, struct spd *spd)
{
	inv_frame->fault.cnt = spd->fault.cnt;
	/* printk("inv_frame_fltcnt_update: n_spd %d\n", spd_get_index(spd)); */
	return 0;
}

static inline int
ipc_fault_update(struct invocation_cap *cap_entry, struct spd *dest_spd)
{
	cap_entry->fault.cnt = dest_spd->fault.cnt;
	return 0;
}

static inline int
pop_fault_update(struct thd_invocation_frame *frame)
{
	frame->fault.cnt = frame->spd->fault.cnt;
	return 0;
}

static inline int
switch_thd_fault_update(struct thread *thd)
{
	struct spd *n_spd;
	struct thd_invocation_frame *tif;

	tif    = thd_invstk_top(thd);
	n_spd  = tif->spd;
	tif->fault.cnt = n_spd->fault.cnt;
	/* printk("switch_flt_update: thd %d n_spd %d\n", thd_get_id(thd), spd_get_index(n_spd)); */
	return 0;
}

static inline int
interrupt_fault_update(struct thread *next) /* timer or network thread */
{
	struct spd *n_spd;
	struct thd_invocation_frame *tif;

	tif    = thd_invstk_top(next);
	n_spd  = tif->spd;
	
	tif->fault.cnt = n_spd->fault.cnt;
	return 0;
}

/*
  For COS_CAP_FAULT_UPDATE:
  return 0: successfully trigger or update (client needs fcounter++)
  return -1: error
  return n: how many times that the faulty component has occurred
*/

static inline int
fault_cnt_syscall_helper(int spdid, int option, spdid_t d_spdid, unsigned int cap_no)
{
	int i;
	int ret = 0;
	struct spd *d_spd, *this_spd, *dest_spd;
	struct invocation_cap *cap_entry;
	unsigned int cap_no_origin;

	this_spd  = spd_get_by_index(spdid);
	d_spd     = spd_get_by_index(d_spdid);

	/* printk("passed this_spd is %d\n", spdid); */
	/* printk("passed d_spd is %d\n", d_spdid); */

	if (!this_spd || !d_spd) {
		printk("cos: invalid fault cnt  call for spd %d or spd %d\n",
		       spdid, d_spdid);
		return -1;
	}

	cap_no >>= 20;

	if (unlikely(cap_no >= MAX_STATIC_CAP)) {
		printk("cos: capability %d greater than max\n",
		       cap_no);
		return -1;
	}

	cap_entry = &d_spd->caps[cap_no];

	/* printk("cos: cap_no is %d\n", cap_no); */
	/* printk("cos: cap_entry %p\n", (void *)cap_entry); */
	/* printk("cap_entry fcn %d and its destination fcn %d\n", cap_entry->fault.cnt, */
	/*        cap_entry->destination->fault.cnt); */
	/* printk("dest_spd is %d\n", spd_get_index(cap_entry->destination)); */

	/* for (i = 1; i < d_spd->ncaps ; i++) { */
	/* 	printk("cap_entry->destination %d\n", spd_get_index(d_spd->caps[i].destination)); */
	/* } */
	/* printk("2\n"); */

	switch(option) {
	case COS_SPD_FAULT_TRIGGER:
		/* printk("increase fault counter for spd %d\n", d_spdid); */
		d_spd->fault.cnt++;
		d_spd->reflection.cnt = d_spd->fault.cnt;
		break;
	case COS_CAP_FAULT_UPDATE: 		/* Update fault counter for this client */
		/* Update the fault counter for invocation (e.g.,
		 * destination spd)*/
		if (cap_entry->fault.cnt == cap_entry->destination->fault.cnt) {
			ret = cap_entry->destination->fault.cnt;
			/* printk("(fault update 1) ret %d\n", ret); */
			break;
		}
		
		for (i = 1; i < d_spd->ncaps ; i++) {
			struct invocation_cap *cap = &d_spd->caps[i];
			if (cap->destination == cap_entry->destination) {
				/* printk("update fcnt: d_spd %d -> spd %d (dest on fault detect: %d)\n", */
				/*        d_spdid, spd_get_index(cap->destination), */
				/*        spd_get_index(cap_entry->destination)); */
				cap->fault.cnt = cap_entry->destination->fault.cnt;
			}
		}
		ret = cap_entry->destination->fault.cnt;
		/* printk("(fault update 2) ret\n", ret); */
		break;
	case COS_CAP_REFLECT_UPDATE: 		/* Update reflect counter for this client */
		/* printk("check if reflection counter\n"); */
		if (d_spd->fault.cnt == 0) return 0; // no fault ever
		if (d_spd->reflection.cnt 
		    == d_spd->fault.cnt) {
			/* printk("(1)d_spd->reflection.cnt %d d_spd->fault.cnt %d\n", */
			/*        d_spd->reflection.cnt, d_spd->fault.cnt); */
			d_spd->reflection.cnt++; // to avoid multiple calls
			ret = 1;
		}
		break;
	default:
		assert(0);
		break;
	}
	
	return ret;
}

#else
//*************************************************
/* disable recovery */
//*************************************************
static inline int
init_spd_fault_cnt(struct spd *spd)
{
	return 0;
}

static inline int
init_cap_fault_cnt(struct invocation_cap *cap)
{
	return 0;
}

static inline int
init_invframe_fault_cnt(struct thd_invocation_frame *inv_frame)
{
	return 0;
}

static inline int
inv_frame_fault_cnt_update(struct thread *thd, struct spd *spd)
{
	return 0;
}

/**************  detect ************/
static inline int
ipc_fault_detect(struct invocation_cap *cap_entry, struct spd *dest_spd)
{
	return 0;
}

static inline int
pop_fault_detect(struct thd_invocation_frame *inv_frame, struct thd_invocation_frame *curr_frame)
{
	return 0;
}

static inline int
switch_thd_fault_detect(struct thread *next)
{
	return 0;
}

static inline int
interrupt_fault_detect(struct thread *next)
{
	return 0;
}

/**************  update ************/
static inline int
ipc_fault_update(struct invocation_cap *cap_entry, struct spd *dest_spd)
{
	return 0;
}

static inline int
pop_fault_update(struct thd_invocation_frame *inv_frame, struct thd_invocation_frame *curr_frame)
{
	return 0;
}

static inline int
switch_thd_fault_update(struct thread *next)
{
	return 0;
}

static inline int
interrupt_fault_update(struct thread *next)
{
	return 0;
}


static inline int
fault_cnt_syscall_helper(int spdid, int option, spdid_t d_spdid, unsigned int cap_no)
{
	return 0;
}
#endif

#endif  //RECOVERY_H
