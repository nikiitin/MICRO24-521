#!/usr/bin/python
import string, os, sys, time, datetime, pdb, collections
import benchmarks, htm, caches

gem5root  = os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + '/../..')

gem5path_dirname = 'gem5_path'
gem5path = os.path.join(gem5root, gem5path_dirname)

# System List [HTM system options, cache options]
system_list = []

#####################################################################
##  Target Systems - Configuration
#####################################################################

# Baseline
'''
#01: base-vs-pf 1thread
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_pf, caches.cache_baseline])
'''

'''
#02: base+pf (cfg2) vs l0/l1/l2evict 1thread
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l0rsetevict, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l1rsetevict, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l2rsetevict, caches.cache_baseline])
'''

'''
#03: base+pf+l1rsetevict (cfg3) vs htmrepl
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg3_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg3_l0xactreplac, caches.cache_baseline])
'''

'''
#04: base+pf+l1rsetevict+l0xactreplac (cfg4) vs conf-res policy
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg4_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg4_cdab64, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg4_cdah64, caches.cache_baseline])
'''
'''
#05: base+pf+l1rsetevict+l0xactreplac+cdah (cfg5) vs precise read set
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg5_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg5_precrset_rldstale, caches.cache_baseline])
'''
'''
#06: base+pf+l1rsetevict+l0xactreplac+cdah+precise+reload (cfg6) vs lazycd+token
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg6_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg6_lazycd, caches.cache_baseline])
'''
'''
#07: base+pf+l1rsetevict+l0xactreplac+lazycd+token arb (cfg7) vs magic arb
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg7_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg7_magic, caches.cache_baseline])
'''

#08: baseline vs el vs ll vs ee
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_base, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_el, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_ll, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg8_ee, caches.cache_baseline])


'''
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l0xactreplac, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg2_l0rsetevict_l0xactreplac, caches.cache_baseline])

system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l2rwsetevict_pf_dwng_precise_reqstalls_eagervm, caches.cache_baseline])

system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng_lazycd_magic_cw, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng_lazycd_magic_rw, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng_lazycd_token_cw, caches.cache_baseline])

system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng_precise, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng_precise_reqstalls, caches.cache_baseline])
system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l1rsetevict_pf_dwng_precise_reqstalls_retry64, caches.cache_baseline])

system_list.append(["MESI_Three_Level_HTM_umu", htm.cfg1_l2rwsetevict_pf_dwng_precise_reqstalls_eagervm, caches.cache_baseline])
'''

'''

'''
processor_list = []

#processor_list.append(64)
#processor_list.append(32)
processor_list.append(16)
'''
processor_list.append(16)
processor_list.append(8)
processor_list.append(4)
processor_list.append(2)
processor_list.append(1)
'''

detailed_simulation_cpu_model_list = []
#detailed_simulation_cpu_model_list.append('TimingSimpleCPU')
detailed_simulation_cpu_model_list.append('DerivO3CPU')

####################################################################
##  Benchmark Selection
####################################################################

# benchmark name, input name, arguments, bench dir, binary name, binary suffix, total work items

benchmark_groups = []
#benchmark_groups.append('test-progs-caps-small')
benchmark_groups.append('stamp-small')
#benchmark_groups.append('stamp-medium')

benchmark_list = benchmarks.getBenchmarks(benchmark_groups)

simulation_list = []


#############################################################
# Simulation infrastructure options 
#############################################################
arch_name= "x86_64" # aarch64" # {aarch64,x86_64}"

if arch_name == "x86_64":
    arch = "X86"
    kernel_binary=os.path.join(gem5path, arch_name,
                               'binaries', 'vmlinux-5.4.49')
    os_disk_image=os.path.join(gem5path, arch_name,
                               'disks', 'ubuntu-18-04.img')
    benchmarks_disk_image=os.path.join(gem5path, arch_name,
                                       'disks', arch_name+'-benchmarks.img')
    root_device = '/dev/hda1'
    benchmarks_device = '/dev/hdb1'
    mount_benchmarks_device = True
    benchmarks_disk_image_mountpoint = "/benchmarks"
    enable_kvm=1
    arch_specific_opts=''
    terminal_filename="system.pc.com_1.device"
elif arch_name == "aarch64":
    arch = "ARM"
    kernel_binary=os.path.join(gem5path, arch_name,
                               'binaries', 'vmlinux.arm64')
    os_disk_image=os.path.join(gem5path, arch_name,
                               'disks', 'ubuntu-18.04-arm64-docker.img')
    benchmarks_disk_image=os.path.join(gem5path, arch_name,
                                       'disks', arch_name+'-benchmarks.img')
    root_device = '/dev/sda1'
    benchmarks_device = '/dev/sdb1'
    mount_benchmarks_device = False  # Already mounted in /data
    benchmarks_disk_image_mountpoint = "/data"
    enable_kvm=0
    arch_specific_opts=' --machine-type=VExpress_GEM5_V2 --enable-tme'
    terminal_filename='system.terminal'
else:
    print("Unknown architecture: {}. Choices are: x86_64, aarch64".format(arch_name))
    sys.exit(-1)

# Root directory where benchmarks are located in the disk image, must
# be kept in sync with IMAGE_DESTINATION_DIR in upload-to-image.sh
# script in benchmarks-tm repository.
checkpoint_boot_num=0  # Which boot ckpt number is used (if more than one available, see make-boot-checkpoints-
# Root directory where "boot" checkpoints are located
#checkpoint_boot_root_dir=os.path.join(gem5root, "checkpoints", arch, str(checkpoint_boot_num))
checkpoint_boot_root_dir=os.path.join(gem5path, arch_name, "checkpoints", "booted")
checkpoint_init_root_dir=os.path.join(gem5path, arch_name, "checkpoints", "init")
# Root directory where fast-forward "init" checkpoints will be written
checkpoint_tmpdir_prefix=os.path.join(gem5path,"checkpoints")
# The name of the subdirectory (symlink) in the output directory that
# contains (points to) the "init" checkpoint
checkpoint_subdir="ckpt"
# Name of each generated script for running a simulation
run_script_filename="simulate.sh"
# Name of generated config file with simulation configuration
sim_info_filename = 'simulate.info'
launchscript_filename = 'launch_script.rcS'
reuse_init_checkpoint = 1

# For simulations that have htm_config (HTM_umu protocols), the binary
# suffix is set as an option in the htm_config dict. For the remaining
# cases (including the HTM protocol from gem5), this is the suffix that
# gets appended to the binary.
binary_suffix = '.htm.fallbacklock'  # Use same binaries as HTM_umu configs
disable_transparent_hugepage = 1
template_script_path = os.path.join(gem5path, "scripts/simulate.sh.common")
# Directory where gem5 binaries are copied to for batch jobs
tmp_gem5_binaries_path = os.path.join(gem5path, "tmp-bin")
proc_maps_file = "ckpt/proc_maps"

extra_detailed_args = ""

build_type = 'opt' # 'opt', 'fast','debug'
network_model = 'simple' # 'garnet2.0'
memory_type='DDR3_1600_8x8' # 'DDR3_200cycles'
memory_size='3GB'
run_gdb = 0
exit_at_roi_end = 1
copy_gem5_binary_tmp_dir = 0 # Copy gem5 binary to tmp dir for simulation
run_pdb = 0
debug_flags = "" # "Exec,O3CPUAll,O3HTM,RubyHTM,ProtocolTrace"
debug_time=0
seq_no=27
num_random_seeds=4 # 10 # Number of random seeds to simulate (when '-s' option passed to gen-scripts.py)

preload="" #"/benchmarks/benchmarks-htm/Splash-3/libhooks_chkpoint.so"

simulation_tag="" # In order to tag simulations when applying patches

results_subdir="teaser/08_ee_el_ll" # The subdirectory inside "gem5/results" for
                       # simulation scripts and results

slurm_exclude_nodelist="" # "erc07" # In case a node is faulty (erc07)

for processors in processor_list :
    for (protocol, htm_config, cache_config) in system_list:
        for benchmark_config in benchmark_list :
            for detailed_simulation_cpu_model in detailed_simulation_cpu_model_list:
                benchmark_suite, benchmark, arg_prefix, processors_opt, arg_string, benchmark_subdir, binary_filename  = benchmark_config
                
                configuration = (processors, benchmark_config,
                                 detailed_simulation_cpu_model,
                                 protocol, htm_config, cache_config)
                simulation_list.append(configuration)

