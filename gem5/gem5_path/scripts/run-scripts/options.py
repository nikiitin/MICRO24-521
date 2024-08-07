#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import Option, Benchmark, set_options_module

set_options_module(locals()) # create variables in this scope for the Options defined below

# Basic options
Option("gem5_root", str,
       gem5_option_use = "no",
       runscript_option = "yes:GEM5_ROOT",
       siminfo_exclude = True)
Option("arch", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       descr_dir = "{value}")
Option("m5_arch", str, # derived from arch, but a different notation
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("parsec_arch", str, # derived from arch, but a different notation
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("protocol", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       descr_dir = "{value}")
Option("cpu_model", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       descr_dir = "{value}")
Option("cpu_name", str,
       gem5_option_use = "no",
       descr_dir = "{value}",
       descr_abbrev = "CPU")
Option("cpu_confs", list)
Option("num_cpus", int,
       gem5_option_use = "general",
       launchscript_option = "yes",
       descr_dir = "{value}p")
Option("num_cpus_half", int, # derived as num_cpus / 2 rounded up
       gem5_option_use = "no",
       runscript_option = "omit")
Option("output_directory_root", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("output_directory_sub", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("output_directory_base", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("config_description", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("output_directory", str,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("random_seed", int,
       gem5_option_use = "no",
       launchscript_option = "yes",
       descr_dir = "{value}")
Option("simulation_mode", str, # full-system or syscall-emulation
       gem5_option_use = "no",
       descr_dir = "{value}")
Option("launchscript_filename", str, # unnecesary
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("siminfo_filename", str, # unnecesary
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("runscript_filename", str, # unnecesary
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("runscript_template_filename", str, # unnecesary
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("launchscript_template_filename", str, # unnecesary
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)

Option("gem5_exec_path", str,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("gem5_exec_path_original", str,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("gem5_exec_snapshots_dir", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       siminfo_exclude = True)
Option("m5_path", str,
       gem5_option_use = "no",
       runscript_option = "export:M5_PATH",
       siminfo_exclude = True)

# Debug options
Option("enable_kvm", bool,
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True)
Option("debug_start_tick", int,
       gem5_option_use = "no", # special treatment in simulate.template
       runscript_option = "yes",
       gem5_option = "debug-start",
       siminfo_exclude = True)
Option("debug_end_tick", int,
       gem5_option_use = "no", # special treatment in simulate.template
       runscript_option = "yes",
       gem5_option = "debug-end",
       siminfo_exclude = True)
Option("debug_flags", str,
       gem5_option_use = "no", # special treatment in simulate.template
       runscript_option = "yes",
       siminfo_exclude = True)
Option("build_type", str, # 'opt', 'fast', 'debug'
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("exit_at_roi_end", bool,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("extra_detailed_args", str,
       gem5_option_use = "no")
Option("proc_maps_file", str,
       gem5_option_use = "no")
Option("disable_transparent_hugepages", bool,
       gem5_option_use = "no",
       launchscript_option = "yes")

# Boot options
Option("kernel_binary", str,
       gem5_option = "kernel",
       gem5_option_use = "full-system",
       runscript_option = "yes",
       siminfo_exclude = True)
Option("bootloader", str,
       gem5_option_use = "full-system",
       siminfo_exclude = True)
Option("root_device", str,
       gem5_option_use = "full-system",
       siminfo_exclude = True)
Option("os_disk_image", str,
       gem5_option = "disk-image",
       gem5_option_use = "full-system",
       runscript_option = "yes",
       siminfo_exclude = True)
Option("terminal_filename", str,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("arch_specific_opts", str,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("checkpoint_boot_dir", str,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("checkpoint_init_subdir", str, # unnecesary
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("checkpoint_init_reuse", bool,
       gem5_option_use = "no",
       siminfo_exclude = True)
Option("checkpoint_init_reuse_root_dir", str,
       gem5_option_use = "no",
       siminfo_exclude = True)

# Benchmark options
Option("benchmark", Benchmark,
       gem5_option_use = "no",
       runscript_option = "omit")
Option("benchmark_name", str,
       gem5_option_use = "no",
       runscript_option = "omit")
Option("benchmark_full_name", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       descr_dir = "{value}")
Option("benchmark_size", str,
       gem5_option_use = "no",
       runscript_option = "omit",
       descr_dir = "{value}")
Option("benchmark_subdir", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_binary_suffix", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       runscript_option = "omit")
Option("benchmark_args_string", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_input_filename", str,
       gem5_option = "input",
       gem5_option_use = "syscall-emulation",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_input_filename", str,
       gem5_option = "input",
       gem5_option_use = "syscall-emulation",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_ld_preload", str,
       gem5_option_use = "no",
       launchscript_option = "export:LD_PRELOAD",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_binary", str,
       gem5_option = "cmd",
       gem5_option_use = "syscall-emulation",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_options", str,
       gem5_option = "options",
       gem5_option_use = "syscall-emulation",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmark_se_work_directory", str,
       gem5_option_use = "no",
       launchscript_option = "omit",
       siminfo_exclude = True)
Option("benchmark_environment", str, # Format: concatenation of "VAR=VALUE\n" for each VAR
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True)

# Benchmark source and working directories root
Option("benchmarks_root_dir", str,
       gem5_option_use = "no",
       runscript_option = "yes",
       siminfo_exclude = True)

# Benchmark disk image options
Option("benchmarks_mount_image", bool,
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmarks_disk_image", str,
       gem5_option = "disk-image",
       gem5_option_use = "full-system",
       runscript_option = "yes",
       siminfo_exclude = True)
Option("benchmarks_image_device", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")
Option("benchmarks_image_mountpoint", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       siminfo_exclude = True,
       runscript_option = "omit")

# Other
Option("network_model", str,
       gem5_option = "network")
Option("network_topology", str,
       gem5_option = "topology",
       descr_dir = "{value}")
Option("network_mesh_rows", int,
       gem5_option = "mesh-rows")
Option("memory_type", str,
       gem5_option = "mem-type")
Option("memory_size", str,
       gem5_option = "mem-size",
       gem5_option_use = "general")
Option("dramsim3_ini", str,
       gem5_option = "dramsim3-ini",
       gem5_option_use = "general")

# Cache options
Option("cache_name", str,
       gem5_option_use = "no",
       descr_dir = "{value}")
Option("cache_l0i_size", int,
       gem5_option = "l0i_size")
Option("cache_l0d_size", int,
       gem5_option = "l0d_size")
Option("cache_l0i_assoc", int,
       gem5_option = "l0i_assoc")
Option("cache_l0d_assoc", int,
       gem5_option = "l0d_assoc")
Option("cache_l1i_size", int,
       gem5_option = "l1i_size")
Option("cache_l1d_size", int,
       gem5_option = "l1d_size")
Option("cache_l1i_assoc", int,
       gem5_option = "l1i_assoc")
Option("cache_l1d_assoc", int,
       gem5_option = "l1d_assoc")
Option("cache_l2_num_caches", int,
       gem5_option = "num-l2caches")
#Option("cache_l2_size_total", int,
#       gem5_option_use = "no") # must be cache_l2_size_per_cache * cache_l2_num_caches
Option("cache_l2_size_per_cache", int,
       gem5_option = "l2_size") # must be cache_l2_size_total / cache_l2_num_caches
Option("cache_l2_assoc", int,
       gem5_option = "l2_assoc")
Option("cache_l0_tag_access_latency", int,
       gem5_option = "l0_tag_access_latency")
Option("cache_l0_data_access_latency", int,
       gem5_option = "l0_data_access_latency")
Option("cache_l1_tag_access_latency", int,
       gem5_option = "l1_tag_access_latency")
Option("cache_l1_data_access_latency", int,
       gem5_option = "l1_data_access_latency")
Option("cache_l2_tag_access_latency", int,
       gem5_option = "l2_tag_access_latency")
Option("cache_l2_data_access_latency", int,
       gem5_option = "l2_data_access_latency")
# Prefetcher on L0... there is no implementation for other caches
Option("enable_prefetch", bool,
       gem5_option = "enable-prefetch")

# HTM Options
Option("htm_disable_speculation", bool,
       descr_abbrev = "NoSpec")
Option("htm_binary_suffix", str,
       descr_abbrev = "BinSfx",
       gem5_option_use = "no",
       launchscript_option = "yes",
       runscript_option = "omit") # TODO: unnecesary, see benchmark_binary_suffix
Option("benchmark_htmrt_config", str,
       gem5_option_use = "no",
       launchscript_option = "yes",
       runscript_option = "omit")
Option("htm_lazy_vm", bool,
       descr_abbrev = "LV")
Option("htm_eager_cd", bool,
       descr_abbrev = "ED")
Option("htm_conflict_resolution", str, # requester_wins, committer_wins, requester_stalls_cda_hybrid, requester_stalls_cda_hybrid_ntx, requester_stalls_cda_base_ntx, requester_stalls_cda_base
       descr_abbrev = "CR")
Option("htm_lazy_arbitration", str, # magic or token
       descr_abbrev = "LArb")
Option("htm_allow_read_set_l0_cache_evictions", bool,
       descr_abbrev = "RSL0Ev")
Option("htm_allow_read_set_l1_cache_evictions", bool,
       descr_abbrev = "RSL1Ev")
Option("htm_allow_write_set_l0_cache_evictions", bool,
       descr_abbrev = "WSL0Ev")
Option("htm_allow_write_set_l1_cache_evictions", bool,
       descr_abbrev = "WSL1Ev")
Option("htm_allow_read_set_l2_cache_evictions", bool,
       descr_abbrev = "RSL2Ev")
Option("htm_allow_write_set_l2_cache_evictions", bool,
       descr_abbrev = "WSL2Ev")
Option("htm_precise_read_set_tracking", bool,
       descr_abbrev = "RSPrec")
Option("htm_allow_load_delaying", bool,
       descr_abbrev = "LDelay")
Option("htm_trans_aware_l0_replacements", bool,
       descr_abbrev = "L0Repl")
Option("htm_trans_aware_l1_replacements", bool,
       descr_abbrev = "L1Repl")
Option("htm_reload_if_stale", bool,
       descr_abbrev = "RldStale")
Option("htm_delay_interrupts", bool,
       descr_abbrev = "DlyInt")
Option("htm_l0_downgrade_on_l1_gets", bool,
       descr_abbrev = "DwnG")
Option("htm_value_checker", bool,
       #descr_abbrev = "ValChk",
       siminfo_exclude = True)
Option("htm_isolation_checker", bool,
       #descr_abbrev = "IsolChk",
       siminfo_exclude = True)
Option("htm_visualizer", bool,
       descr_abbrev = "Visual",
       siminfo_exclude = True)
Option("rm_ckpt_after_sim", bool,
       gem5_option_use = "no",
       runscript_option = "yes",
       siminfo_exclude = True)
Option("htm_fwd_mechanism", str, # no_fwd, fwd_naive_cycle_free, fwd_only_prio, fwd_naive_prio, fwd_adapt
       descr_abbrev="FWD")
Option("htm_allow_early_value_forwarding", bool,
       descr_abbrev="EVFwd")
Option("htm_max_consumed_blocks", int,
       descr_abbrev="maxFwd")
Option("htm_remove_false_sharing_forward", bool,
       descr_abbrev="RFS")
Option("htm_cycles_to_validate", int,
       descr_abbrev="CTV")
Option("htm_n_val_fwd", int,
       descr_abbrev="MVAL")
Option("htm_prio_mechanism", str,
       descr_abbrev="Prio_")
Option("htm_fwd_cycle_solve_strategy", str,
       descr_abbrev="FCS")
Option("htm_allowed_data_fwd", str,
       descr_abbrev="Allow_Fwd_")
Option("htm_max_consumers", int,
       descr_abbrev="MaxCons")
Option("htm_no_produce_on_retry", bool,
       descr_abbrev="NoProd")
Option("htm_max_prio_diff", int,
       descr_abbrev="MaxPrioDiff")


Option("htm_NC_not_producer", bool,
       descr_abbrev="NCNP")

# HTM library options
Option("htm_max_retries", int,
       descr_abbrev = "Rtry",
       gem5_option_use = "no",
       launchscript_option = "export:HTM_MAX_RETRIES",
       runscript_option = "omit")
Option("htm_backoff", bool,
       descr_abbrev = "BO",
       gem5_option_use = "no",
       launchscript_option = "export_formatted:d:HTM_BACKOFF",
       runscript_option = "omit")
Option("htm_heap_prefault", bool,
       descr_abbrev = "Pflt",
       gem5_option_use = "no",
       launchscript_option = "export_formatted:d:HTM_HEAP_PREFAULT",
       runscript_option = "omit")
Option("htm_fallback_lock_filename", str, # unnecesary
       gem5_option_use = "no")

# descr_abbrev pseudoption
Option("config_description_abbrev", str,
       runscript_option = "omit",
       gem5_option_use = "no",
       descr_dir = "{value}")

# Warmup options
Option("work_warmup_active", bool,
       descr_abbrev = "WWarmup")
Option("work_warmup_count", int,
       descr_abbrev = "")