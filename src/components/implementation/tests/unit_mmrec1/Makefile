C_OBJS=memory_rec1.o
ASM_OBJS=
COMPONENT=mm_rec1.o
INTERFACES=
DEPENDENCIES=periodic_wake sched printc lock mem_mgr unit_mmrec2 timed_blk evt ll_log valloc_local
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
