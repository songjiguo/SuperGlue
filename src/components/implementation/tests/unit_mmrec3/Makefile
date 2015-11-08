C_OBJS=memory_rec3.o
ASM_OBJS=
COMPONENT=mm_rec3.o
INTERFACES= unit_mmrec3
#DEPENDENCIES= sched printc lock mem_mgr ll_log valloc_local
DEPENDENCIES= sched printc lock mem_mgr ll_log valloc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
