/*
  name server API
 */

#ifndef NAMESERVER_H
#define NAMESERVER_H

#include <cos_component.h>

int ns_getid(spdid_t spdid);
int ns_setid(spdid_t spdid, int old_id, int curr_id);
int ns_delid(spdid_t spdid, int id) ;
int ns_del_norecevied();  // see mapping_create in evt_manager

int ns_reflection(spdid_t spdid, int id);  // see evt_free on client interface

#endif /* NAMESERVER_H */ 
