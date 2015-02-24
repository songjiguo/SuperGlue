#!/bin/sh

#;!rotar.o,a7

#httpt.o-sm.o|l.o|print.o|fprr.o|mm.o|buf.o|[server_]rotar.o|te.o|va.o|pfr.o;\
#rotar.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|eg.o|va.o|initfs.o|pfr.o;\
# httperf --server=10.0.2.8 --port=200 --uri=/fs/bar --num-conns=7000

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
\
!sm.o,a1;!mpool.o, ;!buf.o,a5;!bufp.o, ;!va.o,a2;!vm.o,a1;!tif.o,a2;!tip.o, ;\
!port.o, ;!l.o,a4;!te.o,a3;!tnet.o, ;!eg.o,a5;!evtns.o, ;\
!stconnmt.o, '4:10:200:/bind:0:%d/listen:255';\
!pfr.o, ;!httpt.o,a8;!rfs.o, ;!initfs.o,a3;!unique_map.o, ;!popcgi.o, ;!fi.o, :\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-print.o|[parent_]llboot.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
\
l.o-fprr.o|mm.o|print.o|pfr.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o|eg.o|pfr.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
pfr.o-fprr.o|mm.o|print.o|boot.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
vm.o-fprr.o|print.o|mm.o|l.o|boot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|vm.o;\
fi.o-sm.o|va.o|fprr.o|print.o|mm.o|te.o|eg.o;\
\
stconnmt.o-sm.o|print.o|fprr.o|mm.o|va.o|l.o|httpt.o|te.o|[server_]tnet.o|buf.o|bufp.o|eg.o|pfr.o;\
\
httpt.o-sm.o|l.o|print.o|fprr.o|mm.o|buf.o|eg.o|bufp.o|[server_]rfs.o|te.o|va.o|pfr.o;\
initfs.o-fprr.o|print.o|mm.o|va.o|l.o|pfr.o|buf.o|bufp.o;\
\
tnet.o-sm.o|fprr.o|mm.o|print.o|l.o|te.o|eg.o|[server_]tip.o|port.o|va.o|buf.o|bufp.o|pfr.o;\
tip.o-sm.o|[server_]tif.o|va.o|fprr.o|print.o|te.o|l.o|eg.o|buf.o|bufp.o|mm.o|pfr.o;\
tif.o-sm.o|print.o|fprr.o|mm.o|l.o|va.o|te.o|eg.o|buf.o|bufp.o|pfr.o;\
port.o-sm.o|l.o|va.o|fprr.o|mm.o|print.o|pfr.o;\
\
rfs.o-sm.o|fprr.o|print.o|mm.o|buf.o|bufp.o|l.o|va.o|unique_map.o|eg.o|pfr.o;\
unique_map.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o|buf.o|bufp.o;\
popcgi.o-sm.o|fprr.o|print.o|mm.o|buf.o|bufp.o|va.o|l.o|[server_]rfs.o|eg.o|te.o;\
\
evtns.o-fprr.o|print.o|mm.o|l.o|va.o|llboot.o;\
eg.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o|pfr.o|evtns.o|llboot.o\
" ./gen_client_stub


