#ifndef   	VALLOC_H
#define   	VALLOC_H

/* Virtual address space allocation for a component */

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages);

//Jiguo: restore heap pointer
int valloc_reset_hp(spdid_t spdid, spdid_t dest);  
 // Jiguo: upcall to rebuild alias
int valloc_upcall(spdid_t spdid, vaddr_t addr, int upcall_type);  

// upcall type for recovery 
#define REC_PARENT   0
#define REC_SUBTREE  1

// upcall type for remove
#define REC_REMOVE 0

#endif 	    /* !VALLOC_H */
