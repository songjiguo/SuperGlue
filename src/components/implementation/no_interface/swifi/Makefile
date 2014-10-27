C_OBJS=swifi.o
ASM_OBJS=
COMPONENT=fi.o
INTERFACES=
DEPENDENCIES=periodic_wake sched printc cbuf_c lock mem_mgr_large valloc evt timed_blk ll_log

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
