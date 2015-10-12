#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!sm.o,a1;!mpool.o, ;!buf.o,a5;!bufp.o, ;!va.o,a2;!vm.o,a1;\
!l.o,a4;!te.o,a3;!eg.o,a5;!evtns.o, ;!ec3cli1.o, ;!ec3ser1.o, ;!ec3ser2.o, ;\
!pfr.o, ;!fi.o, :\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-print.o|[parent_]llboot.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
\
l.o-fprr.o|mm.o|print.o|pfr.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o|eg.o|pfr.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
pfr.o-fprr.o|mm.o|print.o|boot.o|[home_]llboot.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
fi.o-sm.o|va.o|fprr.o|print.o|mm.o|te.o|eg.o;\
\
evtns.o-fprr.o|print.o|mm.o|l.o|va.o|llboot.o;\
eg.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o|pfr.o|evtns.o|llboot.o;\
\
ec3cli1.o-print.o|va.o|fprr.o|l.o|mm.o|ec3ser1.o|ec3ser2.o|buf.o|bufp.o|te.o|eg.o;\
ec3ser1.o-print.o|l.o|va.o|fprr.o|mm.o|te.o|eg.o|ec3ser2.o|buf.o|bufp.o;\
ec3ser2.o-print.o|l.o|va.o|fprr.o|mm.o|te.o|eg.o|buf.o|bufp.o\
" ./gen_client_stub


# #!/bin/sh

# ./cos_loader \
# "c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
# !l.o,a1;!te.o,a3;!eg.o,a4;!buf.o,a5;!bufp.o, ;!ec3cli1.o, ;!ec3ser1.o, ;!ec3ser2.o, ;!evtns.o, ;!pfr.o, ;!sm.o,a1;!mpool.o, ;!va.o,a2;!vm.o,a1:\
# c0.o-llboot.o;\
# fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
# mm.o-[parent_]llboot.o|print.o|[faulthndlr_]llboot.o;\
# boot.o-print.o|fprr.o|mm.o|llboot.o;\
# \
# vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
# va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
# l.o-fprr.o|mm.o|print.o|pfr.o;\
# pfr.o-fprr.o|mm.o|print.o|boot.o;\
# eg.o-fprr.o|print.o|mm.o|l.o|va.o|pfr.o|evtns.o;\
# buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
# bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o;\
# sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
# mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
# te.o-print.o|fprr.o|mm.o|va.o|eg.o;\
# evtns.o-fprr.o|print.o|mm.o|l.o|va.o;\
# \
# ec3cli1.o-print.o|va.o|fprr.o|l.o|buf.o|bufp.o|mm.o|ec3ser1.o|ec3ser2.o|te.o|eg.o;\
# ec3ser1.o-print.o|l.o|va.o|fprr.o|buf.o|bufp.o|mm.o|te.o|eg.o|ec3ser2.o;\
# ec3ser2.o-print.o|l.o|va.o|fprr.o|mm.o|buf.o|bufp.o|te.o|eg.o\
# " ./gen_client_stub
