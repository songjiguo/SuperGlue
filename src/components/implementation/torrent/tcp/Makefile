C_OBJS=cos_net.o
COMPONENT=tnet.o
INTERFACES=torrent
DEPENDENCIES=sched mem_mgr_large printc lock periodic_wake timed_blk evt name_server net_internet net_portns valloc uniq_map cbufp cbuf_c torrent ll_log
IF_LIB=./net_stack.o
FN_PREPEND=server_

pre: first all
first:
	$(info |     [CP]   copying network stack from lwip)
	@cp $(LWIPOBJ) .

include ../../Makefile.subsubdir
