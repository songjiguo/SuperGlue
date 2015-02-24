#ifndef   	VALLOC_H
#define   	VALLOC_H

/* Virtual address space allocation for a component */

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
int valloc_free(spdid_t spdid, spdid_t dest, void *addr, unsigned long npages);

int valloc_reset_hp(spdid_t spdid, spdid_t dest);  //Jiguo: add to restore heap pointer

#endif 	    /* !VALLOC_H */
