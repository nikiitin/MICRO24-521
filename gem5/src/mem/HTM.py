from m5.objects.ClockedObject import ClockedObject
from m5.params import *
from m5.proxy import *
from m5.objects.System import System

class HTM(ClockedObject):
    type = 'HTM'
    cxx_header = "mem/htm.hh"
    cxx_class = 'gem5::HTM'

    # Hardware transactional memory model from University of Murcia.
    # Replaces memory-side implementation of gem5-21 with an
    # alternative, highly flexible implementation.

    # Disable HTM support by forcing abort upon htm start instruction,
    # useful to run "single-global-lock" simulations using unmodified
    # HTM binaries
    disable_speculation = Param.Bool(False, "Disable HTM speculation")

    # General HTM system configuration options

    # Supported version management policies: eager (log-based) or lazy
    # (cache as write buffer).
    lazy_vm = Param.Bool(False, "Lazy version management")
    # Supported conflict detection policies: eager (on each mem.ref)
    # or lazy (on commit).
    eager_cd = Param.Bool(False, "Has eager conflict detection")
    # Supported conflict resolution policies.
    conflict_resolution = Param.String("requester_wins",
        "Set conflict resolution policy")
    # Commit arbitration scheme used by systems with lazy conflict
    # detection.
    lazy_arbitration = Param.String("magic",
        "Lazy validation policy")
    # Conflict resolution for validated transactions in lazy_cd
    lazy_commit_width = Param.Unsigned(4, "Maximum number of"
        " outstanding write-set block requests during lazy commit")
    # Whether the L0 cache cache allows evictions of cache blocks in
    # the read-set of the transaction. If set, 3-level protocol allows
    # silent L0 replacements of Rset blocks but L1 local invalidations
    # are still nacked to force L1 to keep forwarding traffic to L0
    # for conlict detection.
    allow_read_set_l0_cache_evictions = Param.Bool(False,
        "Allow read set evictions from the L0 cache")
    allow_write_set_l0_cache_evictions = Param.Bool(False,
        "Allow write set evictions from the L0 cache")
    # Whether the L1 cache allows evictions of cache blocks in the
    # read-set of the transaction. If set, 3-level protocols allow L1
    # invalidations of Rset blocks using "sticky-S" L2 directory
    # states to keep receiving traffic. For 2-level protocols, this
    # option equals allow_read_set_lower_private_cache_evictions
    allow_read_set_l1_cache_evictions = Param.Bool(False,
        "Allow read set evictions from the L1 cache")
    allow_write_set_l1_cache_evictions = Param.Bool(False,
        "Allow write set evictions from the L1 cache")
    allow_read_set_l2_cache_evictions = Param.Bool(False,
        "Allow read set evictions from the L2 cache")
    allow_write_set_l2_cache_evictions = Param.Bool(False,
        "Allow write set evictions from the L2 cache")
    # Whether speculatively-read cache block are added to the read-set
    # upon load access (imprecise) or load retirement  (precise)
    precise_read_set_tracking = Param.Bool(False,
        "Loads added to read set when load retires"
        " (default: when load executes)")
    # Whether the replacement policy favours read-write sets blocks
    # in detriment of non-transactional blocks, so that the latter are
    # always chosen as victims over the former.
    trans_aware_l0_replacements = Param.Bool(False,
        "Replacement policy always chooses non-transactional blocks"
        " over transactional blocks as candidates (L0 cache)")
    trans_aware_l1_replacements = Param.Bool(False,
        "Replacement policy always chooses non-transactional blocks"
        " over transactional blocks as candidates (L1 cache)")

    # L0 downgrades from E/M to S when L1 receives remote
    # transactional GETS request (otherwise: invalidate L0 copy)
    l0_downgrade_on_l1_gets = Param.Bool(False,
        "L0 downgrades from E/M to S when L1 receives GETS"
        " (default: L1 requests L0 invalidation (conflict)")

    # Re-execute loads that 1) have not yet been retired from the
    # processor, 2) have not added the block to the read set, and 3)
    # observe a conflicting invalidation. precise_read_set_tracking
    # required, as otherwise the CPU cannot tell if this was the first
    # load to the block
    reload_if_stale = Param.Bool(False, "Re-execute trans. loads that may"
    " have obtained stale data (False: abort transaction)")
    reload_if_stale_max_retries = Param.Int(10, "Max number or reloads "
    " before aborting the transaction (prevent livelocks)")
    delay_interrupts = Param.Bool(False, "Delay interrupts that occur "
    " during transaction until its end (False: abort transaction)")

    allow_early_value_forwarding = Param.Bool(False, "Forward data "
     "from non-committed transactions to consumers")
    max_consumed_blocks = Param.Unsigned(4, "Number of maximum blocks "
     "that can be forwarded to consumers")

    remove_false_sharing_forward = Param.Bool(False, "Avoid false sharing "
      "aborts on forwarded data validation")

    cycles_to_validate = Param.Int(100, "Number of cycles each validation "
        "event is going to be delayed")

    fwd_mechanism = Param.String("no_fwd",
        "Value forwarding mechanism used")
    prio_mechanism = Param.String("no_prio",
        "Priorities mechanism used")

    fwd_cycle_solve_strategy = Param.String("no_strategy",
        "Strategy used to solve cycle on forwarding")

    allowed_data_fwd = Param.String("all_data",
        "Situations where data is allowed to be forwarded")

    NC_not_producer = Param.Bool(False, "Do not allow NC nodes to fwd other high prio")

    n_val_fwd = Param.Int(40, "Number of validations performed "
        "before aborting a spec transaction. Use it only with "
        "naive plain forwarding")
    max_prio_diff = Param.Int(1, "Max priority difference between "
        "a spec transaction and a consumer to allow forwarding")

    max_consumers = Param.Int(1, "Max number of consumers that can "
        "be forwarded a block")
    
    no_produce_on_retry = Param.Bool(False, "Do not produce on retry")
    # Debugging/profiling facilities

    # Address of the fallback lock, generated by HTM library during
    # initialization and written to file 'fallback_lock'.
    # gem5_path/benchmarks/benchmarks-htm/libs/handlers/abort_handlers.c
    # This option is required for the proper collection of detailed
    # statistic as well as facilities such as lockstep execution
    fallbacklock_addr = Param.String("",
        "Address of the fallback lock used by transactions")
    # Basic value-based sanity checks to ensure consistency of
    # read/written values produced/consumed by transactions
    value_checker = Param.Bool(False, "Enable value checker")
    # Basic sanity checks to ensure that transaction read-write sets
    # remain isolated (e.g. no writes made coherence while outstanding
    # readers)
    isolation_checker = Param.Bool(False, "Enable isolation checker")
    # Thread text-based visualization facility, showing state for each
    # thread at each give tick in the simulation
    visualizer = Param.Bool(False,
        "Generate visual trace of transactional execution")
    visualizer_filename = Param.String("htm_visualizer",
                                       "Filename where visualizer output "
                                       "dumped to, stderr if not specified")
    profiler = Param.Bool(True,
                          "Profiling of transactional events")
