#ifndef   	PERIODIC_WAKE_H
#define   	PERIODIC_WAKE_H


/* #define TEST_PTE_CREATE_BEFORE  // pte create  -- still has lock */
#define TEST_PTE_CREATE_AFTER   // pte create  -- does not have lock
/* #define TEST_PTE_WAIT_BEFORE  // pte wait  -- before evt_wait */
/* #define TEST_PTE_WAIT_AFTER   // pte wait  -- after evt_wait */

/* #define TEST_PTE_TIMER_THD_BEFORE  // timer thread might fail */
/* #define TEST_PTE_TIMER_THD_AFTER   // timer thread might fail */


/* #define BENCHMARK_MEAS_CREATE */
/* #define BENCHMARK_MEAS_WAIT */
/* #define BENCHMARK_MEAS_TIMER_THD */


int periodic_wake_create(spdid_t spdinv, unsigned int period);

// c^3 api for recreate pte thread
int c3_periodic_wake_create(spdid_t spdinv, unsigned int period, unsigned int c3_ticks);

int periodic_wake_remove(spdid_t spdinv, unsigned short int tid);
int periodic_wake_wait(spdid_t spdinv);
int periodic_wake_get_misses(unsigned short int tid);
int periodic_wake_get_deadlines(unsigned short int tid);
long periodic_wake_get_lateness(unsigned short int tid);
long periodic_wake_get_miss_lateness(unsigned short int tid);
int periodic_wake_get_period(unsigned short int tid);

#endif 	    /* !PERIODIC_WAKE_H */
