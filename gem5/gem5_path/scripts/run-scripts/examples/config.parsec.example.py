#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Run with: ./gem5_gen_scripts.py --config examples/config.parsec.example.py

from gem5_run import configs_set, configs_update, configs_vary, Vary, get_benchmarks, update
from options import *
import templates

configs_set(templates.base)
configs_update(templates.cache_baseline) 

configs_update({
    build_type: "opt", # Set binary_type (the dafult comes from tasks-gem5 and may include several values using Vary)
    htm_visualizer: False,
    disable_transparent_hugepages: True,
})

configs_update({
    protocol: "MESI_Three_Level",
    arch: Vary("x86_64", "aarch64"),

    benchmark: Vary(*(   get_benchmarks(suite = "parsec", size = "test")
                       + get_benchmarks(suite = "splash2", size = "test")
                       + get_benchmarks(suite = "splash2x", size = "test")
                      )),

    random_seed: 1,
})

import os
output_dir = os.getenv("OUTPUT_DIR")
if output_dir != None and output_dir != "":
    configs_update({ output_directory_base: os.path.realpath(output_dir) })

