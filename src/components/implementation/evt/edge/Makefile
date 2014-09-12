C_OBJS=evt.o
ASM_OBJS=
COMPONENT=e.o
INTERFACES=evt
DEPENDENCIES=sched printc mem_mgr_large lock valloc ll_log pgfault name_server
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
