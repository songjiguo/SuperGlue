#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <objtype.h>

#include <sched.h>
#include <lock.h>
#include <cstub.h>

/* CSTUB_FN(unsigned long, lock_component_alloc) (struct usr_inv_cap *uc, */
/* 					       spdid_t spdid) */
/* { */
/* 	long fault; */
/* 	unsigned long ret; */

/* 	CSTUB_INVOKE(ret, fault, uc, 1, spdid); */

/* 	return ret; */
/* } */

/* CSTUB_FN(int, lock_component_free) (struct usr_inv_cap *uc, */
/* 				    spdid_t spdid, unsigned long lock_id) */
/* { */
/* 	long fault = 0; */
/* 	int ret; */

/* 	CSTUB_INVOKE(ret, fault, uc, 2 , spdid, lock_id); */

/* 	return ret; */
/* } */

/* CSTUB_FN(int, lock_component_pretake) (struct usr_inv_cap *uc, */
/* 				       spdid_t spdid, unsigned long lock_id,  */
/* 				       unsigned short int thd) */
/* { */
/* 	long fault = 0; */
/* 	int ret; */
	
/* 	CSTUB_INVOKE(ret, fault, uc, 3, spdid, lock_id, thd); */

/* 	return ret; */
/* } */

/* CSTUB_FN(int, lock_component_take) (struct usr_inv_cap *uc, */
/* 				    spdid_t spdid, */
/* 				    unsigned long lock_id, unsigned short int thd) */
/* { */
/* 	long fault = 0; */
/* 	int ret; */
	
/* 	CSTUB_INVOKE(ret, fault, uc, 3, spdid, lock_id, thd); */
	
/* 	return ret; */
/* } */

/* CSTUB_FN(int, lock_component_release) (struct usr_inv_cap *uc, */
/* 				       spdid_t spdid, unsigned long lock_id) */
/* { */
/* 	long fault = 0; */
/* 	int ret; */

/* 	CSTUB_INVOKE(ret, fault, uc, 2, spdid, lock_id); */

/* 	return ret; */
/* } */
