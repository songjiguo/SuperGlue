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

/* get a new server side id for old id. The reason to have a new api
 * for recovery is that evt_trigger is a special api which can be
 * called from a different component that the event is not
 * created. This is a more general problem for any service requires a
 * global name space. For example, cbuf/cbufp is such service. Torrent
 * is a service that we always expect the operation on the object is
 * done in the same component that the object is created (e.g, tsplit,
 * tread, twrite, trelease are always in the same component)
*/
long evt_re_create(spdid_t spdid, long old_evtid);
long evt_reflection(spdid_t spdid, long evtid);

int evt_trigger_all(spdid_t spdid);  // trigger all blocked wait threads via evt_wait : Jiguo

long evt_grp_wait(spdid_t spdid);
int evt_grp_mult_wait(spdid_t spdid, struct cos_array *data);
int evt_set_prio(spdid_t spdid, long extern_evt, int prio);
unsigned long *evt_stats(spdid_t spdid, unsigned long *stats);
int evt_stats_len(spdid_t spdid);

#endif 	    /* !EVT_H */
