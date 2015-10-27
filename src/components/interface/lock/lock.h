#ifndef   	LOCK_H
#define   	LOCK_H

#include <cos_synchronization.h>

unsigned long *lock_stats(spdid_t spdid, unsigned long *s);
int lock_stats_len(spdid_t spdid);

// release all locks that were contended in fault component (dest) : Jiguo
int lock_trigger_all(spdid_t spdid, int dest);  

void client_fault_notification(spdid_t spdid);  // to replace above function

#endif 	    /* !LOCK_H */
