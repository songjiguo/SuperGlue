C_OBJS=extended_ser2.o
ASM_OBJS=
COMPONENT=ec3ser2.o
INTERFACES=ec3_ser2
DEPENDENCIES=periodic_wake sched printc rtorrent lock cbufp cbuf_c timed_blk evt name_server mem_mgr_large valloc uniq_map ec3_ser3 mem_mgr
IF_LIB=
FN_PREPEND=fs_

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
