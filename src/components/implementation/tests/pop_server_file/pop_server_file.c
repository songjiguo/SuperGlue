#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <rtorrent.h>

#include <timed_blk.h>

char buffer[1024];

/* extern td_t server_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid); */
/* extern void server_trelease(spdid_t spdid, td_t tid); */
/* extern int server_treadp(spdid_t spdid, td_t td, int len, int *off, int *sz); */
/* extern int server_twritep(spdid_t spdid, td_t td, int cbid, int sz); */

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

	cbufp_t cb;
	char *d;
	d = cbufp_alloc(strlen(data1), &cb);
	if (!d) return -1;
	cbufp_send(cb);
	memcpy(d, data1, strlen(data1));
	ret1 = twritep(cos_spd_id() , t1, cb, strlen(data1));
	cbufp_deref(cb);

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

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 8;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (second == 0) {
			second = 1;
			timed_event_block(cos_spd_id(), 1);
			pop_cgi();			
		}
	}	
	return;
}
