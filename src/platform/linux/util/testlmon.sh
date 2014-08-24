#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;lllog.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!mon_p.o, ;!lmoncli1.o, ;\
!lmonser1.o, ;!lmonser2.o, ;!va.o,a2:\
c0.o-llboot.o;\
lllog.o-print.o|[parent_]llboot.o;\
mm.o-print.o|[faulthndlr_]llboot.o|[parent_]llboot.o|lllog.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o|lllog.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o|lllog.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|lllog.o|llboot.o;\
l.o-fprr.o|mm.o|print.o|lllog.o|llboot.o;\
te.o-print.o|fprr.o|mm.o|l.o|lllog.o|llboot.o;\
mon_p.o-print.o|fprr.o|mm.o|te.o|lllog.o|llboot.o;\
\
lmoncli1.o-print.o|va.o|fprr.o|l.o|mm.o|lmonser1.o|te.o|lllog.o|llboot.o;\
lmonser2.o-print.o|l.o|va.o|fprr.o|mm.o|lllog.o|llboot.o;\
lmonser1.o-print.o|l.o|va.o|fprr.o|mm.o|lllog.o|te.o|lmonser2.o|llboot.o\
" ./gen_client_stub
