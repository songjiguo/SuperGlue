#!/usr/bin/python
## script helps to maintain two different version of code base
## 1. normal base
## 2. recovery base
##    able to choose the combination of above two

import os
import sys

RECOVERY = 0
########################################
# interface
#######################################
service_names = ['mem_mgr','ramfs', 'sched', 'timed_blk', 'periodic_wake', 'cbuf_c', 'llboot', 'lock', 'evt', 'mbox']

# interface
interface_path = '/src/components/interface/'
rec_stubs = '__stubs_rec'
reflect_stubs = '__stubs_rec_reflection'
log_stubs = '__stubs_log'
normal_stubs = '__stubs'
using_stubs = 'stubs'

mm_rec_h = '__mem_mgr_rec.h'
mm_nor_h = '__mem_mgr.h'
mm_header = 'mem_mgr.h'

ramfs_rec_h = '__torrent_h_ramfs'
mbox_rec_h = '__torrent_h_mbox'
torrent_nor_h = '__torrent_h'
torrent_header = 'torrent.h'

cbufc_rec_h = '__cbuf_c_h_rec'
cbufc_nor_h = '__cbuf_c_h'
cbufc_header = 'cbuf_c.h'

tmem_rec_h = '__tmem_conf_rec'
tmem_nor_h = '__tmem_conf'
tmem_conf = 'tmem_conf.h'

########################################
# component
#######################################
# MM
mem_man_component_path = '/src/components/implementation/mem_mgr/naive/'
mm_rec_c = '__mem_man_rec'
mm_nor_c = '__mem_man'
mem_man_c = 'mem_man.c'

#SCHED
sched_component_path = '/src/components/implementation/sched/'
sched_rec_c = '__cos_sched_base_rec'
sched_nor_c = '__cos_sched_base'
sched_c = 'cos_sched_base.c'

#LOCK
lock_component_path = '/src/components/implementation/lock/two_phase/'
lock_rec_c = '__lock_rec'
lock_nor_c = '__lock'
lock_c = 'lock.c'

#EVT
evt_component_path = '/src/components/implementation/evt/edge/'
evtgrp_component_path = '/src/components/implementation/evt/edge_grp/'
evt_rec_c = '__evt_rec'
evt_nor_c = '__evt'
evt_c = 'evt.c'
evt_rec_reflection = '__evt_rec_reflection'

#FS
ramfs_component_path = '/src/components/implementation/torrent/ramfs/'
ramfs_rec_c = '__ramfs_rec'
ramfs_nor_c = '__ramfs'
ramfs_c = 'ramfs.c'

#MBOX
mbox_component_path = '/src/components/implementation/torrent/async_conn/'
mbox_rec_c = '__as_connector_rec'
mbox_nor_c = '__as_connector'
mbox_c = 'as_connector.c'

#TE
te_component_path = '/src/components/implementation/timed_blk/timed_evt/'
te_rec_c = '__timed_event_rec'
te_nor_c = '__timed_event'
te_c = 'timed_event.c'

#PE
pe_component_path = '/src/components/implementation/timed_blk/timed_evt/'
pe_rec_c = '__timed_event_rec'
pe_nor_c = '__timed_event'
pe_c = 'timed_event.c'

#CBUF
cbuf_component_path = '/src/components/implementation/cbuf_c/naive/'
cbuf_rec_c = '__cbuf_rec'
cbuf_nor_c = '__cbuf'
cbuf_c = 'cbuf.c'

#llboot (boot_deps.h)
llboot_component_path = '/src/components/implementation/no_interface/llboot/'
llboot_rec_h = '__boot_deps_rec'
llboot_nor_h = '__boot_deps'
llboot_h = 'boot_deps.h'

########################################
# system setting (e.g. component/include...)
#######################################
sys_names = ['cos_types', 'cos_component', 'inv', 'spd', 'thread', 'mmap', 'hijack', 'fs', 'cbuf', 'ring_buff']

cos_types_path = '/src/kernel/include/shared/'
cos_types_rec_h = '__cos_types_rec'
cos_types_nor_h = '__cos_types'
cos_types_h = 'cos_types.h'

cos_component_path = '/src/components/include/'
cos_component_rec_h = '__cos_component_rec'
cos_component_nor_h = '__cos_component'
cos_component_h = 'cos_component.h'

fs_path = '/src/components/include/'
fs_rec_h = '__fs_rec'
fs_nor_h = '__fs'
fs_h = 'fs.h'

cbuf_path = '/src/components/include/'
cbuf_rec_h = '__cbuf_h_rec'
cbuf_nor_h = '__cbuf_h'
cbuf_h = 'cbuf.h'

inv_path = '/src/kernel/'
inv_rec_c = '__inv_rec'
inv_nor_c = '__inv'
inv_c = 'inv.c'

spd_path = '/src/kernel/include/'
spd_rec_h = '__spd_rec'
spd_nor_h = '__spd'
spd_h = 'spd.h'

spdc_path = '/src/kernel/'
spd_rec_c = '__spd_rec'
spd_nor_c = '__spd'
spd_c = 'spd.c'

thread_path = '/src/kernel/include/'
thread_rec_h = '__thread_rec'
thread_nor_h = '__thread'
thread_h = 'thread.h'

mmap_path = '/src/kernel/include/'
mmap_rec_h = '__mmap_rec'
mmap_nor_h = '__mmap'
mmap_h = 'mmap.h'

mmapc_path = '/src/kernel/'
mmap_rec_c = '__mmap_rec'
mmap_nor_c = '__mmap'
mmap_c = 'mmap.c'

ring_buff_path = '/src/kernel/'
ring_buff_rec_c = '__ring_buff_rec'
ring_buff_nor_c = '__ring_buff'
ring_buff_c = 'ring_buff.c'

hijack_path = '/src/platform/linux/module/'
hijack_rec_c = '__hijack_rec'
hijack_nor_c = '__hijack'
hijack_c = 'hijack.c'


########################################
# functions to update the symbolic
#######################################
def query(name, default = "0"):

    valid = {"n":"normal","r":"recovery", "l":"log"}

    prompt = " [n/r/l] "
#    print 
    sys.stdout.write(name + prompt)

    choice = raw_input().lower()
    if default is not None and choice == '':
        return default
    elif choice in valid.keys():
        return valid[choice]
    else:
        print "please type n/r/l"
        return '0'

def set_reflection_interface(name, par):
    
    prefix = path + interface_path + name
    os.chdir(prefix)

    if os.path.exists(p_dst):
        os.unlink(p_dst)
        sys.exit
    # interface
    if (par == 'normal') or (par == 'n'):
        os.system("ln -s " + p_nor + " " + p_dst)
    elif (par == 'recovery') or (par == 'r'):
        os.system("ln -s " + p_reflect + " " + p_dst)
    else:
        os.system("ln -s " + p_nor + " " + p_dst)
    return

def set_interface(name, par):

    global RECOVERY

    prefix = path + interface_path
    if (name == 'ramfs') or (name == 'mbox'):
        prefix = prefix + 'torrent'
    else:
        prefix = prefix + name

    os.chdir(prefix)
    if (par == 0):
        ret = query(name, '0')
        if (ret == '0'):
            os.system("ls -alF | grep '>' | grep 'stub'| awk '{print $8 $9 $10}'")
            return ret

        if os.path.exists(p_dst):
            os.unlink(p_dst)
            # interface
        if (ret == 'normal') or (ret == 'n'):
            #print p_dst
            os.system("ln -s " + p_nor + " " + p_dst)
        elif (ret == 'recovery') or (ret == 'r'):
            RECOVERY = 1
            if (name == 'ramfs') or (name == 'mbox'):
                os.system("ln -s " + p_rec + "_"+ name + " " + p_dst)
            else:
                os.system("ln -s " + p_rec + " " + p_dst)
        elif (ret == 'log') or (ret == 'l'):
            os.system("ln -s " + p_log + " " + p_dst)
        else:
            os.system("ln -s " + p_nor + " " + p_dst)

        if (name == 'mbox') or (name == 'periodic_wake'):
            set_reflection_interface('evt', ret)

        if (name == 'lock') or (name == 'evt'):
            set_reflection_interface('sched', ret)

        if (name == 'mbox') or (name == 'evt') or (name == 'ramfs'):
            set_reflection_interface('lock', ret)

        return ret

def set_link(name, c_path, dest, nor, rec, par):

    global RECOVERY

    if (par == 0):
        ret = set_interface(name, par)
        if (ret == '0'):
            return
    elif (par == 1):   # system settings
        ret = "recovery"
        # ret = query(name, '0')
        # if (ret == '0'):
        #     prefix_ramfs = path + c_path
        #     os.chdir(prefix_ramfs)
        #     print name
        #     os.system("ls -alF | grep '>' | grep " + name + "| awk '{print $8 $9 $10}'")
        #     return
    elif not par:
        return
    else:
        ret = par

    prefix_ramfs = path + c_path
    os.chdir(prefix_ramfs)

    if os.path.exists(dest):
        os.unlink(dest)
    if (name == "llboot"):
        if os.path.exists("Makefile"):
            os.unlink("Makefile")
    if (ret == 'normal') or (ret == 'n'):
        os.system("ln -s " + nor + " " + dest)
        if (name == "llboot"):
            os.system("ln -s __Makefile  Makefile")
    elif (ret == 'recovery') or (ret == 'r'):
        RECOVERY = 1
        os.system("ln -s " + rec + " " + dest)
        if (name == "llboot"):
            os.system("ln -s __Makefile_rec  Makefile") 
    else:
        os.system("ln -s " + nor + " " + dest)

    return ret

def main():

    global p_rec, p_reflect, p_nor, p_dst, path
    path = os.path.dirname(os.getcwd())

    p_rec = rec_stubs
    p_reflect = reflect_stubs
    p_log = log_stubs
    p_nor = normal_stubs
    p_dst = using_stubs

    print
    print "setting normal version or fault tolerance version"
    print "(n:normal, r:recovery, nothing: current version)"

    for i in range(0,len(service_names)):
        # # component MM
        if (service_names[i] == 'mem_mgr'):
            print service_names[i]
            ret = set_link(service_names[i], mem_man_component_path, mem_man_c, mm_nor_c, mm_rec_c, 0)
            set_link(service_names[i], interface_path+service_names[i], mm_header, mm_nor_h, mm_rec_h, ret)
            print
        # # component SCHED
        if (service_names[i] == 'sched'):
            print service_names[i]
            ret = set_link(service_names[i], sched_component_path, sched_c, sched_nor_c, sched_rec_c, 0)
            print            
        # component FS
        if (service_names[i] == 'ramfs'):
            print service_names[i]
            ret = set_link(service_names[i], ramfs_component_path, ramfs_c, ramfs_nor_c, ramfs_rec_c, 0)
            set_link(service_names[i], interface_path +'torrent', torrent_header, torrent_nor_h, ramfs_rec_h, ret)
            print
        # component MBOX
        if (service_names[i] == 'mbox'):
            print service_names[i]
            set_link(service_names[i], mbox_component_path, mbox_c, mbox_nor_c, mbox_rec_c, 0)
            set_link(service_names[i], interface_path+'torrent', torrent_header, torrent_nor_h, mbox_rec_h, ret)
            print
        # component TE
        if (service_names[i] == 'timed_blk'):
            print service_names[i]
            set_link(service_names[i], te_component_path, te_c, te_nor_c, te_rec_c, 0)
            print
        # component PE
        if (service_names[i] == 'periodic_wake'):
            print service_names[i]
            set_link(service_names[i], pe_component_path, pe_c, pe_nor_c, pe_rec_c, 0)
            print
        # component LOCK
        if (service_names[i] == 'lock'):
            print service_names[i]
            set_link(service_names[i], lock_component_path, lock_c, lock_nor_c, lock_rec_c, 0)
            print
        # component EVT
        if (service_names[i] == 'evt'):
            print service_names[i]
            set_link(service_names[i], evt_component_path, evt_c, evt_nor_c, evt_rec_c, 0)
            set_link(service_names[i], evtgrp_component_path, evt_c, evt_nor_c, evt_rec_c, 0)
            print
            
        # # component CBUF
        if (service_names[i] == 'cbuf_c'):
            print service_names[i]
            ret = set_link(service_names[i], cbuf_component_path, cbuf_c, cbuf_nor_c, cbuf_rec_c, 0)
            ret = set_link(service_names[i], interface_path+service_names[i], cbufc_header, cbufc_nor_h, cbufc_rec_h, 0)
            set_link(service_names[i], interface_path+service_names[i], tmem_conf, tmem_nor_h, tmem_rec_h, ret)
            print
        # component llboot
        if (service_names[i] == 'llboot'):
            print service_names[i]
            set_link(service_names[i], llboot_component_path, llboot_h, llboot_nor_h, llboot_rec_h, 1)
            print


    if (RECOVERY == 1):
        print "System setting..."
        for i in range(0,len(sys_names)):
            # system wide symbolic link update
            if (sys_names[i] == 'cos_types'):
                set_link(sys_names[i], cos_types_path, cos_types_h, cos_types_nor_h, cos_types_rec_h, 1)
            if (sys_names[i] == 'cos_component'):
                set_link(sys_names[i], cos_component_path, cos_component_h, cos_component_nor_h, cos_component_rec_h, 1)
            if (sys_names[i] == 'inv'):
                set_link(sys_names[i], inv_path, inv_c, inv_nor_c, inv_rec_c, 1)
            if (sys_names[i] == 'spd'):            
                set_link(sys_names[i], spd_path, spd_h, spd_nor_h, spd_rec_h, 1)
                set_link(sys_names[i], spdc_path, spd_c, spd_nor_c, spd_rec_c, 1)
            if (sys_names[i] == 'thread'):
                set_link(sys_names[i], thread_path, thread_h, thread_nor_h, thread_rec_h, 1)
            if (sys_names[i] == 'mmap'):
                set_link(sys_names[i], mmap_path, mmap_h, mmap_nor_h, mmap_rec_h, 1)
                set_link(sys_names[i], mmapc_path, mmap_c, mmap_nor_c, mmap_rec_c, 1)
            if (sys_names[i] == 'ring_buff'):
                set_link(sys_names[i], ring_buff_path, ring_buff_c, ring_buff_nor_c, ring_buff_rec_c, 1)
            if (sys_names[i] == 'fs'):
                set_link(sys_names[i], fs_path, fs_h, fs_nor_h, fs_rec_h, 1)
            if (sys_names[i] == 'cbuf'):
                set_link(sys_names[i], cbuf_path, cbuf_h, cbuf_nor_h, cbuf_rec_h, 1)
            if (sys_names[i] == 'hijack'):
                set_link(sys_names[i], hijack_path, hijack_c, hijack_nor_c, hijack_rec_c, 1)
        print "done"
main()

