#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import subprocess
import shutil
from gem5_run import gem5_root, known_options, error
import options as opt

config_preferred_order_first = [ opt.arch, opt.cpu_model, opt.protocol, opt.config_description_abbrev, opt.cache_name, opt.num_cpus, opt.benchmark_full_name, opt.benchmark_size ]
config_preferred_order_last = [opt.random_seed ]

# config options of conf, in a fixed order
def config_list_options(conf):
    first = [k for k in config_preferred_order_first if k in conf]
    rest = [k for k in known_options if k in conf and not k in config_preferred_order_first and not k in config_preferred_order_last]
    last = [k for k in config_preferred_order_last if k in conf]
    return first + rest + last

config_describe_ignored_options=set([])

def config_describe_set_ignored_options(l):
    global config_describe_ignored_options
    config_describe_ignored_options=set(l)

def config_describe(conf):
    dirs = [d for d in [c.descr_dir_text_value(conf) for c in config_list_options(conf) if not c in config_describe_ignored_options] if d != ""]
    return os.path.join("/".join(dirs))

def config_describe_abbrev(conf):
    abbrevs = [a for a in [c.descr_abbrev_text_value(conf) for c in config_list_options(conf)] if a != ""]
    return "_".join(abbrevs)

def print_config(conf):
    print("{")
    for c in config_list_options(conf):
        print(f"  {c.name}: {c(conf, allow_vary = True)}")
    print("}")

def constant_options(configs):
    if not configs:
        return known_options
    else:
        missing = object()
        first_c = configs[0]
        def constant_opt(o):
            def get(opt, conf):
                if opt in conf:
                    return opt(conf)
                else:
                    return missing
            first_v = get(o, first_c)
            return all(get(o, c) == first_v for c in configs)
        return [o for o in known_options if constant_opt(o)]
    
option_value_abbreviations = {
    "requester_wins": "rw",
    "requester_wins_power": "rwp",
    "requester_loses": "rl",
    "requester_loses_power": "rlp",
    "magic": "mg",
    "token": "tkn",
    "committer_wins": "cw",
    "requester_stalls_cda_base": "cdab",
    "requester_stalls_cda_base_ntx": "cdabntx",
    "requester_stalls_cda_hybrid": "cdah",
    "requester_stalls_cda_hybrid_ntx": "cdahntx",
}

def abbrev_option_value(s):
    if s in option_value_abbreviations:
        return option_value_abbreviations[s]
    else:
        return s

cache_config_from_tasks_gem5 = {}
def config_from_tasks_gem5(s):
    if not s in cache_config_from_tasks_gem5:
        gem5_tasks = os.path.join(gem5_root, "gem5_path", "scripts", "tasks-gem5", "tasks-gem5")
        # WORKAROUND: Sometimes the either the command fails or subprocess.check_output raises an exception incorrectly. It seems to work after a retry.
        retries = 0
        while True:
            try:
                cache_config_from_tasks_gem5[s] = subprocess.check_output([gem5_tasks, "query-config", s],
                                                                          stderr = subprocess.DEVNULL,
                                                                          encoding = "UTF-8")
                break
            except subprocess.CalledProcessError:
                retries = retries + 1
                if retries > 10:
                    error(f"Could not execute '{gem5_tasks}' query-config '{s}'.")
                else:
                    print(f"RETRYING query-config {retries}")
    return cache_config_from_tasks_gem5[s]


cache_git_revisions = {}
def get_git_revision(conf):
    global cache_git_revisions
    r = gem5_root # TODO: should use conf
    if not r in cache_git_revisions:
        cache_git_revisions[r] = subprocess.check_output(['git', 'describe', '--dirty', '--always', '--tags'],
                                                         cwd = r, 
                                                         stderr = subprocess.DEVNULL,
                                                         encoding = "UTF-8").strip()
    return cache_git_revisions[r]

def file_hash(f):
    return subprocess.check_output(['sha1sum', f],
                                   stderr = subprocess.DEVNULL,
                                   encoding = "UTF-8").strip().split(' ')[0]

snapshot_binary_cache = {}
def snapshot_binary(binary, snapshot_directory):
    global snapshot_binary_cache
    if not binary in snapshot_binary_cache:
        if not os.path.exists(binary):
            error(f"File not found '{binary}'.")
        snap = os.path.join(snapshot_directory, file_hash(binary))
        if not os.path.exists(snap): # don't overwrite if already copied by a previous execution of the script
            if not os.path.exists(os.path.dirname(snap)):
                os.makedirs(os.path.dirname(snap))
            shutil.copy2(binary, snap)
        snapshot_binary_cache[binary] = snap
    return snapshot_binary_cache[binary]
