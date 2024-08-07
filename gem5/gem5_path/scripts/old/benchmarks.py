#!/usr/bin/python
####################################################################
##  Benchmark Selection
####################################################################

# Benchmark suites and their directory in the disk image
benchmark_suites = {}
benchmark_suites['test-progs-caps'] = "test-progs/caps"
benchmark_suites['stamp'] = "benchmarks-htm/stamp"
benchmark_suites["splash3"] = "benchmarks-htm/Splash-3"

def getBenchmarks(benchmark_groups):
    benchmark_list = []

    if 'test-progs-caps-small' in benchmark_groups:
        #                       suite,             bench-name,   inputname,  nthreads opt, args_string,     benchmark_subdir,  binary_filename
        benchmark_list.append(("test-progs-caps", "simpletest",    "small",      "-t",       "-a1048576",     "sumarray",       "bin/x86/sumarray"))

    if 'stamp-small' in benchmark_groups:
        benchmark_list.append(("stamp", "vacation-h","small", "-c", "-n4 -q60 -u90 -r16384 -t4096",   "vacation", "vacation"))
        benchmark_list.append(("stamp", "kmeans-h",  "small", "-p", "-m15 -n15 -t0.05 -i inputs/random-n2048-d16-c16.txt", "kmeans", "kmeans"))
        benchmark_list.append(("stamp", "intruder",  "small", "-t", " -a10 -l4 -n2048 -s1"  ,         "intruder", "intruder"))
        benchmark_list.append(("stamp", "yada",      "small", "-t", "-a20 -i inputs/633.2",           "yada",     "yada"))
        benchmark_list.append(("stamp", "ssca2",     "small", "-t", "-s13 -i1.0 -u1.0 -l3 -p3",       "ssca2",    "ssca2"))
        '''
        benchmark_list.append(("stamp", "genome",    "small", "-t", "-g256 -s16 -n16384",             "genome",   "genome"))
        benchmark_list.append(("stamp", "intruder",  "small", "-t", " -a10 -l4 -n2048 -s1"  ,         "intruder", "intruder"))
        benchmark_list.append(("stamp", "kmeans-l",  "small", "-p", "-m40 -n40 -t0.05 -i inputs/random-n2048-d16-c16.txt", "kmeans",  "kmeans"))
        benchmark_list.append(("stamp", "kmeans-h",  "small", "-p", "-m15 -n15 -t0.05 -i inputs/random-n2048-d16-c16.txt", "kmeans", "kmeans"))
        benchmark_list.append(("stamp", "ssca2",     "small", "-t", "-s13 -i1.0 -u1.0 -l3 -p3",       "ssca2",    "ssca2"))
        benchmark_list.append(("stamp", "vacation-l","small", "-c", "-n2 -q90 -u98 -r16384 -t4096",   "vacation", "vacation"))
        benchmark_list.append(("stamp", "vacation-h","small", "-c", "-n4 -q60 -u90 -r16384 -t4096",   "vacation", "vacation"))
        benchmark_list.append(("stamp", "yada",      "small", "-t", "-a20 -i inputs/633.2",           "yada",     "yada"))
        #benchmark_list.append(("stamp", "bayes",     "small", "-t", "-v32 -r1024 -n2 -p20 -i2 -e2", "bayes",   "bayes"))
        #benchmark_list.append(("stamp", "labyrinth", "small", "-t", "-i inputs/random-x32-y32-z3-n96.txt", "labyrinth", "labyrinth"))
        '''

    if 'stamp-medium' in benchmark_groups:
        benchmark_list.append(("stamp", "genome",    "medium", "-t", "-g512 -s32 -n32768",             "genome",   "genome"))
        benchmark_list.append(("stamp", "intruder",  "medium", "-t", " -a10 -l16 -n4096 -s1",          "intruder", "intruder"))
        #benchmark_list.append(("stamp", "kmeans-l",  "medium", "-p", "-m40 -n40 -t0.05 -i inputs/random-n16384-d24-c16.txt", "kmeans",  "kmeans"))
        benchmark_list.append(("stamp", "kmeans-h",  "medium", "-p", "-m15 -n15 -t0.05 -i inputs/random-n16384-d24-c16.txt", "kmeans", "kmeans"))
        benchmark_list.append(("stamp", "ssca2",     "medium", "-t", "-s14 -i1.0 -u1.0 -l9 -p9",       "ssca2",    "ssca2"))
        #benchmark_list.append(("stamp", "vacation-l","medium", "-c", "-n2 -q90 -u98 -r1048576 -t4096", "vacation", "vacation"))
        benchmark_list.append(("stamp", "vacation-h","medium", "-c", "-n4 -q60 -u90 -r1048576 -t4096", "vacation", "vacation"))
        # NOTE: yada input 633.2 is the recommended small, but ttimeu10000 has much longer simulation times than the remaining medium inputs benchmarks...
        benchmark_list.append(("stamp", "yada",      "medium", "-t", "-a20 -i inputs/633.2",           "yada",     "yada"))
        #benchmark_list.append(("stamp", "yada",      "medium", "-t", "-a10 -i inputs/ttimeu10000.2",   "yada",     "yada"))
        #benchmark_list.append(("stamp", "bayes",     "medium", "-t", "-v32 -r4096 -n2 -p20 -i2 -e2",   "bayes",   "bayes"))
        #benchmark_list.append(("stamp", "labyrinth", "medium", "-t", "-i inputs/random-x48-y48-z3-n64.txt", "labyrinth", "labyrinth"))

    if 'splash3-small' in benchmark_groups:
        benchmark_list.append(("splash3", "barnes", "small", "< inputs/n16384-p", "",             "barnes",   "BARNES"))
        benchmark_list.append(("splash3", "fmm",    "small", "< inputs/input.16384.", "",           "fmm",   "FMM")) # WARNING: Input filename has been modified to work: 'for i in {1,2,4,8,16,32,64}; do mv input.${i}.16384 input.16384.${i}; done'
        benchmark_list.append(("splash3", "ocean-c",   "small", "-p", "-n258",             "ocean-contiguous_partitions",   "OCEAN-CONT"))
        benchmark_list.append(("splash3", "ocean-nc",  "small", "-p", "-n258",         "ocean-non_contiguous_partitions",   "OCEAN-NOCONT"))
        benchmark_list.append(("splash3", "radiosity", "small", "-p ", " -ae 5000 -bf 0.1 -en 0.05 -room -batch", "radiosity", "RADIOSITY"))
        benchmark_list.append(("splash3", "raytrace",  "small", "-p", "-m64 inputs/car.env",         "raytrace", "RAYTRACE"))
        benchmark_list.append(("splash3", "volrend",   "small", "", "inputs/head 8",                 "volrend",  "VOLREND"))
        benchmark_list.append(("splash3", "water-ns",  "small", "< inputs/n512-p", "",               "water-nsquared",   "WATER-NSQUARED"))
        benchmark_list.append(("splash3", "water-sp",  "small", "< inputs/n512-p", "",               "water-spatial",    "WATER-SPATIAL"))
        benchmark_list.append(("splash3", "cholesky",  "small", "-p", "inputs/tk15.O",               "cholesky", "CHOLESKY"))
        benchmark_list.append(("splash3", "fft",       "small", "-p", "-m16",                        "fft",   "FFT"))
        benchmark_list.append(("splash3", "radix",     "small", "-p", "-n1048576",                   "radix", "RADIX"))
        benchmark_list.append(("splash3", "lu-c",      "small", "-p", "-n512",                       "lu-contiguous_blocks",     "LU-CONT"))
        benchmark_list.append(("splash3", "lu-nc",     "small", "-p", "-n512",                       "lu-non_contiguous_blocks", "LU-NOCONT"))

    return benchmark_list
