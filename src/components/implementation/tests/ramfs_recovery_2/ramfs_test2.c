#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>

#include <torrent.h>

char buffer[1024];

void ramfs_test2(void)
{
	td_t t1, t2, t0;
	long evt1, evt2, evt0;
	char *params0, *params1, *params2, *params3;
	char *data0, *data1, *data2, *data3;
	unsigned int ret1, ret2, ret0;
	char *strl, *strh;

	printc("\n<<thread %d now is entering spd %ld>>\n", cos_get_thd_id(), cos_spd_id());

	if (cos_get_thd_id() == 13) { /* can passed this tid in here */
		params0 = "wariscoming";
		params1 = "bcr/";
		params2 = "foo/";
		params3 = "foo/bcr/wariscoming";
		data0 = "hsongyuxuan";
		data1 = "hqianduoduo";
		data2 = "aajshdh";
		data3 = "aajshdhhqianduoduo";
		strh = "onemore_h";
	}

	if (cos_get_thd_id() == 14) {
		params0 = "who";
		params1 = "barimpossible/";
		params2 = "koo/";
		params3 = "koo/barimpossible/who";
		data0 = "fromotherfiles";
		data1 = "isdifferent";
		data2 = "iknowthis";
		data3 = "iknowthisisdifferent";
		strl = "twomore_l";
	}

	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	evt0 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0 && evt0 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("  split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("  split1 failed %d\n", t2); return;
	}

	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t0 < 1) {
		printc("  split0 failed %d\n", t0); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	printc("thread %d writes str %s in %s\n", cos_get_thd_id(), data1, params2);

	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), data2, params2, params1);
	
	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0));
	printc("thread %d writes str %s in %s%s%s\n", cos_get_thd_id(), data0, params2, params1, params0);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t1 < 1 || t2 < 1 || t0 < 1) {
		printc("later splits failed\n");
		return;
	}

	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printc("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1);
	buffer[0] = '\0';
	
	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret2 > 0) buffer[ret2] = '\0';
	printc("thread %d read %d: %s\n", cos_get_thd_id(), ret2, buffer);
	buffer[0] = '\0';

	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023);
	if (ret0 > 0) buffer[ret0] = '\0';
	printc("thread %d read %d: %s\n", cos_get_thd_id(), ret0, buffer);
	buffer[0] = '\0';
		
	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	printc("\n<<thread %d now is leaving spd %ld>>\n", cos_get_thd_id(), cos_spd_id());
	return;
}


void cos_init(void)
{
	return;

}


#if (!LAZY_RECOVERY)
void eager_recovery_all();
#endif
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_EAGER_RECOVERY:
#if (!LAZY_RECOVERY)
		printc("eager upcall: thread %d (in spd %ld)\n", cos_get_thd_id(), cos_spd_id());
		eager_recovery_all();
#endif
		break;
	default:
		cos_init();
	}

	return;
}
