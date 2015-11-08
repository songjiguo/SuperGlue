C_OBJS=ping2.o
ASM_OBJS=
COMPONENT=pi2.o
INTERFACES=ping2
#DEPENDENCIES=sched printc lock mem_mgr pong_test logmonitor ll_log valloc_local
DEPENDENCIES=sched printc lock mem_mgr pong_test logmonitor ll_log valloc
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
