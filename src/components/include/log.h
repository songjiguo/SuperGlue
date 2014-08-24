/*
   Jiguo:
   The header file for only interface, used for the log monitor
   latent fault (state machine)
*/

#ifndef LOGGER_H
#define LOGGER_H

extern vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
extern vaddr_t llog_cs_init(spdid_t spdid, vaddr_t addr);
extern int llog_process(spdid_t spdid);
extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);


#include "../implementation/sched/cos_sched_sync.h"

#include <cos_list.h>
#include <ck_ring_cos.h>

//#define MEAS_WITH_LOG
//#define MEAS_WITHOUT_LOG

/* event entry in the ring buffer (per event) */
struct event_info {
	int thd_id;
	spdid_t from_spd;
	int dest_info;  // can be cap_no for dest spd or return spd from the stack
	unsigned long long time_stamp;

	int func_num;   // hard code which function call FIXME:naming space
	int dep_thd;   // hard code dependency, for scheduler_blk only
};

// context tracking
struct cs_info {
	int curr_thd;
	int next_thd;
	unsigned long long time_stamp;

	int flags;
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __RING_DEFINED
#define __RING_DEFINED
CK_RING(event_info, logevts_ring);
CK_RING_INSTANCE(logevts_ring) *cli_ring;
CK_RING(cs_info, logcs_ring);
CK_RING_INSTANCE(logcs_ring) *cs_ring;
#endif

// Events monitoring
static inline int 
ring_is_full()
{
	int capacity, size;

	assert(cli_ring);
	capacity = CK_RING_CAPACITY(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	size = CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	if (capacity == size + 1) return 1;
	else return 0;
}

static inline void 
monevt_conf(struct event_info *monevt, int param, int func_num, int dep)
{
	unsigned long long ts;
	rdtscll(ts);
	monevt->time_stamp = ts;

	monevt->thd_id = cos_get_thd_id();
	monevt->from_spd = cos_spd_id();
	monevt->dest_info = param;

	monevt->func_num = func_num;
	monevt->dep_thd = dep;

	return;
}

// print info for an event
static void 
print_evt(struct event_info *entry)
{
	assert(entry);

	printc("thd id %d ", entry->thd_id);
	printc("from spd %d ", entry->from_spd);

	int dest = entry->dest_info;
	if (dest > MAX_NUM_SPDS) {
		if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0);
	}
	printc("dest spd %d ", dest);

	printc("func num %d ", entry->func_num);
	printc("dep thd %d ", entry->dep_thd);

	// FIXME: to_spd info can be pushed into stack an pop when return
	/* printc("next spd %d\n", entry->dest_info); */

	printc("time_stamp %llu\n", entry->time_stamp);
	
	return;
}

static inline void 
monevt_enqueue(int param, int func_num, int dep)
{
	struct event_info monevt;

	if (unlikely(!cli_ring)) {
		printc("evt enqueue (spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id());
		vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		assert(cli_addr);
		if (!(cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(llog_init(cos_spd_id(), cli_addr)))) BUG();
	}

	if (unlikely(ring_is_full())) {
		/* printc("evt_ring is full(spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id()); */
		llog_process(cos_spd_id());
	}

	monevt_conf(&monevt, param, func_num, dep);

	if (func_num == 22 || func_num == 212) {
		printc("ADD EVT: ");
		print_evt(&monevt);
	}

	/* if (dep) { */
	/* 	printc("ADD EVT: "); */
	/* 	print_evt(&monevt); */
	/* } */

        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevts_ring, cli_ring, &monevt)));

	return;
}

// Context switch monitoring

// print info a cs
static void 
print_cs(struct cs_info *entry)
{
	assert(entry);

	printc("CS: curr_thd id %d ", entry->curr_thd);
	printc("next_thd id %d ", entry->next_thd);
	printc("time_stamp %llu\n", entry->time_stamp);
	
	return;
}


static inline int 
csring_is_full()
{
	int capacity, size;

	assert(cs_ring);
	capacity = CK_RING_CAPACITY(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)cs_ring));
	size = CK_RING_SIZE(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)cs_ring));
	
	if (capacity == size + 1) return 1;
	else return 0;
}

static inline void 
moncs_conf(struct cs_info *moncs, int flags, int thd_id)
{
	unsigned long long ts;

	moncs->curr_thd = cos_get_thd_id();
	rdtscll(ts);
	moncs->time_stamp = ts;
	moncs->flags = flags;
	moncs->next_thd = thd_id;

	return;
}

static int ready_log  = 0;

/* only called by scheduler  */
static inline void 
moncs_enqueue(unsigned short int thd_id, int flags)
{
	struct cs_info moncs;

	/* assert(thd_id); */

	if (cos_get_thd_id() == thd_id) return;

	if (unlikely(!cs_ring)) {
		printc("cs enqueue (spd %ld, thd %d) ", cos_spd_id(), cos_get_thd_id());
		vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		assert(cli_addr);
		if (!(cs_ring = (CK_RING_INSTANCE(logcs_ring) *)(llog_cs_init(cos_spd_id(), cli_addr)))) BUG();
	}

	if (unlikely(csring_is_full())) {
		/* printc("csring is full(spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id()); */
		llog_process(cos_spd_id());
	}

	moncs_conf(&moncs, flags, thd_id);

	/* printc("ADD CS: "); */
	/* print_cs(&moncs); */

	while (unlikely(!CK_RING_ENQUEUE_SPSC(logcs_ring, cs_ring, &moncs)));

	return;
}

#endif /* LOGGER_H */
