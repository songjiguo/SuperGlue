#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!mpool.o,a3;!sm.o,a4;!eg.o,a4;!buf.o,a5;!bufp.o, ;!evtns.o, ;!ec3cli1.o, ;!ec3ser1.o, ;!ec3ser2.o, ;!ec3ser3.o, ;!pfr.o, ;!va.o,a2;!vm.o,a1:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
l.o-fprr.o|mm.o|print.o|pfr.o;\
pfr.o-fprr.o|mm.o|print.o|boot.o;\
mpool.o-print.o|va.o|fprr.o|mm.o|boot.o|l.o;\
sm.o-print.o|va.o|fprr.o|mm.o|boot.o|l.o|mpool.o;\
buf.o-sm.o|va.o|fprr.o|boot.o|print.o|l.o|mm.o|mpool.o;\
bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o;\
evtns.o-fprr.o|print.o|mm.o|l.o|va.o;\
eg.o-fprr.o|print.o|mm.o|l.o|va.o|pfr.o|evtns.o;\
te.o-print.o|fprr.o|mm.o|va.o|eg.o;\
\
ec3cli1.o-print.o|va.o|fprr.o|l.o|mm.o|buf.o|bufp.o|ec3ser1.o|ec3ser2.o|te.o|eg.o;\
ec3ser1.o-print.o|l.o|va.o|fprr.o|mm.o|buf.o|bufp.o|te.o|eg.o|evtns.o|ec3ser2.o|ec3ser3.o;\
ec3ser2.o-print.o|l.o|va.o|fprr.o|mm.o|te.o|eg.o|evtns.o;\
ec3ser3.o-print.o|l.o|va.o|fprr.o|mm.o|te.o|eg.o|evtns.o\
" ./gen_client_stub
