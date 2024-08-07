#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates

configs_set(templates.base)
configs_update(templates.model_final)
configs_update(templates.htm_cfg2_l0rsetevict)

configs_vary(
    update(templates.htm_cfg2_l0rsetevict, { num_cpus: Vary(16) }),
)
configs_vary(
             {cpu_model: "DerivO3CPU"},
)
configs_vary(
    {work_warmup_active: False}
)

configs_vary(
### Baseline
    { htm_binary_suffix: '.htm.fallbacklock', htm_conflict_resolution: "requester_wins",  htm_max_retries: Vary(1,2,4,6,8,16,32,48,64), htm_allow_early_value_forwarding: False},
### Naive
    { htm_binary_suffix: '.htm.fallbacklock', htm_conflict_resolution: "requester_loses",  htm_max_retries: Vary(1,2,4,6,8,16,32,48,64), htm_allow_early_value_forwarding: True, htm_fwd_mechanism: "naive", htm_max_consumed_blocks: 8, htm_n_val_fwd:16, htm_cycles_to_validate: 100},
### Powertm
       {
         htm_binary_suffix: '.htm.powertm',
         htm_conflict_resolution: "requester_wins_power",
         htm_max_retries: Vary(1,2,4,6,8,16,32,48,64),
         htm_allow_early_value_forwarding: False,
       },
### CHATS
       {
        htm_binary_suffix: '.htm.fallbacklock',
        htm_conflict_resolution: "requester_loses",
        htm_max_retries: Vary(1,2,4,6,8,16,32,48,64),
        htm_allow_early_value_forwarding: True,
        htm_fwd_mechanism: "WN_DM",
        htm_prio_mechanism: "numeric",
        htm_allowed_data_fwd: "only_written",
        htm_fwd_cycle_solve_strategy:"abort",
        htm_max_consumed_blocks: 256,
        htm_cycles_to_validate: 50
        },
### PCHATS
      {
        htm_binary_suffix: '.htm.powertm',
        htm_conflict_resolution: "requester_loses_power",
        htm_max_retries: Vary(1,2,4,6,8,16,32,48,64),
        htm_allow_early_value_forwarding: True,
        htm_fwd_mechanism: "WN_DM",
        htm_prio_mechanism: "numeric",
        htm_allowed_data_fwd: "only_written",
        htm_fwd_cycle_solve_strategy:"abort",
        htm_max_consumed_blocks: 256,
        htm_cycles_to_validate: 50
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
    rm_ckpt_after_sim: True,

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

import os
output_dir = os.getenv("OUTPUT_DIR")
if output_dir != None and output_dir != "":
    configs_update({ output_directory_base: os.path.realpath(output_dir) })

