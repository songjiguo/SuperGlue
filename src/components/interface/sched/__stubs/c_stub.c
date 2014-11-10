/* 
 C3 -- scheduler interface Scheduler recovery: client stub interface
 recovery data structure, mainly for block/wakeup.

 Issue: The threads created in scheduler (timer/IPI/idle/init) are
 already taken cared during the reboot and reinitialize in the
 scheduler (sched_reboot(), and kernel introspection)
*/

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

static unsigned long long meas_start, meas_end;
static int meas_flag = 0;

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

/* recovery data structure for threads */
struct rec_data_thd {
	unsigned int thd;
	unsigned int dep_thd;;
	
	unsigned int  state;
	unsigned long fcnt;
};

/* the state of a thread object */
enum {
	THD_STATE_CREATE,
	THD_STATE_READY,
	THD_STATE_WAIT,
	THD_STATE_RUNNING,
	THD_STATE_CREATE_DEFAULT,   // now, only booter thread does this
	THD_STATE_COMPONENT_TAKE,
	THD_STATE_COMPONENT_RELEASE
};

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
redo:
	printc("thread %d calls << sched_create_thd >>\n", cos_get_thd_id());
#ifdef MEASU_SCHED_INTERFACE_CREATE
	rdtscll(start);
#endif
	
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, sched_param0, sched_param1, sched_param2);
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}


CSTUB_FN(int, sched_create_thread_default) (struct usr_inv_cap *uc,
					    spdid_t spdid, u32_t sched_param0, 
					    u32_t sched_param1, u32_t sched_param2)
{
	long fault = 0;
	long ret;
	
	unsigned long long start, end;
redo:
	printc("thread %d calls << sched_create_thd_default >>\n", cos_get_thd_id());

	CSTUB_INVOKE(ret, fault, uc, 4, spdid, sched_param0, sched_param1, sched_param2);
        if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}

CSTUB_FN(int, sched_wakeup) (struct usr_inv_cap *uc,
			     spdid_t spdid, unsigned short int dep_thd)
{
	long fault = 0;
	long ret;

        unsigned long long start, end;

        int crash_flag = 0;
redo:
	printc("thread %d calls << sched_wakeup thd %d>>\n",cos_get_thd_id(),dep_thd);
#ifdef MEASU_SCHED_INTERFACE_WAKEUP
	rdtscll(start);
#endif
	
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, dep_thd);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

#ifdef MEASU_SCHED_INTERFACE_WAKEUP
	rdtscll(end);
	printc("<<< entire cost (sched_wakeup): %llu >>>>\n", (end-start));
#endif

	return ret;
}

CSTUB_FN(int, sched_block) (struct usr_inv_cap *uc,
			    spdid_t spdid, unsigned short int thd_id)
{
	long fault = 0;
	long ret;
	
        unsigned long long start, end;
        struct period_thd *item;
        int crash_flag = 0;
redo:
	printc("thread %d calls << sched_block thd %d>>\n",cos_get_thd_id(),thd_id);
	
#ifdef MEASU_SCHED_INTERFACE_BLOCK
	rdtscll(start);
#endif
	CSTUB_INVOKE(ret, fault, uc, 2, spdid, thd_id);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

#ifdef MEASU_SCHED_INTERFACE_BLOCK
	       rdtscll(end);
	       printc("<<< entire cost (sched_block): %llu >>>>\n", (end-start));
#endif

	return ret;
}


CSTUB_FN(int, sched_component_take) (struct usr_inv_cap *uc,
				     spdid_t spdid)
{
	long fault = 0;
	long ret;
	
	unsigned long long start, end;
redo:
	printc("thread %d calls << sched_component_take >>\n",cos_get_thd_id());

#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
	rdtscll(start);
#endif

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		printc("see a fault during sched_component_take\n");
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
	rdtscll(end);
	printc("<<< entire cost (sched_component_take): %llu >>>>\n", (end-start));
#endif
	
	return ret;
}

CSTUB_FN(int, sched_component_release) (struct usr_inv_cap *uc,
					spdid_t spdid)
{
	long fault = 0;
	long ret;

	unsigned long long start, end;
redo:
	printc("thread %d calls << sched_component_release >>\n",cos_get_thd_id());
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
	rdtscll(start);
#endif
      
	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
	rdtscll(end);
	printc("<<< entire cost (sched_component_release): %llu >>>>\n", (end-start));
#endif
	
	return ret;
}

CSTUB_FN(int, sched_timeout_thd) (struct usr_inv_cap *uc,
				  spdid_t spdid)
{
	long fault = 0;
	long ret;
	
redo:
	printc("<< cli: sched_timeout_thd >>>\n");

	CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}

CSTUB_FN(int, sched_timeout) (struct usr_inv_cap *uc,
			      spdid_t spdid, unsigned long amnt)
{
	long fault = 0;
	long ret;
	
	struct period_thd *item;
redo:
	printc("<< cli: sched_timeout >>>\n");

	CSTUB_INVOKE(ret, fault, uc, 2, spdid, amnt);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}
	
	return ret;
}


CSTUB_FN(int, sched_timestamp) (struct usr_inv_cap *uc)
{
	long fault = 0;
	long ret;
	
	struct period_thd *item;
redo:
	printc("<< cli: sched_timestamp >>>\n");

	CSTUB_INVOKE_NULL(ret, fault, uc);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}

CSTUB_FN(int, sched_create_net_acap) (struct usr_inv_cap *uc, 
				      spdid_t spdid, int acap_id, unsigned short int port)
{
	long fault = 0;
	long ret;
	
	struct period_thd *item;
redo:
	printc("<< cli: sched_create_net_acap >>>\n");

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, acap_id, port);
	if (unlikely (fault)){
		CSTUB_FAULT_UPDATE();
		goto redo;
	}

	return ret;
}
