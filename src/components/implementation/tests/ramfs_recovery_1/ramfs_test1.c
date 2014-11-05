#include <cos_component.h>
#include <print.h>
#include <stdlib.h>
#include <sched.h>
#include <evt.h>
#include <rtorrent.h>
#include <periodic_wake.h>
#include <timed_blk.h>

#include <ramfs_test2.h>

#include <pgfault.h>

#include <cbuf.h>

#define ITER 2000
#define MAX_SZ 4096
#define NCBUF 100

int high, low;
char buffer[1024];

/* #define CBUFP_NUM 32 */
/* cbufp_t p[CBUFP_NUM]; */
/* char *buf[CBUFP_NUM]; */
/* cbufp_t p3[CBUFP_NUM]; */
/* char *buf3[CBUFP_NUM]; */

static void
test0(void)
{
	/* struct cbuf_alloc_desc *d; */
	/* int i; */

	/* d = &cbufp_alloc_freelists[0]; */
	/* assert(EMPTY_LIST(d, next, prev)); */
	/* for (i = 0 ; i < CBUFP_NUM ; i++) { */
	/* 	buf[i] = cbufp_alloc(4096, &p[i]); */
	/* 	cbufp_send(p[i]); */
	/* 	assert(buf[i]); */
	/* } */
	/* for (i = 0 ; i < CBUFP_NUM ; i++) { */
	/* 	cbufp_deref(p[i]); */
	/* } */


	td_t t1, t2, t3;
	long evt1, evt2, evt3;
	char *params1 = "bar";
	char *params2 = "foo/";
	char *params3 = "foo/bar";
	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890";
	unsigned int ret1, ret2;

	evt1 = evt_split(cos_spd_id(), 0, 0);
	evt2 = evt_split(cos_spd_id(), 0, 0);
	/* evt3 = evt_create(cos_spd_id()); */
	assert(evt1 > 0 && evt2 > 0);
	
	printc("\nRAMFS Testing Starting.....(thd %d)\n\n", cos_get_thd_id());

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t1);
		return;
	}
	trelease(cos_spd_id(), t1);
	
	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("UNIT TEST FAILED: split3 failed %d\n", t2); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printc("write %d & %d, ret %d & %d\n", strlen(data1), strlen(data2), ret1, ret2);


	/* This is important!!!! release in the opposite order */
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t1);

	printc("\n>>>>>>ramfs test phase 3 start .... read\n");
	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	printc("\n[[[[[[.... 2nd tsplit\n");
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t1 < 1 || t2 < 1) {
		printc("UNIT TEST FAILED: later splits failed\n");
		return;
	}

	printc("\n[[[[[[.... 1st tread\n");
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	assert(!strcmp(buffer, data1));
	// treadp does not return length, instead return cbid
	/* assert(ret1 == strlen(data1)); */
	printc("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1);
	buffer[0] = '\0';

	ret1 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	assert(!strcmp(buffer, data2));
 	/* assert(ret1 == strlen(data2)); */
	printc("read %d (%d): %s (%s)\n", ret1, strlen(data2), buffer, data2);
	buffer[0] = '\0';

	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t1);

	printc("\nRAMFS Testing Done.....\n\n");
	return;
}

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		test0();
	}
	return;
}



/* #include <cos_component.h> */
/* #include <print.h> */
/* #include <sched.h> */
/* #include <cbuf.h> */
/* #include <evt.h> */
/* #include <rtorrent.h> */

/* #include <periodic_wake.h> */
/* #include <timed_blk.h> */

/* #include <ramfs_test2.h> */

/* #include <pgfault.h> */

/* #define SIZE  4096 */

/* #define VERBOSE 1 */
/* #ifdef VERBOSE */
/* #define printv(fmt,...) printc(fmt, ##__VA_ARGS__) */
/* #else */
/* #define printv(fmt,...)  */
/* #endif */

/* char buffer[1024]; */

/* int high, low; */

/* void test0(void) */
/* { */
/* 	td_t t1, t2, t3; */
/* 	long evt1, evt2, evt3; */
/* 	char *params1 = "bar"; */
/* 	char *params2 = "foo/"; */
/* 	char *params3 = "foo/bar"; */
/* 	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890"; */
/* 	unsigned int ret1, ret2; */

/* 	evt1 = evt_create(cos_spd_id()); */
/* 	evt2 = evt_create(cos_spd_id()); */
/* 	/\* evt3 = evt_create(cos_spd_id()); *\/ */
/* 	assert(evt1 > 0 && evt2 > 0); */
	
/* 	printc("1\n"); */

/* 	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1); */
/* 	if (t1 < 1) { */
/* 		printc("UNIT TEST FAILED: split failed %d\n", t1); */
/* 		return; */
/* 	} */
/* 	trelease(cos_spd_id(), t1); */
/* 	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
/* 	if (t1 < 1) { */
/* 		printc("UNIT TEST FAILED: split2 failed %d\n", t1); return; */
/* 	} */
/* 	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
/* 	if (t2 < 1) { */
/* 		printc("UNIT TEST FAILED: split3 failed %d\n", t2); return; */
/* 	} */

/* 	printc("1.5\n"); */


/* 	printc("2\n"); */


/* 	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */
/* 	printc("3\n"); */
/* 	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2)); */
/* 	printv("write %d & %d, ret %d & %d\n", strlen(data1), strlen(data2), ret1, ret2); */

/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */

/* 	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
/* 	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
/* 	if (t1 < 1 || t2 < 1) { */
/* 		printc("UNIT TEST FAILED: later splits failed\n"); */
/* 		return; */
/* 	} */

/* 	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023); */
/* 	if (ret1 > 0) buffer[ret1] = '\0'; */
/* 	/\* printv("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1); *\/ */
/* 	assert(!strcmp(buffer, data1)); */
/* 	assert(ret1 == strlen(data1)); */
/* 	buffer[0] = '\0'; */

/* 	ret1 = tread_pack(cos_spd_id(), t2, buffer, 1023); */
/* 	if (ret1 > 0) buffer[ret1] = '\0'; */
/* 	assert(!strcmp(buffer, data2)); */
/* 	assert(ret1 == strlen(data2)); */
/* 	/\* printv("read %d: %s\n", ret1, buffer); *\/ */
/* 	buffer[0] = '\0'; */

/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */

/* 	return; */
/* } */


/* void test1(void) */
/* { */
/* 	td_t t1, t2, t3; */
/* 	long evt1, evt2, evt3; */
/* 	char *params4, *params1, *params2, *params3, *params0, *params99; */
/* 	char *data0, *data1, *data2, *data3, *big_data; */
/* 	int ret1, ret2, ret3; */
/* 	char *strl, *strh; */

/* 	params0 = "doo/"; */
/* 	params99 = "koo/"; */

/* 	params1 = "foo/"; */
/* 	params2 = "bar1234567/"; */
/* 	params3 = "who"; */
/* 	params4 = "foo/bar1234567/who"; */

/* 	data1 = "l0987654321"; */
/* 	data2 = "laabbccddeeff"; */
/* 	data3 = "lkevinandy"; */
/* 	strl = "testmore_l"; */

/* 	big_data = (char *)malloc(SIZE); */
	
/* 	/\* int i; *\/ */
/* 	/\* for (i = 0; i<SIZE ; i++) big_data[i] = 'A'; *\/ */
/* 	/\* for (i = 0; i<SIZE ; i++) printc("%c", big_data[i]); *\/ */
/* 	/\* printc("\n"); *\/ */

/* 	char *merge = "delete"; */

/* 	printc("\n<<< TEST 1 START (thread %d)>>>\n", cos_get_thd_id()); */

/* 	evt1 = evt_create(cos_spd_id()); */
/* 	evt2 = evt_create(cos_spd_id()); */
/* 	evt3 = evt_create(cos_spd_id()); */

/* 	volatile unsigned long long start, end; */
/* 	rdtscll(start); */
/* 	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1); */
/* 	rdtscll(end); */
/* 	/\* printc("tsplit cost: %llu\n", end-start); *\/ */
/* 	rdtscll(start); */
/* 	t2 = tsplit(cos_spd_id(), td_root, params0, strlen(params0), TOR_ALL, evt2); */
/* 	rdtscll(end); */
/* 	/\* printc("tsplit cost: %llu\n", end-start); *\/ */
/* 	rdtscll(start); */
/* 	t3 = tsplit(cos_spd_id(), td_root, params99, strlen(params99), TOR_ALL, evt3); */
/* 	rdtscll(end); */
/* 	/\* printc("tsplit cost: %llu\n", end-start); *\/ */

/* 	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */
/* 	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2)); */
/* 	ret3 = twrite_pack(cos_spd_id(), t3, data3, strlen(data3)); */
/* 	/\* ret3 = twrite_pack(cos_spd_id(), t3, big_data, SIZE); *\/ */

/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */
/* 	trelease(cos_spd_id(), t3); */

/* 	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1); */
/* 	t2 = tsplit(cos_spd_id(), td_root, params0, strlen(params0), TOR_ALL, evt2); */
/* 	t3 = tsplit(cos_spd_id(), td_root, params99, strlen(params99), TOR_ALL, evt3); */

/* 	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023); */
/* 	if (ret1 > 0 && ret1 <= 1023) buffer[ret1] = '\0'; */
/* 	/\* printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1); *\/ */
/* 	assert(!strcmp(buffer, data1)); */
/* 	buffer[0] = '\0'; */
	
/* 	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023); */
/* 	if (ret2 > 0 && ret2 <= 1023) buffer[ret2] = '\0'; */
/* 	/\* printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret2, strlen(data2), buffer, data2); *\/ */
/* 	assert(!strcmp(buffer, data2)); */
/* 	buffer[0] = '\0'; */
	
/* 	ret3 = tread_pack(cos_spd_id(), t3, buffer, 1023); */
/* 	if (ret3 > 0 && ret3 <= 1023) buffer[ret3] = '\0'; */
/* 	/\* printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret3, strlen(data3), buffer, data3); *\/ */
/* 	assert(!strcmp(buffer, data3)); */
/* 	buffer[0] = '\0'; */
		
/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */
/* 	trelease(cos_spd_id(), t3); */
/* 	printc("<<< TEST 1 PASSED (thread %d)>>>\n\n", cos_get_thd_id()); */

/* 	return; */
/* } */


/* void test2(void) */
/* { */
/* 	td_t t1, t2, t0; */
/* 	long evt1, evt2, evt0; */
/* 	char *params0, *params1, *params2, *params3; */
/* 	char *data0, *data1, *data2, *data3; */
/* 	int ret1, ret2, ret0; */
/* 	char *strl, *strh; */

/* 	params0 = "who"; */
/* 	params1 = "barasd/"; */
/* 	params2 = "foo/"; */
/* 	params3 = "foo/barasd/who"; */
/* 	data0 = "kevinandy"; */
/* 	data1 = "0987654321"; */
/* 	data2 = "aabbccddeeff"; */
/* 	data3 = "aabbccddeeff0987654321"; */
/* 	strl = "testmore"; */

/* 	printc("\n<<< TEST 2 START (thread %d)>>>\n", cos_get_thd_id()); */

/* 	evt1 = evt_create(cos_spd_id()); */
/* 	evt2 = evt_create(cos_spd_id()); */
/* 	evt0 = evt_create(cos_spd_id()); */
/* 	assert(evt1 > 0 && evt2 > 0 && evt0 > 0); */

/* 	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
/* 	printc("t1 %d\n", t1); */
/* 	if (t1 < 1) { */
/* 		printc("  split2 failed %d\n", t1); return; */
/* 	} */

/* 	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
/* 	if (t2 < 1) { */
/* 		printc("  split1 failed %d\n", t2); return; */
/* 	} */

/* 	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0); */
/* 	if (t0 < 1) { */
/* 		printc("  split0 failed %d\n", t0); return; */
/* 	} */

/* 	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */
/* 	printc("thread %d writes str %s in %s\n", cos_get_thd_id(), data1, params2); */

/* 	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2)); */
/* 	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), data2, params2, params1); */
	
/* 	ret2 = twrite_pack(cos_spd_id(), t2, strl, strlen(strl)); */
/* 	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strl, params2, params1); */

/* 	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0)); */
/* 	printc("thread %d writes str %s in %s%s%s\n", cos_get_thd_id(), data0, params2, params1, params0); */

/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */
/* 	trelease(cos_spd_id(), t0); */

/* 	/\* let low wake up high here *\/ */
/* 	if (cos_get_thd_id() == low) { */
/* 		printc("thread %d is going to wakeup thread %d\n", low, high); */
/* 		sched_wakeup(cos_spd_id(), high); */
/* 	} */


/* 	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
/* 	printc("t1 %d\n", t1); */
/* 	if (t1 < 1) { */
/* 		printc("  split2 failed %d\n", t1); return; */
/* 	} */

/* 	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
/* 	if (t2 < 1) { */
/* 		printc("  split1 failed %d\n", t2); return; */
/* 	} */

/* 	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0); */
/* 	if (t0 < 1) { */
/* 		printc("  split0 failed %d\n", t0); return; */
/* 	} */

/* 	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023); */
/* 	if (ret1 > 0 && ret1 <= 1023) buffer[ret1] = '\0'; */
/* 	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1); */
/* 	assert(!strcmp(buffer, data1)); */
/* 	buffer[0] = '\0'; */
	
/* 	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023); */
/* 	if (ret2 > 0 && ret2 <= 1023) buffer[ret2] = '\0'; */
/* 	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret2, strlen(data2), buffer, data2); */
/* 	assert(!strcmp(buffer, data2)); */
/* 	buffer[0] = '\0'; */
	
/* 	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023); */
/* 	if (ret0 > 0 && ret0 <= 1023) buffer[ret0] = '\0'; */
/* 	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret0, strlen(data0), buffer, data0); */
/* 	assert(!strcmp(buffer, data0)); */
/* 	buffer[0] = '\0'; */

/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */
/* 	trelease(cos_spd_id(), t0); */

/* 	printc("<<< TEST 2 PASSED (thread %d)>>>\n\n", cos_get_thd_id()); */

/* 	return; */
/* } */


/* void test3(void) */
/* { */
/* 	td_t t1, t2, t0; */
/* 	long evt1, evt2, evt0; */
/* 	char *params0, *params1, *params2, *params3; */
/* 	char *data0, *data1, *data2, *data3; */
/* 	int ret1, ret2, ret0; */
/* 	char *strl, *strh; */

/* 	if (cos_get_thd_id() == high) { */
/* 		params0 = "who"; */
/* 		params1 = "bbrabc/"; */
/* 		params2 = "foo/"; */
/* 		params3 = "foo/bbrabc/who"; */
/* 		data0 = "handykevin"; */
/* 		data1 = "h1234567890"; */
/* 		data2 = "hasdf;lkj"; */
/* 		data3 = "hasdf;lkjh1234567890"; */
/* 		strh = "testmore_h"; */
/* 	} */


/* 	if (cos_get_thd_id() == low) { */
/* 		params0 = "who"; */
/* 		params1 = "bar/"; */
/* 		params2 = "foo/"; */
/* 		params3 = "foo/bar/who"; */
/* 		data0 = "lkevinandy"; */
/* 		data1 = "l0987654321"; */
/* 		data2 = "laabbccddeeff"; */
/* 		data3 = "laabbccddeeffl0987654321"; */
/* 		strl = "testmore_l"; */
/* 	} */

/* 	evt1 = evt_create(cos_spd_id()); */
/* 	evt2 = evt_create(cos_spd_id()); */
/* 	evt0 = evt_create(cos_spd_id()); */
/* 	assert(evt1 > 0 && evt2 > 0 && evt0 > 0); */

/* 	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
/* 	if (t1 < 1) { */
/* 		printc("  split2 failed %d\n", t1); return; */
/* 	} */

/* 	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
/* 	if (t2 < 1) { */
/* 		printc("  split1 failed %d\n", t2); return; */
/* 	} */

/* 	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0); */
/* 	if (t0 < 1) { */
/* 		printc("  split0 failed %d\n", t0); return; */
/* 	} */

/* 	printc("\nthread %d writes str %s in %s\n", cos_get_thd_id(), data1, params2); */
/* 	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */

/* 	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2)); */
/* 	printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), data2, params2, params1); */
	
/* 	if (cos_get_thd_id() == low) { */
/* 		ret2 = twrite_pack(cos_spd_id(), t2, strl, strlen(strl)); */
/* 		printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strl, params2, params1); */
/* 	} */
/* 	if (cos_get_thd_id() == high) { */
/* 		ret2 = twrite_pack(cos_spd_id(), t2, strh, strlen(strh)); */
/* 		printc("thread %d writes str %s in %s%s\n", cos_get_thd_id(), strh, params2, params1); */
/* 	} */

/* 	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0)); */
/* 	printc("thread %d writes str %s in %s%s%s\n", cos_get_thd_id(), data0, params2, params1, params0); */

/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */
/* 	trelease(cos_spd_id(), t0); */

/* 	/\* let low wake up high here *\/ */
/* 	if (cos_get_thd_id() == low) { */
/* 		printc("thread %d is going to wakeup thread %d\n", low, high); */
/* 		sched_wakeup(cos_spd_id(), high); */
/* 	} */

/* #ifdef TEST_4 */
/* 	ramfs_test2();  	/\* all another component to mimic the situation *\/ */
/* #endif */

/* 	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1); */
/* 	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2); */
/* 	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0); */
/* 	if (t1 < 1 || t2 < 1 || t0 < 1) { */
/* 		printc("later splits failed\n"); */
/* 		return; */
/* 	} */

/* 	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023); */
/* 	if (ret1 > 0) buffer[ret1] = '\0'; */
/* 	printv("thread %d read %d (%d): %s (%s)\n", cos_get_thd_id(),  ret1, strlen(data1), buffer, data1); */
/* 	buffer[0] = '\0'; */
	
/* 	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023); */
/* 	if (ret2 > 0) buffer[ret2] = '\0'; */
/* 	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret2, buffer); */
/* 	buffer[0] = '\0'; */

/* 	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023); */
/* 	if (ret0 > 0) buffer[ret0] = '\0'; */
/* 	printv("thread %d read %d: %s\n", cos_get_thd_id(), ret0, buffer); */
/* 	buffer[0] = '\0'; */
		
/* 	trelease(cos_spd_id(), t1); */
/* 	trelease(cos_spd_id(), t2); */
/* 	trelease(cos_spd_id(), t0); */

/* 	return; */
/* } */


/* void cos_init(void) */
/* { */
/* 	static int first = 0; */
/* 	union sched_param sp; */
/* 	int i; */
	
/* 	if(first == 0){ */
/* 		first = 1; */

/* 		sp.c.type = SCHEDP_PRIO; */
/* 		sp.c.value = 11; */
/* 		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0); */

/* 		sp.c.type = SCHEDP_PRIO; */
/* 		sp.c.value = 12; */
/* 		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0); */
/* 	} else { */

/* #ifdef TEST_0 */
/* 	if (cos_get_thd_id() == high) { */
/* 		timed_event_block(cos_spd_id(), 1); */
/* 		/\* for fault injection, this is too slow, instead, we can just loop *\/ */
/* 		/\* periodic_wake_create(cos_spd_id(), 1); *\/ */
/* 		i = 0; */
/* 		while(i++ < 50) { */
/* 			test0(); /\* unit test *\/ */
/* 			/\* periodic_wake_wait(cos_spd_id()); *\/ */
/* 		} */
/* 	} */
/* #endif */

/* #ifdef TEST_1 */
/* 	if (cos_get_thd_id() == high) { */
/* 		timed_event_block(cos_spd_id(), 1); */
/* 		periodic_wake_create(cos_spd_id(), 2); */
/* 		printc("ram fs test...\n"); */
/* 		/\* while(1) { *\/ */
/* 		i = 0; */
/* 		while(i++ < 50) { */
/* 			test1(); */
/* 			periodic_wake_wait(cos_spd_id()); */
/* 		} */
/* 	} */
/* #endif */

/* #ifdef TEST_2 */
/* 	if (cos_get_thd_id() == high) sched_block(cos_spd_id(), 0); */
/* 	test2(); */
/* #endif */

/* #ifdef TEST_3 */
/* 	if (cos_get_thd_id() == high) sched_block(cos_spd_id(), 0); */
/* 	test3(); */
/* #endif */

/* 	} */
	
/* 	return; */
/* } */

/* #if (!LAZY_RECOVERY) */
/* void eager_recovery_all(); */
/* #endif */
/* void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3) */
/* { */
/* 	switch (t) { */
/* 	case COS_UPCALL_EAGER_RECOVERY: */
/* #if (!LAZY_RECOVERY) */
/* 		/\* printc("eager upcall: thread %d (in spd %ld)\n", cos_get_thd_id(), cos_spd_id()); *\/ */
/* 		eager_recovery_all(); */
/* #endif */
/* 		break; */
/* 	default: */
/* 		cos_init(); */
/* 	} */

/* 	return; */
/* } */
