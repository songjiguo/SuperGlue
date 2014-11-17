#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!eg.o,a4;!mm_rec1.o, ;!mm_rec2.o, ;!mm_rec3.o, ;!mm_rec4.o, ;!va.o,a2;!vm.o,a1:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
l.o-fprr.o|mm.o|print.o;\
eg.o-fprr.o|print.o|mm.o|l.o|va.o;\
te.o-print.o|fprr.o|mm.o|va.o|eg.o;\
\
mm_rec1.o-print.o|fprr.o|va.o|l.o|mm.o|mm_rec2.o|te.o;\
mm_rec2.o-print.o|fprr.o|va.o|l.o|mm.o|mm_rec3.o|mm_rec4.o;\
mm_rec3.o-print.o|fprr.o|va.o|l.o|mm.o;\
mm_rec4.o-print.o|fprr.o|va.o|l.o|mm.o\
" ./gen_client_stub
