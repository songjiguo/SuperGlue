#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <unit_mmrec2.h>


#define BEST_TEST

#define TOTAL_AMNT 128

#define PAGE_NUM 10

vaddr_t s_addr[PAGE_NUM];
vaddr_t d_addr[PAGE_NUM];

#define THREAD1 10

volatile unsigned long long start, end;

static void
revoke_test()
{
	int i;
	vaddr_t addr = 0;
	printc("\n<<< REVOKE TEST BEGIN! >>>\n");

#ifdef TEN2TEN  		/* 10 to 10 */
	for (i = 0; i<PAGE_NUM; i++) {
		addr = s_addr[i];
		/* printc("s_addr %p\n", addr); */
		/* rdtscll(start); */
		mman_revoke_page(cos_spd_id(), addr, 0);
		/* rdtscll(end); */
		/* printc("COST (mman_revoke_page) %llu\n", end - start); */
	}
#else  /* 1 to 10 */
	addr = s_addr[0];
	/* printc("s_addr %p\n", addr); */
	mman_revoke_page(cos_spd_id(), addr, 0);
#endif
	printc("<<< REVOKE TEST END! >>>\n\n");
	return;
}

static void
alias_test()
{
	int i;
	vaddr_t addr = 0;
	printc("\n<<< ALIAS TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		d_addr[i] = mm_test2();

		#ifdef TEN2TEN  /* 10 to 10 */
		addr = s_addr[i];
		#else  /* 1 to 10 */
		addr = s_addr[0];
		#endif

		/* printc("s_addr %p d_addr %p\n", addr, d_addr[i]); */
		/* rdtscll(start); */
		if (d_addr[i]!= mman_alias_page(cos_spd_id(), addr, cos_spd_id()+1, d_addr[i], MAPPING_RW)) BUG();
		/* rdtscll(end); */
		/* printc("cost %llu\n", end - start); */
		
	}

#ifdef BEST_TEST
	mm_test2_34();
#endif
	
	printc("<<< ALIAS TEST END! >>>\n\n");
	return;
}


static void
get_test()
{
	int i;
	printc("\n<<< GET TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		s_addr[i] = (vaddr_t)cos_get_vas_page();
		if (unlikely(!s_addr[i])) {
			printc("Cannot get vas for comp %ld!\n", cos_spd_id());
			BUG();
		}
		/* printc("s_addr %p\n", s_addr[i]); */
		/* rdtscll(start); */
		if (s_addr[i]!= mman_get_page(cos_spd_id(), s_addr[i], 0)) BUG();
		/* rdtscll(end); */
		/* printc("cost %llu\n", end - start); */
	}
	printc("<<< GET TEST END! >>>\n\n");
	return;
}


static void
all_in_one()
{
	int i;
	for (i = 0; i<PAGE_NUM; i++) {
		s_addr[i] = (vaddr_t)cos_get_vas_page();
		d_addr[i] = mm_test2();
	}

	for (i = 0; i<PAGE_NUM; i++) {
		/* rdtscll(start); */
		mman_get_page(cos_spd_id(), s_addr[i], 0);
		mman_alias_page(cos_spd_id(), s_addr[i], cos_spd_id()+1, d_addr[i], MAPPING_RW);
		mman_revoke_page(cos_spd_id(), s_addr[i], 0);

		/* rdtscll(end); */
		/* printc("grant-alias-revoke cost %llu\n", end - start); */
		/* mman_release_page(cos_spd_id(), s_addr[i], 0); */
	}
	return;
}

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i;
	

	if(first == 0){
		first = 1;

		for (i=0; i<PAGE_NUM; i++) s_addr[i] = 0;
		for (i=0; i<PAGE_NUM; i++) d_addr[i] = 0;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		timed_event_block(cos_spd_id(), 50);
		periodic_wake_create(cos_spd_id(), 1);
		i = 0;
		while(i++ < 80) { /* 80 x 10 x 4k  < 4M */
			printc("<<< MM RECOVERY TEST START (thd %d) >>>\n", cos_get_thd_id());
			get_test();
#ifdef BEST_TEST
			alias_test();
			revoke_test();
#endif

			/* all_in_one(); */

			printc("<<< MM RECOVERY TEST DONE!! >>> {%d}\n\n\n", i);
			periodic_wake_wait(cos_spd_id());
		}
	}
	
	return;
}

#ifdef CLI_UPCALL_ENABLE
void alias_replay(vaddr_t s_addr);
void eager_replay();
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
#if (!LAZY_RECOVERY)
		/* printc("EAGER!!! UNIT_MMREC 1 upcall: thread %d\n", cos_get_thd_id()); */
		eager_replay();
#else
		printc("UNIT_MMREC 1 upcall: thread %d\n", cos_get_thd_id());
		alias_replay((vaddr_t)arg3);
#endif
		break;
	default:
		cos_init();
	}

	return;
}
#endif
