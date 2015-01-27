/*
  name server API
 */

#ifndef NAMESERVER_H
#define NAMESERVER_H

#include <cos_component.h>

long ns_alloc(spdid_t server_spd, spdid_t cli_spdid);
int ns_free(spdid_t spdid, spdid_t cli_spdid, int id);  // delete all ids on the list of id
long ns_lookup(spdid_t spdid, int id); // return the id of the list "head" -- old cli id
/* add curr id to the list of old id */
int ns_update(spdid_t spdid, int old_id, int curr_id, long par);

/* check if an entry is presented for different type, e.g, if exist,
 * or fault counter of the server..... Now, 1 for getting the
 * ser_ftcnt*/
long ns_reflection(spdid_t spdid, int id, int type);  

// upcall into each client to eagerly rebuild the event state
int ns_upcall(spdid_t spdid);    
#endif /* NAMESERVER_H */ 
