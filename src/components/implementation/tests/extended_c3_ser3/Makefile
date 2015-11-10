C_OBJS=extended_ser3.o
ASM_OBJS=
COMPONENT=ec3ser3.o
INTERFACES=ec3_ser3
DEPENDENCIES=periodic_wake sched printc lock mem_mgr_large valloc timed_blk evt name_server
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
