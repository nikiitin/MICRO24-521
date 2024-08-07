#!/usr/bin/python3
# -*- coding: utf-8 -*-

from gem5_run import Benchmark

Benchmark(
        suite = "test-progs-caps",
        name = "simpletest",
        size = "small",
        args_string = "-t${num_cpus} -a1048576",
        subdir_template = "test-progs/caps/sumarray",
        binary_filename_template = "bin/x86/sumarray")

# STAMP
for (name, size, subdir, binary_base, args) in [
        ("genome",       "small",  "genome",               "genome",    "-t${num_cpus} -g256 -s16 -n16384"),
        ("intruder",     "small",  "intruder",             "intruder",  "-t${num_cpus} -a10 -l4 -n2048 -s1"),
        ("intruder-nfs", "small",  "intruder-no-fsharing", "intruder",  "-t${num_cpus} -a10 -l4 -n2048 -s1"  ),
        ("intruder-qs",  "small",  "intruder-queuesync",   "intruder",  "-t${num_cpus} -a10 -l4 -n2048 -s1"  ),
        ("intruder-rmw", "small",  "intruder-rmwonly",     "intruder",  "-t${num_cpus} -a10 -l4 -n2048 -s1"  ),
        ("kmeans-l",     "small",  "kmeans",               "kmeans",    "-p${num_cpus} -m40 -n40 -t0.05 -i inputs/random-n2048-d16-c16.txt"),
        ("kmeans-qs-l",  "small",  "kmeans-queuesync",     "kmeans",    "-p${num_cpus} -m40 -n40 -t0.05 -i inputs/random-n2048-d16-c16.txt"),
        ("kmeans-h",     "small",  "kmeans",               "kmeans",    "-p${num_cpus} -m15 -n15 -t0.05 -i inputs/random-n2048-d16-c16.txt"),
        ("kmeans-qs-h",  "small",  "kmeans-queuesync",     "kmeans",    "-p${num_cpus} -m15 -n15 -t0.05 -i inputs/random-n2048-d16-c16.txt"),
        ("ssca2",        "small",  "ssca2",                "ssca2",     "-t${num_cpus} -s13 -i1.0 -u1.0 -l3 -p3"),
        ("ssca2-tx",     "small",  "ssca2-txphase",        "ssca2",     "-t${num_cpus} -s13 -i1.0 -u1.0 -l3 -p3"),
        ("vacation-l",   "small",  "vacation",             "vacation",  "-c${num_cpus} -n2 -q90 -u98 -r16384 -t4096"),
        ("vacation-h",   "small",  "vacation",             "vacation",  "-c${num_cpus} -n4 -q60 -u90 -r16384 -t4096"),
        ("yada",         "small",  "yada",                 "yada",      "-t${num_cpus} -a20 -i inputs/633.2"),
        #("bayes",       "small",  "bayes",                "bayes",     "-t${num_cpus} -v32 -r1024 -n2 -p20 -i2 -e2"),
        #("labyrinth",   "small",  "labyrinth",            "labyrinth", "-t${num_cpus} -i inputs/random-x32-y32-z3-n96.txt"),

        ("genome",       "medium", "genome",               "genome",    "-t${num_cpus} -g512 -s32 -n32768"),
        ("intruder",     "medium", "intruder",             "intruder",  "-t${num_cpus} -a10 -l16 -n4096 -s1"),
        ("intruder-nfs", "medium", "intruder-no-fsharing", "intruder",  "-t${num_cpus} -a10 -l16 -n4096 -s1"),
        ("intruder-qs",  "medium", "intruder-queuesync",   "intruder",  "-t${num_cpus} -a10 -l16 -n4096 -s1"),
        ("intruder-rmw", "medium", "intruder-rmwonly",     "intruder",  "-t${num_cpus} -a10 -l16 -n4096 -s1"),
        ("kmeans-l",     "medium", "kmeans",               "kmeans",    "-p${num_cpus} -m40 -n40 -t0.05 -i inputs/random-n16384-d24-c16.txt"),
        ("kmeans-qs-l",  "medium", "kmeans-queuesync",     "kmeans",    "-p${num_cpus} -m40 -n40 -t0.05 -i inputs/random-n16384-d24-c16.txt"),
        ("kmeans-h",     "medium", "kmeans",               "kmeans",    "-p${num_cpus} -m15 -n15 -t0.05 -i inputs/random-n16384-d24-c16.txt"),
        ("kmeans-qs-h",  "medium", "kmeans-queuesync",     "kmeans",    "-p${num_cpus} -m15 -n15 -t0.05 -i inputs/random-n16384-d24-c16.txt"),
        ("ssca2",        "medium", "ssca2",                "ssca2",     "-t${num_cpus} -s14 -i1.0 -u1.0 -l9 -p9"),
        ("ssca2-tx",     "medium", "ssca2-txphase",        "ssca2",     "-t${num_cpus} -s14 -i1.0 -u1.0 -l9 -p9"),
        ("vacation-l",   "medium", "vacation",             "vacation",  "-c${num_cpus} -n2 -q90 -u98 -r1048576 -t4096"),
        ("vacation-h",   "medium", "vacation",             "vacation",  "-c${num_cpus} -n4 -q60 -u90 -r1048576 -t4096"),
        # NOTE: yada input 633.2 is the recommended small, but ttimeu10000 has much longer simulation times than the remaining medium inputs benchmarks...
        #("yada",         "medium", "yada",                 "yada",      "-t${num_cpus} -a20 -i inputs/633.2"),
        ("yada",        "medium", "yada",                 "yada",      "-t${num_cpus} -a10 -i inputs/ttimeu10000.2")        ,
        ("bayes",       "medium", "bayes",                "bayes",     "-t${num_cpus} -v32 -r4096 -n2 -p20 -i2 -e2"),
        ("labyrinth",   "medium", "labyrinth",            "labyrinth", "-t${num_cpus} -i inputs/random-x48-y48-z3-n64.txt"),
]:
    Benchmark(
        suite = "stamp",
        name = name,
        size = size,
        args_string = args,
        subdir_template = f"benchmarks-htm/stamp/{subdir}",
        binary_filename_template = f"{binary_base}.${{arch}}${{benchmark_binary_suffix}}")

# MICROBENCHES
for (name, size, subdir, binary_base, args) in [
        ("cadd",     "small",  "clusteradding",               "clusteradding",  "-t${num_cpus} -l4 -d64 -s1 -c128"),
        ("llb-l",     "small",  "llbenchsimple",               "llbenchsimple",  "-t${num_cpus} -l256 -e16 -n128 -s1"),
        ("llb-h",     "small",  "llbenchsimple",               "llbenchsimple",  "-t${num_cpus} -l256 -e64 -n128 -s1"),
        ("llb-l",     "medium",  "llbenchsimple",               "llbenchsimple",  "-t${num_cpus} -l512 -e16 -n256 -s1"),
        ("llb-h",     "medium",  "llbenchsimple",               "llbenchsimple",  "-t${num_cpus} -l512 -e64 -n256 -s1"),
        ("cadd",     "medium",  "clusteradding",               "clusteradding",  "-t${num_cpus} -l16 -d128 -s1 -c512"),
]:
    Benchmark(
        suite = "microbenches",
        name = name,
        size = size,
        args_string = args,
        subdir_template = f"benchmarks-htm/microbenches/{subdir}",
        binary_filename_template = f"{binary_base}.${{arch}}${{benchmark_binary_suffix}}")

# HTMBench
for (name, size, subdir, binary_base, args) in [
        ("berkely-db", "small", "berkely-db/bench", "ex_thread", "-i 256 -r ${num_cpus_half} -w ${num_cpus_half}"),
        ("berkely-db", "medium", "berkely-db/bench", "ex_thread", "-i 4096 -r ${num_cpus_half} -w ${num_cpus_half}"),
        ("avl_tree", "small", "avl_tree", "avl", "${num_cpus} 32768 0.5 0.0 2000000"),
        ("avl_tree", "medium", "avl_tree", "avl", "${num_cpus} 1000000 1 0.8 100000"),
        ("bplus-tree", "small", "bplus-tree", "test-threaded-rw", "--threads=${num_cpus} --items=64 --times=10"),
        ("bplus-tree", "medium", "bplus-tree", "test-threaded-rw", "--threads=${num_cpus} --items=100 --times=100"),
        ("dedup", "small", "parsec-2.1/pkgs/kernels/dedup", "dedup", "-t ${num_cpus} -c -p -f -r 1 -i inputs/simsmall.dat"),
        ("dedup", "medium", "parsec-2.1/pkgs/kernels/dedup", "dedup", "-t ${num_cpus} -c -p -f -r 1 -i inputs/simmedium.dat"),
        ("dedup", "large", "parsec-2.1/pkgs/kernels/dedup", "dedup", "-t ${num_cpus} -c -p -f -r 1 -i inputs/simlarge.dat"),
        ("dedup-cp", "small", "parsec-2.1/pkgs/kernels/dedup-cp", "dedup", "-t ${num_cpus} -c -p -f -r 1 -i inputs/simsmall.dat"),
        ("dedup-cp", "medium", "parsec-2.1/pkgs/kernels/dedup-cp", "dedup", "-t ${num_cpus} -c -p -f -r 1 -i inputs/simmedium.dat"),
        ("dedup-cp", "large", "parsec-2.1/pkgs/kernels/dedup-cp", "dedup", "-t ${num_cpus} -c -p -f -r 1 -i inputs/simlarge.dat"),
]:
    Benchmark(
        suite = "htmbench",
        name = name,
        size = size,
        args_string = args,
        subdir_template = f"benchmarks-htm/HTMBench/benchmark/{subdir}",
        binary_filename_template = f"build/${{arch}}/${{benchmark_htmrt_config}}/{binary_base}")

# PARSEC and splash2
for (suite, name) in [
        ("parsec", "blackscholes"),
        ("parsec", "bodytrack"),
        ("parsec", "canneal"),
        ("parsec", "dedup"),
        ("parsec", "facesim"),
        ("parsec", "ferret"),
        ("parsec", "fluidanimate"),
        ("parsec", "freqmine"),
        #("parsec", "netdedup"),   # Does not work or x86 or arm
        #("parsec", "netferret"),  # Does not work for arm, huge variability for x86
        #("parsec", "netstreamcluster"),  # Does not work for arm, huge variability for x86
        ("parsec", "raytrace"),
        ("parsec", "streamcluster"),
        ("parsec", "swaptions"),
        ("parsec", "vips"),
        ("parsec", "x264"),
        ("splash2", "barnes"),
        ("splash2", "cholesky"),
        ("splash2", "fft"),
        ("splash2", "fmm"),
        ("splash2", "lu_cb"),
        ("splash2", "lu_ncb"),
        ("splash2", "ocean_cp"),
        ("splash2", "ocean_ncp"),
        ("splash2", "radiosity"),
        ("splash2", "radix"),
        ("splash2", "raytrace"),
        ("splash2", "volrend"),
        ("splash2", "water_nsquared"),
        ("splash2", "water_spatial"),
]:
    for size in ["test", "simdev", "simsmall", "simmedium", "simlarge"]:
        Benchmark(
            suite = suite,
            name = name,
            size = size,
            args_string = f"-a run -c gcc-hooks -p {suite}.{name} -i {size} -n ${{num_cpus}}",
            subdir_template = "parsec",
            binary_filename_template = "parsecmgmt-env", 
            environment_template = "LD_LIBRARY_PATH=${benchmarks_root_dir}/parsec/pkgs/libs/hooks/inst/${parsec_arch}.gcc-hooks/lib/\n")

# Splash2x (from PARSEC ditribution)        
for (name, size, subdir, args, input) in [       
        ("barnes",         "simsmall",  "apps",    "${num_cpus}", "inputs/input.simsmall.${num_cpus}"),
        ("barnes",         "simmedium", "apps",    "${num_cpus}", "inputs/input.simmedium.${num_cpus}"),
        ("barnes",         "simlarge",  "apps",    "${num_cpus}", "inputs/input.simlarge.${num_cpus}"),
        ("cholesky",       "simsmall",  "kernels", "-p${num_cpus}", "inputs/tk29.O"),
        ("cholesky",       "simmedium", "kernels", "-p${num_cpus}", "inputs/tk29.O"),
        ("cholesky",       "simlarge",  "kernels", "-p${num_cpus}", "inputs/tk29.O"),
        ("fft",            "simsmall",  "kernels", "-m20 -p${num_cpus}", None),
        ("fft",            "simmedium", "kernels", "-m22 -p${num_cpus}", None),
        ("fft",            "simlarge",  "kernels", "-m24 -p${num_cpus}", None),
        ("fmm",            "simsmall",  "apps",    "${num_cpus}", "inputs/input.simlarge.${num_cpus}"),
        ("fmm",            "simmedium", "apps",    "${num_cpus}", "inputs/input.simlarge.${num_cpus}"),
        ("fmm",            "simlarge",  "apps",    "${num_cpus}", "inputs/input.simlarge.${num_cpus}"),
        ("lu_cb",          "simsmall",  "kernels", "-p${num_cpus} -n512 -b16", None),
        ("lu_cb",          "simmedium", "kernels", "-p${num_cpus} -n1024 -b16", None),
        ("lu_cb",          "simlarge",  "kernels", "-p${num_cpus} -n2048 -b16", None),
        ("lu_ncb",         "simsmall",  "kernels", "-p${num_cpus} -n512 -b16", None),
        ("lu_ncb",         "simmedium", "kernels", "-p${num_cpus} -n1024 -b16", None),
        ("lu_ncb",         "simlarge",  "kernels", "-p${num_cpus} -n2048 -b16", None),
        ("ocean_cp",       "simsmall",  "apps",    "-n514 -p${num_cpus} -e1e-07 -r20000 -t28800", None),
        ("ocean_cp",       "simmedium", "apps",    "-n1026 -p${num_cpus} -e1e-07 -r20000 -t28800", None),
        ("ocean_cp",       "simlarge",  "apps",    "-n2050 -p${num_cpus} -e1e-07 -r20000 -t28800", None),
        ("ocean_ncp",      "simsmall",  "apps",    "-n514 -p${num_cpus} -e1e-07 -r20000 -t28800", None),
        ("ocean_ncp",      "simmedium", "apps",    "-n1026 -p${num_cpus} -e1e-07 -r20000 -t28800", None),
        ("ocean_ncp",      "simlarge",  "apps",    "-n2050 -p${num_cpus} -e1e-07 -r20000 -t28800", None),
        ("radiosity",      "simsmall",  "apps",    "-bf 1.5e-1 -batch -room -p ${num_cpus}", None),
        ("radiosity",      "simmedium", "apps",    "-bf 1.5e-2 -batch -room -p ${num_cpus}", None),
        ("radiosity",      "simlarge",  "apps",    "-bf 1.5e-3 -batch -room -p ${num_cpus}", None),
        ("radix",          "simsmall",  "kernels", "-p${num_cpus} -r4096 -n4194304 -m2147483647", None),
        ("radix",          "simmedium", "kernels", "-p${num_cpus} -r4096 -n16777216 -m2147483647", None),
        ("radix",          "simlarge",  "kernels", "-p${num_cpus} -r4096 -n67108864 -m2147483647", None),
        ("raytrace",       "simsmall",  "apps",    "-s -p${num_cpus} -a8 inputs/teapot.env", None),
        ("raytrace",       "simmedium", "apps",    "-s -p${num_cpus} -a2 inputs/balls4.env", None),
        ("raytrace",       "simlarge",  "apps",    "-s -p${num_cpus} -a8 inputs/balls4.env", None),
        ("volrend",        "simsmall",  "apps",    "${num_cpus} inputs/head-scaleddown4 20", None),
        ("volrend",        "simmedium", "apps",    "${num_cpus} inputs/head-scaleddown2 50", None),
        ("volrend",        "simlarge",  "apps",    "${num_cpus} inputs/head-scaleddown2 100", None),
        ("water_nsquared", "simsmall",  "apps",    "${num_cpus}", "inputs/input.simsmall.${num_cpus}"),
        ("water_nsquared", "simmedium", "apps",    "${num_cpus}", "inputs/input.simmedium.${num_cpus}"),
        ("water_nsquared", "simlarge",  "apps",    "${num_cpus}", "inputs/input.simlarge.${num_cpus}"),
        ("water_spatial",  "simsmall",  "apps",    "${num_cpus}", "inputs/input.simsmall.${num_cpus}"),
        ("water_spatial",  "simmedium", "apps",    "${num_cpus}", "inputs/input.simmedium.${num_cpus}"),
        ("water_spatial",  "simlarge",  "apps",    "${num_cpus}", "inputs/input.simlarge.${num_cpus}"),
]:
    Benchmark(
        suite = "splash2x",
        name = name,
        size = size,
        args_string = args,
        subdir_template = f"parsec/ext/splash2x/{subdir}/{name}",
        binary_filename_template = f"inst/${{parsec_arch}}.gcc-hooks/bin/{name}", 
        input_filename_template = input,
        environment_template = "LD_LIBRARY_PATH=${benchmarks_root_dir}/parsec/pkgs/libs/hooks/inst/${parsec_arch}.gcc-hooks/lib/\n")

# Splash-3
for (name, subdir, exe_name, args, input) in [       
    ("barnes",         "apps/barnes",                          "BARNES",         "", "inputs/n16384-p${num_cpus}"),
    ("cholesky",       "kernels/cholesky",                     "CHOLESKY",       "-p${num_cpus}", "inputs/tk15.O"),
    ("fft",            "kernels/fft",                          "FFT",            "-p${num_cpus} -m16", None),
    ("fmm",            "apps/fmm",                             "FMM",            "", "inputs/input.${num_cpus}.16384"),
    ("lu_cb",          "kernels/lu/contiguous_blocks",         "LU",             "-p${num_cpus} -n512", None),
    ("lu_ncb",         "kernels/lu/non_contiguous_blocks",     "LU",             "-p${num_cpus} -n512", None),
    ("ocean_cp",       "apps/ocean/contiguous_partitions",     "OCEAN",          "-p${num_cpus} -n258", None),
    ("ocean_ncp",      "apps/ocean/non_contiguous_partitions", "OCEAN",          "-p${num_cpus} -n258", None),
    ("radiosity",      "apps/radiosity",                       "RADIOSITY",      "-p ${num_cpus} -ae 5000 -bf 0.1 -en 0.05 -room -batch", None),
    ("radix",          "kernels/radix",                        "RADIX",          "-p${num_cpus} -n1048576", None),
    ("raytrace",       "apps/raytrace",                        "RAYTRACE",       "-p${num_cpus} -m64 inputs/car.env", None),
    ("volrend",        "apps/volrend",                         "VOLREND",        "${num_cpus} inputs/head 8", None),
    ("water_nsquared", "apps/water-nsquared",                  "WATER-NSQUARED", "${num_cpus}", "inputs/n512-p${num_cpus}"),
    ("water_spatial",  "apps/water-spatial",                   "WATER-SPATIAL",  "${num_cpus}", "inputs/n512-p${num_cpus}"),
]:
    Benchmark(
        suite = "splash3",
        name = name,
        size = "recommended",
        args_string = args,
        subdir_template = f"Splash-3/codes/{subdir}",
        binary_filename_template = f"build/${{arch}}/{exe_name}", 
        input_filename_template = input)
