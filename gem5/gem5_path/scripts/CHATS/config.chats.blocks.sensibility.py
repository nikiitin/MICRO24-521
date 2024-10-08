#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates
import os

configs_set(templates.base)
configs_update(templates.cache_alder_lake_new)
configs_vary(templates.cpu_dramsim)
configs_update(templates.htm_cfg2_l0rsetevict)

configs_vary(
    update(templates.htm_cfg2_l0rsetevict, { num_cpus: Vary(16) }),
)
configs_vary(
             {cpu_model: "DerivO3CPU"},
)
kvm_enabled = os.getenv("KVM_ACTIVE", "False")
kvm_enabled = True if kvm_enabled.lower() == "true" else False
configs_vary(
    {enable_kvm: kvm_enabled},
)
configs_vary(
    {work_warmup_active: False}
)

configs_vary(
      {
        htm_binary_suffix: '.htm.fallbacklock',
        htm_conflict_resolution: "requester_loses",
        htm_max_retries: 32,
        htm_allow_early_value_forwarding: True,
        htm_fwd_mechanism: "WN_DM",
        htm_prio_mechanism: "numeric",
        htm_allowed_data_fwd: "no_write_in_flight",
        htm_fwd_cycle_solve_strategy:"abort",
        htm_max_consumed_blocks: Vary(1,2,4,8,16,32),
        htm_cycles_to_validate: Vary(50,100,200,400,800,1600)
        },
      {
        htm_binary_suffix: '.htm.powertm',
        htm_conflict_resolution: "requester_loses_power",
        htm_max_retries: 1,
        htm_allow_early_value_forwarding: True,
        htm_fwd_mechanism: "WN_DM",
        htm_prio_mechanism: "numeric",
        htm_allowed_data_fwd: "no_write_in_flight",
        htm_fwd_cycle_solve_strategy:"abort",
        htm_max_consumed_blocks: Vary(1,2,4,8,16,32),
        htm_cycles_to_validate: Vary(50,100,200,400,800,1600)
        },
)

configs_update({
    build_type: "opt", # Set binary_type (the dafult comes from tasks-gem5 and may include several values using Vary)
    htm_visualizer: False,
    htm_isolation_checker: False,
    htm_precise_read_set_tracking: True,
    htm_reload_if_stale : True,
    htm_l0_downgrade_on_l1_gets: True, # Check why it causes a slowdown in some cases (intruder-nfs)
    htm_trans_aware_l0_replacements: True,
    htm_trans_aware_l1_replacements: True,
    disable_transparent_hugepages: True,
    exit_at_roi_end: True,

    benchmark: Vary( *(get_benchmarks(suite = "stamp", size = "medium",
                                     name = [ "bayes",
                                             "genome",
                                             "kmeans-l",
                                             "kmeans-h",
                                             "intruder",
                                             "vacation-l",
                                             "vacation-h",
                                             "labyrinth",
                                             "yada",
                                             "ssca2"
                                             ])) ,
                   *(get_benchmarks(suite = "microbenches", size="medium",
                                    name = [ "cadd",
                                            "llb-l",
                                            "llb-h"
                                           ]))
                                           ),
    random_seed: Vary(*range(3)),
})

output_dir = os.getenv("OUTPUT_DIR")
if output_dir != None and output_dir != "":
    configs_update({ output_directory_base: os.path.realpath(output_dir) })

