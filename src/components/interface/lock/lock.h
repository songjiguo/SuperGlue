#ifndef   	LOCK_H
#define   	LOCK_H

#include <cos_synchronization.h>

unsigned long *lock_stats(spdid_t spdid, unsigned long *s);
int lock_stats_len(spdid_t spdid);

// release all locks that were contended in fault component (dest) : Jiguo
int lock_trigger_all(spdid_t spdid, int dest);  

#endif 	    /* !LOCK_H */
