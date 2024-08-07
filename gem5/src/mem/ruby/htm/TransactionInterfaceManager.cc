/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/
#include "mem/ruby/htm/TransactionInterfaceManager.hh"

#include <cassert>
#include <cstdlib>
#include <limits>

#include "debug/FwdGraphTrace.hh"
#include "debug/FwdGraphTraceVerbose.hh"
#include "debug/HTMInstTracer.hh"
#include "debug/RubyHTM.hh"
#include "debug/RubyHTMlog.hh"
#include "debug/RubyHTMvaluepred.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/ruby/htm/EagerTransactionVersionManager.hh"
#include "mem/ruby/htm/LazyTransactionCommitArbiter.hh"
#include "mem/ruby/htm/LazyTransactionVersionManager.hh"
#include "mem/ruby/htm/TransactionConflictManager.hh"
#include "mem/ruby/htm/TransactionIsolationManager.hh"
#include "mem/ruby/htm/XactIsolationChecker.hh"
#include "mem/ruby/htm/XactValueChecker.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/profiler/XactProfiler.hh"
#include "mem/ruby/slicc_interface/RubySlicc_Util.hh"
#include "mem/ruby/structures/CacheMemory.hh"

namespace gem5
{
namespace ruby
{

#define XACT_LAZY_VM config_lazyVM()
#define XACT_EAGER_CD config_eagerCD()
#define XACT_PROFILER m_ruby_system->getProfiler()->getXactProfiler()

TransactionInterfaceManager::TransactionInterfaceManager(const Params &p)

    : ClockedObject(p),
      _params(p)
{
    m_sequencer = p.sequencer;
    m_ruby_system = p.ruby_system;
    m_htm = m_ruby_system->params().system->getHTM();
    assert(m_htm);
    m_version = m_sequencer->getId();
    m_dataCache_ptr = p.dcache;
    m_l1Cache_ptr   = p.l1_cache;
    m_dataCache_ptr->setTransactionManager(this);
    m_l1Cache_ptr->setTransactionManager(this);

    m_sequencer->setTransactionManager(this);
    m_ruby_system->registerTransactionInterfaceManager(this);

    m_xactIsolationManager = new TransactionIsolationManager(this, m_version);
    m_xactConflictManager  = new TransactionConflictManager(this, m_version);
    if (XACT_LAZY_VM) {
        if (!XACT_EAGER_CD) {
            // Lazy CD + lazy VM in write buffer

            // lazy version manager acts as dedicated write buffer
            m_xactLazyVersionManager   =
                new LazyTransactionVersionManager(this,
                                              m_version,
                                              m_dataCache_ptr);
            // lazy commit arbiter ensures proper detection and
            // resolution of conflicts at commit time
            m_xactLazyCommitArbiter  =
                new LazyTransactionCommitArbiter(this, m_version,
                                                 m_htm->params().
                                                 lazy_arbitration);
        } else {
            // Eager-lazy (eager CD + lazy VM in cache)
        }
        // No wset evictions from private cache are possible
        assert(!m_htm->params().allow_write_set_l0_cache_evictions);
        assert(!m_htm->params().allow_write_set_l1_cache_evictions);
        assert(!m_htm->params().allow_write_set_l2_cache_evictions);
    } else { // Eager CD + Eager VM (LogTM)
        assert(XACT_EAGER_CD);
        m_xactEagerVersionManager   =
            new EagerTransactionVersionManager(this,
                                              m_version,
                                              m_dataCache_ptr);
        // Asume wset evictions from L1 (and L0) are possible
        assert(m_htm->params().allow_write_set_l0_cache_evictions);
        assert(m_htm->params().allow_write_set_l1_cache_evictions);
        assert(m_htm->params().allow_write_set_l2_cache_evictions);
    }

    m_transactionLevel   = 0;
    m_currentHtmUid      = 0;
    m_htmXid             = std::numeric_limits<long>::min();
    m_escapeLevel        = 0;
    m_abortFlag          = false;
    m_unrollingLogFlag   = false;
    m_atCommit           = false;
    m_discardFwdData     = false;
    m_abortCause         = HTMStats::AbortCause::Undefined;
    m_abortSourceNonTransactional = false;
    m_lastFailureCause   = HtmFailureFaultCause::INVALID;
    m_capacityAbortWriteSet = false;
    // Only supported HTM protocol by TransactionInterfaceManager
    assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");

    // Sanity checks
    if (config_allowReadSetL1CacheEvictions()) {
        assert(m_htm->params().allow_read_set_l0_cache_evictions);
    }
    if (config_allowWriteSetL1CacheEvictions() ||
        config_allowWriteSetL2CacheEvictions()) {
        // Evicting write set blocks requires eager versioning
        assert(!XACT_LAZY_VM);
    }

    m_htmstart_tick = 0;
    m_htmstart_instruction = 0;
    m_hasSentDataToConsumer.clear();
    m_hasReceivedDataFromProducer = createInvalidMachineID();
    m_currentValidationAddress = 0;
    m_hwTransPrio = NC_PRIO_VALUE;
    m_hasConsumedValidData = false;
    m_hasValidatedAnyData = false;
    m_dataConsumedFromPower = false;
    m_consumerLowPrio = false;
    m_reconsumer = false;
    m_producerTurnedConsumer = false;
    m_consumerTurnedProducer = false;
    validationEvent = new ValidateConsumedDataEvent(m_sequencer);
    fwdMechanism = getHTM()->getFwdMechanism();
    prioMechanism = getHTM()->getHwPrioMechanism();
}


TransactionInterfaceManager::~TransactionInterfaceManager() {
}

void
TransactionInterfaceManager::setVersion(int version) {
    m_version = version;
    m_xactIsolationManager->setVersion(version);
    m_xactConflictManager->setVersion(version);
}


int
TransactionInterfaceManager::getVersion() const {
    return m_version;
}

int
TransactionInterfaceManager::getProcID() const{
    return m_version;
}

TransactionIsolationManager*
TransactionInterfaceManager::getXactIsolationManager(){
    return m_xactIsolationManager;
}

TransactionConflictManager*
TransactionInterfaceManager::getXactConflictManager(){
    return m_xactConflictManager;
}

LazyTransactionVersionManager*
TransactionInterfaceManager::getXactLazyVersionManager(){
  return m_xactLazyVersionManager;
}

EagerTransactionVersionManager*
TransactionInterfaceManager::getXactEagerVersionManager(){
  return m_xactEagerVersionManager;
}

LazyTransactionCommitArbiter*
TransactionInterfaceManager::getXactLazyCommitArbiter(){
    return m_xactLazyCommitArbiter;
}

TransactionalSequencer *
TransactionInterfaceManager::getSequencer() {
    return m_sequencer;
}

bool
TransactionInterfaceManager::canProduce() {
    // TODO: check policy here!
    if (m_xactConflictManager->isProducerPolicy()) {
    switch (fwdMechanism)
    {
    case HTM::HtmFwdMechanism::no_fwd:
        return false;
    case HTM::HtmFwdMechanism::simple:
        // Simple fwd mechanism require no priority
        assert(prioMechanism ==
            HTM::HtmHwPrioMechanism::no_prio);
    case HTM::HtmFwdMechanism::W1_D1:
    case HTM::HtmFwdMechanism::WN_D1:
        return !isConsumer();
    case HTM::HtmFwdMechanism::WX_DX:
        return config_maxPrioDiff() == 1 && isConsumer() ? false : true;
    case HTM::HtmFwdMechanism::W1_DN:
    case HTM::HtmFwdMechanism::WN_DM:
    case HTM::HtmFwdMechanism::naive:
        return true;
    default:
        panic("Invalid policy found at canProduce!");
        return false;
    }
    }
    // Policy not eligible to produce
    return false;

}

bool
TransactionInterfaceManager::shouldBecomeProducer(Addr addr,
                                                  TransactionBit remote_trans,
                                                  int remote_priority,
                                                  bool remote_sw_priority,
                                                  MachineID consumer,
                                                  bool writeInFlight)
{
    int transactionPriority =
        getTransactionPriority();
    bool isSwPrio = getSwTransactionPriority();
    assert(config_allowEarlyValueFwd());
    assert(inTransaction());
    assert(remote_trans == TransactionBit_Trans);
    assert(!isAborting());
    if (config_noProduceOnRetry() && isRetryingTx()) {
        return false;
    }
    if (getHTM()->getAllowedDataFwd() ==
            HTM::HtmAllowedDataFwd::only_written &&
        !checkWriteSignature(addr) &&
        !getXactConflictManager()->isPowered())
    {
        // Do not forward with RS data
        if (getXactConflictManager()->isReqLosesPolicy() ||
            getXactConflictManager()->isRequesterStallsPolicy())
        {
            return false;
        }
        else if (getXactConflictManager()->isReqWinsPolicy())
        {
            // Abort
            setAbortFlag(addr, consumer,
                         remote_trans,
                         remote_priority,
                         remote_sw_priority,
                         false /*capacity*/,
                         false /*wset*/,
                         false /*dataStale*/,
                         false /*validation*/,
                         false /*specDepthOverflow*/,
                         false /*specMaxCapacityOverflow*/,
                         false /*specWriteInFlight*/,
                         true /*specReadConflict*/);
            return false;
        }
        else
        {
            panic("Invalid policy found!");
        }
    }
    else if (getHTM()->getAllowedDataFwd() ==
                 HTM::HtmAllowedDataFwd::no_write_in_flight &&
             writeInFlight &&
             !getXactConflictManager()->isPowered())
    {
        // Do not forward with RS data
        // if a write is already in flight
        if (getXactConflictManager()->isReqLosesPolicy() ||
            getXactConflictManager()->isRequesterStallsPolicy())
        {
            return false;
        }
        else if (getXactConflictManager()->isReqWinsPolicy())
        {
            // Abort due to write in flight
            // stats related flag
            setAbortFlag(addr, consumer,
                         remote_trans,
                         remote_priority,
                         remote_sw_priority,
                         false /*capacity*/,
                         false /*wset*/,
                         false /*dataStale*/,
                         false /*validation*/,
                         false /*specDepthOverflow*/,
                         false /*specMaxCapacityOverflow*/,
                         true /*specWriteInFlight*/,
                         false /*specReadConflict*/);
            return false;
        }
        else
        {
            panic("Invalid policy found!");
        }
    }
    // Remote should not hold sw prio
    // As local is considering to send fwd data
    assert(!remote_sw_priority);
    bool shouldFwd = false;
    if (isSwPrio ||
        fwdMechanism == HTM::HtmFwdMechanism::naive) {
        // Sw prio always forward
        // Naive always forward
        return true;
    }
    if ((transactionPriority - 1) == NC_PRIO_VALUE) {
        // NOTE:
        // Cannot reduce priority further from
        // NC value, abort requester tx
        return false;
    }
    if (isConsumer()) {
        // Consumer cannot change its priority
        // send data only in case remote is less prio
        shouldFwd =
            remote_priority < transactionPriority;
    } else {
        // Any transaction that do not have
        // hw prio can fwd to any other
        assert((m_hwTransPrio >= remote_priority ||
                !isConsumer()) &&
                !remote_sw_priority);
        shouldFwd = true;
    }
    if (!shouldFwd) return false; // No need to continue
    switch (fwdMechanism)
    {
    case HTM::HtmFwdMechanism::naive:
        {
            // Naive fwd mechanism require no priority
            // else it wouldn't be naive anymore
            // assert(prioMechanism ==
            //     HTM::HtmHwPrioMechanism::no_prio);
            // Never reach here, this condition is
            // already checked, note upper `if´ in this function
            panic("Should not reach here");
            // Always forward
            return true;
        }
    case HTM::HtmFwdMechanism::simple:
        {
            // Simple fwd mechanism require no priority
            assert(prioMechanism ==
                HTM::HtmHwPrioMechanism::no_prio);
            // Forward if not consumer
            if (!isConsumer()) {
                return true;
            } else {
                // Consumers should not be reaching here
                assert(false);
                return false;
            }
        }
    case HTM::HtmFwdMechanism::W1_D1:
        {
            // Consumers should not be reaching here
            assert(!isConsumer());
            // Produce to the same consumer always
            // Producers cannot consume
            // Consumers cannot produce
            return (!isProducer() ||
                (m_hasSentDataToConsumer.find(consumer) != m_hasSentDataToConsumer.end()));
        }
    case HTM::HtmFwdMechanism::WN_D1:
        {
            // Consumers should not be reaching here
            assert(!isConsumer());
            // Produce to anyone that conflict
            // and is less prio
            // Producers cannot consume
            // Consumers cannot produce
            return true;
        }
    case HTM::HtmFwdMechanism::W1_DN:
        {
            // Produce to the same consumer always
            // Producers can consume
            // Consumers can produce
            return (!isProducer() ||
                (m_hasSentDataToConsumer.find(consumer) != m_hasSentDataToConsumer.end()));
        }
    case HTM::HtmFwdMechanism::WN_DM:
        {
            // Produce to anyone that conflict
            // and is less prio
            // Producers can consume
            // Consumers can produce
            return true;
        }
    case HTM::HtmFwdMechanism::WX_DX:
        {
            return m_hasSentDataToConsumer.size() < config_maxConsumers() ||
                (m_hasSentDataToConsumer.find(consumer) != m_hasSentDataToConsumer.end());
            // Produce to anyone that conflict
            // and is less prio
            // Producers can consume
            // Consumers can produce
        }
    case HTM::HtmFwdMechanism::no_fwd:
    {
        panic("no_fwd policy reached shouldBecomeProducer!");
        return false;
    }
    default:
    {
        panic("Specified fwd policy do not match any impl!");
        return false;
    }
    }
}

void TransactionInterfaceManager::addBlockToNackReg(Addr addr,
                                              MachineID consumer) {
    if (!isPowerMode()) {
        return;
    }
    DataBlock* datablock_ptr;
    // must find write hit for all lines in the write buffer
    bool hit = m_dataCache_ptr->
        tryCacheAccess(addr,
                       RubyRequestType_ST,
                       datablock_ptr, true);
    if (!hit) {
        // Finish here, do not add it to register
        // anyway transaction will abort...
        return;
    }
    DataBlock db;
    db.copyFrom(datablock_ptr);
    auto it = m_nackedAddressesReg.find(addr);
    if (it == m_nackedAddressesReg.end()) {
        m_nackedAddressesReg
            .insert(std::pair<Addr,DataBlock>(addr,db));
    } else {
        // Block was already conflicting... update?
    }

}

void
TransactionInterfaceManager::switchHwPrio(int remote_hw_prio,
                                               bool remote_sw_prio,
                                               bool isReceived) {
    if (prioMechanism == HTM::HtmHwPrioMechanism::no_prio) {
        return;
    }
    // Hw priority only tested with these two systems
    assert(config_conflictResPolicy() ==
               HtmPolicyStrings::requester_stalls_pic_base ||
           config_allowEarlyValueFwd());
    if (isReceived) {
        // Receiver set its priority only if it is not set already
        if (m_hwTransPrio == NC_PRIO_VALUE && !remote_sw_prio) {
            // Sw prio never receive
            assert(!getSwTransactionPriority());

            // Do not switch prio if remote
            // sw prio tx
            m_hwTransPrio = remote_hw_prio - 1;
        }
    } else {
        if (getSwTransactionPriority()) {
            assert(m_hwTransPrio == NC_PRIO_VALUE);
            // Do not switch prio if sw tx
            return;
        }
            // Stalled transactions cannot
            // change priority
        if ((config_conflictResPolicy() ==
                 HtmPolicyStrings::requester_stalls_pic_base &&
             !isStalled()) ||
            (config_allowEarlyValueFwd() && !isConsumer())) {
            if (remote_hw_prio == NC_PRIO_VALUE) {
                m_hwTransPrio = m_hwTransPrio == remote_hw_prio
                                    ? INITIAL_PRIO_VALUE
                                    : m_hwTransPrio;
            } else {
                assert((!isStalled() && !isConsumer()) ||
                       m_hwTransPrio > remote_hw_prio);
                m_hwTransPrio = (m_hwTransPrio > remote_hw_prio)
                                    ? m_hwTransPrio
                                    : remote_hw_prio + 1;
            }
        }
    }
}

void TransactionInterfaceManager::addConsumer(Addr addr,
                                              MachineID consumer,
                                              int remote_priority,
                                              bool remote_sw_priority) {
    if (!isProducer() && hasConsumedData())
    {
        m_consumerTurnedProducer = true;
    }
    m_hasSentDataToConsumer.insert(consumer);
    int lastPrio = m_hwTransPrio;
    switchHwPrio(remote_priority, remote_sw_priority, false);
    DPRINTFR(FwdGraphTrace, "%d-%d-%d-Produce-%d-%#x-%s-%s-%s\n",
        curTick(),
        getProcID(),
        m_currentHtmUid,
        machineIDToNodeID(consumer),
        addr,
        remote_priority != NC_PRIO_VALUE ? to_string(remote_priority) : "NC",
        lastPrio != NC_PRIO_VALUE ? to_string(lastPrio) : "NC",
        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");
    DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-Produce-Cons:%d-%#x-remote_prio:%s-local_prio:%s-new_local_prio:%s\n",
        curTick(),
        getProcID(),
        m_currentHtmUid,
        machineIDToNodeID(consumer),
        addr,
        remote_priority != NC_PRIO_VALUE ? to_string(remote_priority) : "NC",
        lastPrio != NC_PRIO_VALUE ? to_string(lastPrio) : "NC",
        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");
    assert (!m_abortFlag && canProduce());
    DPRINTF(RubyHTMvaluepred, "HTMvp: Power tx producer nacks"
            " remote consumer %d address %#x\n",
            machineIDToNodeID(consumer), addr);

    DataBlock* datablock_ptr;
    // must find write hit for all lines in the write buffer
    bool hit = m_dataCache_ptr->
        tryCacheAccess(addr,
                       RubyRequestType_ST,
                       datablock_ptr, true);
    // Careful here, if data evicted from L0
    // (allow_l0_replcmnt) all these will fail
    // Shouldn't we touch the block everytime?
    // Even with that, it is not clear that this
    // block won't evict, maybe restrict to not
    // evict data from L0?
    if (!hit) {
        DPRINTF(RubyHTMvaluepred, "HTMvp: producer is reader"
            " nacks remote consumer writer %d address %#x\n",
                machineIDToNodeID(consumer), addr);
        hit = m_dataCache_ptr->
            tryCacheAccess(addr,
                           RubyRequestType_LD,
                           datablock_ptr, true);
    }
    assert(hit);
    map<Addr,DataBlock>::iterator it;
    it = m_sentDataForwardedToConsumer.find(addr);
    if (it != m_sentDataForwardedToConsumer.end()) {
        auto it2 = m_allSentData.find(addr);
        assert(it2 != m_allSentData.end());
        it2->second.consumers.insert(consumer);
        it2->second.forwardings.back().modified ?
            it2->second.forwardings.push_back({FwdInfo(consumer)}) :
            /* Tricky cast but value won't be used :)*/
            (void)it2->second.forwardings.back().consumers.insert(consumer);
        // Check if value has changed since last conflict
        DataBlock db =(*it).second;
        if (datablock_ptr->equal(db)) {
            DPRINTF(RubyHTMvaluepred, "HTMvp: (Power tx) nacks"
                    " consumed value unmodified since "
                    " last conflict - addr %#x\n", addr);
        } else {
            DPRINTF(RubyHTMvaluepred, "HTMvp: (Power tx) nacks"
                    " consumed value changed since "
                    " last conflict - addr %#x\n", addr);
        }
    } else {
        m_allSentData.
            insert(std::pair<Addr, ProducerInfo>(addr, {consumer, curTick()}));
    }
    DataBlock db;
    db.copyFrom(datablock_ptr);

    m_sentDataForwardedToConsumer.
        insert(std::pair<Addr,DataBlock>(addr, db));

    DPRINTF(RubyHTMvaluepred, "HTMvp: Proc %d"
            " becomes producer for addr %#x "
            " remote consumer is %d\n",
            getProcID(), addr,
            machineIDToNodeID(consumer));
    if (hasConsumedData()) {
        DPRINTF(RubyHTMvaluepred, "HTMvp: Consumer %d"
                " becomes producer for addr %#x "
                " remote consumer is %d\n",
                getProcID(), addr,
                machineIDToNodeID(consumer));
    }
}

void
TransactionInterfaceManager::notifyForwardedDataFromProducer(Addr addr,
                                                             const DataBlock& db,
                                                             MachineID producer,
                                                             Cycles remote_timestamp,
                                                             int remote_priority,
                                                             bool remote_sw_priority) {
    if (!hasConsumedData()) {
        if (isProducer()) {
            assert(!m_producerTurnedConsumer &&
                !m_consumerTurnedProducer);
            m_producerTurnedConsumer = true;
        }
    }
    assert(addr == makeLineAddress(addr));
    assert(!isBlockConsumed(addr));
    // Nack+data received from producer
    // check correctness here. Never
    // accept data from older transaction
    PacketPtr pkt = m_sequencer->getPacketFromRequestTable(addr);
    if (!inTransaction() ||
        pkt->getHtmTransactionUid() < m_currentHtmUid) {
        assert(false);
        // Race condition where a lingering
        // request from aborted transaction
        // can notify wrongly of spec data
        // Controlled on protocol, so now
        // we should never end here
        return;

    }
    // Received speculative data
    int lastPrio = m_hwTransPrio;
    if (getXactConflictManager()->isRequesterStallsPolicy()) {
        // Priority is based on Nacks received, so call
        // NotifyReceiveNack to check cycles
        notifyReceiveNack(addr,
                        remote_timestamp,
                        TransactionBit_Trans /* Only tx can forward */,
                        remote_priority,
                        remote_sw_priority,
                        producer,
                        true/* unused */);
    } else {
        // Switch priority if needed
        switchHwPrio(remote_priority, remote_sw_priority, true);
    }

    // Trace the reception!
    DPRINTFR(FwdGraphTrace, "%d-%d-%d-Consume-%d-%#x-%s-%s-%s\n",
        curTick(),
        getProcID(),
        m_currentHtmUid,
        machineIDToNodeID(producer),
        addr,
        remote_priority != NC_PRIO_VALUE ? to_string(remote_priority) : "NC",
        lastPrio != NC_PRIO_VALUE ? to_string(lastPrio) : "NC",
        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");
    DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-Consume-Prod:%d-%#x-remote_prio:%s-local_prio:%s-new_local_prio:%s\n",
        curTick(),
        getProcID(),
        m_currentHtmUid,
        machineIDToNodeID(producer),
        addr,
        remote_priority != NC_PRIO_VALUE ? to_string(remote_priority) : "NC",
        lastPrio != NC_PRIO_VALUE ? to_string(lastPrio) : "NC",
        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");

    if (!isConsumer())
    {
        if (!hasConsumedData()) {
            assert(m_allReceivedData.find(addr) ==
               m_allReceivedData.end());
        } else {
            m_reconsumer = true;
            // Old consumer is consuming again!
        }
        // Insert received data
        m_allReceivedData.insert(std::pair<Addr, ConsumerInfo>(addr, {producer, db, curTick()}));
        m_recvDataForwardedFromProducer.insert(std::pair<Addr, DataForwarded>(addr, {db, false, WriteMask(), 0}));
        // Not currently a consumer, so validation
        // is not yet scheduled
        // TODO: remove this flag
        m_consumerLowPrio = true;
        DPRINTF(RubyHTMvaluepred, "HTMvp: Proc %d"
                                  " becomes consumer for addr %#x "
                                  " remote producer is %d\n",
                getProcID(), addr,
                machineIDToNodeID(producer));
        // This is a bit tricky... but right now it
        // allow us to know if we are consuming data.
        // TODO: add a buffer to track all current and
        // historical producers
        m_hasReceivedDataFromProducer = producer;
        assert(!validationEvent->scheduled());
        // Schedule validation
        if (!isAborting()) {
            scheduleValidationOfConsumedData(addr, false);
        }
    }
    else
    {
        // Insert received data
        m_allReceivedData.insert(std::pair<Addr, ConsumerInfo>(addr, {producer, db, curTick()}));
        m_recvDataForwardedFromProducer.insert(std::pair<Addr, DataForwarded>(addr, {db, false, WriteMask(), 0}));
        // Already a consumer
        DPRINTF(RubyHTMvaluepred, "HTMvp: Proc %d"
                                  " keeps consuming, now addr %#x "
                                  " remote producer is %d\n",
                getProcID(), addr,
                machineIDToNodeID(producer));
    }

    AnnotatedRegion lastRegion = m_ruby_system->getProfiler()->
        getXactProfiler()->getCurrentRegion(m_version);
    /**
     * TODO: Would be cool to have the profiler to track the priority
     *
     */
    if (lastRegion != AnnotatedRegion_TRANSACTIONAL_SPECULATING &&
        (!atCommit() && !isAborting())) {
        m_ruby_system->getProfiler()->
            getXactProfiler()->moveTo(m_version,
                AnnotatedRegion_TRANSACTIONAL_SPECULATING);
    }
    /**
     * Check if this transaction speculated over the size of
     * validation table. If so, abort it.
     * Cause is specMaxCapacityOverflow
     */
    if (m_recvDataForwardedFromProducer.size() >
        m_htm->params().max_consumed_blocks) {
        setAbortFlag(addr, producer,
                TransactionBit_Trans,
                remote_priority,
                remote_sw_priority,
                false/*capacity*/,
                false/*wset*/,
                false/*dataStale*/,
                false/*validation*/,
                false/*specDepthOverflow*/,
                true/*specMaxCapacityOverflow*/,
                false/*specWriteInFlight*/,
                false/*specReadConflict*/);
    }
    /**
     * Check for configuration where graph's depth is limited
     * if the limit is reached, abort transaction (received a nack).
     * TODO: Add an argument to setAbortFlag to specify the cause
     * as depth limit reached.
     */
    if (fwdMechanism == HTM::HtmFwdMechanism::WX_DX &&
        (abs((INITIAL_PRIO_VALUE - m_hwTransPrio)) >
            config_maxPrioDiff())) {
            setAbortFlag(addr, producer,
                TransactionBit_Trans,
                remote_priority,
                remote_sw_priority,
                false/*capacity*/,
                false/*wset*/,
                false/*dataStale*/,
                false/*validation*/,
                true/*specDepthOverflow*/,
                false/*specMaxCapacityOverflow*/,
                false/*specWriteInFlight*/,
                false/*specReadConflict*/);
    }
}

/**
 * @brief Logic added for false sharing checking.
 * Set an access to an specific part of a data block.
 * It could be a read or a write. Writes are track
 * at setModifiedConsumedBlock
 *
 * @param addr
 * @param offset
 * @param len
 */
void
TransactionInterfaceManager::setAccessOnForwardBlock(Addr addr,
                                                     int offset,
                                                     int len,
                                                     bool write)
{
    auto it = m_recvDataForwardedFromProducer
        .find(addr);
    auto it2 = m_allReceivedData.find(addr);
    assert(it != m_recvDataForwardedFromProducer.end() &&
        it2 != m_allReceivedData.end());
    it->second.mask.setMask(offset, len);
    it2->second.accessMask.setMask(offset, len);
    if (write) {
        it->second.modified = true;
        it2->second.locallyModified = true;
        it2->second.wMask.setMask(offset, len);
    }
    
}

/**
 * @brief Logic added for false sharing checking.
 * Set an access to an specific part of a data block.
 * It could be a read or a write. Writes are track
 * at setModifiedConsumedBlock
 *
 * @param addr
 * @param offset
 * @param len
 */
void
TransactionInterfaceManager::setAccessOnProducedBlock(Addr addr,
                                                     int offset,
                                                     int len,
                                                     bool write)
{
    auto it = m_allSentData.find(addr);
    assert(it != m_allSentData.end());
    if (write) {
        it->second.timesModifiedAfterSharing++;
        it->second.forwardings.back().modified = true;
    }
}

bool
TransactionInterfaceManager::isBlockConsumed(Addr addr)
{
    map<Addr,DataForwarded>::iterator it;
    return (m_recvDataForwardedFromProducer.find(makeLineAddress(addr)) !=
            m_recvDataForwardedFromProducer.end());
}

bool
TransactionInterfaceManager::isBlockProduced(Addr addr)
{
    map<Addr,DataBlock>::iterator it;
    return (m_sentDataForwardedToConsumer.find(makeLineAddress(addr)) !=
            m_sentDataForwardedToConsumer.end());
}

bool
TransactionInterfaceManager::shouldForwardDataToConsumer(Addr addr, MachineID consumer,
                                                         TransactionBit remote_trans,
                                                         int remote_priority,
                                                         bool remote_sw_prio,
                                                         bool writeInFlight)
{
    return (config_allowEarlyValueFwd() &&
                      !m_abortFlag &&
                      canProduce() &&
                      shouldBecomeProducer(addr, remote_trans, remote_priority, remote_sw_prio, consumer, writeInFlight));
}

/**
 * @brief Method implemented to detect if a received nack is illegal.
 * Could be for several reasons:
 * 1. Received nack with data from a transaction with same or lower
 * priority. This usually imply a cycle in forwarding graphs.
 * Act as cycle solve strategy requires.
 * 2. Received nack for a transaction that has already aborted
 * and its abort was already managed, but in-flight requests are
 * still arriving. Marked as discarded. TODO: May be a good idea
 * to not abort and act in consequence.
 *
 * @param addr
 * @param source
 * @param remote_priority
 * @param remote_sw_priority
 * @return true Data is illegal
 * @return false Data is not illegal
 */
bool
TransactionInterfaceManager::isIllegalNack(Addr addr,
                                           MachineID source,
                                           int remote_priority,
                                           bool remote_sw_priority)
{
    PacketPtr pkt = m_sequencer->getPacketFromRequestTable(addr);
    if ((pkt->getHtmTransactionUid() < m_currentHtmUid ||
         isAborting())) {
        // Not neccessarily only with fwd
        // but with any nack received if
        // policy is nack-based
        //assert(config_allowEarlyValueFwd());
        m_discardFwdData = true;
        return true;
    }
    if (m_htm->getHwPrioMechanism() !=
            HTM::HtmHwPrioMechanism::no_prio &&
        m_htm->getFwdCycleSolveStrategy() !=
            HTM::HtmFwdCycleSolveStrategy::no_strategy &&
        (remote_priority <= m_hwTransPrio ||
         (!config_NCnotProducer() &&
          remote_priority == NC_PRIO_VALUE))) {
        // Note1: local priority must ALWAYS be
        // lower if received a nack, else it would
        // mean that both transactions can commit
        // independently, which is not true (nack
        // imply commit dependency)
        // Note2: Cycles/Illegal Nack reception
        // can happen in any system that requires
        // to serialize transactions with Nacks
        assert(config_allowEarlyValueFwd() ||
            config_conflictResPolicy() ==
            HtmPolicyStrings::requester_stalls_pic_base);
        DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-ILLEGAL-Origin:%d-remote_prio:%s-local_prio:%s\n",
                 curTick(),
                 getProcID(),
                 m_currentHtmUid,
                 machineIDToNodeID(source),
                 remote_priority != NC_PRIO_VALUE ? to_string(remote_priority) : "NC",
                 m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");
        return true;
    }
    return false;
}

/**
 * @brief Method that clarify if speculative data should be accepted
 * by this core, currently in transaction or not. Here, D1 should only
 * send data if is not already a producer.
 *
 * @param addr
 * @param source
 * @param remote_priority
 * @param remote_sw_priority
 * @return true Spec data can be accepted
 * @return false Spec data cannot be accepted
 */
bool
TransactionInterfaceManager::shouldAcceptForwardedDataFromProducer(Addr addr,
                                                                   MachineID source,
                                                                   int remote_priority,
                                                                   bool remote_sw_priority){
    switch (fwdMechanism)
    {
    case HTM::HtmFwdMechanism::simple:
        // Simple fwd mechanism require no priority
        assert(prioMechanism ==
            HTM::HtmHwPrioMechanism::no_prio);
    case HTM::HtmFwdMechanism::W1_D1:
    case HTM::HtmFwdMechanism::WN_D1:
        return isProducer() ? false : true;
    case HTM::HtmFwdMechanism::WX_DX:
    // For D1 behavior should be the same here than other cases
        return config_maxPrioDiff() == 1 && isProducer() ? false : true;
    case HTM::HtmFwdMechanism::W1_DN:
    case HTM::HtmFwdMechanism::WN_DM:
    case HTM::HtmFwdMechanism::naive:
        return true;
    default:
        panic("Not known fwd mechanism received fwd data");
    }
}

/**
 * @brief Get the action to follow on protocol if received nack is illegal
 *
 * @return ActionOnIllegalWriteData The action to follow
 */
ActionOnIllegalWriteData
TransactionInterfaceManager::getActionOnIllegalData()
{
    // Received a nack from a previous transaction
    // and must be discarded
    if (m_discardFwdData) {
        m_discardFwdData = false;
        // Maybe the abort name is a bit misleading
        // but it means that it will just be taken into
        // account as a nack event (failedCallback)
        // which will imply, either a retry or an abort
        // depending on the policy
        // This is conservative but correct, else
        // more logic should be added to handle
        // discard of in flight nacks due to requests
        // from old transactions
        // TODO:
        // return ActionOnIllegalWriteData::
        //     ActionOnIllegalWriteData_Discard;
        return ActionOnIllegalWriteData::
            ActionOnIllegalWriteData_Abort;
    }
    switch (m_htm->getFwdCycleSolveStrategy())
    {
    case HTM::HtmFwdCycleSolveStrategy::retry:
        return ActionOnIllegalWriteData::
            ActionOnIllegalWriteData_Retry;
    case HTM::HtmFwdCycleSolveStrategy::abort:
        return ActionOnIllegalWriteData::
            ActionOnIllegalWriteData_Abort;
    default:
        return ActionOnIllegalWriteData::
            ActionOnIllegalWriteData_Abort;
    }
}

bool
TransactionInterfaceManager::isConsumer()
{
    return !m_recvDataForwardedFromProducer.empty();
}

bool
TransactionInterfaceManager::isProducer()
{
    if (!m_hasSentDataToConsumer.empty()) {
        assert(canProduce());
        assert(!m_sentDataForwardedToConsumer.empty());
        return true;
    } else {
        assert(m_sentDataForwardedToConsumer.empty());
        return false;
    }
}

void
TransactionInterfaceManager::scheduleValidationOfConsumedData(Addr addr, bool retry)
{
    // NOTE: This method is called before hitCallback, so the rw-sets have
    // not yet been updated

    // Create a new req+pkt for validation: current (program) _pkt
    // will be deallocated when the request completes.  The
    // validation packet shall never be sent to the CPU, should be
    // intercepted when validation completes (the block arrives
    // with adequate coherence permissions)
    assert(!validationEvent->scheduled());
    auto it = m_recvDataForwardedFromProducer.find(addr);
    auto it2 = m_allReceivedData.find(addr);
    assert(it != m_recvDataForwardedFromProducer.end());
    assert(it2 != m_allReceivedData.end());
    assert(!isAborting());
    validationEvent->setAddr(addr);
    Cycles timeToValidate;
    if (retry || atCommit() ||
        hasValidatedAnyData()) {
        timeToValidate = Cycles(1);
    } else {
        timeToValidate = Cycles(config_cyclesToValidate());
    }
    schedule(validationEvent,
             clockEdge(timeToValidate));
    DPRINTF(RubyHTMvaluepred, "Scheduled validation event"
            " for addr %#x\n", addr);
}

bool
TransactionInterfaceManager::validateDataBlock(Addr addr,
                                               MachineID source,
                                               DataBlock& cacheData,
                                               const DataBlock& validatedData,
                                               Cycles remote_timestamp,
                                               bool dirty, bool isNack)
{
    assert(config_allowEarlyValueFwd());
    assert(isConsumer());
    map<Addr, DataForwarded>::iterator it = m_recvDataForwardedFromProducer.find(addr);
    auto it2 = m_allReceivedData.find(addr);
    assert(it != m_recvDataForwardedFromProducer.end());
    assert(it2 != m_allReceivedData.end());
    bool validationCheck;
    it->second.nVal++;
    it2->second.timesVal++;
    // Add the producer to the vector
    // if producer changes, we want to
    // know all of them. Set will avoid
    // duplicates
    it2->second.producers.insert(source);
    // Only used if falseSharing removed
    bool removeFalseSharing =
        config_removeFalseSharingForward();
    if (removeFalseSharing || dirty) {
        // Be careful on reference initialization and datablock management
        // = operator is redefined in DataBlock class!
        WriteMask mask = (*it).second.mask;
        DataBlock& consumedData = (*it).second.data;
        if (removeFalseSharing) {
            // Check against masked accesses
            // And do it against buffered data
            validationCheck = consumedData
                .comparePartial(validatedData, mask);
            // Update block if it is not the same
            if (!consumedData.equal(validatedData)) {
                DPRINTF(RubyHTMvaluepred, "HTMvp: Consumer %d received modified block"
                    " for addr %#x but did not abort, False sharing abort avoid\n",
                    getProcID(), addr);
                // Update block in buffer
                consumedData = validatedData;
                if (dirty) {
                    // Update cache block and copy dirty data
                    // into it
                    const DataBlock auxDB(cacheData);
                    cacheData = validatedData;
                    cacheData.copyPartial(auxDB, mask);
                } else {
                    // Just update cache block
                    cacheData = validatedData;
                }
            }
        } else {
            // Check against whole block
            assert(dirty);
            validationCheck = consumedData
                .equal(validatedData);
        }
    } else {
        // Not dirty either remove false sharing
        // check against cache block
        validationCheck = cacheData
            .equal(validatedData);
    }

    // For full naive implementation there is no cycle
    // detection, so, check how many times the block was
    // validated and if a threshold is overcome count it as
    // wrong validation
    validationCheck = fwdMechanism ==
        HTM::HtmFwdMechanism::naive ?
        (validationCheck && it->second.nVal <= config_nValFwd()) :
        validationCheck;
    
    // Any validation failure cause a transaction to abort
    // (reason spec)
    if (!validationCheck) {
        setAbortFlag(addr, createInvalidMachineID(), TransactionBit_NonTrans,
                     NC_PRIO_VALUE,
                     false, false, false, false, true/*spec*/);;
    }
    DPRINTF(RubyHTMvaluepred, "HTMvp: Validating buffered data against"
            " received data (%s): prediction is %s for address %#x\n",
            isNack ? "nack" : "with permissions",
            validationCheck ? "correct" : "incorrect", addr);
    if (!isNack) {
        // Finally, remove from "consumed blocks buffer" if this is
        // not a nack (we got data + coherence permissions)
        assert(isBlockConsumed(addr));
        m_recvDataForwardedFromProducer.erase(addr);
        it2->second.tickValidated = curTick();
        m_hasValidatedAnyData = true;
        DPRINTF(RubyHTMvaluepred, "HTMvp: Consumer %d got data+permissions"
                " for addr %#x (prediction was %s), removed from buffer\n",
                getProcID(), addr,
                validationCheck ? "correct" : "incorrect");
        if (!isConsumer() && !isAborting()) {
            // All forwarded blocks validated correctly
            m_hasConsumedValidData = true;
            m_consumerLowPrio = false;
        } else {
            if (!isAborting()) {
                it++;
                if (it != m_recvDataForwardedFromProducer.end()) {
                    scheduleValidationOfConsumedData(it->first, false);
                } else {
                    scheduleValidationOfConsumedData(
                        m_recvDataForwardedFromProducer
                        .begin()
                        ->first,
                        false);
                }
            }
        }
    } else {
        if (m_xactConflictManager->isRequesterStallsPolicy()) {
            notifyReceiveNack(addr,
                        remote_timestamp,
                        TransactionBit_Trans /* Only tx can forward */,
                        NC_PRIO_VALUE,
                        false,
                        source,
                        true/* unused */);
        }
        if (!isAborting()) {
            it++;
            if (it != m_recvDataForwardedFromProducer.end()) {
                scheduleValidationOfConsumedData(it->first, false);
            } else {
                scheduleValidationOfConsumedData(
                    m_recvDataForwardedFromProducer
                    .begin()
                    ->first,
                    false);
            }
        }

    } // If we got nacked but validation passed, keep block in
              // "consumed buffer" (isBlockConsumed) to repeat
              // validation event scheduling

    return validationCheck;
}

bool TransactionInterfaceManager::isRetryingTx()
{
    return (getXactConflictManager()->getNumRetries() != 0);
}

void
TransactionInterfaceManager::beginTransaction(PacketPtr pkt)
{
    // No nesting in STAMP: assume close nesting (flattening) though
    // support not yet tested
    assert(m_transactionLevel == 0);
    assert(m_escapeLevel == 0);
    assert(!m_atCommit);

    m_transactionLevel++;
    if (m_transactionLevel == 1){
        assert(!m_unrollingLogFlag);
        assert(m_currentHtmUid < pkt->getHtmTransactionUid());
        assert(m_htmXid == std::numeric_limits<long>::min());
        m_currentHtmUid = pkt->getHtmTransactionUid();
        m_htmXid = pkt->getHtmXid();

        m_xactIsolationManager->beginTransaction();
        m_xactConflictManager->beginTransaction(pkt->req->isHTMPower());
        if (config_allowEarlyValueFwd()) {
            assert(!isProducer());
            assert(!isConsumer());
            assert(!hasConsumedData());
            assert(m_sentDataForwardedToConsumer.empty());
            assert(m_allReceivedData.empty());
            assert(m_allSentData.empty());
            assert(!m_consumerTurnedProducer);
            assert(!m_producerTurnedConsumer);
        }
        if (XACT_LAZY_VM) {
            if (XACT_EAGER_CD) {
                // EL system use the L1D cache to store speculative updates
            }
            else {
                m_xactLazyVersionManager->beginTransaction(pkt);
                m_xactLazyCommitArbiter->beginTransaction();
            }
        }
        else { // LogTM
            m_xactEagerVersionManager->beginTransaction();
        }
        if (isPowerMode()) {
            XACT_PROFILER->moveTo(getProcID(),
                              AnnotatedRegion_TRANSACTIONAL_POWER);
        } else {
            XACT_PROFILER->moveTo(getProcID(),
                              AnnotatedRegion_TRANSACTIONAL);
        }

        if (getXactConflictManager()->getNumRetries() == 0) {

        }
        m_htmstart_tick = pkt->req->time();
        m_htmstart_instruction = pkt->req->getInstCount();
        if (isAborting()) {
            DPRINTF(RubyHTM, "HTM: beginTransaction found abort flag set\n");
            XACT_PROFILER->moveTo(getProcID(), AnnotatedRegion_ABORTING);
        }
        if (config_allowEarlyValueFwd()) {
            DPRINTFR(FwdGraphTrace, "%d-%d-%d-BEGIN\n",
                        curTick(),
                        getProcID(),
                        m_currentHtmUid);
            DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-BEGIN\n",
                        curTick(),
                        getProcID(),
                        m_currentHtmUid);
            assert(m_hwTransPrio == NC_PRIO_VALUE);
        }
    }
    DPRINTF(RubyHTM, "HTM: beginTransaction "
            "xact_level=%d \n",  m_transactionLevel);
    Addr pcInst {pkt->req->hasPC() ? pkt->req->getPC() : 0};
    DPRINTF(HTMInstTracer,
            "BeginTransaction - Xid: %ld - "
            "uid: %llu - address-(pkt)=%#x"
            " PC=%#x (%d)\n",
            m_htmXid, m_currentHtmUid,
            pkt->getAddr(),
            pcInst, pcInst);
}

bool
TransactionInterfaceManager::canCommitTransaction(PacketPtr pkt) const
{
    if (XACT_EAGER_CD) {
        if (config_allowEarlyValueFwd()) {
            if (m_abortFlag) {
                return true; // Signal abort during validation
            } else {
                return m_recvDataForwardedFromProducer.empty();
            }
        }
        return true;
    } else {
        if (m_xactLazyVersionManager->committed()) {
            return true;
        } else if (m_xactLazyVersionManager->committing()) {
            return false;
        } else if (m_xactLazyCommitArbiter->shouldValidateTransaction()) {
            if (m_abortFlag) {
                return true; // Signal abort during validation
            } else if (m_xactLazyCommitArbiter->validated()) {
                return false;
            } else { // Pending validation
                return false;
            }
        } else { // No validation required, but not committing yet
            return false;
        }
    }
}

void
TransactionInterfaceManager::initiateCommitTransaction(PacketPtr pkt)
{
    if (config_allowEarlyValueFwd()) {
        assert(XACT_EAGER_CD);
        if (!m_atCommit) {
            m_atCommit = true;
            XACT_PROFILER->moveTo(getProcID(),
                        AnnotatedRegion_COMMITTING); // TODO: make another region
        }
        return;
    }
    assert(!XACT_EAGER_CD);
    m_atCommit = true;
    if (!m_xactLazyVersionManager->committed()) {
        if (m_xactLazyCommitArbiter->shouldValidateTransaction() &&
            !m_xactLazyCommitArbiter->validated()) {
            XACT_PROFILER->moveTo(getProcID(),
                                  AnnotatedRegion_ARBITRATION);
            // If validation required, initiate actions to determine
            // if this transaction can commit
            m_xactLazyCommitArbiter->
                initiateValidateTransaction();
        } else {
            if (!getXactLazyVersionManager()->committing()) {
                XACT_PROFILER->moveTo(getProcID(),
                                      AnnotatedRegion_COMMITTING);
            }
            // Arbitration passed, or not required (best-effort):
            // initiate commit actions
            m_xactLazyVersionManager->commitTransaction();
        }
    }
}

void TransactionInterfaceManager::commitTransaction(PacketPtr pkt)
{
    if (m_transactionLevel < 1) {
        DPRINTF(RubyHTM, "HTM: ERROR NOT IN XACT! commitTransaction "
                         "xact_level=%d\n",
                m_transactionLevel);
        panic("HTM: Error not inside a transaction, cannot commit!");
    }

    assert(m_transactionLevel >= 1);
    assert(!m_abortFlag);
    assert(m_htmXid != std::numeric_limits<long>::min());

    if (m_transactionLevel == 1) { // Outermost commit

        /* EL SYSTEM: L1D cache is used for lazy versioning of speculative data.
         * Conflicts were resolved eagerly as each individual store acquires
         * exclusive ownership before it completes, so commit can happen instantly.
         */

        if (!m_atCommit) {
            // Move to committing unless we have already done so
            XACT_PROFILER->moveTo(getProcID(),
                                  AnnotatedRegion_COMMITTING);
        }

        if (config_enableValueChecker()) {
            m_ruby_system->getXactValueChecker()->commitTransaction(getProcID(), this, m_dataCache_ptr);
        }

        if (!config_allowReadSetL0CacheEvictions()) {
            // Sanity checks: All Rset blocks must be cached at commit
            vector<Addr> *rset = getXactIsolationManager()->getReadSet();
            for (int i = 0; i < rset->size(); i++)
            {
                // Must have read access permissions
                Addr addr = rset->at(i);
                _unused(addr);
                // TODO assert(m_dataCache_ptr->isTagPresentPermissions(addr));
            }
            _unused(rset);
            delete rset; // Caller must delete this
        }
        if (XACT_LAZY_VM) {
            if (XACT_EAGER_CD) {
                // EL system use the L1D cache to store speculative
                // updates: nothing to do, write set (SM bits) cleared
                // by isolation manager below
                if (!m_nackedAddressesReg.empty()) {
                    bool checkMod = false;
                    for (map<Addr, DataBlock>::iterator it =
                             m_nackedAddressesReg.begin();
                         it != m_nackedAddressesReg.end() && !checkMod;
                         ++it) {
                        Addr addr = (*it).first;
                        DataBlock db = (*it).second;
                        // Check if it has changed
                        DataBlock *datablock_ptr;
                        // must find write hit for all lines in the write buffer
                        bool hit = m_dataCache_ptr->tryCacheAccess(makeLineAddress(addr),
                                                                   RubyRequestType_ST,
                                                                   datablock_ptr, false);
                        if (!hit) {
                            hit = m_dataCache_ptr->tryCacheAccess(addr,
                                                                  RubyRequestType_LD,
                                                                  datablock_ptr, false);
                        }
                        if (!hit) {
                            // This point should be reached?
                            // Maybe if nack sent to writer,
                            // if this is the case, count it
                            // as could have been forwarded
                            m_htm_transaction_power_modified[0]++;
                        }
                        else {
                            assert(hit);
                            if (!datablock_ptr->equal(db)) {
                                checkMod = true;
                            }
                        }
                    }

                    if (checkMod) {
                        m_htm_transaction_power_modified[1]++;
                    }
                    else {
                        m_htm_transaction_power_modified[0]++;
                    }
                    m_nackedAddressesReg.clear();
                }
                profileHtmFwdTransaction(false, 0);
                m_htm_executed_tx_xid.sample(m_htmXid);
                m_htm_committed_tx_xid.sample(m_htmXid);
                if (config_allowEarlyValueFwd())
                {
                    // All consumed blocks must have been validated
                    DPRINTFR(FwdGraphTrace, "%d-%d-%d-COMMIT-%s\n",
                             curTick(),
                             getProcID(),
                             m_currentHtmUid,
                             m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");
                    DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-COMMIT-Prio:%s\n",
                             curTick(),
                             getProcID(),
                             m_currentHtmUid,
                             m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC");
                    assert(!isConsumer());
                    if (m_hwTransPrio != NC_PRIO_VALUE)
                    {
                        m_htm_PiC_committed.sample(m_hwTransPrio);
                    }
                    if (hasConsumedData())
                    {
                        DPRINTF(RubyHTMvaluepred, "HTMvp: %sConsumer commits"
                                                  " - producer was %d\n",
                                isProducer() ? "Producer-" : "",
                                machineIDToNodeID(m_hasReceivedDataFromProducer));
                    }
                    m_atCommit = false; // Reset
                    if (isProducer())
                    {
                        std::string consumersString = "[";
                        for (auto it : m_hasSentDataToConsumer)
                        {
                            consumersString += to_string(machineIDToNodeID(it)) + " ";
                        }
                        consumersString += "]";
                        DPRINTF(RubyHTMvaluepred, "HTMvp: Producer aborts"
                                                  " - consumers were: %s\n",
                                consumersString);
                        // Iterate over "sent data" to check whether
                        // it has changed since it was forwarded (profiling)
                        for (map<Addr, DataBlock>::iterator it =
                                 m_sentDataForwardedToConsumer.begin();
                             it != m_sentDataForwardedToConsumer.end();
                             ++it)
                        {
                            Addr addr = (*it).first;
                            DataBlock db = (*it).second;
                            // Check if it has changed
                            DataBlock *datablock_ptr;
                            // must find write hit for all lines in the write buffer
                            bool hit = m_dataCache_ptr->tryCacheAccess(makeLineAddress(addr),
                                                                       RubyRequestType_ST,
                                                                       datablock_ptr, false);
                            if (!hit)
                            {
                                DPRINTF(RubyHTMvaluepred, "HTMvp: producer was reader"
                                                          " of consumed written data address %#x\n",
                                        addr);
                                hit = m_dataCache_ptr->tryCacheAccess(addr,
                                                                      RubyRequestType_LD,
                                                                      datablock_ptr, false);
                            }
                            if (!hit)
                            {
                                // This will cause some performance issues
                                // Consumer will be aborted from this eviction
                                // as it will get nacked on the next validation
                                // TODO: make this data stay in L0?
                                warn("Produced data from read-set evicted from L0"
                                     " but still in read-set and S state");
                            }
                            else
                            {
                                assert(hit);
                                if (datablock_ptr->equal(db))
                                {
                                    DPRINTF(RubyHTMvaluepred, "HTMvp: (Power tx) commits"
                                                              " consumed value, unmodified since "
                                                              " last conflict - addr %#x\n",
                                            addr);
                                }
                                else
                                {
                                    DPRINTF(RubyHTMvaluepred, "HTMvp: (Power tx) commits"
                                                              " consumed value, changed since "
                                                              " last conflict - addr %#x\n",
                                            addr);
                                    DPRINTF(RubyHTMvaluepred, "HTMvp: (Power tx) diff"
                                                              " %s \n",
                                            datablock_ptr->diff(db));
                                    DPRINTF(RubyHTMvaluepred, "HTMvp: (Power tx) old "
                                                              " %s \n",
                                            db.toString());
                                }
                            }
                        }
                    }
                    // Reset
                    m_sentDataForwardedToConsumer.clear();
                    m_hasReceivedDataFromProducer = createInvalidMachineID();
                    m_hasSentDataToConsumer.clear();
                    m_allReceivedData.clear();
                    m_allSentData.clear();
                    m_consumerTurnedProducer = false;
                    m_producerTurnedConsumer = false;
                    m_hasConsumedValidData = false;
                    m_hasValidatedAnyData = false;
                    m_dataConsumedFromPower = false;
                    m_consumerLowPrio = false;
                    m_reconsumer = false;
                    m_currentValidationAddress = 0;
                    assert(!validationEvent->scheduled());
                }
            }
            else
            {
                // Clear committing/committed flags, sanity checks
                assert(m_atCommit);
                m_xactLazyVersionManager->notifyCommittedTransaction();
                m_xactLazyCommitArbiter->commitTransaction();
                m_atCommit = false; // Reset
            }
        }
        else
        {
            m_xactEagerVersionManager->commitTransaction();
            m_dataCache_ptr->checkHtmLogPendingClear();
        }
        m_xactConflictManager->commitTransaction();
        m_xactIsolationManager->commitTransaction();
        if (config_enableIsolationChecker())
        {
            m_ruby_system->getXactIsolationChecker()->clearReadSet(m_version);
            m_ruby_system->getXactIsolationChecker()->clearWriteSet(m_version);
        }
        assert(m_writeSetDiscarded.empty());
        assert(m_abortCause == HTMStats::AbortCause::Undefined);
        m_lastFailureCause = HtmFailureFaultCause::INVALID;
        m_hwTransPrio = NC_PRIO_VALUE;

        DPRINTF(RubyHTM, "HTM: commitTransaction "
                         "xact_level=%d\n",
                m_transactionLevel);
        Addr pcInst {pkt->req->hasPC() ? pkt->req->getPC() : 0};
        DPRINTF(HTMInstTracer,
            "CommitTransaction - Xid: %ld - "
            "uid: %llu - address-(pkt)=%#x"
            " PC=%#x (%d)\n",
            m_htmXid, m_currentHtmUid,
            pkt->getAddr(),
            pcInst, pcInst);
        Tick transaction_ticks = pkt->req->time() - m_htmstart_tick;
        Cycles transaction_cycles = ticksToCycles(transaction_ticks);
        m_htm_transaction_cycles.sample(transaction_cycles);
        m_htm_transaction_commit_cycles.sample(transaction_cycles);
        m_htm_cycles_xid.sample(m_htmXid, transaction_cycles);
        m_htm_committed_cycles_xid.sample(m_htmXid, transaction_cycles);
        while (!m_retriesTicks.empty())
        {
            double percentageExecuted =
                ((double)m_retriesTicks.top() / (double)transaction_ticks) * 100.0;
            m_htm_percentage_executed_on_abort.sample(percentageExecuted);
            m_retriesTicks.pop();
        }

        m_htmstart_tick = 0;
        Counter transaction_instructions =
            pkt->req->getInstCount() - m_htmstart_instruction;
        m_htm_transaction_instructions.sample(
            transaction_instructions);
        m_htmstart_instruction = 0;
    }
    m_htmXid = std::numeric_limits<long>::min();
    m_transactionLevel--;
    XACT_PROFILER->moveTo(getProcID(),
                          AnnotatedRegion_DEFAULT);
}

void TransactionInterfaceManager::profileHtmFwdTransaction(
    bool aborted, int abort_cause_idx) {
    // assert(config_allowEarlyValueFwd());
    // Producer stats
    bool hasModifiedBlock = false;
    bool producerForModifiedBlock = false;
    // Profile all sent data to consumers
    std::set<MachineID> consumers;
    Tick firstConsumedTick = MaxTick;
    // Consumer stats
    bool hasModifiedConsumedBlock = false;
    std::set<MachineID> producers;
    // Prepare producer stats to be profiled
    if (isProducer()) {
        for (auto it : m_allSentData) {
            // Any produced block has been modified
            hasModifiedBlock =
                hasModifiedBlock || it.second.timesModifiedAfterSharing > 0;
            // Check which transactions have consumed
            consumers.insert(it.second.consumers.begin(),
                             it.second.consumers.end());
            // Sample how many times a block has been modified after
            // sharing
            m_htm_producer_n_modified_produced_data.sample(
                it.second.timesModifiedAfterSharing);
            // Transaction that produced a consumed data block
            producerForModifiedBlock =
                producerForModifiedBlock ||
                (m_allReceivedData.find(it.first) != m_allReceivedData.end());
            // Profile all forwardings here
            // Profile from the end and treat the first
            // element as the last forwarding made
            auto it2 = it.second.forwardings.rbegin();
            int i;
            if (aborted) {
                for (i = 0; i < it2->consumers.size(); i++) {
                    // Forwardings from aborted transaction
                    m_htm_forwardings[2]++;
                }
            } else {
                for (i = 0; i < it2->consumers.size(); i++) {
                    // Forwardings that are modified or not
                    it2->modified ? m_htm_forwardings[1]++
                                  : m_htm_forwardings[0]++;
                }
            }
            it2++; // Skip last element
            for (; it2 != it.second.forwardings.rend(); it2++) {
                // Rest must be modified forwardings
                assert(it2->modified);
                for (i = 0; i < it2->consumers.size(); i++) {
                    m_htm_forwardings[1]++;
                }
            }
        }
    }
    // Prepare consumer stats to be profiled
    if (hasConsumedData()) {
        // Consumer transactions MUST have validated
        // all its data
        assert((isConsumer() && aborted) || !isConsumer());
        // Profile all received data from producers
        for (auto it : m_allReceivedData) {
            // Check when did it consume the first block
            firstConsumedTick = std::min(firstConsumedTick,
                                         it.second.tickConsumed);
            // Consumer modified any received block
            hasModifiedConsumedBlock =
                hasModifiedConsumedBlock || it.second.locallyModified;
            // Insert all elements from it->second.producers into
            // producers set
            producers.insert(it.second.producers.begin(),
                             it.second.producers.end());
        }
    }

    // Producer common stats profiling
    if (isProducer()) {
        // Profile the amount of consumers this tx had
        m_htm_producer_transactions_nprod_commit.sample(consumers.size());
        if (producerForModifiedBlock) {
            assert(hasConsumedData() && isProducer());
            // Tx that produced the consumed block
            m_htm_transactions_produced_consumed_same++;
        }
    }
    // Consumer common stats profiling
    if (hasConsumedData()) {
        // Sample all consumed blocks
        if (aborted) {
            m_htm_consumer_transactions_nprod_abort.sample(producers.size());
            m_htm_delayed_time_fwd_on_abort.sample(
                ticksToCycles(curTick() - firstConsumedTick));
        } else {
            m_htm_consumer_transactions_nprod_commit.sample(producers.size());
        }
    }
    // Other tx common stats
    if (m_reconsumer) {
        if (aborted) {
            m_htm_reconsumer_tx[abort_cause_idx + 1]++;
        } else {
            m_htm_reconsumer_tx[0]++;
        }
    }

    // Specific stats for fwd
    if (isPowerMode()) {
        if (hasConsumedData()) {
            panic("Power transaction cannot be consumer\n");
        }
        // Power
        if (isProducer()) {
            // Producer power
            m_htm_transaction_executed_type_rate[2 * 5 + (aborted ? 1 : 0)]++;
            // Check if producer has modified data
            if (aborted) {
                // Set abort cause
                m_htm_powered_tx[abort_cause_idx + 3]++;
            } else if (hasModifiedBlock) {
                m_htm_powered_tx[2]++;
            } else {
                m_htm_powered_tx[1]++;
            }
        } else {
            // Common power
            m_htm_powered_tx[0]++;
            m_htm_transaction_executed_type_rate[2 * 4 + (aborted ? 1 : 0)]++;
        }
    } else {
        if (isProducer() && hasConsumedData()) {
            // Consumer-Producer
            m_htm_transaction_executed_type_rate[2 * 3 + (aborted ? 1 : 0)]++;
            if (aborted) {
                // Set abort cause
                m_htm_producers_consumers_tx[abort_cause_idx + 2]++;
            } else if (hasModifiedBlock) {
                m_htm_producers_consumers_tx[1]++;
            } else {
                m_htm_producers_consumers_tx[0]++;
            }

            // Sample if transaction became consumer or viceversa
            if (m_producerTurnedConsumer) {
                assert(!m_consumerTurnedProducer);
                m_htm_producer_consumer_transactions[0]++;
            } else if (m_consumerTurnedProducer) {
                assert(!m_producerTurnedConsumer);
                m_htm_consumer_producer_transactions[0]++;
            } else {
                panic("Logic fault in Producer-Consumer!\n");
            }
        } else if (!isProducer() && hasConsumedData()) {
            // Consumer
            m_htm_transaction_executed_type_rate[2 * 2 + (aborted ? 1 : 0)]++;
            if (aborted) {
                // Set abort cause
                m_htm_consumers_tx[abort_cause_idx + 2]++;
            } else if (hasModifiedConsumedBlock) {
                m_htm_consumers_tx[1]++;
            } else {
                m_htm_consumers_tx[0]++;
            }
        } else if (isProducer() && !hasConsumedData()) {
            // Producer
            m_htm_transaction_executed_type_rate[2 * 1 + (aborted ? 1 : 0)]++;
            if (aborted) {
                // Set abort cause
                m_htm_producers_tx[abort_cause_idx + 2]++;
            } else if (hasModifiedBlock) {
                m_htm_producers_tx[1]++;
            } else {
                m_htm_producers_tx[0]++;
            }
        } else {
            // Common transaction
            m_htm_transaction_executed_type_rate[2 * 0 + (aborted ? 1 : 0)]++;
        }
    }
}

void
TransactionInterfaceManager::discardWriteSetFromL1DataCache() {
    vector<Addr> *wset = getXactIsolationManager()->
        getWriteSet();

    for (int i=0; i < wset->size(); i++) {
        Addr addr=wset->at(i);
        // Must find all write-set blocks in cache, except perhaps
        // those addresses that have already been discarded before
        // abort completed (including those that triggered it)
        bool discarded = m_writeSetDiscarded.erase(addr);
        // In fact, it must find all blocks with write permissions
        DataBlock* datablock_ptr;
        // must find write hit for all lines in the write buffer
        bool hit = m_dataCache_ptr->
            tryCacheAccess(makeLineAddress(addr),
                           RubyRequestType_ST,
                           datablock_ptr, false);
        if (discarded) {
            assert(!hit);
        } else {
            assert(hit);
            m_dataCache_ptr->deallocate(addr);
            DPRINTF(RubyHTM, "HTM: %d deallocated L1DCache address %#x\n",
                    m_version, addr);
        }
        _unused(hit);
    }
    delete wset;
    assert(m_writeSetDiscarded.empty());
}

void
TransactionInterfaceManager::discardForwardedDataFromL1DataCache() {
    for (map<Addr, DataForwarded>::iterator it =
             m_recvDataForwardedFromProducer.begin();
         it != m_recvDataForwardedFromProducer.end();
         ++it) {
        Addr addr =(*it).first;
        // In fact, it must find all blocks with write permissions
        DataBlock* datablock_ptr;
        // must find write hit for all lines in the write buffer
        bool hit = m_dataCache_ptr->
            tryCacheAccess(makeLineAddress(addr),
                           RubyRequestType_LD,
                           datablock_ptr, false);
        assert(hit);
        m_dataCache_ptr->deallocate(addr);
        DPRINTF(RubyHTM, "HTM: %d deallocated L1DCache address %#x (forwarded data)\n",
                m_version, addr);
        _unused(hit);
        if (checkWriteSignature(addr)) {
            // Add it to the following map in order to pass sanity
            // checks when discarding Wset blocks
            m_writeSetDiscarded.
                insert(std::pair<Addr,char>(addr, 'y'));
        }
    }
}

void
TransactionInterfaceManager::abortTransaction(PacketPtr pkt){
    /* RT: Called when Ruby receives a XactionAbort packet from the CPU.
     * The abort may have been initiated by Ruby via the setAbortFlag method,
     * or it may have been directly triggered by the CPU via txAbort instruction.
     * NOTE: This method shall NOT be used to signal an abort: use setAbortFlag.
     */
    assert(m_transactionLevel == 1);
    assert(m_escapeLevel == 0);
    assert(m_htmXid != std::numeric_limits<long>::min());

    // Profile before discarding speculative state so that we can
    // perform some sanity checks on read-write sets, etc.
    HtmFailureFaultCause cause = pkt->req->getHtmAbortCause();
    profileHtmFailureFaultCause(cause);
    Addr pcAbort {pkt->req->hasPC() ?
                pkt->req->getPC() :
                0};
    DPRINTF(HTMInstTracer,
            "AbortTransaction - Xid: %ld - "
            "uid: %llu - cause=%s - address-(pkt)=%#x"
            " PC=%#x (%d)\n",
            m_htmXid, m_currentHtmUid,
            HTMStats::AbortCause_to_string(m_abortCause),
            pkt->getAddr(),
            pcAbort, pcAbort);

    if (XACT_LAZY_VM) {
        if (config_enableValueChecker()) {
            // Lazy systems can perform value checks immediately
            m_ruby_system->getXactValueChecker()->
                restartTransaction(getProcID());
        }
        if (XACT_EAGER_CD) {
            if (isPowerMode()) {
                if (!m_nackedAddressesReg.empty()) {
                    m_htm_transaction_power_modified[2]++;
                    m_nackedAddressesReg.clear();
                }
                warn("WARNING! Power transaction aborted! \n");
                DPRINTF(RubyHTM, "WARNING! Power transaction aborted! \n");
            }
            if (config_allowEarlyValueFwd()) {
                if (m_hwTransPrio != NC_PRIO_VALUE)
                {
                    m_htm_PiC_aborted.sample(m_hwTransPrio);
                }
                DPRINTFR(FwdGraphTrace, "%d-%d-%d-ABORT-%s-%s\n",
                        curTick(),
                        getProcID(),
                        m_currentHtmUid,
                        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC",
                        HTMStats::AbortCause_to_string(m_abortCause));
                DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-ABORT-Prio:%d-Cause:%s\n",
                        curTick(),
                        getProcID(),
                        m_currentHtmUid,
                        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC",
                        HTMStats::AbortCause_to_string(m_abortCause));
                // Profile stats for producer aborts
                if (isProducer()) {
                    std::string consumersString = "[";
                    for (auto it : m_hasSentDataToConsumer) {
                        consumersString += to_string(machineIDToNodeID(it)) + " ";
                    }
                    consumersString += "]";
                    DPRINTF(RubyHTMvaluepred, "HTMvp: Producer aborts"
                        " - consumers were: %s\n",
                        consumersString);
                    DPRINTF(RubyHTM, "WARNING! Producer aborted! \n");
                    // Profile stats for producer aborts
                    std::set<MachineID> consumers;
                    for (auto it : m_allSentData) {
                        consumers.insert(it.second.consumers.begin(), it.second.consumers.end());
                    }
                    m_htm_producer_transactions_nprod_abort.sample(consumers.size());
                }
                if (hasConsumedValidData()) {
                    DPRINTF(RubyHTM, "WARNING! Power consumer transaction aborted! \n");

                }
                if (hasConsumedData()) {
                    DPRINTF(RubyHTMvaluepred, "HTMvp: Consumer aborts"
                            " cause is %s - producer was %d\n",
                            HTMStats::AbortCause_to_string(m_abortCause),
                            machineIDToNodeID(m_hasReceivedDataFromProducer));
                }
                discardForwardedDataFromL1DataCache();
                // Reset
                m_hasReceivedDataFromProducer = createInvalidMachineID();
                m_hasSentDataToConsumer.clear();
                m_sentDataForwardedToConsumer.clear();
                m_recvDataForwardedFromProducer.clear();
                m_allReceivedData.clear();
                m_allSentData.clear();
                m_consumerTurnedProducer = false;
                m_producerTurnedConsumer = false;
                m_hasConsumedValidData = false;
                m_hasValidatedAnyData = false;
                m_dataConsumedFromPower = false;
                m_consumerLowPrio = false;
                m_reconsumer = false;
                if (validationEvent->scheduled()) {
                    deschedule(validationEvent);
                }
            }
            m_hwTransPrio = NC_PRIO_VALUE;

            discardWriteSetFromL1DataCache();
            if (m_atCommit) {
                assert(config_allowEarlyValueFwd());
                m_atCommit = false;
                DPRINTF(RubyHTM, "WARNING! Aborting transaction at commit! \n");
            }
        }
        else {
            if (m_atCommit) {
                if (getXactLazyVersionManager()->committing()) {
                    if (!m_abortFlag) { // If CPU-triggered abort
                        // cancel pending writes, if any left
                        getXactLazyVersionManager()->
                            cancelWriteBufferFlush();
                    }
                    if (m_xactLazyVersionManager->committed()) {
                        DPRINTF(RubyHTM, "Aborted after write buffer"
                                " completely flushed \n");
                    }
                    // Discard cache lines already written during
                    // write buffer flush
                    discardWriteSetFromL1DataCache();
                } else {
                    if (m_xactLazyCommitArbiter->validated()) {
                        // Only admitted cause of a abort for already
                        // validated transaction are:

                        // a) acquisition of fallback lock by
                        // non-transactional thread,
                        // b) non-transactional conflicting access
                        // c) Capacity abort
                        // TODO: will fail for L0/L1 capacity aborts
                        // (written blocks not part of the read set)
                        if (m_abortCause ==
                            HTMStats::AbortCause::FallbackLock) {
                            // Remote killer may be transactional if
                            // reader mistaken for writer (downgrade
                            // on L1 GETS disabled)
                            assert(m_abortSourceNonTransactional ||
                                   !RubySystem::enableL0DowngradeOnL1Gets());
                        } else if ((m_abortCause ==
                                   HTMStats::AbortCause::Conflict) ||
                                   (m_abortCause ==
                                    HTMStats::AbortCause::ConflictStale)) {
                            assert(m_htm->params().conflict_resolution ==
                                   HtmPolicyStrings::requester_wins);
                            //assert(m_abortSourceNonTransactional);
                        } else if (m_abortCause ==
                                   HTMStats::AbortCause::L2Capacity) {
                        } else if (m_abortCause ==
                                   HTMStats::AbortCause::L1Capacity) {
                        } else if (m_abortCause ==
                                   HTMStats::AbortCause::L0Capacity) {
                            assert(m_capacityAbortWriteSet ||
                                   !m_htm->params().
                                   allow_read_set_l0_cache_evictions);
                        } else if (m_abortCause ==
                                   HTMStats::AbortCause::Undefined) {
                            // CPU-triggered abort
                        } else {
                            panic("Unexpected abort cause for"
                                  " validated transaction\n");
                        }
                    } else {
                        assert(m_xactLazyCommitArbiter->validating());
                    }
                }
                m_atCommit = false;
            }
            m_xactLazyVersionManager->restartTransaction();
            m_xactLazyCommitArbiter->restartTransaction();
        }
        // Profile the time spent in an aborted transaction
        Tick transaction_ticks = pkt->req->time() - m_htmstart_tick;
        Cycles transaction_cycles = ticksToCycles(transaction_ticks);
        m_retriesTicks.push(transaction_ticks);
        m_htm_transaction_cycles.sample(transaction_cycles);
        m_htm_transaction_abort_cycles.sample(transaction_cycles);
        m_htm_cycles_xid.sample(m_htmXid, transaction_cycles);
        m_htm_aborted_cycles_xid.sample(m_htmXid, transaction_cycles);
        // Restart conflict management
        getXactConflictManager()->restartTransaction();
        m_htmXid = std::numeric_limits<long>::min();
    }
    else {
        // LogTM: we need to pass log size to the abort handler (as
        // part of the abort status): do not reset log state now, wait
        // until log unroll done (reset via endLogUnroll)
    }

    if (XACT_LAZY_VM) {
        // Profiling for isolation
        int m_htm_accessed_mem = getXactIsolationManager()->
                                        getReadSetSize() +
                                        getXactIsolationManager()->
                                        getWriteSetSize();
        m_htm_accessed_mem_abort.sample(m_htm_accessed_mem);
        // Release isolation (clear filters/signatures)
        getXactIsolationManager()->releaseIsolation();
        if (config_enableIsolationChecker()) {
            m_ruby_system->getXactIsolationChecker()->
                clearReadSet(m_version);
            m_ruby_system->getXactIsolationChecker()->
                clearWriteSet(m_version);
        }
    }
    else { // LogTM
        if (m_xactEagerVersionManager->getLogNumEntries() == 0) {
            // No log unroll required: abort completes now

            // Release isolation (clear filters/signatures)
            getXactIsolationManager()->releaseIsolation();
            if (config_enableIsolationChecker()) {
                m_ruby_system->getXactIsolationChecker()->
                    clearReadSet(m_version);
                m_ruby_system->getXactIsolationChecker()->
                    clearWriteSet(m_version);
            }
            // Reset log num entries
            m_xactEagerVersionManager->restartTransaction();
            // Restart conflict management
            getXactConflictManager()->restartTransaction();
        } else {
            // Only release isolation over read set
            getXactIsolationManager()->releaseReadIsolation();
            if (config_enableIsolationChecker()) {
                m_ruby_system->getXactIsolationChecker()->
                    clearReadSet(m_version);
            }
            int wsetsize = getXactIsolationManager()->
                getWriteSetSize();
            int logsize =m_xactEagerVersionManager->getLogNumEntries();
            if (wsetsize != logsize) {
                // It is possible that a logged trans store does not
                // complete after its target block has been added to the
                // log. The following assert may fail in non-TSO since
                // more than one of such pending but already logged stores
                // exists upon abort
                assert(wsetsize+1 == logsize);
            }

            // Keep detecting conflicts on Wset despite xact_level being
            // 0. We could use escape actions, but then we would need to
            // leave xact level > 0 until we get the "endLogUnroll" signal
            m_unrollingLogFlag = true;
            // All log unroll accesses will be escaped (not marked as
            // transactional)
            m_escapeLevel = 1;
            // Leaves xact level to 1 until log unrolled in order to
            // detect conflicts on Wset
            assert(m_transactionLevel == 1);
        }
    }

    if (!XACT_EAGER_CD && m_atCommit) {
        // aborting lazy transaction that has reached commit
        if (m_xactLazyCommitArbiter->validated()) {
            assert(m_xactLazyCommitArbiter->shouldValidateTransaction());
            assert(config_conflictResPolicy() !=
                   HtmPolicyStrings::committer_wins);
        } else if (m_xactLazyCommitArbiter->shouldValidateTransaction()) {
            // Abort before tx validated
            DPRINTF(RubyHTM, "Abort before lazy validation passed\n");
        } else {
            // No validation performed (best-effort lazy commit)
            panic("Best-effort lazy commit not tested!\n");
        }
    } else {
        if (!config_allowEarlyValueFwd()) {
            assert(!m_atCommit);
        }

    }

    // Update transaction level unless going into log unroll
    if (!m_unrollingLogFlag) {
        m_transactionLevel = 0;
    } else {
        assert(!XACT_LAZY_VM); // LogTM
        assert(m_transactionLevel == 1);
        assert(m_xactEagerVersionManager->getLogNumEntries() > 0);
    }

    if (m_abortFlag) {
        // This abort was triggered from ruby (conflict/overflow)
        m_abortFlag = false; // Reset
        m_abortCause = HTMStats::AbortCause::Undefined;
        m_abortAddress = Addr(0);
    } else {
        // CPU-triggered abort (fault, interrupt, lsq conflict)
        XACT_PROFILER->moveTo(getProcID(), AnnotatedRegion_ABORTING);
    }
    XACT_PROFILER->moveTo(getProcID(), AnnotatedRegion_ABORT_HANDLER);
}

int
TransactionInterfaceManager::getTransactionLevel(){
    return m_transactionLevel;
}

TransactionBit
TransactionInterfaceManager::getTransactionBit() {
    if (inTransaction())
    {
        return TransactionBit_Trans;
    }
    else
    {
        return TransactionBit_NonTrans;
    }
}

int
TransactionInterfaceManager::getTransactionPriority()
{
    switch (prioMechanism)
    {
    case HTM::HtmHwPrioMechanism::no_prio:
        return NC_PRIO_VALUE;
    case HTM::HtmHwPrioMechanism::numeric:
        return m_hwTransPrio;
    default:
        panic("Unrecognized priority mechanism");
        return 0;
    }
}

uint64_t
TransactionInterfaceManager::getCurrentHtmTransactionUid() const
{
    return m_currentHtmUid;
}

bool
TransactionInterfaceManager::inTransaction(){
    return (m_transactionLevel > 0 && m_escapeLevel == 0);
}

Addr
TransactionInterfaceManager::getAbortAddress(){
    assert(m_abortAddress != Addr(0));
    return m_abortAddress;
}


void
TransactionInterfaceManager::isolateTransactionLoad(Addr addr){
    // Ignore transaction level (may be 0) since trans loads may
    // overtake xbegin in O3CPU if not using precise read set tracking
    // (loads isolated as soon as issued by sequencer)
    if (m_transactionLevel == 0) {
        assert(!m_htm->params().precise_read_set_tracking);
    } else {
        // Nesting not tested
        assert(m_transactionLevel == 1);
    }

    Addr physicalAddr = makeLineAddress(addr);

    m_xactIsolationManager->
        addToReadSetPerfectFilter(physicalAddr); // default TL is 1

    DPRINTF(RubyHTMverbose, "isolateTransactionLoad "
            "address %#x\n", physicalAddr);
}

void
TransactionInterfaceManager::addToRetiredReadSet(Addr addr){
    assert(m_transactionLevel == 1);
    Addr physicalAddr = makeLineAddress(addr);
    m_xactIsolationManager->
        addToRetiredReadSet(physicalAddr);
    DPRINTF(RubyHTMverbose, "retiredTransactionLoad "
            "address %#x\n", physicalAddr);

    if (config_enableIsolationChecker()) {
        m_ruby_system->getXactIsolationChecker()->
            addToReadSet(m_version,
                         physicalAddr);
    }
}

bool
TransactionInterfaceManager::inRetiredReadSet(Addr addr)
{
    return m_xactIsolationManager->
        inRetiredReadSet(makeLineAddress(addr));
}

void
TransactionInterfaceManager::profileTransactionAccess(bool miss, bool isWrite,
                                                      MachineType respMach,
                                                      Addr addr,
                                                      Addr pc, int bytes)
{
}


void
TransactionInterfaceManager::isolateTransactionStore(Addr addr){
    assert(m_transactionLevel > 0);

    Addr physicalAddr = makeLineAddress(addr);

    m_xactIsolationManager->
        addToWriteSetPerfectFilter(physicalAddr);
    if (config_enableIsolationChecker()) {
        m_ruby_system->getXactIsolationChecker()->
            addToWriteSet(m_version, physicalAddr);
    }
    DPRINTF(RubyHTMverbose, "HTM: isolateTransactionStore "
            "address %#x\n", physicalAddr);
}

bool
TransactionInterfaceManager::config_isReqLosesPolicy() {
    return getXactConflictManager()->isReqLosesPolicy();
}

bool
TransactionInterfaceManager::isPowerMode() {
    return getXactConflictManager()->isPowered();
}

bool
TransactionInterfaceManager::getSwTransactionPriority() {
    // Check if it is in the software graph
    return isPowerMode();
}

bool
TransactionInterfaceManager::shouldReloadIfStale() {
    if (inTransaction())
    {
        int transactionState =
            getTransactionPriority();
        // These are the highest priority transactions
        // in case of stale, retry as these could be
        // aborted by lowest priority transactions
        // and could even due to a race condition
        // in the protocol, very common in req-loses
        if (transactionState > NC_PRIO_VALUE)
        {
            // If transaction state is higher than 0
            // means it is not an standard transaction
            return true;
        }
    }
    // Else just check if this config is on
    return config_reloadIfStale();
}

bool
TransactionInterfaceManager::hasConsumedData() {
    if (hasConsumedValidData()) {
        // Then it has consumed
        assert(m_hasReceivedDataFromProducer != createInvalidMachineID());
    } else if (m_hasReceivedDataFromProducer != createInvalidMachineID()) {
        // Current consumer with blocks pending validation
        if (!isConsumer()) { // Only allowed case when we don't have
                             // pending blocks is when validation
                             // failed and we are aborting
            assert(isAborting());
        }
    }
    return m_hasReceivedDataFromProducer != createInvalidMachineID();
}

void
TransactionInterfaceManager::
profileHtmFailureFaultCause(HtmFailureFaultCause cause)
{
    assert(cause != HtmFailureFaultCause::INVALID);

    HtmFailureFaultCause preciseFaultCause = cause;
    m_lastFailureCause = cause;
    switch (m_abortCause) {
    case HTMStats::AbortCause::Undefined:
        // CPU-triggered abort due to fault, interrupt, lsq conflict
        assert(!isAborting());
        assert((cause == HtmFailureFaultCause::EXCEPTION) ||
               (cause == HtmFailureFaultCause::INTERRUPT) ||
               (cause == HtmFailureFaultCause::DISABLED) ||
               (cause == HtmFailureFaultCause::LSQ));

        if (cause == HtmFailureFaultCause::LSQ) {
            // "True" LSQ conflict: precise read set tracking, do not
            // reload if stale, block not yet in read set (NOTE:
            // m_abortAddress is unset, so need to pass address in CPU
            // pkt for further sanity addr-specific checks
            assert(!m_htm->params().reload_if_stale);
            // Note that it is possible to have a block in the
            // write-set which is then targeted by the first load when
            // the conflicting invalidation arrives
        }
        break;
    case HTMStats::AbortCause::Explicit:
        // HTMCancel
        assert(cause == HtmFailureFaultCause::EXPLICIT ||
               cause == HtmFailureFaultCause::EXPLICIT_FALLBACKLOCK);
        break;
    case HTMStats::AbortCause::L0Capacity:
    case HTMStats::AbortCause::L1Capacity:
    case HTMStats::AbortCause::L2Capacity:
    case HTMStats::AbortCause::WrongL0:
        // Capacity
        if (cause == HtmFailureFaultCause::SIZE) {
            if (m_abortCause == HTMStats::AbortCause::L2Capacity) {
                preciseFaultCause = HtmFailureFaultCause::SIZE_LLC;
            } else if (m_abortCause == HTMStats::AbortCause::WrongL0) {
                preciseFaultCause = HtmFailureFaultCause::SIZE_WRONG_CACHE;
            } else if (m_abortCause == HTMStats::AbortCause::L1Capacity) {
                preciseFaultCause = HtmFailureFaultCause::SIZE_L1PRIV;
            } else {
                assert(m_abortCause == HTMStats::AbortCause::L0Capacity);
                preciseFaultCause = m_capacityAbortWriteSet ?
                    HtmFailureFaultCause::SIZE_WSET :
                    HtmFailureFaultCause::SIZE_RSET ;
            }
        } else {
            // Conflicting snoops can race with replacements of
            // read-write set blocks, so LSQ is also possible
            // here. CPU-determined cause prevails over Ruby
            assert((cause == HtmFailureFaultCause::EXCEPTION) ||
                   (cause == HtmFailureFaultCause::LSQ) ||
                   (cause == HtmFailureFaultCause::INTERRUPT));

            DPRINTF(RubyHTM, "CPU signaled abort (reason=%s)"
                    " before observing capacity overflow\n",
                    htmFailureToStr(preciseFaultCause));
        }
        break;
    case HTMStats::AbortCause::ConflictStale:
        if (m_htm->params().precise_read_set_tracking) {
            // Can get data stale for addr not yet in Rset
        } else {
            assert(checkReadSignature(m_abortAddress));
        }
        assert((cause == HtmFailureFaultCause::MEMORY) ||
               (cause == HtmFailureFaultCause::LSQ));
        preciseFaultCause = HtmFailureFaultCause::MEMORY_STALEDATA;
        break;
    case HTMStats::AbortCause::FallbackLock:
    case HTMStats::AbortCause::ConflictPower:
    case HTMStats::AbortCause::Conflict:
        // Conflict
        if (cause == HtmFailureFaultCause::MEMORY ||
            // Can also get LSQ cause if block in R/W set and CPU
            // found outstanding load in lsq (see checkSnoop) and
            // HTM config says not to reload stale data
            (cause == HtmFailureFaultCause::LSQ)) {
#if 0
            Addr addr = m_abortAddress;
            // Sanity checks
            if (getXactConflictManager()->isReqLosesPolicy() ||
                getXactConflictManager()->isPowerTMPolicy()) {
                // TODO: May be aborted for an address that is not
                // part of our RWset (e.g. write-first block, write
                // gets nacked, so it is not added to Wset, nor Rset)
            }
            else if (m_htm->params().precise_read_set_tracking &&
                     getXactConflictManager()->isRequesterStallsPolicy()) {
 // Some of these checks do not always hold
                // It is possible to have conflict-induced aborts on
                // addresses that are not yet part of the read set
                // because the trans load has been repeatedly nacked
                bool hybrid_policy =
                    ((config_conflictResPolicy() ==
                      HtmPolicyStrings::requester_stalls_cda_hybrid) ||
                     (config_conflictResPolicy() ==
                      HtmPolicyStrings::requester_stalls_cda_hybrid_ntx));
                assert(getXactConflictManager()->nackReceived() ||
                       (hybrid_policy  && !checkWriteSignature(addr)) ||
                       m_abortSourceNonTransactional);
                assert(checkWriteSignature(addr) ||
                       checkReadSignature(addr) ||
                       (addr == getXactConflictManager()->
                        getNackedPossibleCycleAddr()));
            } else {
                //assert(checkWriteSignature(addr) ||
                //       checkReadSignature(addr));
            }
#endif

            if (cause == HtmFailureFaultCause::LSQ) {
                // "False" LSQ conflicts, will be categorized as
                // MEMORY since Ruby set abortCause to Conflict (block
                // was in r/w set). Note that "true" LSQ conflicts
                // reported as "lsq_conflicts" are only possible with
                // precise read sets (when block not yet in Rset) and
                // must find abortCause undefined (see case above)

                // Aborts of this type are only possible when CPU does
                // not re-execute conflicting loads
                assert(!m_htm->params().reload_if_stale);
            }

            if (m_abortCause == HTMStats::AbortCause::FallbackLock) {
                preciseFaultCause = HtmFailureFaultCause::MEMORY_FALLBACKLOCK;
            } else if (m_abortCause ==
                       HTMStats::AbortCause::ConflictStale) {
                preciseFaultCause = HtmFailureFaultCause::MEMORY_STALEDATA;
            } else if (m_abortCause ==
                       HTMStats::AbortCause::ConflictPower) {
                preciseFaultCause = HtmFailureFaultCause::MEMORY_POWER;
            } else {
                preciseFaultCause = HtmFailureFaultCause::MEMORY;
                if (!XACT_EAGER_CD &&
                    (m_htm->params().lazy_arbitration ==
                     HtmPolicyStrings::token)) {
                    if ((getXactLazyVersionManager()->
                         getNumReadBytesWrittenRemotely() == 0) &&
                        (getXactLazyVersionManager()->
                         getNumWrittenBytesWrittenRemotely() == 0)) {
                        preciseFaultCause =
                            HtmFailureFaultCause::MEMORY_FALSESHARING;
                    }
                }
            }
        } else { // CPU has aborted for another reason before
                 // observing the conflict
            assert((cause == HtmFailureFaultCause::EXCEPTION) ||
                   (cause == HtmFailureFaultCause::INTERRUPT));

            DPRINTF(RubyHTM, "CPU signaled abort (reason=%s)"
                    " before observing memory conflict\n",
                    htmFailureToStr(preciseFaultCause));
        }
        break;
    case HTMStats::AbortCause::IllegalData:
        preciseFaultCause = HtmFailureFaultCause::SPEC_ILLEGAL;
        break;
    case HTMStats::AbortCause::SpecValidation:
        preciseFaultCause = HtmFailureFaultCause::SPEC_VALIDATION;
        break;
    case HTMStats::AbortCause::SpecDepthOverflow:
        preciseFaultCause = HtmFailureFaultCause::SPEC_DEPTH;
        break;
    case HTMStats::AbortCause::SpecMaxCapacityOverflow:
        preciseFaultCause = HtmFailureFaultCause::SPEC_MAX_CAPACITY;
        break;
    case HTMStats::AbortCause::SpecWriteInFlight:
        preciseFaultCause = HtmFailureFaultCause::SPEC_WRITE_IN_FLIGHT;
        break;
    case HTMStats::AbortCause::SpecReadConflict:
        preciseFaultCause = HtmFailureFaultCause::SPEC_READ_CONFLICT;
        break;
    default:
        panic("Invalid htm failure fault cause\n");
    }
    auto cause_idx = static_cast<int>(preciseFaultCause);
    m_htm_transaction_abort_cause[cause_idx]++;
    m_htm_aborted_tx_xid_cause[cause_idx].sample(m_htmXid);
    profileHtmFwdTransaction(true, cause_idx);
    m_htm_executed_tx_xid.sample(m_htmXid);
    DPRINTF(RubyHTM, "htmAbort - reason=%s\n",
            htmFailureToStr(preciseFaultCause));
}

HtmCacheFailure
TransactionInterfaceManager::getHtmTransactionalReqResponseCode()
{
    switch (m_abortCause) {
    case HTMStats::AbortCause::Undefined:
        assert(!isAborting());
        return HtmCacheFailure::NO_FAIL;
    case HTMStats::AbortCause::Explicit:
        // HTMCancel: Must return NO_FAIL for CPU to call
        // XabortCompleteAcc, which sets the abort cause and generates
        // the fault to abort the transaction
        return HtmCacheFailure::NO_FAIL;
    case HTMStats::AbortCause::L0Capacity:
    case HTMStats::AbortCause::L1Capacity:
    case HTMStats::AbortCause::L2Capacity:
    case HTMStats::AbortCause::WrongL0:
        return HtmCacheFailure::FAIL_SELF;
    case HTMStats::AbortCause::Conflict:
    case HTMStats::AbortCause::ConflictStale:
    case HTMStats::AbortCause::FallbackLock:
    case HTMStats::AbortCause::ConflictPower:
    case HTMStats::AbortCause::SpecValidation:
    case HTMStats::AbortCause::SpecDepthOverflow:
    case HTMStats::AbortCause::SpecMaxCapacityOverflow:
    case HTMStats::AbortCause::SpecWriteInFlight:
    case HTMStats::AbortCause::SpecReadConflict:
        return HtmCacheFailure::FAIL_REMOTE;
    default:
        panic("Invalid htm return code\n");
        return HtmCacheFailure::FAIL_OTHER;
    }
}

void
TransactionInterfaceManager::setAbortFlag(Addr addr,
                                          MachineID abortSource,
                                          TransactionBit remote_trans,
                                          int remote_priority,
                                          bool remote_sw_priority,
                                          bool capacity, bool wset,
                                          bool dataStale, bool validation,
                                          bool specDepthOverflow,
                                          bool specMaxCapacityOverflow,
                                          bool specWriteInFlight,
                                          bool specReadConflict)
{
    /**
     * That is a lot of parameters... TODO: refactor
     *
     */
    /* RT: This method is called from any ruby method to signal an
     * abort.  (e.g. conflict detected from the protocol, eviction of
     * speculative data...)  This kind of "asynchronous" aborts have to
     * be communicated to the CPU so that the required actions (e.g
     * restore reg. file) take place at the right time. This is
     * different for CPU-initiated aborts, which happen "synchronously"
     * (see abortTransaction method).
     */

    /* We use this other flag in Ruby to keep track of this pending
     * abort: the actual abort actions are triggered when the abort
     * command is received from the CPU by
     * TransactionalSequencer::notifyXactionEvent(PacketPtr), which
     * then calls xact_mgr->abortTransaction.
     */
    if (m_transactionLevel == 0) {
        assert(!m_htm->params().precise_read_set_tracking);
        assert(getXactIsolationManager()->
               wasOvertakingRead(addr));
        DPRINTF(RubyHTM, "HTM: setAbortFlag for address=%#x"
                " with TL=0\n", addr);
        panic("setAbortFlag with TL=0!");
    } else {
        if (config_enableIsolationChecker()) {
            if (checkReadSignature(addr)) {
                m_ruby_system->getXactIsolationChecker()->
                    removeFromReadSet(m_version, addr);
            }
            if (checkWriteSignature(addr)) {
                m_ruby_system->getXactIsolationChecker()->
                    removeFromWriteSet(m_version, addr);
            }
        }
    }

    if (!XACT_LAZY_VM) { // LogTM
        assert(!isUnrollingLog());
    }
    if (!m_abortFlag) { // Only send abort signal to CPU once
        m_abortFlag = true;
        m_abortAddress = makeLineAddress(addr);

        if (m_transactionLevel > 0) {
            // Do not move to aborting until  TL > 0
            XACT_PROFILER->moveTo(getProcID(), AnnotatedRegion_ABORTING);
        }
        if (!XACT_EAGER_CD) { // Lazy-lazy
            if (getXactLazyVersionManager()->committing()) {
                // cancel pending writes, if any left
                getXactLazyVersionManager()->
                    cancelWriteBufferFlush();
            }
        }
        assert(m_abortCause == HTMStats::AbortCause::Undefined);
        if (validation) {
            m_abortCause = HTMStats::AbortCause::SpecValidation;
        } else if (specDepthOverflow) {
            m_abortCause = HTMStats::AbortCause::SpecDepthOverflow;
        } else if (specMaxCapacityOverflow) {
            m_abortCause = HTMStats::AbortCause::SpecMaxCapacityOverflow;
        } else if (specWriteInFlight) {
            m_abortCause = HTMStats::AbortCause::SpecWriteInFlight;
        } else if (specReadConflict) {
            m_abortCause = HTMStats::AbortCause::SpecReadConflict;
        } else if (dataStale) {
            // Source of abort is Data_Stale event (inv seen while
            // outstanding trans load)
            m_abortCause = HTMStats::AbortCause::ConflictStale;
        } else if (remote_sw_priority) {
            // Source of abort is power transaction
            m_abortCause = HTMStats::AbortCause::ConflictPower;
        } else if (machineIDToNodeID(abortSource) == getProcID() &&
            machineIDToMachineType(abortSource) != MachineType_L2Cache) {
            // Source of abort is self L0/L1 cache
            if (machineIDToMachineType(abortSource) ==
                MachineType_L0Cache) {
                if (!capacity) {
                    // Transactional block evicted because it was in the
                    // wrong L0 cache (e.g. trans ST to Rset block in Icache)
                    m_abortCause = HTMStats::AbortCause::WrongL0;
                    DPRINTF(RubyHTM, "HTM: setAbortFlag for address=%#x"
                            " in wrong L0 cache\n", addr);
                } else {
                    // Source of abort is self at cache level used for
                    // speculative versioning: L0/L1 overflow
                    m_abortCause = HTMStats::AbortCause::L0Capacity;
                    m_capacityAbortWriteSet = wset;
                }
            } else {
                // L1 replacement of L0 transactional block
                assert(machineIDToMachineType(abortSource) ==
                       MachineType_L1Cache);
                assert(!capacity); // L1 overflows not signaled as capacity
                m_abortCause = HTMStats::AbortCause::L1Capacity;
            }
        } else if (machineIDToMachineType(abortSource) ==
                   MachineType_L2Cache) {
            m_abortCause = HTMStats::AbortCause::L2Capacity;
        } else if (machineIDToNodeID(abortSource) != getProcID()) {
            // Remote conflicting requestor, for now assume L1 cache
            assert(machineIDToMachineType(abortSource) == MachineType_L1Cache);
            m_abortSourceNonTransactional = !isTransactionalRequest(remote_trans);

            // Conflict-induced aborts are split into fallback-lock
            // conflicts vs rest
            assert(m_abortAddress);
            if (m_abortAddress == m_htm->getFallbackLockPAddr()) {
                m_abortCause = HTMStats::AbortCause::FallbackLock;
#if 0
            } else if (!checkWriteSignature(m_abortAddress) &&
                       checkReadSignature(m_abortAddress) &&
                       !inRetiredReadSet(m_abortAddress)) {
                // Conflict on read-set block that is not part of the
                // "retired read set", i.e. referenced by outstanding
                // load(s)
                m_abortCause = HTMStats::AbortCause::ConflictStale;
#endif
            } else {
                m_abortCause = HTMStats::AbortCause::Conflict;
#if 0
                // Add this abort to the remote killer's remote abort count
                TransactionInterfaceManager *remote_mgr =
                    m_ruby_system->getTransactionInterfaceManager(killer);
                remote_mgr->profileRemoteAbort();
#endif
            }
        }

        // Do not send packet to CPU now, transaction failure will be
        // notified to CPU upon subsequent memory access or HTM command
        DPRINTF(RubyHTM, "HTM: setAbortFlag cause=%s address=%#x"
                " source=%d\n",
                HTMStats::AbortCause_to_string(m_abortCause),
                addr, machineIDToNodeID(abortSource));
        DPRINTF(HTMInstTracer, "setAbortFlag - Xid: %ld - "
                "uid: %llu - cause=%s - address=%#x"
                " source=%d\n",
                m_htmXid,
                m_currentHtmUid,
                HTMStats::AbortCause_to_string(m_abortCause),
                addr, machineIDToNodeID(abortSource));

        if (hasConsumedData()) {
            DPRINTF(RubyHTMvaluepred, "HTMvp: Consumer aborts"
                    " %s having validated data, cause is %s"
                    " address=%#x, source=%d\n",
                    hasConsumedValidData() ? "after" : "before",
                    HTMStats::AbortCause_to_string(m_abortCause),
                    addr, machineIDToNodeID(abortSource));
        }
        DPRINTFR(FwdGraphTraceVerbose, "%d-Proc:%d-Uid:%d-SETABORTFLAG-Local_Prio:%d-Cause:%s-Source:%d\n",
                        curTick(),
                        getProcID(),
                        m_currentHtmUid,
                        m_hwTransPrio != NC_PRIO_VALUE ? to_string(m_hwTransPrio) : "NC",
                        HTMStats::AbortCause_to_string(m_abortCause),
                        machineIDToNodeID(abortSource));
    }
    else {
        DPRINTF(RubyHTM, "HTM: setAbortFlag "
                "for address %#x but abort flag was already set\n",
                addr);
    }
}

void
TransactionInterfaceManager::cancelTransaction(PacketPtr pkt)
{
    assert(!m_abortFlag);
    m_abortFlag = true;
    assert(m_abortCause == HTMStats::AbortCause::Undefined);
    m_abortCause = HTMStats::AbortCause::Explicit;

    XACT_PROFILER->moveTo(getProcID(), AnnotatedRegion_ABORTING);
    DPRINTF(RubyHTM, "HTM: cancelTransaction explicitly aborts transaction\n");
}

bool
TransactionInterfaceManager::isCancelledTransaction()
{
    return (m_abortFlag &&
            m_abortCause == HTMStats::AbortCause::Explicit);
}

bool
TransactionInterfaceManager::isTransactionAbortedByRemotePower()
{
    return (m_abortFlag &&
            m_abortCause == HTMStats::AbortCause::ConflictPower);
}

void
TransactionInterfaceManager::setAbortCause(HTMStats::AbortCause cause)
{
    if (!m_abortFlag) { // CPU-triggered abort
        m_abortCause = cause;
    }
    else { // Ruby-triggered abort, cause already set
        if (m_abortCause != cause) {
            m_abortCause = cause;
            // warn("HTM: setAbortCause found mismatch in causes: "
            //      "CPU cause is %s, Ruby cause is %s\n",
            //      HTMStats::AbortCause_to_string(cause),
            //      HTMStats::AbortCause_to_string(m_abortCause));
        }
    }
}

AnnotatedRegion_t
TransactionInterfaceManager::
getWaitForRetryRegionFromPreviousAbortCause()
{
    // Meant to be called when fallback lock acquired, to figure out
    // reason for taking fallback path
    assert(m_transactionLevel == 0);
    if (m_lastFailureCause == HtmFailureFaultCause::SIZE) {
        return AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_SIZE;
    } else if (m_lastFailureCause == HtmFailureFaultCause::EXCEPTION) {
        return AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_EXCEPTION;
    } else {
        return AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_THRESHOLD;
    }
}


bool
TransactionInterfaceManager::isAborting() {
    return inTransaction() && m_abortFlag;
}

bool TransactionInterfaceManager::isDoomed() {
    if (!XACT_LAZY_VM) { // LogTM
        if (isUnrollingLog()) {
            // Abort flag already cleared but TL>0 so consider it
            // doomed
            assert(!m_abortFlag);
            return true;
        }
    }
    return m_abortFlag;
}

void
TransactionInterfaceManager::xactReplacement(Addr addr, MachineID source,
                                             bool capacity, bool dataStale, bool spec) {
    bool wset = false;
    assert(makeLineAddress(addr) == addr);
    if (checkWriteSignature(addr)) {
        if (!XACT_LAZY_VM) { // LogTM
            // Allowed, do not abort
            DPRINTF(RubyHTMlog, "HTM: tolerated xactReplacement"
                    " of logged write-set address %#x \n", addr);
            return;
        }
        wset = true;
        DPRINTF(RubyHTM, "HTM: xactReplacement "
                "for write-set address %#x \n", addr);
        if (capacity) {
            // Keep track of overflows for sanity checks when
            // discarding write set (expect not present)
            m_writeSetDiscarded.
                insert(std::pair<Addr,char>(addr, 'y'));
        }
    }
    else if (dataStale) {
        // "Data_Stale": Address may or may not be in read set
        // depending on precise_read_set_tracking. Call setAbortFlag
        // to set detailed abort cause (ConflictStale)

        // Do not set the abort flag if already set or no longer in a
        // transaction
        if (isDoomed() || !inTransaction()) return;
        PacketPtr pkt = m_sequencer->getPacketFromRequestTable(addr);
        if (pkt->getHtmTransactionUid() < m_currentHtmUid) {
            warn("HTM: dataStale abort from lingering transactional access ");
            // Add to Rset to pass sanity checks
            if (!m_htm->params().precise_read_set_tracking) {
                isolateTransactionLoad(addr);
            }
        }
    } else if (spec) {
        // Do not set the abort flag if already set or no longer in a
        // transaction
        assert(isBlockConsumed(addr));
        // Check that we did not miss this fwded block
        assert(m_allReceivedData.find(addr) !=
               m_allReceivedData.end());
        // Block is already deallocated so remove from buffer to
        // avoid assert failure
        m_recvDataForwardedFromProducer.erase(addr);
        warn("HTM: consumed block evicted aborting");
        if (isDoomed() || !inTransaction()) return;
        // Corner case where the block got invalidated even before
        // being introduced in read set. Very very uncommon but can
        // happpen if block falls into wrong L0 and evicted due to
        // inst fetch to the same block... very very very uncommon
        // the next action notifycpu will abort but will take some
        // time, in the case it really aborts so this way we are
        // covered, abort from cache and forget
        if (!checkReadSignature(addr)) {
            warn("HTM: consumed block eviction not in rs");
        }
    } else {
        assert(checkReadSignature(addr));
        DPRINTF(RubyHTM, "HTM: xactReplacement "
                "for read-set address %#x \n", addr);
        if (!XACT_LAZY_VM) { // LogTM
            if (isUnrollingLog()) {
                DPRINTF(RubyHTMlog, "HTM: read-set eviction"
                        " during log unroll is ignored\n");
                return;
            }
        }
        MachineType sourceMachType = machineIDToMachineType(source);
        if (machineIDToNodeID(source) == getProcID() &&
            sourceMachType != MachineType_L2Cache) {
            // Self L0/L1
            if (config_allowReadSetL0CacheEvictions() &&
                (sourceMachType == MachineType_L0Cache)) {
                // Lower level cache: allowed
                DPRINTF(RubyHTM, "HTM: read-set eviction tolerated"
                        " for address %#x \n", addr);
                return;
            }
            if (config_allowReadSetL1CacheEvictions()) {
                // L1 cache: allowed
                DPRINTF(RubyHTM, "HTM: read-set eviction from L1 tolerated"
                        " for address %#x \n", addr);
                return;
            }
        }
    }
    // Do not set spec as this is always a replacement,
    // spec here is no matter, it is just to avoid
    // asserts to fail
    setAbortFlag(addr, source, TransactionBit_NonTrans, NC_PRIO_VALUE, false,
                 capacity, wset, dataStale, false);
}

void
TransactionInterfaceManager::profileCommitCycle()
{

}

bool
TransactionInterfaceManager::shouldNackLoad(Addr addr,
                                            MachineID requestor,
                                            Cycles remote_timestamp,
                                            TransactionBit remote_trans,
                                            int remote_priority,
                                            bool remote_sw_priority)
{
    assert(addr == makeLineAddress(addr));
    bool nack = getXactConflictManager()->shouldNackLoad(addr, requestor,
                                                    remote_timestamp,
                                                    remote_trans,
                                                    remote_priority,
                                                    remote_sw_priority);
    return nack;
}

bool
TransactionInterfaceManager::shouldNackStore(Addr addr,
                                             MachineID requestor,
                                             Cycles remote_timestamp,
                                             TransactionBit remote_trans,
                                             int remote_priority,
                                             bool remote_sw_priority,
                                             bool local_is_exclusive)
{
    assert(addr == makeLineAddress(addr));
    bool nack =  getXactConflictManager()->
        shouldNackStore(addr, requestor,
                        remote_timestamp,
                        remote_trans,
                        remote_priority,
                        remote_sw_priority,
                        local_is_exclusive);
    return nack;
}

void
TransactionInterfaceManager::notifyReceiveNack(Addr addr,
                                               Cycles remote_timestamp,
                                               TransactionBit remote_trans,
                                               int remote_priority,
                                               bool remote_sw_priority,
                                               MachineID remote_id,
                                               bool writer)
{
    getXactConflictManager()->notifyReceiveNack(addr,
                                                remote_timestamp,
                                                remote_trans,
                                                remote_priority,
                                                remote_sw_priority,
                                                remote_id, writer);
}
Cycles
TransactionInterfaceManager::getOldestTimestamp()
{
    return getXactConflictManager()->getOldestTimestamp();
}

bool
TransactionInterfaceManager::checkReadSignature(Addr addr)
{
    return getXactIsolationManager()->isInReadSetPerfectFilter(addr);
}

bool
TransactionInterfaceManager::checkWriteSignature(Addr addr)
{
    return getXactIsolationManager()->isInWriteSetPerfectFilter(addr);
}

bool
TransactionInterfaceManager::hasConflictWith(TransactionInterfaceManager *o)
{
    return getXactIsolationManager()->
        hasConflictWith(o->getXactIsolationManager());
}

void
TransactionInterfaceManager::redirectStoreToWriteBuffer(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();

    assert(getTransactionLevel() > 0);
    getXactLazyVersionManager()->
        addToWriteBuffer(addr, pkt->getSize(),
                         pkt->getPtr<uint8_t>());

    DPRINTF(RubyHTM, "Redirecting store to lazy write buffer,"
            " vaddr %#x paddr %#x\n",
            pkt->req->getVaddr(),
            addr);


    Addr request_address = makeLineAddress(pkt->getAddr());
    getXactIsolationManager()->
        redirectedStoreToWriteBuffer(request_address);
}

void
TransactionInterfaceManager::bypassLoadFromWriteBuffer(PacketPtr pkt,
                                                       DataBlock& datablock) {
  assert(XACT_LAZY_VM && !XACT_EAGER_CD);

  uint8_t buffer[256];
  _unused(buffer);
  bool forwarding = false;
  assert(pkt->isRead() && !pkt->req->isInstFetch());
  std::vector<uint8_t> data = getXactLazyVersionManager()->
      forwardData(pkt->getAddr(),
                  pkt->getSize(),
                  datablock,
                  forwarding);
  if (forwarding) { // Load "hit" in write buffer
      // Sanity checks: only forward spec data to transactional
      // loads. In O3CPU, xend is membarrier so non-transactional
      // loads should not reorder w.r.t. xend
      assert(pkt->isHtmTransactional());
      assert(data.size() == pkt->getSize());
      assert(data.size() < sizeof(buffer));
      for (unsigned int i = 0; i < pkt->getSize(); i++){
          buffer[i] = (uint8_t)data[i];
      }
      //  Replace loaded data with data from write buffer
      memcpy(pkt->getPtr<uint8_t>(),
             buffer,
             pkt->getSize());
  }
}


void
TransactionInterfaceManager::mergeDataFromWriteBuffer(PacketPtr pkt,
                                                      DataBlock& datablock)
{
    Addr address = pkt->getAddr();
    getXactLazyVersionManager()->
        mergeDataFromWriteBuffer(address, datablock);
}

bool
TransactionInterfaceManager::isLogReady()
{
    return m_xactEagerVersionManager->isReady();
}

bool
TransactionInterfaceManager::isAccessToLog(Addr addr)
{
    return m_xactEagerVersionManager->isAccessToLog(addr);
}

bool
TransactionInterfaceManager::isEndLogUnrollSignal(PacketPtr pkt)
{
    return m_xactEagerVersionManager->isEndLogUnrollSignal(pkt);
}

void
TransactionInterfaceManager::setupLogTranslation(Addr vaddr,
                                                 Addr paddr)
{
    m_xactEagerVersionManager->setupLogTranslation(vaddr, paddr);
}

Addr
TransactionInterfaceManager::translateLogAddress(Addr vaddr) const
{
    return m_xactEagerVersionManager->translateLogAddress(vaddr);
}

Addr
TransactionInterfaceManager::addLogEntry()
{
    return m_xactEagerVersionManager->addLogEntry();
}

int
TransactionInterfaceManager::getLogNumEntries()
{
    return m_xactEagerVersionManager->getLogNumEntries();
}

bool
TransactionInterfaceManager::isUnrollingLog(){
    if (!XACT_LAZY_VM) // LogTM
        return m_unrollingLogFlag;
    else
        return false;
}

void
TransactionInterfaceManager::endLogUnroll(){
    assert(m_transactionLevel == 1);
    assert(m_escapeLevel == 1);
    assert(!XACT_LAZY_VM); // LogTM
    assert(m_unrollingLogFlag);

    // Reset log num entries
    m_xactEagerVersionManager->restartTransaction();
    // Restart conflict management
    getXactConflictManager()->restartTransaction();

    // Release isolation over write set
    getXactIsolationManager()->releaseIsolation();
    if (config_enableIsolationChecker()) {
        m_ruby_system->getXactIsolationChecker()->
            clearReadSet(m_version);
        m_ruby_system->getXactIsolationChecker()->
            clearWriteSet(m_version);
    }

    m_escapeLevel = 0;
    m_transactionLevel = 0;
    m_unrollingLogFlag = false;
    DPRINTF(RubyHTMlog, "HTM: done unrolling log, abort"
            " is now complete!\n");

    // Value sanity checks done after log unrolled
    if (config_enableValueChecker()) {
        m_ruby_system->getXactValueChecker()->
            restartTransaction(getProcID());
    }
}

void
TransactionInterfaceManager::beginEscapeAction()
{
    assert(m_escapeLevel == 0);
    m_escapeLevel++;
}

void
TransactionInterfaceManager::endEscapeAction()
{
    assert(m_escapeLevel == 1);
    m_escapeLevel--;
}

bool
TransactionInterfaceManager::inEscapeAction()
{
    return m_escapeLevel > 0;
}

void TransactionInterfaceManager::profileFlAcquisition(long htmXid) {
    m_htmstart_tick = curTick();
}

void TransactionInterfaceManager::profileFlRelease(long htmXid) {
    m_htm_transaction_executed_type_rate[2 * 6]++;
    m_committed_fl_tx_xid.sample(htmXid);
    Tick transaction_ticks = curTick() - m_htmstart_tick;
    Cycles transaction_cycles {ticksToCycles(transaction_ticks)};
    m_htm_fl_cycles_tx_xid.sample(htmXid, transaction_cycles);
    while (!m_retriesTicks.empty())
    {
        double percentageExecuted =
            ((double)m_retriesTicks.top() / (double)transaction_ticks) * 100.0;
        m_htm_percentage_executed_on_abort.sample(percentageExecuted);
        m_retriesTicks.pop();
    }
}

void
TransactionInterfaceManager::regStats()
{
    ClockedObject::regStats();
    // hardware transactional memory
    m_htm_transaction_cycles
        .init(10)
        .name(name() + ".htm_transaction_cycles")
        .desc("number of cycles spent in an outer transaction")
        .flags(statistics::pdf | statistics::dist | statistics::nozero | statistics::nonan)
        ;
    //! Histogram of cycle latencies of committed HTM transactions
    m_htm_transaction_commit_cycles
        .init(10)
        .name(name() + ".htm_transaction_commit_cycles")
        .desc("number of cycles spent in a committed outer transaction")
        .flags(statistics::pdf | statistics::dist | statistics::nozero | statistics::nonan)
        ;
    //! Histogram of cycle latencies of aborted HTM transactions
    m_htm_transaction_abort_cycles
        .init(10)
        .name(name() + ".htm_transaction_abort_cycles")
        .desc("number of cycles spent in an aborted outer transaction")
        .flags(statistics::pdf | statistics::dist | statistics::nozero | statistics::nonan)
        ;
    m_htm_transaction_instructions
        .init(10)
        .name(name() + ".htm_transaction_instructions")
        .desc("number of instructions spent in an outer transaction")
        .flags(statistics::pdf | statistics::dist | statistics::nozero | statistics::nonan)
        ;
    auto num_causes = static_cast<int>(HtmFailureFaultCause::NUM_CAUSES);
    m_htm_transaction_abort_cause
        .init(num_causes)
        .name(name() + ".htm_transaction_abort_cause")
        .desc("cause of htm transaction abort")
        .flags(statistics::total | statistics::pdf | statistics::dist)
        ;
    m_htm_producers_tx
        .init(2 + num_causes)
        .name(name() + ".htm_producers_tx")
        .flags(Stats::total | Stats::pdf | Stats::dist)
        ;
    m_htm_producers_tx.subname(0, "commit_nm");
    m_htm_producers_tx.subname(1, "commit_m");
    m_htm_consumers_tx
        .init(2 + num_causes)
        .name(name() + ".htm_consumers_tx")
        .flags(Stats::total | Stats::pdf | Stats::dist)
        ;
    m_htm_consumers_tx.subname(0, "commit_nm");
    m_htm_consumers_tx.subname(1, "commit_m");
    m_htm_transaction_power_modified
        .init(3)
        .name(name() + ".m_htm_transaction_power_modified")
        .desc("times a power modified data forwarded")
        .flags(Stats::total | Stats::pdf | Stats::dist)
        ;
    m_htm_producers_consumers_tx
        .init(2 + num_causes)
        .name(name() + ".htm_producers_consumers_tx")
        .desc("Producers that modified data")
        .flags(Stats::total | Stats::pdf | Stats::dist);
    m_htm_producers_consumers_tx.subname(0, "commit_nm");
    m_htm_producers_consumers_tx.subname(1, "commit_m");

    m_htm_powered_tx
        .init(3 + num_causes)
        .name(name() + ".htm_powered_tx")
        .desc("Power transactions")
        .flags(Stats::total | Stats::pdf | Stats::dist);

    m_htm_powered_tx.subname(0, "commit_common");
    m_htm_powered_tx.subname(1, "commit_nm");
    m_htm_powered_tx.subname(2, "commit_m");

    m_htm_forwardings
        .init(3)
        .name(name() + ".htm_forwardings")
        .desc("Forwardings")
        .flags(Stats::total | Stats::pdf | Stats::dist);

    m_htm_forwardings.subname(0, "commit_nm");
    m_htm_forwardings.subname(1, "commit_m");
    m_htm_forwardings.subname(2, "abort");
    m_htm_reconsumer_tx
        .init(1 + num_causes)
        .name(name() + ".htm_reconsumer_tx")
        .desc("Reconsumer transactions")
        .flags(Stats::total | Stats::pdf | Stats::dist);
    m_htm_reconsumer_tx.subname(0, "commit");

    m_htm_transactions_produced_consumed_same
        .name(name() + ".htm_transactions_produced_consumed_same");

    m_htm_aborted_tx_xid_cause
        .init(num_causes, 0, 9, 1)
        .name(name() + ".htm_aborted_tx_xid_cause")
        .flags(Stats::total | Stats::pdf | Stats::dist);

    for (unsigned cause_idx = 0; cause_idx < num_causes; ++cause_idx) {
        m_htm_producers_consumers_tx.subname(
            cause_idx + 2,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
        m_htm_producers_tx.subname(
            cause_idx + 2,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
        m_htm_consumers_tx.subname(
            cause_idx + 2,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
        m_htm_powered_tx.subname(
            cause_idx + 3,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
        m_htm_reconsumer_tx.subname(
            cause_idx + 1,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
        m_htm_transaction_abort_cause.subname(
            cause_idx,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
        m_htm_aborted_tx_xid_cause.subname(
            cause_idx,
            htmFailureToStr(HtmFailureFaultCause(cause_idx)));
    }
    // Transactional types observed
    // 0: common
    // 1: producer
    // 2: consumer
    // 3: producer_consumer
    // 4: Power
    // 5: Power_producer
    // 6: FallbackLock
    auto num_types = 7;
    m_htm_transaction_executed_type_rate
        .init(num_types * 2)
        .name(name() + ".htm_transaction_executed_type_rate")
        .desc("executed transactions by its type")
        .flags(Stats::total | Stats::pdf | Stats::dist);
    for (unsigned type = 0; type < 2; type++) {
        m_htm_transaction_executed_type_rate.subname(
            2 * 0 + type, type == 0 ? "Common_commit" : "Common_abort");
        m_htm_transaction_executed_type_rate.subname(
            2 * 1 + type, type == 0 ? "Producer_commit" : "Producer_abort");
        m_htm_transaction_executed_type_rate.subname(
            2 * 2 + type, type == 0 ? "Consumer_commit" : "Consumer_abort");
        m_htm_transaction_executed_type_rate.subname(
            2 * 3 + type, type == 0 ? "Producer_consumer_commit" : "Producer_consumer_abort");
        m_htm_transaction_executed_type_rate.subname(
            2 * 4 + type, type == 0 ? "Power_commit" : "Power_abort");
        m_htm_transaction_executed_type_rate.subname(
            2 * 5 + type, type == 0 ? "Power_producer_commit" : "Power_producer_abort");
        m_htm_transaction_executed_type_rate.subname(
            2 * 6 + type, type == 0 ? "FallbackLock_commit" : "FallbackLock_abort");
    }

    m_htm_consumer_producer_transactions
        .init(2)
        .name(name() + ".htm_consumer_producer_transactions")
        .desc("Consumer that turned producer")
        .flags(Stats::total | Stats::pdf | Stats::dist);
    m_htm_consumer_producer_transactions.subname(0, "commit");
    m_htm_consumer_producer_transactions.subname(1, "abort");


    m_htm_producer_consumer_transactions
        .init(2)
        .name(name() + ".htm_producer_consumer_transactions")
        .desc("Producer that turned consumer")
        .flags(Stats::total | Stats::pdf | Stats::dist);
    m_htm_producer_consumer_transactions.subname(0, "commit");
    m_htm_producer_consumer_transactions.subname(1, "abort");

    m_htm_producer_n_modified_produced_data
            .init(0,16,1)
            .name(name() + ".htm_producer_n_modified_produced_data")
            .flags(Stats::pdf | Stats::dist);
    m_htm_consumer_transactions_nprod_abort
        .init(0, 16, 1)
        .name(name() + ".htm_consumer_transactions_nprod_abort")
        .flags(Stats::pdf | Stats::dist);
    m_htm_consumer_transactions_nprod_commit
        .init(0, 16, 1)
        .name(name() + ".htm_consumer_transactions_nprod_commit")
        .flags(Stats::pdf | Stats::dist);
    m_htm_producer_transactions_nprod_abort
        .init(0, 16, 1)
        .name(name() + ".htm_producer_transactions_nprod_abort")
        .flags(Stats::pdf | Stats::dist);
    m_htm_producer_transactions_nprod_commit
        .init(0, 16, 1)
        .name(name() + ".htm_producer_transactions_nprod_commit")
        .flags(Stats::pdf | Stats::dist);

    m_htm_PiC_committed
        .init(-16, 16, 1)
        .name(name() + ".htm_PiC_committed")
        .flags(Stats::pdf | Stats::dist);

    m_htm_accessed_mem_abort
        .init(0, 128, 16)
        .name(name() + ".htm_accessed_mem_abort")
        .flags(Stats::pdf | Stats::dist);

    m_htm_PiC_aborted
        .init(-16, 16, 1)
        .name(name() + ".htm_PiC_aborted")
        .flags(Stats::pdf | Stats::dist);

    m_htm_cycles_xid
        .init(0, 9, 1)
        .name(name() + ".htm_cycles_xid")
        .flags(Stats::pdf | Stats::dist);

    m_htm_executed_tx_xid
        .init(0, 9, 1)
        .name(name() + ".htm_executed_tx_xid")
        .flags(Stats::pdf | Stats::dist);

    m_htm_committed_tx_xid
        .init(0, 9, 1)
        .name(name() + ".htm_committed_tx_xid")
        .flags(Stats::pdf | Stats::dist);

    m_committed_fl_tx_xid
        .init(0, 9, 1)
        .name(name() + ".committed_fl_tx_xid")
        .flags(Stats::pdf | Stats::dist);

    m_htm_aborted_cycles_xid
        .init(0, 9, 1)
        .name(name() + ".htm_aborted_cycles_xid")
        .flags(Stats::pdf | Stats::dist);

    m_htm_committed_cycles_xid
        .init(0, 9, 1)
        .name(name() + ".htm_committed_cycles_xid")
        .flags(Stats::pdf | Stats::dist);

    m_htm_fl_cycles_tx_xid
        .init(0, 9, 1)
        .name(name() + ".htm_fl_cycles_tx_xid")
        .flags(Stats::pdf | Stats::dist);

    m_htm_transaction_power_modified
        .subname(
            0, "m_htm_transaction_power_modified_commit_nm"
        );
    m_htm_transaction_power_modified
        .subname(
            1, "m_htm_transaction_power_modified_coomit_m"
        );
    m_htm_transaction_power_modified
        .subname(
            2, "m_htm_transaction_power_modified_abort"
        );

    m_htm_percentage_executed_on_abort
        .init(10)
        .name(name() + ".htm_percentage_executed_on_abort")
        .flags(statistics::pdf | statistics::dist | statistics::total)
        ;

    m_htm_delayed_time_fwd_on_abort
        .init(10)
        .name(name() + ".htm_delayed_time_fwd_on_abort")
        .flags(statistics::pdf | statistics::dist | statistics::total)
        ;
}

std::vector<TransactionInterfaceManager*>
TransactionInterfaceManager::getRemoteTransactionManagers() const
{
    return m_ruby_system->getTransactionInterfaceManagers();
}

} // namespace ruby
} // namespace gem5
