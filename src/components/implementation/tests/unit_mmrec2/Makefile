C_OBJS=memory_rec2.o
ASM_OBJS=
COMPONENT=mm_rec2.o
INTERFACES=unit_mmrec2
#DEPENDENCIES=sched printc lock mem_mgr unit_mmrec3 unit_mmrec4 ll_log valloc_local
DEPENDENCIES=sched printc lock mem_mgr unit_mmrec3 unit_mmrec4 ll_log valloc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
