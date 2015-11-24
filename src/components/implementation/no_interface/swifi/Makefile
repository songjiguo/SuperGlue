C_OBJS=swifi.o
ASM_OBJS=
COMPONENT=fi.o
INTERFACES=
DEPENDENCIES=periodic_wake printc sched cbuf_c valloc mem_mgr_large evt timed_blk ll_log recovery_upcall

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
