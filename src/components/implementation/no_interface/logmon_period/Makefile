C_OBJS=logmon_period.o
ASM_OBJS=
COMPONENT=mon_p.o
INTERFACES=
DEPENDENCIES=periodic_wake sched printc mem_mgr evt timed_blk ll_log valloc_local

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
