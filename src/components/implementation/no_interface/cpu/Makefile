C_OBJS=spin.o
ASM_OBJS=
COMPONENT=cpu.o
INTERFACES=
#DEPENDENCIES=periodic_wake sched printc mem_mgr timed_blk evt ll_log valloc_local
DEPENDENCIES=periodic_wake sched printc mem_mgr timed_blk evt ll_log valloc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif

