import collections
############################################################################
##  Cache Configuration
############################################################################

# Cache config options (Sizes in Bytes):           gem5 option name, shared cache
CacheOption = collections.namedtuple('CacheOption', ['gem5opt','shared'])
cache_name = CacheOption("name", False)
cache_l0i_size = CacheOption("l0i_size", False)
cache_l0d_size = CacheOption("l0d_size", False)
cache_l0i_assoc = CacheOption("l0i_assoc", False)
cache_l0d_assoc = CacheOption("l0d_assoc", False)
cache_l1i_size = CacheOption("l1i_size", False)
cache_l1d_size = CacheOption("l1d_size", False)
cache_l1i_assoc = CacheOption("l1i_assoc", False)
cache_l1d_assoc = CacheOption("l1d_assoc", False)
cache_l2_caches = CacheOption("num-l2caches", False)
cache_l2_size = CacheOption("l2_size", True) #  Total L2 size is divided into 'num-l2caches' slices)
cache_l2_assoc = CacheOption("l2_assoc", False)

cache_config_options = []
cache_config_options.append(cache_name)
cache_config_options.append(cache_l0i_size)
cache_config_options.append(cache_l0d_size)
cache_config_options.append(cache_l0i_assoc)
cache_config_options.append(cache_l0d_assoc)
cache_config_options.append(cache_l1i_size)
cache_config_options.append(cache_l1d_size)
cache_config_options.append(cache_l1i_assoc)
cache_config_options.append(cache_l1d_assoc)
cache_config_options.append(cache_l2_caches)
cache_config_options.append(cache_l2_size)
cache_config_options.append(cache_l2_assoc)

cache_baseline = collections.OrderedDict()
cache_baseline[cache_name] = "DefaultCache"
cache_baseline[cache_l0i_size] = 32768
cache_baseline[cache_l0d_size] = 32768
cache_baseline[cache_l0i_assoc] = 8
cache_baseline[cache_l0d_assoc] = 8
cache_baseline[cache_l1i_size] = 262144
cache_baseline[cache_l1d_size] = 262144
cache_baseline[cache_l1i_assoc] = 8
cache_baseline[cache_l1d_assoc] = 8
cache_baseline[cache_l2_caches] = 0
cache_baseline[cache_l2_size] = 33554432
cache_baseline[cache_l2_assoc] = 16

cache_tests = collections.OrderedDict(cache_baseline)
cache_tests[cache_name] = "TestCache"
cache_tests[cache_l0i_size] = 8192
cache_tests[cache_l0d_size] = 8192
cache_tests[cache_l1i_size] = 32768
cache_tests[cache_l1d_size] = 32768
cache_tests[cache_l2_size] = 262144

cache_small = collections.OrderedDict(cache_baseline)
cache_small[cache_name] = "SmallCache"
cache_small[cache_l0i_size] = 4096
cache_small[cache_l0d_size] = 4096
cache_small[cache_l1i_size] = 32768
cache_small[cache_l1d_size] = 32768

cache_baseline_2level = collections.OrderedDict(cache_baseline)
cache_baseline_2level[cache_name] = "DefaultTwoLevelCache"
cache_baseline_2level[cache_l0i_size] = 0
cache_baseline_2level[cache_l0d_size] = 0
cache_baseline_2level[cache_l1i_size] = 32768
cache_baseline_2level[cache_l1d_size] = 32768
cache_baseline_2level[cache_l2_size] = 8388608 # 8MB
