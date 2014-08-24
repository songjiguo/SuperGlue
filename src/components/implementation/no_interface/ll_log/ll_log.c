#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print
#include <recovery_upcall.h>

#define LM_SYNC_PERIOD 50
static unsigned int lm_sync_period;

struct logmon_info logmon_info[MAX_NUM_SPDS];
struct logmon_cs lmcs;
struct thd_trace thd_trace[MAX_NUM_THREADS];

static struct event_info last_evt_entry;  // to continue from last time on evt_ring in some spd
static struct cs_info last_cs_entry;  // to continue from last time on cs_ring

/* required timing data */
#define CPU_FREQUENCY  (CPU_GHZ*1000000000)
#define TIMER_FREQ     (CPU_TIMER_FREQ)
#define CYC_PER_TICK   (CPU_FREQUENCY/TIMER_FREQ)
// hard code the prio with thread for now. PI test only

/* [ 6233.442649] cobj lllog:2 found at 0x40c35000:8f04, size 62894 -> 41c00000 */
/* [ 6233.442658] cobj fprr:3 found at 0x40c3df20:15188, size 40bbc -> 42000000 */
/* [ 6233.442667] cobj mm:4 found at 0x40c530c0:94c0, size 3b194 -> 42400000 */
/* [ 6233.442675] cobj print:5 found at 0x40c5c580:266c, size 2b6a8 -> 42800000 */
/* [ 6233.442682] cobj boot:6 found at 0x40c5ec00 -> 42c00000 */
/* [ 6233.443718] cobj l:7 found at 0x42c36000:9390, size 3306c -> 43c00000 */
/* [ 6233.443721] cobj te:8 found at 0x42c3f3a0:cd38, size 37a08 -> 44000000 */
/* [ 6233.443723] cobj mon_p:9 found at 0x42c4c0e0:b4f0, size 34230 -> 44400000 */
/* [ 6233.443726] cobj lmoncli1:10 found at 0x42c575e0:de64, size 36b90 -> 44800000 */
/* [ 6233.443728] cobj lmonser1:11 found at 0x42c65460:10dac, size 39aec -> 44c00000 */
/* [ 6233.443730] cobj va:12 found at 0x42c76220 -> 45000000 */

static void
prio_init(int thdid) 
{
	switch (thdid) {
		case 4:
			thd_trace[thdid].prio = 31;
			break;
		case 5:
			thd_trace[thdid].prio = 30;
			break;
		case 7:
			thd_trace[thdid].prio = 0;
			break;
		case 8:
			thd_trace[thdid].prio = 29;
			break;
		case 9:
			thd_trace[thdid].prio = 3;
			break;
		case 10:
			thd_trace[thdid].prio = 5;
			break;
		case 11:
			thd_trace[thdid].prio = 10;
			break;
		case 12:
			thd_trace[thdid].prio = 11;
			thd_trace[thdid].period = 10*CYC_PER_TICK;
			thd_trace[thdid].execution = 2*CYC_PER_TICK;
			break;
		case 13:
			thd_trace[thdid].prio = 15;
			thd_trace[thdid].period = 22*CYC_PER_TICK;
			thd_trace[thdid].execution = 4*CYC_PER_TICK;
			break;
		case 14:
			thd_trace[thdid].prio = 20;
			thd_trace[thdid].period = 56*CYC_PER_TICK;
			thd_trace[thdid].execution = 12*CYC_PER_TICK;
			break;
		default:
			break;
		}
	return;
}

static void 
init_thread_trace()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id	 = i;
		thd_trace[i].block_dep	 = 0;
		thd_trace[i].pi_duration = 0;

		thd_trace[i].period	    = 0;
		thd_trace[i].execution	    = 0;

		thd_trace[i].release	    = 0;
		thd_trace[i].runtime	    = 0;
		thd_trace[i].completion	    = 0;
		thd_trace[i].wait_for_block = 0;

		prio_init(i);
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].alpha_exec[j]    = 0;
			thd_trace[i].last_exec[j]     = 0;
			thd_trace[i].avg_exec[j]      = 0;
			thd_trace[i].tot_wcet[j]      = 0;
			thd_trace[i].tot_upto_wcet[j] = 0;
			thd_trace[i].upto_wcet[j]     = 0;
			thd_trace[i].this_wcet[j]     = 0;
			thd_trace[i].wcet[j]	      = 0;
			thd_trace[i].tot_spd_exec[j]  = 0;
			thd_trace[i].tot_inv[j]	      = 0;
		}
		thd_trace[i].tot_exec = 0;

		thd_trace[i].curr_spd_info.spdid    = 0;
		thd_trace[i].curr_spd_info.from_spd = 0;
	}
	return;
}


// set up shared ring buffer between spd and log manager, a page for now
static int 
shared_ring_setup(spdid_t spdid, vaddr_t cli_addr, int type) {
        struct logmon_info *spdmon = NULL;
        vaddr_t log_ring, cli_ring = 0;
	char *addr, *hp;

	assert(cli_addr);
        assert(spdid);
	addr = cos_get_heap_ptr();
	if (!addr) {
		printc("fail to allocate a page from the heap\n");
		goto err;
	}
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));
	if ((vaddr_t)addr != __mman_get_page(cos_spd_id(), (vaddr_t)addr, 0)) {
		printc("fail to get a page in logger\n");
		goto err;
	}

	if (type == 0) {   // events
		spdmon = &logmon_info[spdid];
		assert(spdmon->spdid == spdid);
		spdmon->mon_ring = (vaddr_t)addr;
	} else {           // cs
		lmcs.mon_csring = (vaddr_t)addr;
	}

	cli_ring = cli_addr;
        if (unlikely(cli_ring != __mman_alias_page(cos_spd_id(), (vaddr_t)addr, spdid, cli_ring))) {
                printc("alias rings %d failed.\n", spdid);
		assert(0);
        }

	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	if (type == 0) {
		spdmon->cli_ring = cli_ring;
		CK_RING_INIT(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE-sizeof(struct ck_ring_logevts_ring))/ sizeof(struct event_info)));
	} else {
		lmcs.sched_csring = cli_ring;
		CK_RING_INIT(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE-sizeof(struct ck_ring_logcs_ring))/ sizeof(struct cs_info)));
	}

        return 0;
err:
        return -1;
}

static void
update_pi_info(struct thd_trace *thd_trace_list, struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	struct thd_trace *h_thd_trace_list;
	
	assert(thd_trace_list);
	assert(entry);

	if (!(thd_trace_list->block_dep = entry->dep_thd)) return;

	int dest = entry->dest_info;
	assert(dest < MAX_NUM_SPDS);
	/* if (dest > MAX_NUM_SPDS) { */
	/* 	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0); */
	/* } */

	/* printc("PI....(dest %d)d\n", dest); */
	print_evtinfo(entry);
	
	if (dest == 3 && entry->func_num == 11) {
		printc("(thd %d) PI start at %llu (dep on thd %d)\n", entry->thd_id, entry->time_stamp, entry->dep_thd); 
		thd_trace_list->pi_duration = entry->time_stamp;
		printc("\n");
	}

	if (dest == 3 && entry->func_num == 12) {
		h_thd_trace_list = &thd_trace[entry->dep_thd];
		assert(thd_trace_list);	
		printc("last PI %llu\n", h_thd_trace_list->pi_duration);
		if (h_thd_trace_list->pi_duration == 0)  return; // ensure it was blocked before
		printc("(thd %d) PI end at %llu\n", entry->dep_thd, entry->time_stamp);
		h_thd_trace_list->pi_duration = entry->time_stamp - h_thd_trace_list->pi_duration;
		printc("(thd %d) PI lasts for %llu (dep on thd %d)\n", entry->dep_thd, h_thd_trace_list->pi_duration, entry->thd_id);
		h_thd_trace_list->pi_duration = 0;
		printc("\n");
	}

	
	return;
}

static void
update_stack_info(struct thd_trace *thd_trace_list, struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	assert(thd_trace_list);
	assert(entry);
	
	/* print_evtinfo(entry); */
	
	if (entry->from_spd && entry->dest_info) {
	    /* entry->from_spd != entry->dest_info) { */
		thd_trace_list->curr_spd_info.spdid = entry->from_spd;
		thd_trace_list->curr_spd_info.from_spd = entry->dest_info;
		/* printc("from spd %d to spd %d\n", entry->from_spd, entry->dest_info); */
	}

	return;
}

static void 
update_timing_info(struct thd_trace *thd_trace_list, struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	unsigned long long exec_tmp = 0;

	assert(thd_trace_list);
	assert(entry);

	int curr_spd = entry->from_spd;

	if (unlikely(!thd_trace_list->alpha_exec[curr_spd])) {
		thd_trace_list->alpha_exec[curr_spd] = entry->time_stamp;
		thd_trace_list->last_exec[curr_spd] = entry->time_stamp;
	} else {
		if (entry->dest_info && entry->dest_info == curr_spd) {
			thd_trace_list->last_exec[curr_spd] = entry->time_stamp;
		} else {
			exec_tmp = entry->time_stamp - thd_trace_list->last_exec[curr_spd];
			assert(exec_tmp);
			thd_trace_list->tot_exec += exec_tmp;
			thd_trace_list->tot_spd_exec[curr_spd] += exec_tmp;
			thd_trace_list->this_wcet[curr_spd] += exec_tmp;
		}
	}

        // this must be the return from server side, update the wcet in this spd
	if (!entry->dest_info) {
		// update the wcet of this thd up to this spd
		exec_tmp = entry->time_stamp - thd_trace_list->alpha_exec[curr_spd];
		thd_trace_list->alpha_exec[curr_spd] = 0;
		if (exec_tmp > thd_trace_list->upto_wcet[curr_spd])
			thd_trace_list->upto_wcet[curr_spd] = exec_tmp;
		/* printc(" thd %d << wcet %llu >>\n ",thd_trace_list->thd_id, exec_tmp);		 */
		thd_trace_list->tot_upto_wcet[curr_spd] += thd_trace_list->upto_wcet[curr_spd];

		// update the wcet of this thd in this spd
		if (thd_trace_list->this_wcet[curr_spd] > thd_trace_list->wcet[curr_spd])
			thd_trace_list->wcet[curr_spd] = thd_trace_list->this_wcet[curr_spd];
		thd_trace_list->tot_wcet[curr_spd] += thd_trace_list->wcet[curr_spd];

		// reset local wcet record and increase the invs to this spd
		thd_trace_list->this_wcet[curr_spd] = 0;
		thd_trace_list->tot_inv[curr_spd]++;
	}

	/* printc("thd %d (total exec %llu, wcet %llu) in spd %d \n", thd_trace_list->thd_id, thd_trace_list->tot_spd_exec[curr_spd], thd_trace_list->wcet[curr_spd], curr_spd); */
	/* printc("thd %d (total wcet %llu) upto spd %d (invocations %d)\n\n ", thd_trace_list->thd_id, thd_trace_list->tot_wcet[curr_spd], curr_spd, thd_trace_list->tot_inv[curr_spd]); */
	
	return;
}

static void 
update_info(struct thd_trace *thd_trace_list, struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	/* update_timing_info(thd_trace_list, entry, cs_entry_curr, cs_entry_next); */
	/* update_stack_info(thd_trace_list, entry, cs_entry_curr, cs_entry_next); */
	update_pi_info(thd_trace_list, entry, cs_entry_curr, cs_entry_next);

	return;
}


static void
next_to_curr_evt(struct event_info *curr, struct event_info *next)
{
	
	curr->thd_id = next->thd_id;
	curr->from_spd = next->from_spd;
	curr->dest_info = next->dest_info;
	curr->time_stamp= next->time_stamp;
	curr->func_num= next->func_num;
	curr->dep_thd= next->dep_thd;
	return;
}

static void
next_to_curr_cs(struct cs_info *curr, struct cs_info *next)
{
	
	curr->curr_thd = next->curr_thd;
	curr->next_thd = next->next_thd;
	curr->time_stamp = next->time_stamp;
	curr->flags = next->flags;
	return;
}

static void 
cap_to_dest(struct event_info *entry)
{
	int dest = entry->dest_info;
	if (dest > MAX_NUM_SPDS) {
		if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0);
		entry->dest_info = dest;
	}
	
	return;
}

static int sync_evt_cs(struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next);

static int
invariance_check(struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	struct thd_trace *thd_trace_list;
	if (!sync_evt_cs(entry, cs_entry_curr, cs_entry_next)) return 0;
	thd_trace_list = &thd_trace[entry->thd_id];
	assert(thd_trace_list);
	update_info(thd_trace_list, entry, cs_entry_curr, cs_entry_next);
	return 1;
}

static unsigned long long old_curr_t;
static unsigned long long old_next_t;

static int
deadline_check(struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	struct thd_trace *thd_trace_list;
	thd_trace_list = &thd_trace[cs_entry_curr->next_thd];
	assert(thd_trace_list);
	int spd = entry->dest_info;
	assert(spd < MAX_NUM_SPDS);


	if (thd_trace_list->release
	    && (old_curr_t != cs_entry_curr->time_stamp) 
	    && (old_next_t != cs_entry_next->time_stamp)) {
		printc("thd %d runtime update ", cs_entry_curr->next_thd);
		printc("cs curr %llu cs next %llu\n", cs_entry_curr->time_stamp,cs_entry_next->time_stamp);
		thd_trace_list->runtime  += cs_entry_next->time_stamp - cs_entry_curr->time_stamp;
		old_curr_t = cs_entry_curr->time_stamp;
		old_next_t = cs_entry_next->time_stamp;
	}

	// unblock from periodic_wake_wait
	if (entry->dest_info == 0 && entry->from_spd == 8 && entry->func_num == 212) {
		printc("unblock: thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp);
		thd_trace_list->release = entry->time_stamp;
		thd_trace_list->wait_for_block = 0;
	}

	// call periodic_wake_wait to block (need wait till actual sched_block)
	if (entry->dest_info == 8 && entry->from_spd == 8 && entry->func_num == 212) {
		printc("wait to block: thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp);
		assert(thd_trace_list->wait_for_block == 0);
		thd_trace_list->wait_for_block = 1;		
	}

	// actual block in periodic_wake
	if (entry->dest_info == 3 && entry->from_spd == 8 
	    && entry->func_num == 1 && thd_trace_list->wait_for_block == 1) {
		printc("periodic wake blocked!:thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp);
		thd_trace_list->completion = entry->time_stamp;

		printc("thd %d -- release time %llu\n", thd_trace_list->thd_id, thd_trace_list->release);
		printc("thd %d -- completion time %llu\n", thd_trace_list->thd_id, thd_trace_list->completion);

		printc("thd %d -- expected execution time %llu\n", thd_trace_list->thd_id, thd_trace_list->execution);
		printc("thd %d -- actual   execution time %llu\n", thd_trace_list->thd_id, thd_trace_list->runtime);
		thd_trace_list->runtime = 0;
		thd_trace_list->release = 0;
		old_curr_t = 0;
		old_next_t = 0;
	}

	return 1;
}

static int
sync_evt_cs(struct event_info *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next)
{
	CK_RING_INSTANCE(logcs_ring) *csring;
	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring;
	assert(csring);

	/* printc("before: cs_curr  %llu  cs_next %llu (entry time %llu)\n",cs_entry_curr->time_stamp, cs_entry_next->time_stamp, entry->time_stamp); */
	/* printc("thd %d spd %d  entry time %llu (entry thd %d)\n",cs_entry_curr->next_thd, entry->from_spd, entry->time_stamp, entry->thd_id); */

	cap_to_dest(entry); 	// convert the cap to spd if this is an invocation

	// same thread, no other thread switch occurred
	if (entry->time_stamp < cs_entry_next->time_stamp && entry->thd_id == cs_entry_curr->next_thd) {

		if (cs_entry_next->next_thd == 0 && cs_entry_next->flags == 0) {
			cs_entry_next->next_thd = cs_entry_next->curr_thd;
			assert(cs_entry_next->next_thd == 7);
			cs_entry_next->curr_thd = cs_entry_curr->next_thd;
		}

		deadline_check(entry, cs_entry_curr, cs_entry_next);
		/* if (entry->dest_info == 8 && entry->func_num == 22) { */
		/* 	printc("thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); */
		/* } */
		/* printc("same thread (%d) within the same cs\n", entry->thd_id); */
		return 1;
	}

	if (cs_entry_curr->time_stamp == 0 && cs_entry_next->time_stamp == 0) { 	// first time
		if (!CK_RING_DEQUEUE_SPSC(logcs_ring, csring, cs_entry_curr)) return 0;
	}
	if (cs_entry_curr->time_stamp != 0 && cs_entry_next->time_stamp != 0) { 	// during the process
		next_to_curr_cs(cs_entry_curr, cs_entry_next);
	}

	while (CK_RING_DEQUEUE_SPSC(logcs_ring, csring, cs_entry_next)) {  	      // get 2nd cs entry

		if (cs_entry_next->next_thd == 0 && cs_entry_next->flags == 0) {
			cs_entry_next->next_thd = cs_entry_next->curr_thd;
			assert(cs_entry_next->next_thd == 7);
			cs_entry_next->curr_thd = cs_entry_curr->next_thd;
		}

		deadline_check(entry, cs_entry_curr, cs_entry_next);
		/* if (entry->dest_info == 8 && entry->func_num == 22) { */
		/* 	printc("thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); */
		/* } */
		/* deadline_check(entry, cs_entry_curr, cs_entry_next); */
		/* printc("sync: cs_curr  %llu (thd %d)  cs_next %llu\n",cs_entry_curr->time_stamp, cs_entry_curr->curr_thd, cs_entry_next->time_stamp); */
		/* printc("sync: thd %d spd %d  entry time %llu (entry thd %d)\n",cs_entry_curr->next_thd, entry->from_spd, entry->time_stamp, entry->thd_id); */
		if (cs_entry_next->time_stamp < entry->time_stamp) {
			next_to_curr_cs(cs_entry_curr, cs_entry_next);
		}
		else {
			/* printc("BREAK HERE\n"); */
			return 1;
		}
	}

	// No cs record is found to cover the event entry, so will process in the next time
	next_to_curr_evt(&last_evt_entry, entry);
	next_to_curr_cs(&last_cs_entry, cs_entry_next);
	/* printc("after: cs_curr  %llu  cs_next %llu (entry time %llu)\n",cs_entry_curr->time_stamp, cs_entry_next->time_stamp, entry->time_stamp); */
	/* printc("thd %d spd %d  entry time %llu (entry thd %d)\n",cs_entry_curr->next_thd, entry->from_spd, entry->time_stamp, entry->thd_id); */
	/* } */
	
	return 0;
}

static struct logmon_info *
find_earliest_entry_spd()
{
	int i, j, spdid = 0;
        struct logmon_info *spdmon, *ret = NULL;
	struct thd_trace *thd_trace_list, *this_list;;
	unsigned long long ts = LLONG_MAX;
	CK_RING_INSTANCE(logevts_ring) *evtring;
	
	struct event_info *entry;
	for (i = 1; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		entry = &spdmon->first_entry;
		evtring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
		if (!evtring) continue;
		if (!entry->thd_id) {  // fill the 1st entry
			if (!CK_RING_DEQUEUE_SPSC(logevts_ring, evtring, entry)) continue;
		}
		/* if (!entry->time_stamp) continue; */
		if (entry->time_stamp < ts) {
			ts = entry->time_stamp;
			ret = spdmon;
		}
	}
	return ret;
}

static struct event_info *
find_next_entry(int dest, int thdid)
{
	struct event_info *ret = NULL;
        struct logmon_info *spdmon;

	if (dest > MAX_NUM_SPDS) {
		if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0);
 		spdmon = &logmon_info[dest]; // this must be a different spd (invocation)
		if (spdmon->first_entry.thd_id != thdid) {
			spdmon = find_earliest_entry_spd();
		}
	} else {
		spdmon = find_earliest_entry_spd();
	}

	if (!spdmon) return NULL;
	ret = &spdmon->first_entry;
	return ret;
}


static void
walk_through_events_new()
{
	int cs_thd, this_thd;
	unsigned long long s,e;

        struct logmon_info *spdmon;
	struct event_info *entry_curr = NULL, *entry_next = NULL;
	struct thd_trace *thd_trace_list;
	struct cs_info cs_entry_curr, cs_entry_next;

	memset(&cs_entry_curr, 0, sizeof(struct cs_info));
	memset(&cs_entry_next, 0, sizeof(struct cs_info));

	// if there is any record left from last process?
	if (last_cs_entry.time_stamp && last_evt_entry.time_stamp) {
		next_to_curr_cs(&cs_entry_curr, &last_cs_entry);
		if (!invariance_check(&last_evt_entry, &cs_entry_curr, &cs_entry_next)) return;
		last_evt_entry.time_stamp = 0;
		last_cs_entry.time_stamp = 0;
	}

	/* printc("cs_curr  %llu  cs_next %llu (entry time %llu)\n",cs_entry_curr.time_stamp, cs_entry_next.time_stamp, entry_curr->time_stamp); */
	/* printc("curr_thd %d next_thd %d from_spd %d next_spd %d entry time %llu (entry thd %d)\n",cs_entry_curr.curr_thd, cs_entry_curr.next_thd, entry_curr->from_spd, dest_tmp, entry_curr->time_stamp, entry_curr->thd_id); */
	spdmon = find_earliest_entry_spd();
	if (!spdmon) return;
	entry_curr = &spdmon->first_entry;
	if (!invariance_check(entry_curr, &cs_entry_curr, &cs_entry_next)) return;

	int dest, thdid;
	int from_spd;
	while(1) {
		dest = entry_curr->dest_info;
		thdid = entry_curr->thd_id;
		from_spd = entry_curr->from_spd;
		entry_curr->thd_id = 0;

		entry_next = find_next_entry(dest, thdid);
		if (!entry_next) break;

		/* printc("curr_thd %d next_thd %d from_spd %d next_spd %d entry time %llu (entry thd %d) ",cs_entry_curr.curr_thd, cs_entry_curr.next_thd, from_spd, entry_next->from_spd, entry_next->time_stamp, entry_next->thd_id); */
		/* printc("cs curr %llu cs next %llu\n",cs_entry_curr.time_stamp, cs_entry_next.time_stamp); */

		if (!invariance_check(entry_next, &cs_entry_curr, &cs_entry_next)) break;

		entry_curr = entry_next;		
	}
	return;
}


static void 
llog_report()
{
	printc("\n<<print tracking info >>\n");
	int i, j;
	unsigned long long exec, wcet;

	for (i = 1; i < MAX_NUM_THREADS; i++) {
		/* report_thd_stack_list(i); */
		for (j = 1; j < MAX_NUM_SPDS; j++) {
			exec = report_avg_in_wcet(i, j);
		}
		exec = report_tot_exec(i);
		wcet = report_tot_wcet(i);
	}
	printc("\n");
	return;
}

unsigned int
llog_get_syncp(spdid_t spdid)
{
	return LM_SYNC_PERIOD;
}


static int test_num = 0;

int
llog_process(spdid_t spdid)
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	int i;

	printc("llog_process is called by thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);

	LOCK();

	/* recovery_upcall(cos_spd_id(), COS_UPCALL_LOG_PROCESS, cos_spd_id(), 0); */
	walk_through_events_new();

	/* test_num++; */
	/* if (test_num < 4) { */

	/* llog_report(); */
	/* } */

	UNLOCK();

	return 0;
}

vaddr_t
llog_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_init thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);
	LOCK();

	assert(spdid);
	spdmon = &logmon_info[spdid];
	
	assert(spdmon);
	// limit one RB per component here, not per interface
	if (spdmon->mon_ring && spdmon->cli_ring){
		ret = spdmon->mon_ring;
		goto done;
	}

	/* printc("llog_init...(thd %d spd %d)\n", cos_get_thd_id(), spdid); */
	// 1 for cs rb and 0 for event rb
	if (shared_ring_setup(spdid, addr, 0)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(spdmon->cli_ring);
	ret = spdmon->cli_ring;
done:
	UNLOCK();
	return ret;
}

vaddr_t
llog_cs_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_cs_init thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);
	LOCK();

	assert(spdid == LLBOOT_SCHED);

	// limit one RB per component here, not per interface
	if (lmcs.mon_csring && lmcs.sched_csring){
		ret = lmcs.sched_csring;
		goto done;
	}

	printc("llog_cs init...(thd %d spd %d)\n", cos_get_thd_id(), spdid);
	// 1 for cs rb and 0 for event rb
	if (shared_ring_setup(spdid, addr, 1)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(lmcs.sched_csring);
	ret = lmcs.sched_csring;
done:
	UNLOCK();
	return ret;
}

// do this once
static inline void
log_init(void)
{
	int i;
	printc("ll_log (spd %ld) has thread %d\n", cos_spd_id(), cos_get_thd_id());
	// Initialize log manager here...
	memset(logmon_info, 0, sizeof(struct logmon_info) * MAX_NUM_SPDS);
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		logmon_info[i].spdid = i;
		logmon_info[i].mon_ring = 0;
		logmon_info[i].cli_ring = 0;
		memset(&logmon_info[i].first_entry, 0, sizeof(struct event_info));
	}
	
	memset(&lmcs, 0, sizeof(struct logmon_cs));
	lmcs.mon_csring = 0;
	lmcs.sched_csring = 0;

	memset(&last_evt_entry, 0, sizeof(struct event_info));
	memset(&last_cs_entry, 0, sizeof(struct cs_info));

	init_thread_trace();

	return;
}


void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_THD_CREATE:
		log_init(); break;
	default:
		BUG(); return;
	}

	return;
}
