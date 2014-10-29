#ifndef   	LOCK_H
#define   	LOCK_H

#include <cos_synchronization.h>


/* #define TEST_LOCK_ALLOC */
/* #define TEST_LOCK_PRETAKE */
/* #define TEST_LOCK_TAKE_BEFORE */
/* #define TEST_LOCK_TAKE_AFTER */
/* #define TEST_LOCK_RELEASE_BEFORE */
/* #define TEST_LOCK_RELEASE_AFTER */

/* #define BENCHMARK_MEAS_TAKE */
/* #define BENCHMARK_MEAS_PRETAKE */
/* #define BENCHMARK_MEAS_ALLOC */

/* #define BENCHMARK_MEAS_CREATION_TIME */

unsigned long *lock_stats(spdid_t spdid, unsigned long *s);
int lock_stats_len(spdid_t spdid);

// release all locks that were contended in fault component (dest) : Jiguo
int lock_trigger_all(spdid_t spdid, int dest);  

#endif 	    /* !LOCK_H */
