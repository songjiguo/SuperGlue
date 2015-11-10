#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!pfr.o, ;!fi.o, ;!mpool.o,a3;!sm.o,a4;!l.o,a1;!te.o,a3;!eg.o,a4;!buf.o,a5;!bufp.o, ;!rfs.o, ;!ec3cli1.o, ;!ec3ser1.o, ;!ec3ser2.o, ;!ec3ser3.o, ;!va.o,a2;!vm.o,a1;!unique_map.o, ;!evtns.o, :\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-print.o|[parent_]llboot.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
pfr.o-fprr.o|mm.o|print.o|boot.o|[home_]llboot.o;\
fi.o-sm.o|va.o|print.o|mm.o|fprr.o|te.o|eg.o|llboot.o;\
\
l.o-fprr.o|mm.o|print.o|pfr.o;\
te.o-va.o|sm.o|print.o|fprr.o|mm.o|eg.o|pfr.o;\
evtns.o-va.o|fprr.o|print.o|mm.o|l.o|llboot.o;\
eg.o-va.o|fprr.o|print.o|mm.o|l.o|evtns.o|pfr.o;\
rfs.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|bufp.o|l.o|eg.o|unique_map.o|pfr.o;\
unique_map.o-sm.o|va.o|fprr.o|print.o|mm.o|l.o|eg.o|buf.o|bufp.o;\
\
sm.o-print.o|va.o|fprr.o|mm.o|boot.o|l.o|mpool.o;\
buf.o-sm.o|va.o|fprr.o|boot.o|print.o|l.o|mm.o|mpool.o;\
bufp.o-sm.o|va.o|fprr.o|print.o|l.o|mm.o|mpool.o|buf.o;\
mpool.o-print.o|va.o|fprr.o|mm.o|boot.o|l.o;\
vm.o-mm.o|fprr.o|print.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o|llboot.o;\
\
ec3cli1.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|bufp.o|l.o|[fs_]rfs.o|eg.o|ec3ser1.o|ec3ser2.o|te.o;\
ec3ser1.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|bufp.o|l.o|[fs_]rfs.o|eg.o|ec3ser2.o|ec3ser3.o|te.o|unique_map.o;\
ec3ser2.o-sm.o|va.o|print.o|l.o|buf.o|bufp.o|fprr.o|mm.o|te.o|[fs_]rfs.o|ec3ser3.o|eg.o|unique_map.o;\
ec3ser3.o-sm.o|va.o|print.o|l.o|buf.o|bufp.o|fprr.o|mm.o|te.o|[fs_]rfs.o|eg.o\
" ./gen_client_stub
