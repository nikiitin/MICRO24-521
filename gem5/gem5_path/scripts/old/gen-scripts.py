#!/usr/bin/python3
import string, datetime, os, sys, time, config, pdb, getopt, socket, tempfile, subprocess, math, shutil, collections
import caches, benchmarks, htm
#from config import *

g_default_search_path = os.environ["PATH"]

def set_default_search_path(path):
  global g_default_search_path
  assert(path != "")
  g_default_search_path = path
  return

# returns a list of lines in the file that matches the pattern
def grep(filename, pattern):
    result = [];
    file = open(filename,'r')
    for line in file.readlines():
        if re.match(pattern, line):
            result.append(string.strip(line))
    return result

def usage():
  print("Usage: ")

def parseOptions():
  queue=False
  seeds=False
  config_file="config.py"
  try:
    opts, args = getopt.getopt(sys.argv[1:], "hlqsc:", ["help", "queue", "seeds", "config"])
  except getopt.GetoptError as err:
    # print help information and exit:
    print (str(err))  # will print something like "option -a not recognized"
    usage()
    sys.exit(2)
  for o, a in opts:
    if o in ("-h", "--help"):
      usage()
      sys.exit()
    elif o in ("-q", "--queue"):
      queue=True
    elif o in ("-s", "--seeds"):
      seeds=True
    elif o in ("-c", "--config"):
      config_file=a
    else:
      assert False, "unhandled option"
  return queue, seeds, config_file

def remove_first_if_equals(s, c):
    if (len(s) > 0 and s[0] == c):
        return s[1:]
    else: 
        return(s)

###############################################################################
###                   Microbenchmarks
###############################################################################


######## Use #########
# Simply run this script to generate scripts (to manually run
# benchmarks) In order to submit jobs to the cluster, use '-q'. To
# generate scripts to create init checkpoints, use '-i'

submit_mode, seeds_mode, config_file = parseOptions()

config_file = os.path.splitext(config_file)[0]

print("Loading Config: " + config_file)

config = __import__(config_file)

print("Generating simulation scripts...")

seed_list = [0]


p = subprocess.run(['git', 'describe', '--dirty', '--always', '--tags'],
                   # Python 3.7: capture_output = True)
                   stdout=subprocess.PIPE,
                   stderr=subprocess.PIPE)
if p.returncode != 0:
  print("Failed to obtain repository revision via 'git describe'!")
  sys.exit()
repository_revision = p.stdout.decode().strip()

if submit_mode:
  print("Submitting jobs...")
  config.htm_xact_visualizer = 0  # Disable visualizer if submitting to queue
  config.run_gdb = 0  # No gdb when submitting
  config.copy_gem5_binary_tmp_dir = 1 # Copy binary to tmp dir to prevent overwriting it


if seeds_mode:
  print("[%d random seed(s)]" % config.num_random_seeds)
  seed_list.extend(range(1,config.num_random_seeds));

results_prefix = os.path.join(config.results_subdir,
                              str(datetime.date.today())+'_'+str(config.seq_no),
                              config.arch)

cvsroot_results = os.path.join(config.gem5root, "results", results_prefix)

# Dictionary of paths to gem5 binary, per protocol
gem5_binary_exec_path = {}

# Copy gem5 binaries to tmp dirs, set path to gem5 binary for each protocol
for protocol, htm_config, cache_config in config.system_list:
  # Locate gem5 executable
  gem5_exec_path = os.path.join(config.gem5root, "build",
                                config.arch+"_"+protocol,
                                "gem5."+config.build_type)
  if config.copy_gem5_binary_tmp_dir:
    # NOTE: Copying the binary is not enough to ensure that batch
    # simulations are not affected by changes to the source tree,
    # since Python config scripts in gem5-root/config are still
    # executed when starting a batch job.

    # Create a temporary directory and copy gem5 binary
    # Different binaries for each arch/protocol combination
    tmpdir_prefix = os.path.join(config.tmp_gem5_binaries_path,
                                 config.arch,
                                 protocol)
    if not os.path.exists(tmpdir_prefix):
      print("Creating %s" % tmpdir_prefix)
      os.makedirs(tmpdir_prefix)
    # Different tmpdir for each revision
    tmpdir_path = os.path.join(tmpdir_prefix, "rev-"+repository_revision)
    tmpdir = tempfile.mkdtemp(prefix=tmpdir_path)
    exec_path = os.path.join(tmpdir, "gem5."+config.build_type)
    ret = subprocess.call(["cp", gem5_exec_path, tmpdir])
    if ret != 0:
      print("Failed to copy gem5.%s binary to tmp dir %s" % (config.build_type, tmpdir))
      sys.exit()
  else:
    exec_path = gem5_exec_path

  # Add tmpdir to protocols
  gem5_binary_exec_path[protocol] = exec_path

scripts = {}
for random_seed in seed_list:
  for (processors, benchmark_config, cpu_model,
       protocol, htm_config, cache_config) in config.simulation_list:
    benchmark_suite, benchmark, arg_prefix, processors_opt, arg_string, benchmark_subdir, binary_filename = benchmark_config

    if protocol not in gem5_binary_exec_path:
      print("Could not find gem5 executable path for protocol %s, build type %s" % (protocol, config.build_type))
      sys.exit()

    gem5_executable_filepath = gem5_binary_exec_path[protocol]

    if config.slurm_exclude_nodelist != None:
      nodelist = "--exclude=" + config.slurm_exclude_nodelist

    benchmark_name = benchmark
    if htm_config:
      binary_suffix = htm_config[htm.htm_binary_suffix]
    else:
      binary_suffix = config.binary_suffix

    htm_options_str = ''
    htm_config_description = ''
    if protocol == 'MESI_Three_Level_HTM_umu' or \
       protocol == 'MESI_Two_Level_HTM_umu':
      for option in htm.htm_config_options:
        # For sanity, all available htm config options must have been
        # set in htm_config
        if option not in htm_config:
          print("HTM option htm_%s not specified!" % option.gem5opt)
          sys.exit(2)
        opt_value = htm_config[option]
        if option.gem5opt != None:  # None: HTM parameter is not gem5 option
            if htm_config[option] is None:
              continue
            elif type(htm_config[option]) is bool:
              assert(option.isbool)
              # Only append to description if true
              if htm_config[option] is True:
                htm_options_str += ' --htm-'+option.gem5opt
                if option.descr: # Append abbrev name to description
                  htm_config_description += option.abbrev+"_"
            else:
              assert((type(opt_value) is str) or
                     (type(opt_value) is int))
              htm_options_str += ' --htm-'+option.gem5opt+'='+opt_value
              if type(opt_value) is str and \
                 opt_value in htm.htm_option_str_abbreviations:
                opt_value_abbrev=htm.htm_option_str_abbreviations[opt_value]
              else:
                opt_value_abbrev=opt_value
              htm_config_description += option.abbrev+opt_value_abbrev+"_"
        elif option.descr: # Append abbrev name to description, if any
          if opt_value in htm.htm_option_str_abbreviations:
            option_descr = htm.htm_option_str_abbreviations[opt_value]
          elif type(htm_config[option]) is bool:
            option_descr = ''
          else:
            option_descr = str(opt_value)
            # Only append to description if true or non bool
          if type(htm_config[option]) is not bool or \
             htm_config[option] is True:
            htm_config_description += option.abbrev+option_descr+"_"
    cache_config_description="Unknown"
    cache_options_str = ''
    # Create a copy of config to change cache_l2_caches option only
    # for the current number of processors
    cache_conf = collections.OrderedDict(cache_config)
    # Sliced L2 cache: automatically set to number of processors, must
    # be left unset (set to 0) in config.py
    assert (cache_conf[caches.cache_l2_caches] == 0)
    cache_conf[caches.cache_l2_caches] = processors
    for prototype in caches.cache_config_options:
      option = cache_conf[prototype]
      if prototype.gem5opt != None:
         if prototype.gem5opt == "name":
           cache_config_description=option
         elif prototype.shared:
           cache_options_str += ' --'+prototype.gem5opt+'='+str(option // processors)
         elif prototype.gem5opt.startswith("l0") and "Two_Level" in protocol:
           pass
         else:
           cache_options_str += ' --'+prototype.gem5opt+'='+str(option)

    results_bench_config = os.path.join(cvsroot_results, cpu_model, protocol)
    if htm_config:
      results_bench_config = os.path.join(results_bench_config,
                                          htm_config_description)
    results_bench_config = os.path.join(results_bench_config,
                                        cache_config_description,
                                        str(processors)+'p',
                                        benchmark_suite+'-'+arg_prefix,
                                        benchmark_name)

    # Checkpoint reuse disabled by default
    reuse_ckpt_path = None
    results_dir_base = results_bench_config

    if not os.path.exists(results_dir_base):
      os.makedirs(results_dir_base)

    results_dir = os.path.join(results_dir_base, str(random_seed))
    if not os.path.exists(results_dir):
      os.makedirs(results_dir)

    ########### Boot-script generation #########
    launchscript_path = os.path.join(results_dir,
                                   config.launchscript_filename)
    launchscript_file = open("%s" % (launchscript_path), "w")

    launchscript_file.write("#!/bin/bash\n")
    launchscript_file.write("### @launchscript@ ###\n\n")
    launchscript_file.write("PROCESSORS=%d\n" % processors)
    launchscript_file.write("BENCHMARK_DIR=%s\n" % benchmark_subdir)
    launchscript_file.write("BINARY_SUFFIX=%s\n" % binary_suffix)
    launchscript_file.write("ARCH=%s\n" % config.arch_name)

    launchscript_file.write("BINARY_FILENAME=%s\n" % binary_filename)
    launchscript_file.write("BENCHMARK_ARG_STRING='%s'\n" % arg_string)
    launchscript_file.write("RANDOM_SEED='%d'\n" % random_seed)
    launchscript_file.write("\n")
    launchscript_file.write("sync\n") # For tty to show "Welcome to Ubuntu.."

    if config.mount_benchmarks_device:
      launchscript_file.write("mkdir -p %s\n" % config.benchmarks_disk_image_mountpoint)
      launchscript_file.write("mount %s %s\n" %
                              (config.benchmarks_device,
                               config.benchmarks_disk_image_mountpoint))

    # Must set M5_SIMULATOR=1 in order to enable m5 ops. Otherwise,
    # benchmarks typically suppress m5 ops by mmap'ing m5_mem to a
    # zero-filled region of memory instead of /dev/mem.
    launchscript_file.write("export M5_SIMULATOR=1\n")

    #### HTM library configuration options (HTM_umu-specific)
    if htm_config:
      # Maximum number of retries before fallback lock acquired passed
      # to abort handler via environment
      launchscript_file.write("export HTM_MAX_RETRIES=%d\n" % htm_config[htm.htm_max_retries])
      launchscript_file.write("export HTM_HEAP_PREFAULT=%d\n" % htm_config[htm.htm_heap_prefault])
      launchscript_file.write("export HTM_BACKOFF=%d\n" % htm_config[htm.htm_backoff])

    launchscript_file.write("cat /sys/kernel/mm/transparent_hugepage/enabled \n")
    if config.disable_transparent_hugepage:
      # The default option in Ubuntu 18 and Fedora is madvise, but
      # in the 4.8 kernel we are using these days is set to always:
      #always [madvise] never
      launchscript_file.write("echo never > /sys/kernel/mm/transparent_hugepage/enabled \n")
      launchscript_file.write("\n")

    benchmark_suite_root_dir = os.path.join(config.benchmarks_disk_image_mountpoint,
                                            benchmarks.benchmark_suites[benchmark_suite])
    # Variability is only needed if we are not using KVM..
    if not config.enable_kvm:
      launchscript_file.write("sleep 0.${RANDOM_SEED} # Generate variability via random seed \n")
    launchscript_file.write("cd %s\n" % ( os.path.join(benchmark_suite_root_dir,
                                                       benchmark_subdir)))
    launchscript_file.write("export LD_PRELOAD=%s\n" % (config.preload));
    launchscript_file.write("./${BINARY_FILENAME}.${ARCH}${BINARY_SUFFIX} %s${PROCESSORS} ${BENCHMARK_ARG_STRING}\n" % (processors_opt))
    # In case binary not found, give some time to tty to print error message
    launchscript_file.write("echo 'Launch script done. Exiting simulation...(m5 exit)'\n")
    launchscript_file.write("sync; sleep 2\n")
    launchscript_file.write("/sbin/m5 exit\n")
    launchscript_file.close()

    ########### Simulation info  #########
    siminfo_filename = config.sim_info_filename;
    siminfo_path = "%s/%s" % (results_dir, siminfo_filename)
    siminfo_file = open("%s" % (siminfo_path), "w")

    siminfo_file.write("[SimulationInfo]\n")
    siminfo_file.write("num_cpus=%d\n" % processors)
    siminfo_file.write("protocol=%s\n" % protocol)
    siminfo_file.write("cpu_model=%s\n" % cpu_model)
    siminfo_file.write("benchmark_name=%s\n" % benchmark_name)
    siminfo_file.write("benchmark_size=%s\n" % arg_prefix)
    siminfo_file.write("random_seed=%d\n" % random_seed)
    siminfo_file.write("git_revision=%s\n" % repository_revision)

    if protocol == 'MESI_Three_Level_HTM_umu' or \
       protocol == 'MESI_Two_Level_HTM_umu':
      for option in htm.htm_config_options:
        assert(option in htm_config)
        if option.siminfo:
          siminfo_file.write("%s=" % (option.name))
          if htm_config[option] is not None:
            if type(htm_config[option]) is int:
              siminfo_file.write("%d" % (htm_config[option]))
            else:
              opt_value = htm_config[option]
              assert((type(opt_value) is str) or
                     (type(opt_value) is bool))
              siminfo_file.write("%s" % (htm_config[option]))
          siminfo_file.write("\n")
    siminfo_file.close()

    ########### Simulation script generation #########

    script_filename = config.run_script_filename
    script_path = os.path.join(results_dir, script_filename)

    script_file = open("%s" % (script_path), "w")

    script_file.write("#!/bin/bash\n\n")
    script_file.write('set -o nounset\n')
    script_file.write('set -o pipefail\n')
    script_file.write('set -o errexit\n')
    script_file.write('trap \'echo "$SCRIPT_COMMAND: error $? at line $LINENO"\' ERR\n\n')
    script_file.write('SCRIPT_DIR="$(readlink -fm "$(dirname "$0")")"\n')
    script_file.write('SCRIPT_COMMAND="$(basename "$0")"\n')

    script_file.write("GEM5_ROOT=%s\n" % config.gem5root)
    script_file.write("HOST=`hostname`\n")
    script_file.write("\n")

    # Debug configuration
    script_file.write("DEBUG_START_TICK=\n")
    script_file.write("DEBUG_FLAGS=%s\n" % config.debug_flags)
    script_file.write("BUILD_TYPE=%s\n" % config.build_type)
    script_file.write("RUN_GDB=%d\n" % config.run_gdb)
    script_file.write("RUN_PDB=%d\n" % config.run_pdb)
    script_file.write("EXIT_AT_ROI_END=%d\n" % config.exit_at_roi_end)
    script_file.write("ENABLE_KVM=%d\n" % config.enable_kvm)

    script_file.write("EXTRA_DETAILED_ARGS=\" %s \"\n" % config.extra_detailed_args)

    script_file.write("\n### System configuration ### \n")
    script_file.write("ARCH_NAME=%s\n" % config.arch_name)
    script_file.write("KERNEL_BINARY=%s\n" % config.kernel_binary)
    script_file.write("ROOT_DEVICE=%s\n" % config.root_device)
    script_file.write("OS_DISK_IMAGE=%s\n" % config.os_disk_image)
    script_file.write("BENCHMARKS_DEVICE=%s\n" % config.benchmarks_device)
    script_file.write("BENCHMARKS_DISK_IMAGE=%s\n" % config.benchmarks_disk_image)
    script_file.write('ARCH_SPECIFIC_OPTS=" %s "\n' % config.arch_specific_opts)
    script_file.write("TERMINAL_FILENAME=%s\n\n" % config.terminal_filename)
    script_file.write("PROCESSORS=%d\n" % processors)
    script_file.write("BENCHMARK=%s\n" % benchmark)
    script_file.write("BENCHMARK_NAME=%s\n" % benchmark_name)
    script_file.write("BINARY_FILENAME=%s\n" % binary_filename)
    script_file.write("BINARY_SUFFIX=%s\n" % binary_suffix)
    script_file.write("BENCHMARK_ARG_PREFIX=%s\n" % arg_prefix)
    script_file.write("BENCHMARK_ARG_STRING='%s'\n" % arg_string)
    script_file.write("WORKLOAD=%s_%s\n" % (benchmark, arg_prefix))
    script_file.write("ARCH=%s\n" % config.arch)
    script_file.write("PROTOCOL=%s\n" % protocol)
    script_file.write("HTM_CONFIG_DESCRIPTION=%s\n" % htm_config_description)
    script_file.write("RESULTS_DIR=%s\n" % results_dir)
    script_file.write("RANDOM_SEED=%d\n" % random_seed)
    script_file.write("NETWORK_MODEL=%s\n" % config.network_model)
    script_file.write("MEMORY_TYPE=%s\n" % config.memory_type)
    script_file.write("MEMORY_SIZE=%s\n" % config.memory_size)
    script_file.write('CACHE_OPTIONS_STRING="%s"\n' % cache_options_str)

    # HTM configuration options
    if protocol == 'MESI_Three_Level_HTM_umu' or \
       protocol == 'MESI_Two_Level_HTM_umu':
      script_file.write("\n### HTM configuration ### \n")
      script_file.write('HTM_OPTIONS_STRING="%s"\n' % htm_options_str)
      # File containing fallback lock address
      script_file.write("FALLBACK_LOCK_FILE=%s\n" % os.path.join(results_dir,htm.fallback_lock_file))
    script_file.write("SIM_INFO_FILENAME=%s\n" % os.path.join(results_dir,config.sim_info_filename))
    script_file.write("REPOSITORY_REVISION_ID=%s\n" % repository_revision)

    script_file.write("LAUNCH_SCRIPT=%s\n" % launchscript_path)

    script_file.write("SIMULATION_TAG=%s\n" % config.simulation_tag)

    script_file.write("\n### Checkpoint configuration ### \n")
    script_file.write("KEEP_CHECKPOINT=1\n")
    script_file.write("CHECKPOINT_INIT_SUBDIR=%s\n" % config.checkpoint_subdir)
    script_file.write("REUSE_INIT_CHECKPOINT=%d\n" % config.reuse_init_checkpoint)
    if config.reuse_init_checkpoint:
      # To speedup simulations for ARM target, reuse init checkpoints when possible
      assert(config.arch_name == "aarch64")
      # Compute hash from launchscript file (reuse init checkpoint)
      from hashlib import blake2b
      launchscript_file = open("%s" % (launchscript_path))
      launchscript_lines = launchscript_file.readlines()
      h = blake2b()
      for line in launchscript_lines:
        h.update(line.encode('utf-8'))
      lauchscript_digest = h.hexdigest()

      bench_checkpoint_subdir = os.path.join(config.checkpoint_init_root_dir,
                                             lauchscript_digest)
      script_file.write("CHECKPOINT_INIT_REUSED_PATH=%s\n" % bench_checkpoint_subdir)
    script_file.write("CHECKPOINT_BOOT_ROOT_DIR=%s\n" %  config.checkpoint_boot_root_dir)
    script_file.write("CHECKPOINT_TMPDIR_PREFIX=%s\n" % config.checkpoint_tmpdir_prefix)

    # File containing /proc/<pid>/maps of simulated process
    script_file.write("PROC_MAPS_FILE=%s\n" % os.path.join(results_dir,config.proc_maps_file))

    script_file.write("\n### CPU configuration ### \n")
    script_file.write("DETAILED_SIMULATION_CPU_MODEL=%s\n" % cpu_model)

    script_file.write("\n### Cache configuration ### \n")

    # gem5 binary
    if config.copy_gem5_binary_tmp_dir:
      script_file.write("\n### Location of 'gem5' executable in tmp dir  ### \n")
      script_file.write("GEM5_EXEC_PATH=%s\n" % gem5_executable_filepath)
    else:
      script_file.write("\n### Location of 'gem5' executable   ### \n")
      script_file.write('GEM5_EXEC_PATH="${GEM5_ROOT}/build/${ARCH}_${PROTOCOL}/gem5.${BUILD_TYPE}"\n')

    script_file.write("export M5_PATH=%s\n" % os.path.join(config.gem5path,
                                                           config.arch_name))
    script_file.write("\n### Submit mode (SLURM)  ### \n")
    script_file.write("SUBMIT_MODE=%d\n" % submit_mode)

    script_file.write("\n### %s template ### \n" %
                      os.path.basename(config.template_script_path))

    # Now append template script
    with open(config.template_script_path) as infile:
      script_file.write(infile.read())
    script_file.close()

    # Make it executable
    os.chmod(script_path, 0o775)

    # Sanity checks
    if (script_path in scripts):
        print("Duplicated script path " + script_path)
        print("Conflicting configuration: {} {} {} {} {}"
              .format(processors, cpu_model,
                      protocol, htm_config, cache_config))
        sys.exit(-1)
    else:
      scripts[script_path] =  (processors, benchmark_config, cpu_model,
                               protocol, htm_config, cache_config)

    if (submit_mode == 1):
      ## ######################################################
      ## Generate scripts and submit jobs to the cluster
      ## ######################################################
      assert(config.debug_time == 0)

      job_name = results_dir
      print("Job {} submitted".format(job_name))

      submit_job_cmd_str = 'sbatch -J {} {} -e {} -o {} {}'.\
                           format(job_name, nodelist,
                                  os.path.join(results_dir,
                                               'stderr'),
                                  os.path.join(results_dir,
                                               'stdout'),
                                  script_path)
      subprocess.run(submit_job_cmd_str, shell=True, check=True)
    else:
      ## ######################################################
      ## Generate scripts (manual execution)
      ## ######################################################

      print("  Making script %s" % (script_path))


