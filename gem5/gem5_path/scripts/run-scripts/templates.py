#!/usr/bin/python3
# -*- coding: utf-8 -*-
from gem5_run import update, Derived, Vary, get_benchmarks, get_default_output_subdirectory, replace_template
from gem5_config_utils import config_describe, config_describe_abbrev, config_from_tasks_gem5, get_git_revision
from options import *
from options import gem5_root as gem5_root_option
from gem5_run import gem5_root
import os
import math

# Basic config options
base = {
    simulation_mode: "full-system",
    arch: Vary(*config_from_tasks_gem5("${ENABLED_ARCHITECTURES[@]}").split(" ")),
    protocol: Vary(*config_from_tasks_gem5("${ENABLED_PROTOCOLS[@]}").split(" ")),
    cpu_model: "DerivO3CPU", # or "TimingSimpleCPU"
    cpu_name: Derived(lambda c: f"{cpu_model(c)}_{num_cpus(c)}"),
    num_cpus: Vary(*[int(i) for i in config_from_tasks_gem5("${ENABLED_NUM_CPUS[@]}").split(" ")]),
    num_cpus_half: Derived(lambda c: math.ceil(num_cpus(c) / 2)),
    random_seed: 0,

    output_directory_root: Derived(lambda c: os.path.join(gem5_root_option(c), "results")),
    output_directory_sub: Derived(lambda c: get_default_output_subdirectory(output_directory_root(c))),
    output_directory_base: Derived(lambda c: os.path.join(output_directory_root(c), output_directory_sub(c))),
    config_description: Derived(config_describe),
    config_description_abbrev: Derived(config_describe_abbrev),
    output_directory: Derived(lambda c: os.path.join(output_directory_base(c), config_description(c))),

    gem5_exec_path: Derived(lambda c: os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_gem5_binary {arch(c)} {protocol(c)} {build_type(c)})"))),
    m5_path: Derived(lambda c: os.path.join(gem5_root, "gem5_path", arch(c))),
    m5_arch: Derived(lambda c: {"x86_64": "X86",
                                "aarch64": "ARM",
                                "riscv": "riscv"}[arch(c)]),
    m5_arch: Derived(lambda c: {"x86_64": "X86",
                                "aarch64": "ARM",
                                "riscv": "riscv"}[arch(c)]),
    parsec_arch: Derived(lambda c: {"x86_64": "amd64-linux",
                                    "aarch64": "aarch64-linux", # TODO: CHECK
                                    "riscv": "riscv-linux" # TODO: CHECK
                                    }[arch(c)]),
    
    # Benchmark options
    benchmark: Vary(*get_benchmarks()),
    benchmark_name: Derived(lambda c: benchmark(c).name),
    benchmark_full_name: Derived(lambda c: benchmark(c).suite + "." + benchmark(c).name),
    benchmark_size: Derived(lambda c: benchmark(c).size),
    benchmark_subdir: Derived(lambda c: replace_template(benchmark(c).subdir_template, c)),
    benchmark_binary_suffix: Derived(lambda c: htm_binary_suffix(c) if htm_binary_suffix in c else ""),
    benchmark_htmrt_config: Derived(lambda c: htm_binary_suffix(c).replace(".htm.", "") if htm_binary_suffix in c else ""),
    benchmark_args_string: Derived(lambda c: benchmark(c).args_string),
    benchmark_input_filename: Derived(lambda c: replace_template(benchmark(c).input_filename_template, c) if benchmark(c).input_filename_template != None else None),
    benchmark_ld_preload: "",

    benchmark_binary: Derived(lambda c: replace_template(benchmark(c).binary_filename_template, c)),
    benchmark_options: Derived(lambda c: replace_template(benchmark(c).args_string, c)),
    benchmark_se_work_directory: Derived(lambda c: os.path.join(benchmarks_root_dir(c), benchmark_subdir(c))),
    benchmark_environment: Derived(lambda c:
                                   "M5_SIMULATOR=1\n" +
                                   replace_template(benchmark(c).environment_template, c) +
                                   "" ), # TODO: add options with launchscript_option=export

    # Benchmark disk image options
    benchmarks_mount_image: True,
    benchmarks_disk_image: Derived(lambda c: os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_benchmarks_disk_image {arch(c)})"))),
    benchmarks_image_device: Derived(lambda c: {"x86_64": "/dev/hdb1",
                                                "aarch64": "/dev/sdb1",
                                                "riscv": "TODO"}[arch(c)]),
    benchmarks_image_mountpoint: "/benchmarks",
    # Benchmark source and working directories root (syscall-emulation), or benchmarks mountpoint (full-system)
    benchmarks_root_dir: Derived(lambda c: {
        "syscall-emulation": os.path.join(gem5_root, config_from_tasks_gem5("${BENCHMARKS_ROOT_DIR}")),
        "full-system": benchmarks_image_mountpoint(c),
    }[simulation_mode(c)]),

    # other
    gem5_root_option: gem5_root,
    kernel_binary: Derived(lambda c: os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_kernel {arch(c)})"))),
    bootloader: Derived(lambda c: None if config_from_tasks_gem5(f"$(get_bootloader {arch(c)})") == "" else os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_bootloader {arch(c)})"))),
    
    root_device: Derived(lambda c: config_from_tasks_gem5(f"${{ARCH_ROOT_DEVICE[{arch(c)}]}}")),
    os_disk_image: Derived(lambda c: os.path.join(gem5_root, config_from_tasks_gem5(f"$(get_base_image {arch(c)})"))),
    terminal_filename: Derived(lambda c: {"x86_64": "system.pc.com_1.device",
                                          "aarch64": "system.terminal",
                                          "riscv": "TODO"}[arch(c)]),
    arch_specific_opts: Derived(lambda c: config_from_tasks_gem5(f"${{ARCH_EXTRA_OPTIONS[{arch(c)}]}}")),
    checkpoint_boot_dir: Derived(lambda c: os.path.join(gem5_root, "gem5_path", arch(c), "checkpoints", "booted", f"{num_cpus(c)}_cores")),
    
    network_model: "simple", # or 'garnet2.0'
    network_topology: "Crossbar",
    network_mesh_rows: Derived(lambda c: int(math.sqrt(num_cpus(c)))),
    memory_type: "DDR3_1600_8x8", # or 'DDR3_200cycles'  # DDR4_2400_8x8 CL16/16/16
    memory_size: Derived(lambda c: config_from_tasks_gem5(f"${{ARCH_MEMORY[{arch(c)}]}}")),
    
    # debug options
    enable_kvm: Derived(lambda c: {"x86_64": True,
                                   "aarch64": False,
                                   "riscv": False}[arch(c)]),
    debug_flags: "",
    build_type: Vary(*config_from_tasks_gem5("${ENABLED_BUILD_TYPES[@]}").split(" ")),
    exit_at_roi_end: False,
    extra_detailed_args: "",
    proc_maps_file: "ckpt/proc_maps",
    disable_transparent_hugepages: False,

    # unnecesary options:
    launchscript_filename: "launch_script.rcS",
    siminfo_filename: "simulate.info",
    runscript_filename: "simulate",
    runscript_template_filename: os.path.join(gem5_root, "gem5_path/scripts/run-scripts/simulate.template"),
    launchscript_template_filename: os.path.join(gem5_root, "gem5_path/scripts/run-scripts/launchscript.template"),
    checkpoint_init_subdir: "ckpt",
    checkpoint_init_reuse_root_dir: Derived(lambda c: os.path.join(gem5_root, "gem5_path", arch(c), "checkpoints", "init")),
    checkpoint_init_reuse: False,
    gem5_exec_snapshots_dir: Derived(lambda c: os.path.join(gem5_root, "gem5_path", "tmp-bin")),
    enable_prefetch: False,
    rm_ckpt_after_sim: False,
}

# Cpu config templates
cpu_baseline = {}

cpu_valuepred_old = update(cpu_baseline, {
    cpu_name: "old",
    cpu_confs: [
        "branchPred=TournamentBP",
        "fetchWidth=6",
        "decodeWidth=6",
        "renameWidth=6",
        "dispatchWidth=12",
        "issueWidth=12",
        "wbWidth=12",
        "commitWidth=8",
        "LQEntries=192",
        "SQEntries=114",
        "numIQEntries=140",
        "numROBEntries=512"],
})

cpu_tagescl = update(cpu_baseline, {
    cpu_name: "tagescl",
    cpu_confs: [
        "branchPred=TAGE_SC_L_64KB",
        "fetchWidth=6",
        "decodeWidth=6",
        "renameWidth=6",
        "dispatchWidth=12",
        "issueWidth=12",
        "wbWidth=12",
        "commitWidth=8",
        "LQEntries=192",
        "SQEntries=114",
        "numIQEntries=140",
        "numROBEntries=512"],
})

cpu_ltage = update(cpu_baseline, {
    cpu_name: "ltage",
    cpu_confs: [
        "branchPred=LTAGE",
        "fetchWidth=6",
        "decodeWidth=6",
        "renameWidth=6",
        "dispatchWidth=12",
        "issueWidth=12",
        "wbWidth=12",
        "commitWidth=8",
        "LQEntries=192",
        "SQEntries=114",
        "numIQEntries=140",
        "numROBEntries=512"],
})

cpu_garnet = update(cpu_baseline, {
    cpu_name: "garnet",
    network_model: "garnet",
    cpu_confs: [
        "branchPred=LTAGE",
        "fetchWidth=6",
        "decodeWidth=6",
        "renameWidth=6",
        "dispatchWidth=12",
        "issueWidth=12",
        "wbWidth=12",
        "commitWidth=8",
        "LQEntries=192",
        "SQEntries=114",
        "numIQEntries=140",
        "numROBEntries=512"],
})

cpu_ddr4 = update(cpu_baseline, {
    cpu_name: "ddr4",
    network_model: "garnet",
    memory_type: "DDR4_2400_8x8",
    cpu_confs: [
        "branchPred=LTAGE",
        "fetchWidth=6",
        "decodeWidth=6",
        "renameWidth=6",
        "dispatchWidth=12",
        "issueWidth=12",
        "wbWidth=12",
        "commitWidth=8",
        "LQEntries=192",
        "SQEntries=114",
        "numIQEntries=140",
        "numROBEntries=512"],
})

cpu_dramsim = update(cpu_baseline, {
    cpu_name: "test",
    network_model: "garnet",
    memory_type: "DRAMsim3",
    dramsim3_ini: Derived(lambda c: os.path.join(gem5_root, "ext/dramsim3/DRAMsim3/configs/DDR4_8Gb_x8_2400.ini")),
    network_mesh_rows: 0,
    cpu_confs: [
        "branchPred=LTAGE",
        "fetchWidth=6",
        "decodeWidth=6",
        "renameWidth=6",
        "dispatchWidth=12",
        "issueWidth=12",
        "wbWidth=12",
        "commitWidth=8",
        "LQEntries=192",
        "SQEntries=114",
        "numIQEntries=140",
        "numROBEntries=512"],
})



# Cache config templates

cache_baseline = {
    cache_name: "DefaultCache",
    cache_l0i_size: 32 * 1024,
    cache_l0d_size: 32 * 1024,
    cache_l0i_assoc: 8,
    cache_l0d_assoc: 8,
    cache_l1i_size: 256 * 1024,
    cache_l1d_size: 256 * 1024,
    cache_l1i_assoc: 8,
    cache_l1d_assoc: 8,
    cache_l2_num_caches: Derived(lambda c: num_cpus(c)),
    cache_l2_size_per_cache: Derived(lambda c: 32 * 1024 * 1024 // num_cpus(c)),
    cache_l2_assoc: 12,
    cache_l0_tag_access_latency: 1,
    cache_l0_data_access_latency: 1,
    cache_l1_data_access_latency: 2,
    cache_l1_tag_access_latency: 2,
    cache_l2_data_access_latency: 2,
    cache_l2_tag_access_latency: 2,
}

cache_test = update(cache_baseline, {
    cache_name: "TestCache",
    cache_l0i_size: 8 * 1024,
    cache_l0d_size: 8 * 1024,
    cache_l1i_size: 32 * 1024,
    cache_l1d_size: 32 * 1024,
    cache_l2_size_per_cache: Derived(lambda c: 256 * 1024 // num_cpus(c)),})

cache_small = update(cache_baseline, {
    cache_name: "SmallCache",
    cache_l0i_size: 4 * 1024,
    cache_l0d_size: 4 * 1024,
    cache_l1i_size: 32 * 1024,
    cache_l1d_size: 32 * 1024,
})

cache_baseline_2level = update(cache_baseline, {
    cache_name: "DefaultTwoLevelCache",
    cache_l0i_size: 0,
    cache_l0d_size: 0,
    cache_l1i_size: 32 * 1024,
    cache_l1d_size: 32 * 1024,
    cache_l2_size_per_cache: Derived(lambda c: 8 * 1024 * 1024 // num_cpus(c)),
})

cache_alder_lake_old = update(cache_baseline, {
    cache_name: "AlderLakeOld",
    cache_l1i_size: int(512 * 1024),
    cache_l1d_size: int(512 * 1024),
    cache_l1i_assoc: 8,
    cache_l1d_assoc: 8,
    cache_l0d_size: 48 * 1024,
    cache_l0d_assoc: 8,
    cache_l2_assoc: 16,
})

cache_alder_lake_new = update(cache_baseline, {
    cache_name: "AlderLake",
    cache_l1i_size: int(1.25 * 1024 * 1024),
    cache_l1d_size: int(1.25 * 1024 * 1024),
    cache_l1i_assoc: 10,
    cache_l1d_assoc: 10,
    cache_l0d_size: 48 * 1024,
    cache_l0d_assoc: 12,
    cache_l2_assoc: 12,
})
cache_alder_lake_new_lat = update(cache_alder_lake_new, {
    cache_name: "AlderLake_lat",
    cache_l0_tag_access_latency: 1,
    cache_l0_data_access_latency: 1,
    cache_l1_tag_access_latency: 12,
    cache_l2_tag_access_latency: 34,
})
model_old_baseline = update(cpu_baseline, cache_alder_lake_old)
model_tage_baseline = update(cpu_tagescl, cache_alder_lake_old)
model_ltage_baseline = update(cpu_ltage, cache_alder_lake_old)
model_cache_baseline = update(cpu_tagescl, cache_alder_lake_new_lat)
model_garnet_baseline = update(cpu_garnet, cache_alder_lake_new_lat)
model_ddr4_baseline = update(cpu_ddr4, cache_alder_lake_new_lat)
model_dramsim_baseline = update(cpu_dramsim, cache_alder_lake_new_lat)
model_final = update(cpu_dramsim, cache_alder_lake_new_lat)
# HTM templates

htm_cfg1_base = {
    # TODO ? protocol: "MESI_Three_Level_HTM_umu", # These options don't work with e.g., MESI_Three_Level
    htm_disable_speculation: False,
    htm_binary_suffix: '.htm.fallbacklock',
    htm_lazy_vm: True,
    htm_eager_cd: True,
    htm_conflict_resolution: 'requester_wins',
    htm_lazy_arbitration: None,
    htm_allow_read_set_l0_cache_evictions: False,
    htm_allow_write_set_l0_cache_evictions: False,
    htm_allow_read_set_l1_cache_evictions: False,
    htm_allow_write_set_l1_cache_evictions: False,
    htm_allow_read_set_l2_cache_evictions: False,
    htm_allow_write_set_l2_cache_evictions: False,
    htm_precise_read_set_tracking: False,
    htm_trans_aware_l0_replacements: False,
    htm_trans_aware_l1_replacements: False,
    htm_allow_load_delaying: False,
    htm_reload_if_stale: False,
    htm_delay_interrupts: False,
    htm_l0_downgrade_on_l1_gets: False,
    htm_value_checker: False,
    htm_isolation_checker: True,
    htm_visualizer: True,
    htm_max_retries: 6,
    htm_backoff: False,
    htm_heap_prefault: False,
    htm_fallback_lock_filename: "ckpt/fallback_lock",
    htm_allow_early_value_forwarding: False,
}

htm_cfg0_locks = update(htm_cfg1_base, {
    htm_binary_suffix: '.htm.sgl',
})

htm_cfg1_pf = update(htm_cfg1_base, {
    htm_heap_prefault: True,
})

htm_cfg2_base = htm_cfg1_pf

htm_cfg2_l0rsetevict = update(htm_cfg2_base, {
    htm_allow_read_set_l0_cache_evictions: True,
})

htm_cfg2_l1rsetevict = update(htm_cfg2_l0rsetevict, {
    htm_allow_read_set_l1_cache_evictions: True,
})

htm_cfg2_l2rsetevict = update(htm_cfg2_l1rsetevict, {
    htm_allow_read_set_l2_cache_evictions: True,
})

htm_cfg3_base = htm_cfg2_l1rsetevict

htm_cfg3_l0xactreplac = update(htm_cfg3_base, {
    htm_trans_aware_l0_replacements: True,
})

htm_cfg4_base = htm_cfg3_l0xactreplac

htm_cfg4_cdab = update(htm_cfg4_base, {
    htm_conflict_resolution: 'requester_stalls_cda_base',
})

htm_cfg4_cdah = update(htm_cfg4_base, {
    htm_conflict_resolution: 'requester_stalls_cda_hybrid',
})

htm_cfg4_cdab64 = update(htm_cfg4_base, {
    htm_conflict_resolution: 'requester_stalls_cda_base',
    htm_max_retries: 64,
})

htm_cfg4_cdah64 = update(htm_cfg4_base, {
    htm_conflict_resolution: 'requester_stalls_cda_hybrid',
    htm_max_retries: 64,
})

htm_cfg5_base = htm_cfg4_cdah64

htm_cfg5_precrset = update(htm_cfg5_base, {
    htm_precise_read_set_tracking: True,
})

htm_cfg5_precrset_rldstale = update(htm_cfg5_precrset, {
    htm_reload_if_stale: True,
})

htm_cfg6_base = htm_cfg5_precrset_rldstale

htm_cfg6_lazycd = update(htm_cfg6_base, {
    htm_eager_cd: False,
    htm_lazy_arbitration: 'token',
    htm_conflict_resolution: 'committer_wins',
    htm_isolation_checker: False,
    htm_reload_if_stale: False,
    htm_precise_read_set_tracking: False,
})

htm_cfg7_base = htm_cfg6_lazycd

htm_cfg7_magic = update(htm_cfg7_base, {
    htm_lazy_arbitration: 'magic',
})

htm_cfg8_base = htm_cfg1_pf
htm_cfg8_el = htm_cfg6_base
htm_cfg8_ll = htm_cfg6_lazycd
htm_cfg8_ee = update(htm_cfg6_base, {
    htm_lazy_vm: False,
    htm_allow_read_set_l2_cache_evictions: True,
    htm_allow_write_set_l0_cache_evictions: True,
    htm_allow_write_set_l1_cache_evictions: True,
    htm_allow_write_set_l2_cache_evictions: True,
    htm_isolation_checker: True,
    htm_max_retries: 64,
})
