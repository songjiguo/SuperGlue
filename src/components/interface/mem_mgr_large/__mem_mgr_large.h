/* /\** */
/*  * Copyright 2010 by The George Washington University.  All rights reserved. */
/*  * */
/*  * Redistribution of this file is permitted under the GNU General */
/*  * Public License v2. */
/*  * */
/*  * Author: Gabriel Parmer, gparmer@gwu.edu, 2010 */
/*  *\/ */

/* #ifndef   	MEM_MGR_H */
/* #define   	MEM_MGR_H */

/* /\* Map a physical frame into a component. *\/ */
/* vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags); */
/* /\*  */
/*  * remove this single mapping _and_ all descendents.  FIXME: this can */
/*  * be called with the spdid of a dependent component.  We should also */
/*  * pass in the component id of the calling component to ensure that it */
/*  * is allowed to remove the designated page. */
/*  *\/ */
/* int mman_release_page(spdid_t spd, vaddr_t addr, int flags); */
/* /\* remove all descendent mappings of this one (but not this one). *\/  */
/* int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags);  */
/* /\* The invoking component (s_spd) must own the mapping. *\/ */
/* vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr); */
/* static inline vaddr_t */
/* mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags) */
/* { return __mman_alias_page(s_spd, s_addr, ((u32_t)d_spd<<16)|flags, d_addr); } */
/* void mman_print_stats(void); */

/* vaddr_t mman_reflect(spdid_t spd, int src_spd, int cnt); */

/* #endif 	    /\* !MEM_MGR_H *\/ */



// get this from mem_mgr Jiguo

#ifndef   	MEM_MGR_H
#define   	MEM_MGR_H

#include <c3_test.h>

/* #define TEST_MM_GET_PAGE */
/* #define TEST_MM_ALIAS_PAGE */
/* #define TEST_MM_REVOKE_PAGE */

/* Map a physical frame into a component. */
vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags);
/* 
 * remove this single mapping _and_ all descendents.  FIXME: this can
 * be called with the spdid of a dependent component.  We should also
 * pass in the component id of the calling component to ensure that it
 * is allowed to remove the designated page.
 */
int mman_release_page(spdid_t spd, vaddr_t addr, int flags);
/* remove all descendent mappings of this one (but not this one). */ 
int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags); 
/* The invoking component (s_spd) must own the mapping. */
vaddr_t __mman_alias_page(spdid_t s_spd, vaddr_t s_addr, u32_t d_spd_flags, vaddr_t d_addr);
static inline vaddr_t 
mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr, int flags)
{ return __mman_alias_page(s_spd, s_addr, ((u32_t)d_spd<<16)|flags, d_addr); }
void mman_print_stats(void);

vaddr_t mman_reflect(spdid_t spd, int src_spd, int cnt);

/* change the type to long, so we can return the error code*/
vaddr_t mman_get_page_exist(spdid_t spd, vaddr_t addr);

#ifdef MM_C3
extern void mem_mgr_cli_if_recover_upcall_entry(vaddr_t addr);
extern void mem_mgr_cli_if_recover_upcall_subtree_entry(vaddr_t addr);
extern void mem_mgr_cli_if_remove_upcall_subtree_entry(vaddr_t addr);
extern void mem_mgr_cli_if_recover_all_alias_upcall_entry(vaddr_t addr);
#endif

#endif 	    /* !MEM_MGR_H */
