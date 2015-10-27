C_OBJS=extended_ser1.o
ASM_OBJS=
COMPONENT=ec3ser1.o
INTERFACES=ec3_ser1
DEPENDENCIES=periodic_wake sched printc cbufp cbuf_c lock mem_mgr_large valloc timed_blk evt name_server ec3_ser2 ec3_ser3 rtorrent uniq_map mem_mgr
IF_LIB=
FN_PREPEND=fs_

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
