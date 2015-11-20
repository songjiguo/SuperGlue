/*
  name server
 */

#ifndef NAMESERVER_H
#define NAMESERVER_H

#include <cos_component.h>

long ns_alloc(spdid_t server_spd, spdid_t cli_spdid, int existing_id);
int ns_free(spdid_t spdid, spdid_t cli_spdid, int id);

// help to upcall into the client to eagerly rebuild the event state
int ns_upcall(spdid_t spdid, int id, int type);    
#endif /* NAMESERVER_H */ 
