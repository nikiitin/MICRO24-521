#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from gem5_run import gem5_root, get_configs, error, known_options, mix_configs, Derived, Vary, update
from gem5_config_utils import config_list_options, print_config, snapshot_binary, constant_options, config_describe_set_ignored_options
import options

import os
import subprocess   
import argparse
import importlib.util

def create_directory(d):
    if not os.path.exists(d): 
        os.makedirs(d)
        print(f"Created ‘{d}’")
    else:
        print(f"‘{d}’ already exists")

def check_duplicate_outputs(configs):
    m = {}
    for conf in configs:
        od = options.output_directory(conf)
        m.setdefault(od, [])
        m[od].append(conf)
    for i in m:
        if len(m[i]) != 1:
            print_config(mix_configs(m[i]))
            error(f"Duplicate (${len(m[i])} times) output_directory: {options.output_directory(m[i][0])}")

def snapshot_binaries_config(conf):
    return update(conf, {
        options.gem5_exec_path_original: options.gem5_exec_path(conf),
        options.gem5_exec_path: Derived(lambda c: snapshot_binary(options.gem5_exec_path_original(c),
                                                                  options.gem5_exec_snapshots_dir(c))),
    })

def gen_scripts(c):
    # TODO: copy binary with timestamp

    output_directory = options.output_directory(c)
    create_directory(output_directory)
    
    with open(options.launchscript_template_filename(c), "r") as launchscript_template_file:
        with open(os.path.join(output_directory, options.launchscript_filename(c)), "w") as launchscript_file:
            template_text = launchscript_template_file.read()
            variables_text = "".join([o.launchscript_text_value(c) for o in config_list_options(c)])
            launchscript_file.write(template_text.replace("{{{variables}}}", variables_text))
       
    with open(os.path.join(output_directory, options.siminfo_filename(c)), "w") as siminfo_file:
        siminfo_file.write("[SimulationInfo]\n")
        for o in config_list_options(c):
            siminfo_file.write(o.siminfo_text_value(c))

    runscript_filename = os.path.join(output_directory, options.runscript_filename(c))
    with open(options.runscript_template_filename(c), "r") as runscript_template_file:
        with open(runscript_filename, "w") as runscript_file:
            template_text = runscript_template_file.read()
            variables_text = "".join([o.runscript_text_value(c) for o in config_list_options(c)]) + \
                "GEM5_OPTIONS_DETAILED=(\n" + \
                "\n".join([f"    {otv}" for otv in [o.gem5_option_text_value(c, "detailed") for o in config_list_options(c)] if otv != ""]) + \
                ")\n" + \
                "GEM5_OPTIONS_GENERAL=(\n" + \
                "\n".join([f"    {otv}" for otv in [o.gem5_option_text_value(c, "general") for o in config_list_options(c)] if otv != ""]) + \
                ")\n" + \
                "GEM5_OPTIONS_FULL_SYSTEM=(\n" + \
                "\n".join([f"    {otv}" for otv in [o.gem5_option_text_value(c, "full-system") for o in config_list_options(c)] if otv != ""]) + \
                ")\n" + \
                "GEM5_OPTIONS_SYSCALL_EMULATION=(\n" + \
                "\n".join([f"    {otv}" for otv in [o.gem5_option_text_value(c, "syscall-emulation") for o in config_list_options(c)] if otv != ""]) + \
                ")\n"
            runscript_file.write(template_text.replace("{{{variables}}}", variables_text))        
    os.chmod(runscript_filename, 0o755)

    enqueue_script_filename = os.path.join(output_directory, "enqueue")
    with open(enqueue_script_filename, "w") as enqueue_script_file:
        enqueue_script_file.write(f"#!/bin/bash\n")
        # TODO
        # --exclude nodes
        enqueue_script_file.write(
            f"exec sbatch \\\n" +
            f"    --parsable \\\n" +
            f"    -J '{options.config_description(c)}' \\\n" +
            f"    -e '{os.path.join(output_directory, 'stderr')}' \\\n" +
            f"    -o '{os.path.join(output_directory, 'stdout')}' \\\n" +
            f"    " + args.enqueue_options + " \\\n" +
            f"    '{os.path.join(output_directory, options.runscript_filename(c))}' \\\n" + 
            f"  | cut -d';' -f1 | tee '{os.path.join(output_directory, 'job_id')}'\n")
    os.chmod(enqueue_script_filename, 0o755)

def enqueue(c):
    if options.htm_visualizer(c):
        print(f"WARNING: htm_visualizer enabled while enqueueing.")
    cmd = [os.path.join(options.output_directory(c), "enqueue")]
    job_id = subprocess.check_output(cmd, encoding = "UTF-8").strip()
    return job_id

def parse_args(argsp = argparse.ArgumentParser()):
    argsp.add_argument("--enqueue", action="store_true", help="Submit scripts to SLURM")
    argsp.add_argument("--enqueue-options", type=str, default="", help="Extra options for sbatch")
    argsp.add_argument("--no-snapshot-binaries", action="store_true", help="Create snapshots of gem5_exec_path binaries")
    argsp.add_argument("--no-simplify-directories", action="store_true", help="Omit constant options from the description directories")
    argsp.add_argument("--list", action="store_true", help="List configs instead of generating scripts")
    argsp.add_argument("--list-mixed", action="store_true", help="List all configs mixed in one using Vary values, instead of generating scripts")
    argsp.add_argument("--config-file", type=str, default=os.path.join(gem5_root, "gem5_path/scripts/run-scripts/config.py"), help="Config file")
    return argsp.parse_args()

def load_config_file(config_file):
    if config_file[-3:] != ".py":
        error(f"Invalid config file name '{config_file}'. Must end in '.py'")
    if not os.path.exists(args.config_file):
        error(f"Config file '{args.config_file}' not found.\nYou may want to create it using '{os.path.join(gem5_root, 'gem5_path/scripts/run-scripts/config.py.example')}' as a starting point. ")
    spec = importlib.util.spec_from_file_location("config", config_file)
    if spec == None:
        error(f"Invalid config file '{config_file}'.")
    config_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(config_module)

    
args = parse_args()

load_config_file(args.config_file)

configs = get_configs()  

if args.list:
    for c in configs:
        print_config(c)
    print(f"{len(configs)} configurations.")

if args.list_mixed:
    print_config(mix_configs(configs))
    print(f"{len(configs)} configurations.")

if not (args.list or args.list_mixed):
    check_duplicate_outputs(configs)

    if not args.no_snapshot_binaries:
        configs = [snapshot_binaries_config(c) for c in configs]

    for path in set([options.checkpoint_init_reuse_root_dir(c) for c in configs if options.checkpoint_init_reuse(c)]):
        if not os.path.exists(path):
            print("Warning: checkpoint_init_reuse is enabled but checkpoint_init_reuse_root_dir does not exist (%s)" % path)

    if not args.no_simplify_directories:
        config_describe_set_ignored_options(constant_options(configs))

    for c in configs:
        gen_scripts(c)

    if args.enqueue:
        job_ids = []
        for c in configs:
            job_ids.append(enqueue(c))
        print("Jod ids: ", end = '')
        print(*job_ids)

