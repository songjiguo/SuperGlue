C_OBJS=extended_cli1.o
ASM_OBJS=
COMPONENT=ec3cli1.o
INTERFACES=
DEPENDENCIES=periodic_wake sched printc lock  mem_mgr ec3_ser1 ec3_ser2 timed_blk evt
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
