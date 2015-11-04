#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

#include <timed_blk.h>

#include <c3_test.h>

char buffer[1024];

extern td_t server_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);

void pop_cgi(void)
{
	td_t t1;
	long evt1;
	char *params = "/test";
	char *data1 = "hello_world";
	unsigned int ret1, ret2;

	printc("pop the file on the server\n");
	evt1 = evt_split(cos_spd_id(), 0, 0);
	assert(evt1 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params, strlen(params), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("split failed\n");
		return;
	}
	
	/* ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */
	printc("write the file on the server\n");
	ret1 = twritep_pack(cos_spd_id(), t1, data1, strlen(data1));

	printc("close the file on the server\n");
	trelease(cos_spd_id(), t1);

	printc("unsigned long long length %d\n", sizeof(unsigned long long));
	printc("unsigned long length %d\n", sizeof(unsigned long));
	printc("unsigned int length %d\n", sizeof(unsigned int));
	printc("unsigned short int length %d\n", sizeof(unsigned short int));
	printc("\n<<<pop done!!>>>\n\n");
	return;
}

void cos_init(void)
{
	static int first = 0, second = 0;
	union sched_param sp;
	
	if(first == 0){
		first = 1;
		
		printc("popcgi: upcall to call cos_init create new thd(thd %d, spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 8;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (second == 0) {
			second = 1;
			printc("popcgi: upcall to call time_event_block and pop_cgi(thd %d, spd %ld)\n", cos_get_thd_id(), cos_spd_id());
		
			timed_event_block(cos_spd_id(), 1);
			pop_cgi();
		}
	}

	return;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int init_first = 0;
	/* printc("upcall type %d, core %ld, thd %d, args %p %p %p\n", */
	/*        t, cos_cpuid(), cos_get_thd_id(), arg1, arg2, arg3); */

	static int first = 1;
	if (first) { 
		first = 0; 
		/* ??? if we do not do this, cbufp_free_list_get
		 * length is 0 ??? The memory is not initialized? */
		constructors_execute();
	}
	
	switch (t) {
	case COS_UPCALL_THD_CREATE:
	{
		printc("popcgi: upcall to call cos_init (thd %d, spd %ld)\n",
		       cos_get_thd_id(), cos_spd_id());
		if (!arg1) cos_init();
		break;
	}
	case COS_UPCALL_RECEVT:
		/* printc("popcgi: upcall to recover the event (thd %d, spd %ld)\n", */
		/*        cos_get_thd_id(), cos_spd_id()); */
#ifdef EVT_C3
		/* events_replay_all((int)arg1); */
		evt_cli_if_recover_upcall_entry(*(int *)arg1);
		
#endif
		break;
	default:
		return;
	}
	return;
}
