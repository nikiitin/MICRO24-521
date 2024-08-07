#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import time
from shlex import quote

gem5_root = os.path.realpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

# Python dictionary of the scope to which Option objects will be added
# as variables automatically
options_module = None
def set_options_module(m):
    global options_module
    options_module = m

# List of all Option objects in the order that they where defined
known_options = []

def get_option_by_name(name):
    return options_module[name]

# A configuration is a dict of Option objects to values
class Option:
    def __init__(self, name, tipe,
                 gem5_option = "%same_s/_/-/g",
                 gem5_option_use = "detailed", # one of "no", "general", "full-system", "syscall-emulation", "detailed", to use the option in GEM5_OPTIONS_GENERAL, GEM5_OPTIONS_FULL_SYSTEM, GEM5_OPTIONS_SYSCALL_EMULATION or GEM5_OPTIONS_DETAILED
                 launchscript_option = "omit", # one of "omit", "export:VARNAME", "yes[:VARNAME]"
                 runscript_option = "yes_if_no_gem5_option", # one of "omit", "export:VARNAME", "export_formatted:FORMAT:VARNAME", "yes[:VARNAME]", "yes_if_no_gem5_option"
                 siminfo_exclude = False,
                 descr_dir = None, # str, {name} and {value} replaced
                 descr_abbrev = None # str 
                 ):
        self.name = name
        self.tipe = tipe
        
        if gem5_option == "%same":
            self.gem5_option = name
        elif gem5_option == "%same_s/_/-/g":
            self.gem5_option = name.replace("_","-")
        else:
            self.gem5_option = gem5_option

        if gem5_option_use in ["no", "detailed", "general", "full-system", "syscall-emulation"]:
            self.gem5_option_use = gem5_option_use
        else:
            assert False, "Invalid value for gem5_option_use: " + gem5_option_use
            
        if launchscript_option == "omit":
            self.launchscript_option = "omit"
        elif launchscript_option.startswith("export:"):
            self.launchscript_option = launchscript_option
        elif launchscript_option.startswith("export_formatted:"):
            self.launchscript_option = launchscript_option
        elif launchscript_option == "yes" or launchscript_option.startswith("yes:"):
            self.launchscript_option = launchscript_option
        else:
            assert False, "Invalid value for launchscript_option"

        if runscript_option == "omit":
            self.runscript_option = "omit"
        elif runscript_option.startswith("export:"):
            self.runscript_option = runscript_option
        elif runscript_option == "yes_if_no_gem5_option":
            if gem5_option_use == "no" or gem5_option == None:
                self.runscript_option = "yes"
            else:
                self.runscript_option = "omit"
        elif runscript_option == "yes" or runscript_option.startswith("yes:"):
            self.runscript_option = runscript_option
        else:
            assert False, "Invalid value for runscript_option"
        
        self.siminfo_exclude = siminfo_exclude
        self.descr_abbrev = descr_abbrev
        self.descr_dir = descr_dir

        if options_module != None:
            options_module[self.name] = self

        known_options.append(self)
            
    def __repr__(self):
        return self.name

    # gets the value of this option from a config (a dict), taking care of Derived values
    def __call__(self, conf, allow_vary = False): # Vary values should be read only by expand_variations
        v = conf[self]
        if isinstance(v, Derived):
            v = v.deriving_function(conf)
        if isinstance(v, Vary):
            if allow_vary:
                return v
            else:
                raise VaryFound(self)
        elif v == None or isinstance(v, self.tipe):
            return v
        else:
            error(f"Incorrect type {type(v)} instead of {self.tipe} for {self.name}: {v}")
            
    def siminfo_text_value(self, conf):
        if self.siminfo_exclude:
            return ""
        else:
            return f"{self.name}={self(conf)}\n"

    def launchscript_text_value(self, conf):
        if self.launchscript_option == "omit" or self(conf) == None:
            return ""
        elif self.launchscript_option.startswith("export:"):
            name = self.launchscript_option[7:]
            return f"export {name}={quote(str(self(conf)))}\n"
        elif self.launchscript_option.startswith("export_formatted:"):
            format_end = self.launchscript_option.index(":", 17)
            format = self.launchscript_option[17:format_end]
            v_formatted = ("{:" + format + "}").format(self(conf))
            name = self.launchscript_option[(format_end + 1):]
            return f"export {name}={quote(v_formatted)}\n"
        elif self.launchscript_option == "yes" or self.launchscript_option.startswith("yes:"):
            if self.launchscript_option == "yes":
                name = self.name
            else:
                name = self.launchscript_option[4:]
            return f"{name}={quote(str(self(conf)))}\n"
        else:
            assert False, "Invalid value for launchscript_option"
    
    def runscript_text_value(self, conf):
        if self.runscript_option == "omit":
            return ""
        elif self.runscript_option.startswith("export:"):
            name = self.runscript_option[7:]
            return f"export {name}={quote(str(self(conf)))}\n"
        elif self.runscript_option == "yes" or self.runscript_option.startswith("yes:"):
            if self.runscript_option == "yes":
                name = self.name
            else:
                name = self.runscript_option[4:]
            return f"{name}={quote(str(self(conf)))}\n"
        else:
            assert False, "Invalid value for runscript_option"

    def gem5_option_text_value(self, conf, gem5_option_use):
        if self.gem5_option == None or self(conf) == None \
           or gem5_option_use != self.gem5_option_use:
            return ""
        else:
            if self.tipe == bool:
                if self(conf):
                    return f"--{self.gem5_option}"
                else:
                    return ""
            else:
                return f"--{self.gem5_option}={quote(str(self(conf)))}"

    def descr_dir_text_value(self, conf):
        if self.descr_dir == None or self(conf) == None:
            return ""
        else:
            if self.tipe == bool:
                if self(conf):
                    return self.descr_dir.replace("{name}", self.name).replace("{value}", str(self(conf)))
                else:
                    return ""
            else:
                return self.descr_dir.replace("{name}", self.name).replace("{value}", str(self(conf)))

    def descr_abbrev_text_value(self, conf):
        if self.descr_abbrev == None or self(conf) == None:
            return ""
        else:
            if self.tipe == bool:
                if self(conf):
                    return self.descr_abbrev
                else:
                    return ""
            else:
                from gem5_config_utils import abbrev_option_value
                return f"{self.descr_abbrev}{abbrev_option_value(self(conf))}"
            
# For values which depend on other values from the configuration
class Derived:
    def __init__(self, deriving_function): # deriving_function receives a configuration (dict of Option to value)
        self.deriving_function = deriving_function

# For expresing alternative configurations
class Vary:
    def __init__(self, *args):
        self.values = args

    def __repr__(self):
        return "Vary(" + ",".join([str(v) for v in self.values]) + ")"

class VaryFound(Exception):
    def __init__(self, opt):
        self.option = opt # for debug

# Replaces references to options formatted as ${XXX} in a string by their value
def replace_template(string, config):
    i = 0
    ret = ""
    startv = string.find('${', i)
    while startv >= 0:
        ret = ret + string[i:startv]
        endv = string.find('}', startv + 2)
        var = string[(startv + 2):endv]
        ret = ret + str(get_option_by_name(var)(config))
        i = endv + 1
        startv = string.find('${', i)
    return ret + string[i:]

# Benchmarks
known_benchmarks = []

class Benchmark:
    def __init__(self, name, suite, size, args_string, subdir_template, binary_filename_template, input_filename_template = None, environment_template = ""):
        self.name = name
        self.suite = suite
        self.size = size
        self.args_string = args_string
        self.subdir_template = subdir_template
        self.binary_filename_template = binary_filename_template
        self.input_filename_template = input_filename_template
        self.environment_template = environment_template

        known_benchmarks.append(self)
        
    def __repr__(self):
        return f"Benchmark({self.name},{self.suite},{self.size})"
    
import benchmarks

def get_benchmarks(name = None, suite = None, size = None):
    ret = [b for b in known_benchmarks
            if (name == None or b.name == name or (isinstance(name, list) and b.name in name)) and
               (suite == None or b.suite == suite or (isinstance(suite, list) and b.suite in suite)) and
               (size == None or b.size == size or (isinstance(size, list) and b.size in size))]
    if len(ret) == 0:
        error(f"No benchmarks found such that name = {name}, suite = {suite}, size = {size}")
    return ret

# Applies conf2 over list_or_conf1 or over the elements of
# list_or_conf1 if it is a list.  Returns a config (dict) if
# list_or_conf1 is a config (dict), or returns a list of configs if
# list_or_conf1 is a list of configs
def update(list_or_conf1, conf2: dict):
    if isinstance(list_or_conf1, list):
        return [update(c, conf2) for c in list_or_conf1]
    else:
        r = list_or_conf1.copy()
        r.update(conf2)
        return r

# If list_or_conf1 is a config, returns a list containing one element
# for each config in confs, resulting of merging list_or_conf1 and the
# element. If list_or_conf1 is a list, applies the variations to each
# element and returns a list with all the combinations.
def vary(list_or_conf1, conf_variations: list):
    if isinstance(list_or_conf1, list):
        return [update(c, v) for c in list_or_conf1 for v in conf_variations]
    else:
        return [update(list_or_conf1, v) for v in conf_variations]

# Process Vary values
def expand_variations(list_or_conf1):
    if isinstance(list_or_conf1, list):
        return [c for conf in list_or_conf1 for c in expand_variations(conf)]
    else:
        for c in list_or_conf1:
            try:
                v = c(list_or_conf1, allow_vary = True)
                if isinstance(v, Vary):
                    r = expand_variations(vary(list_or_conf1, [{c: x} for x in v.values]))
                    return r
            except VaryFound as e:
                # This c is a Derived and depends on another
                # Vary. Skip it now.  It will be expanded later if
                # necessary (i.e, if it evals to a Vary) after the
                # other Vary is expanded.
                pass 
        return [list_or_conf1]

# global list of configs
configs = []

def get_configs():
    return expand_variations(configs)

def configs_set(*confs):
    configs.clear()
    configs_add(*confs)

def configs_add(*confs):
    configs.extend(confs)
    
def configs_update(conf2: dict):
    global configs
    configs = update(configs, conf2)

def configs_vary(*conf_variations: list):
    global configs
    configs = vary(configs, conf_variations)

# default output_subdirectory can be specified by command line or guessed automatically
default_output_subdirectory = None
def set_default_output_subdirectory(d):
    global default_output_subdirectory
    default_output_subdirectory = d

def get_default_output_subdirectory(root_dir):
    if default_output_subdirectory == None: # By default, automatically choose a nonexisting subdirectory name using the current date and a sequence number
        t = time.strftime("%Y-%m-%d")
        s = 0
        while os.path.exists(os.path.join(root_dir, f"{t}-{s:03d}")):
            s = s + 1
        set_default_output_subdirectory(f"{t}-{s:03d}")
        
    return default_output_subdirectory

# For error reporting when a duplicate output_directory is found
def mix_configs(configs):
    ret = {}
    for o in known_options:
        vals = []
        for c in configs:
            if o in c:
                v = o(c)
            else:
                v = "missing"
                
            if not v in vals:
                vals.append(v)
        if len(vals) == 1:
            if vals[0] != "missing":
                ret[o] = vals[0]
        else:
            ret[o] = Vary(*vals)
    return ret

def error(msg):
    print(msg)
    import sys
    sys.exit(1)
