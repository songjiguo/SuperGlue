C_OBJS=canny.o
ASM_OBJS=
COMPONENT=edge_detect.o
INTERFACES=
DEPENDENCIES=sched printc timed_blk torrent cbufp cbuf_c evt name_server mem_mgr_large valloc lock uniq_map
IF_LIB=./cos_libccv.a $(DIETDIR)/bin-i386/libm.a
OBJLIBS += $(POSIX_LIB)
FN_PREPEND=server_

pre: first all
first:
	$(info |     [CP]   copying cos_libccv.a from libccv)
	@cp $(CCVOBJ) .

include ../../Makefile.subsubdir
