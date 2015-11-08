C_OBJS=base_pong.o
ASM_OBJS=
COMPONENT=b_po.o
INTERFACES=base_pong
#DEPENDENCIES=sched printc lock mem_mgr ll_log valloc_local
DEPENDENCIES=sched printc lock mem_mgr ll_log valloc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif

