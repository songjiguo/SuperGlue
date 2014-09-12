#ifndef   	EVT_H
#define   	EVT_H

typedef enum {
	EVT_NIL   = 0,
	EVT_READ  = 0x1, 
	EVT_WRITE = 0x2, 
	EVT_SPLIT = 0x4, 
	EVT_MERGE = 0x8, 
	EVT_ALL   = (EVT_READ|EVT_WRITE|EVT_SPLIT|EVT_MERGE)
} evt_flags_t;

// The following APIs have fault tolerance support
long evt_split(spdid_t spdid, long parent_evt, int grp);
void evt_free(spdid_t spdid, long extern_evt);
long evt_wait(spdid_t spdid, long extern_evt);
long evt_wait_n(spdid_t spdid, long extern_evt, int n);
int evt_trigger(spdid_t spdid, long extern_evt);

// The following APIs have no fault tolerance support
long evt_create(spdid_t spdid);

long __evt_create(spdid_t spdid); // get a new server side id  :Jiguo
int evt_trigger_all(spdid_t spdid);  // trigger all blocked wait threads via evt_wait : Jiguo

long evt_grp_wait(spdid_t spdid);
int evt_grp_mult_wait(spdid_t spdid, struct cos_array *data);
int evt_set_prio(spdid_t spdid, long extern_evt, int prio);
unsigned long *evt_stats(spdid_t spdid, unsigned long *stats);
int evt_stats_len(spdid_t spdid);

// Jiguo: this function is used to update IDs cache and the link in
// name_server. Only called on the recovery path and assume it does
// not fail
int evt_updateid(spdid_t spdid, int cli_id, int curr_id);

#endif 	    /* !EVT_H */
