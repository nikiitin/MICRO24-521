import m5.util

def setHTMOptions(htm, options):

    if options.htm_disable_speculation != None:
        htm.disable_speculation = options.htm_disable_speculation
    if options.htm_lazy_vm != None:
        htm.lazy_vm = options.htm_lazy_vm
    if options.htm_eager_cd != None:
        htm.eager_cd = options.htm_eager_cd
    if options.htm_conflict_resolution != None:
        htm.conflict_resolution = options.htm_conflict_resolution
    if options.htm_lazy_arbitration != None:
        htm.lazy_arbitration = options.htm_lazy_arbitration
    if options.htm_allow_read_set_l0_cache_evictions != None:
        htm.allow_read_set_l0_cache_evictions = \
        options.htm_allow_read_set_l0_cache_evictions
    if options.htm_allow_read_set_l1_cache_evictions != None:
        htm.allow_read_set_l1_cache_evictions = \
        options.htm_allow_read_set_l1_cache_evictions
    if options.htm_allow_read_set_l2_cache_evictions != None:
        htm.allow_read_set_l2_cache_evictions = \
        options.htm_allow_read_set_l2_cache_evictions
    if options.htm_allow_write_set_l2_cache_evictions != None:
        htm.allow_write_set_l2_cache_evictions = \
        options.htm_allow_write_set_l2_cache_evictions
    if options.htm_allow_write_set_l0_cache_evictions != None:
        htm.allow_write_set_l0_cache_evictions = \
        options.htm_allow_write_set_l0_cache_evictions
    if options.htm_allow_write_set_l1_cache_evictions != None:
        htm.allow_write_set_l1_cache_evictions = \
        options.htm_allow_write_set_l1_cache_evictions
    if options.htm_precise_read_set_tracking != None:
        htm.precise_read_set_tracking = options.htm_precise_read_set_tracking
    if options.htm_trans_aware_l0_replacements != None:
        htm.trans_aware_l0_replacements = \
        options.htm_trans_aware_l0_replacements
    if options.htm_trans_aware_l1_replacements != None:
        htm.trans_aware_l1_replacements = \
        options.htm_trans_aware_l1_replacements
    if options.htm_l0_downgrade_on_l1_gets != None:
        htm.l0_downgrade_on_l1_gets = options.htm_l0_downgrade_on_l1_gets
    if options.htm_reload_if_stale != None:
        htm.reload_if_stale = options.htm_reload_if_stale
        if (htm.reload_if_stale and not \
            htm.precise_read_set_tracking):
            m5.util.panic("reload-if-stale requires precise-read-set-tracking")
    if options.htm_delay_interrupts != None:
        htm.delay_interrupts = options.htm_delay_interrupts
    if options.htm_fwd_mechanism != None:
        htm.fwd_mechanism = options.htm_fwd_mechanism
    if options.htm_prio_mechanism != None:
        htm.prio_mechanism = options.htm_prio_mechanism
    if options.htm_fwd_cycle_solve_strategy != None:
        htm.fwd_cycle_solve_strategy = options.htm_fwd_cycle_solve_strategy
    if options.htm_allowed_data_fwd != None:
        htm.allowed_data_fwd = options.htm_allowed_data_fwd
    if options.htm_allow_early_value_forwarding != None:
        htm.allow_early_value_forwarding = options.htm_allow_early_value_forwarding
        if options.htm_max_consumed_blocks != None:
            htm.max_consumed_blocks = options.htm_max_consumed_blocks
        if options.htm_remove_false_sharing_forward != None:
            htm.remove_false_sharing_forward = options.htm_remove_false_sharing_forward
        if options.htm_cycles_to_validate != None:
            htm.cycles_to_validate = options.htm_cycles_to_validate
        if options.htm_n_val_fwd != None:
            htm.n_val_fwd = options.htm_n_val_fwd
        if options.htm_NC_not_producer != None:
            htm.NC_not_producer = options.htm_NC_not_producer
        if options.htm_max_consumers != None:
            htm.max_consumers = options.htm_max_consumers
        if options.htm_max_prio_diff != None:
            htm.max_prio_diff = options.htm_max_prio_diff
        if options.htm_no_produce_on_retry != None:
            htm.no_produce_on_retry = options.htm_no_produce_on_retry
    if options.htm_fallbacklock_addr != None:
        htm.fallbacklock_addr = options.htm_fallbacklock_addr
    if options.htm_value_checker != None:
        htm.value_checker = options.htm_value_checker
    if options.htm_isolation_checker != None:
        htm.isolation_checker = options.htm_isolation_checker
    if options.htm_visualizer != None:
        htm.visualizer = options.htm_visualizer
    if options.htm_visualizer_filename != None:
        htm.visualizer_filename = options.htm_visualizer_filename

def addGeneralUMUOptions(parser):
    # Pass file containin memory regions (/proc/<pid>/maps) to
    # simulator. Useful for various sanity checks such as detecting
    # stack access, when fetch is pointed outside code regions, etc.
    parser.add_argument("--proc-maps-file", action="store", default=None,
                      help="File with memory map of the simulated benchmark")

def addHTMOptions(parser):
    addGeneralUMUOptions(parser)

    # See src/mem/HTM.py for documentation about each config option
    parser.add_argument("--htm-disable-speculation", action="store_true",
                      default=None,
                      help="Disable HTM speculation, force abort on HTM start")
    parser.add_argument("--htm-lazy-vm", action="store_true", default=None,
                      help="Lazy version management")
    parser.add_argument("--htm-eager-cd", action="store_true", default=None,
                      help="Eager conflict detection")
    parser.add_argument("--htm-conflict-resolution", default="requester_wins",
                      choices=["requester_wins",
                               "requester_loses",
                               "requester_wins_power",
                               "requester_loses_power",
                               "requester_wins_power_consumer",
                               "requester_loses_power_consumer",
                               "committer_wins",
                               "requester_stalls_pic_base",
                               "requester_stalls_cda_hybrid",
                               "requester_stalls_cda_hybrid_ntx",
                               "requester_stalls_cda_base_ntx",
                               "requester_stalls_cda_base"
                      ],
                      help = "Conflict resolution policy")
    parser.add_argument("--htm-fwd-mechanism", default="no_fwd",
                        choices=["no_fwd",
                                 "simple",
                                 "naive",
                                 "W1_D1",
                                 "WN_D1",
                                 "W1_DN",
                                 "WN_DM",
                                 "WX_DX"
                                 ],
                      help = "Conflict resolution policy")
    parser.add_argument("--htm-fwd-cycle-solve-strategy", default="no_strategy",
                        choices=["no_strategy",
                                 "abort",
                                 "retry"],
                        help = "Strategy used to solve cycle on forwarding")
    parser.add_argument("--htm-allowed-data-fwd", default="all_data",
                        choices=["all_data",
                                 "only_written",
                                 "no_write_in_flight"],
                        help = "Situations where data is allowed to be forwarded")
    parser.add_argument("--htm-prio-mechanism", default="no_prio",
                        choices=["no_prio",
                                 "numeric"],
                      help = "Priority mechanism")
    parser.add_argument("--htm-lazy-arbitration",
                        default=None,
                        choices=["magic",
                                 "token"],
                      help = "Lazy arbitration policy")
    parser.add_argument("--htm-allow-read-set-l0-cache-evictions",
                      action="store_true", default=False,
                      help="Allow read-set L0 cache evictions")
    parser.add_argument("--htm-allow-read-set-l1-cache-evictions",
                      action="store_true", default=False,
                      help="Allow read-set L1 cache evictions")
    parser.add_argument("--htm-allow-write-set-l0-cache-evictions",
                      action="store_true", default=False,
                      help="Allow write-set L0 cache evictions")
    parser.add_argument("--htm-allow-write-set-l1-cache-evictions",
                      action="store_true", default=False,
                      help="Allow write-set L1 cache evictions")
    parser.add_argument("--htm-allow-read-set-l2-cache-evictions",
                      action="store_true", default=False,
                      help="Allow read-set L2 cache evictions")
    parser.add_argument("--htm-allow-write-set-l2-cache-evictions",
                      action="store_true", default=False,
                      help="Allow write-set L2 cache evictions")
    parser.add_argument("--htm-precise-read-set-tracking",
                      action="store_true", default=False,
                      help="Allow read-set L0/L1 cache evictions")
    parser.add_argument("--htm-trans-aware-l0-replacements",
                      action="store_true", default=False,
                      help="Avoid replacements of read-write set"
                      " blocks when non-trans candidates present")
    parser.add_argument("--htm-trans-aware-l1-replacements",
                      action="store_true", default=False,
                      help="Avoid replacements of read-write set"
                      " blocks when non-trans candidates present")
    parser.add_argument("--htm-l0-downgrade-on-l1-gets",
                      action="store_true", default=False,
                      help="Downgrade L0 from E/M to S upon trans L1 GETS")
    parser.add_argument("--htm-reload-if-stale",
                      action="store_true", default=False,
                      help="Re-execute loads that may have obtained"
                      " stale data (observed conflicting inv)")
    parser.add_argument("--htm-allow-early-value-forwarding",
                        action="store_true", default=False,
                        help="Speculate data from non-finished transactions"
                        " and validate it to prevent incorrect data usage")
    parser.add_argument("--htm-max-consumed-blocks",
                        action="store", default=None,
                        help="Maximum amount of consumed forwarded blocks")
    parser.add_argument("--htm-remove-false-sharing-forward",
                        action="store_true", default=False,
                        help="Avoid false sharing aborts on forwarded data")
    parser.add_argument("--htm-cycles-to-validate", action="store", type=int,
                        default=100,
                        help="exit at specified work end count")
    parser.add_argument("--htm-n-val-fwd", action="store", type=int,
                        default=40,
                        help="Validations to perform before transaction aborts"
                        " use it only with naive plain fwd scheme")
    parser.add_argument("--htm-NC-not-producer", action="store_true",
                        default=False,
                        help="Do not allow NC nodes to produce")
    parser.add_argument("--htm-max-consumers", action="store", type=int, default=1,
                        help="Maximum number of consumers for each producer")
    parser.add_argument("--htm-no-produce-on-retry", action="store_true",
                        default=False,
                        help="Do not produce on retry")
    parser.add_argument("--htm-max-prio-diff", action="store", type=int, default=1,
                        help="Maximum difference between producer and consumer priority")
    parser.add_argument("--htm-delay-interrupts",
                      action="store_true", default=False,
                      help="Delay interrupts that occur within transactions")
    parser.add_argument("--htm-fallbacklock-addr", action="store", default=None,
                      help="Address of the fallback lock (HTM)")
    parser.add_argument("--htm-value-checker", action="store_true", default=None,
                      help="Enable transaction value checker")
    parser.add_argument("--htm-isolation-checker", action="store_true", default=None,
                      help="Enable transaction isolation checker")
    parser.add_argument("--htm-visualizer", action="store_true", default=None,
                      help="Thread state visualizer")
    parser.add_argument("--htm-visualizer-filename", action="store",
                      default=None,
                      help="File where visualizer trace dumped to")
