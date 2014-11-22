/* mbox client test */

#include <stdlib.h>
#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>
#include <periodic_wake.h>


volatile unsigned long long overhead_start, overhead_end;

#define  ITER 10
void parse_args(int *p, int *n)
{
	char *c;
	int i = 0, s = 0;
	c = cos_init_args();
	while (c[i] != ' ') {
		s = 10*s+c[i]-'0';
		i++;
	}
	*p = s;
	s = 0;
	i++;
	while (c[i] != '\0') {
		s = 10*s+c[i]-'0';
		i++;
	}
	*n = s;
	return ;
}
void cos_init(void *arg)
{
	td_t t1, serv;
	long evt;
	char *params1 = "foo", *params2 = "", *d;
	char *data = "1234567890";
	int period, num, ret, sz, i, j;
	u64_t start = 0, end = 0, re_cbuf;
	cbufp_t cb1;

	union sched_param sp;
	static int first = 1;

	if (first) {
		first = 0;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 9;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return ;
	}

	evt = evt_split(cos_spd_id(), 0, 0);
	assert(evt > 0);
	printc("mb client: tsplit by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	serv = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_RW | TOR_WAIT, evt);
	if (serv < 1) {
		printc("UNIT TEST FAILED (3): split1 failed %d\n", serv); 
		assert(0);
	}

	printc("mb client: thd %d 1\n", cos_get_thd_id());
	evt_wait(cos_spd_id(), evt);
	printc("mb client: thd %d 2\n", cos_get_thd_id());

	printc("client split successfully\n");
	sz = 4096;
	j = 1000*ITER;
	j = 40;
	rdtscll(start);
	for (i=1; i<=j; i++) {
		if (i == j) rdtscll(end);
		d = cbufp_alloc(sz, &cb1);
		if (!d) {
			printc("can not get a cbufp (thd %d)\n", cos_get_thd_id());
			goto done;
		}
		cbufp_send(cb1);
		rdtscll(end);
		((u64_t *)d)[0] = end;
		printc("cli:passed out data is %lld\n", ((u64_t *)d)[0]);
		printc("cli:passed out data in cbuf %d\n", cb1);
		ret = twritep(cos_spd_id(), serv, cb1, sz);

		cbufp_deref(cb1); 
	}

	printc("mb client: finally trelease by thd %d in spd %ld\n", 
	       cos_get_thd_id(), cos_spd_id());

	evt_wait(cos_spd_id(), evt);  // test only
	rdtscll(overhead_start);
	trelease(cos_spd_id(), serv);
	rdtscll(overhead_end);
	printc("mbox client trelease overhead %llu\n", overhead_end - overhead_start);

	return;

	printc("Client snd %d times %llu\n", j-1, (end-start)/(j-1));
	/* 
	 * insert evt_grp_wait(...) into the code below where it makes
	 * sense to.  Simulate if the code were executing in separate
	 * threads.
	 */
	parse_args(&period, &num);
	periodic_wake_create(cos_spd_id(), period);
	re_cbuf = 0;
	for (i=1; i<=ITER; i++) {
		for (j=0; j<num; j++) {
			rdtscll(start);
			d = cbufp_alloc(i*sz, &cb1);
			if (!d) goto done;
			cbufp_send_deref(cb1);
			rdtscll(end);
			re_cbuf = re_cbuf+(end-start);
			rdtscll(end);
			((u64_t *)d)[0] = end;
			ret = twritep(cos_spd_id(), serv, cb1, i*sz);
		}
		periodic_wake_wait(cos_spd_id());
	}
	printc("Client: Period %d Num %d Cbuf %llu\n", period, num, re_cbuf/(num*ITER));
done:
	trelease(cos_spd_id(), serv);
	printc("client UNIT TEST PASSED: split->release\n");

	printc("client UNIT TEST ALL PASSED\n");
	
	return;
}
