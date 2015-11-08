C_OBJS=evt.o
ASM_OBJS=
COMPONENT=eg.o
INTERFACES=evt
DEPENDENCIES=sched lock recovery_upcall printc mem_mgr_large valloc name_server ll_log 
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
