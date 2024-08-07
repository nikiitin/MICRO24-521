/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/TransactionConflictManager.hh"

#include <cassert>
#include <cstdlib>

#include "debug/FwdGraphTraceVerbose.hh"
#include "debug/RubyHTM.hh"
#include "debug/RubyHTMvaluepred.hh"
#include "mem/ruby/htm/LazyTransactionCommitArbiter.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/TransactionIsolationManager.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "mem/ruby/system/Sequencer.hh"

namespace gem5
{
namespace ruby
{
#define XACT_CONFLICT_RES m_policy

TransactionConflictManager::
TransactionConflictManager(TransactionInterfaceManager *xact_mgr,
                           int version) {
  m_version = version;
  m_xact_mgr = xact_mgr;

  // The cycle at which Ruby simulation starts is substracted from
  // current time to generate timestamps in order to avoid overflowing
  // the Timestamp field (int) in SLICC protocol messages (SLICC
  // doesn't support long types). Must set it only once at the
  // beginning, as RubySystem::resetStats updates the start cycle

  m_timestamp      = Cycles(0);
  m_possible_cycle = false;
  m_lock_timestamp = false;
  m_numRetries     = 0;
  m_powered        = false;
  m_sentNack         = false;
  m_receivedNack         = false;
  m_abortedByNack  = false;
  m_doomed         = false;
  m_policy_nack_non_transactional = false;
  m_policy_power = false;

  m_default_policy = xact_mgr->getHTM()->getResolutionPolicy();
  assert(m_default_policy != HTM::ResolutionPolicy::Undefined);

  m_policy = xact_mgr->config_conflictResPolicy();
  if (m_policy == HtmPolicyStrings::power_tm ||
      m_policy == HtmPolicyStrings::woper_tm) {
      m_policy_power = true;
  }
  if (m_policy.find("_cda") == 0) {
      assert(m_default_policy == HTM::ResolutionPolicy::RequesterStalls);
      if (m_policy.find("_ntx") == 0) {
          assert(m_policy == HtmPolicyStrings::requester_stalls_cda_base_ntx ||
                 m_policy == HtmPolicyStrings::requester_stalls_cda_hybrid_ntx);
          m_policy_nack_non_transactional = true;
      }
  }
}

TransactionConflictManager::~TransactionConflictManager() {
}

void
TransactionConflictManager::setVersion(int version) {
  m_version = version;
}

int
TransactionConflictManager::getVersion() const {
  return m_version;
}

int
TransactionConflictManager::getProcID() const{
  return m_xact_mgr->getProcID();
}

void
TransactionConflictManager::beginTransaction(bool power_mode){
  int transactionLevel = m_xact_mgr->getTransactionLevel();
  assert(transactionLevel >= 1);

  if ((transactionLevel == 1) && !(m_lock_timestamp)){
    m_timestamp = m_xact_mgr->curCycle();
    m_lock_timestamp = true;
  }
  if (power_mode) {
      assert(isPowerTMPolicy());
      DPRINTF(RubyHTM, "Power Transaction! num retries counted"
              " by conflict manager: %d\n",
              getNumRetries());
      m_powered = true;
  }
}

void
TransactionConflictManager::commitTransaction(){
  int transactionLevel = m_xact_mgr->getTransactionLevel();
  assert(transactionLevel >= 1);
  assert(!m_doomed);

  if (transactionLevel == 1){
    m_lock_timestamp = false;
    m_numRetries     = 0;
    m_powered        = false;
    m_receivedNack         = false;
    m_abortedByNack  = false;
    clearPossibleCycle();

  }
}

void
TransactionConflictManager::restartTransaction(){
  m_numRetries++;
  clearPossibleCycle();
  m_sentNack         = false;
  m_receivedNack = false;
  m_abortedByNack  = false;
  m_doomed = false;
  m_powered = false;
}

int
TransactionConflictManager::getNumRetries(){
  return m_numRetries;
}

bool
TransactionConflictManager::possibleCycle(){
  return m_possible_cycle;
}

void
TransactionConflictManager::setPossibleCycle(){
  m_possible_cycle = true;
}

void
TransactionConflictManager::clearPossibleCycle(){
  m_possible_cycle = false;
}

bool
TransactionConflictManager::nackReceived(){
  return m_receivedNack;
}

bool
TransactionConflictManager::abortedByNack(){
  return m_abortedByNack;
}

bool
TransactionConflictManager::doomed(){
  return m_doomed;
}

void
TransactionConflictManager::setDoomed(){
    m_doomed = true;
}

Cycles
TransactionConflictManager::getTimestamp(){
  if (m_xact_mgr->getTransactionLevel() > 0)
    return m_timestamp;
  else {
      Cycles ts = m_xact_mgr->curCycle();
      assert(ts > 0); // Detect overflows
      return ts;
  }
}

bool
TransactionConflictManager::isRequesterStallsPolicy(){
    assert(m_default_policy != HTM::ResolutionPolicy::Undefined);
    return m_default_policy == HTM::ResolutionPolicy::RequesterStalls;
}

bool
TransactionConflictManager::isReqLosesPolicy(){
    assert(m_default_policy != HTM::ResolutionPolicy::Undefined) ;
    return m_default_policy == HTM::ResolutionPolicy::RequesterLoses;
}

bool
TransactionConflictManager::isReqWinsPolicy(){
    assert(m_default_policy != HTM::ResolutionPolicy::Undefined) ;
    return m_default_policy == HTM::ResolutionPolicy::RequesterWins;
}

bool
TransactionConflictManager::isPowerTMPolicy(){
    return (m_policy == HtmPolicyStrings::power_tm ||
            m_policy == HtmPolicyStrings::woper_tm);
}

bool
TransactionConflictManager::isProducerPolicy(){
    return (isPowerTMPolicy() ||
        isReqLosesPolicy() ||
        isRequesterStallsPolicy());
}

bool
TransactionConflictManager::isPowered(){
    if (m_powered) {
        assert(isPowerTMPolicy());
        return true;
    }
    else
        return false;
}

Cycles
TransactionConflictManager::getOldestTimestamp(){
  Cycles currentTime = m_xact_mgr->curCycle();
  Cycles oldestTime = currentTime;

  if ((m_xact_mgr->getTransactionLevel() > 0) &&
      (m_timestamp < oldestTime)) {
      oldestTime = m_timestamp;
  }
  assert(oldestTime > 0);
  return oldestTime;
}

bool
TransactionConflictManager::isRemoteOlder(Cycles local_timestamp,
                                          Cycles remote_timestamp,
                                          MachineID remote_id){

  bool older = false;

  if (local_timestamp == remote_timestamp){
      assert(getProcID() != (int) machineIDToNodeID(remote_id));
      older = (int) machineIDToNodeID(remote_id) < getProcID();
  } else {
    older = (remote_timestamp < local_timestamp);
  }
  return older;
}

bool TransactionConflictManager::shouldNackRemoteRequest(
    Addr addr, MachineID remote_id, Cycles remote_timestamp,
    TransactionBit remote_trans, int remote_priority, bool sw_remote_priority,
    bool remoteWriter) {
    if (m_xact_mgr->isAborting() || m_xact_mgr->isDoomed()) {
        // NOTE: Once aborting never send nack
        // Correctness and performance bug
        return false;
    }
    assert(addr == makeLineAddress(addr));
    bool shouldNack; // Leave uninitialize so that compiler warns us if
    bool ignoreNonTrans = false; // // Use it to override behaviour of non-tx
    if (!isTransactionalRequest(remote_trans)) {
        DPRINTF(RubyHTM,
                "WARNING! Conflict with non-transactional code"
                " running in proc %d, addr %#lx\n",
                remote_id, addr);
#if 0
        if (makeLineAddress(m_xact_mgr->getHTM()->getFallbackLockPAddr()) != addr) {
            panic("TODO: Handle conflicts with non-transactional code");
        }
#endif
    }
    if (isRequesterStallsPolicy()) {
        // PiC-based policy
        if (m_policy == HtmPolicyStrings::requester_stalls_pic_base) {
            if (!isTransactionalRequest(remote_trans)) { // non-trans - trans
                shouldNack = false;
            } else {
                // If not stalled, nack remote
                // or if remote has lower priority, nack,
                // else do not nack (local will be aborted)
                shouldNack = chooseWinnerPriority(remote_trans, remote_priority,
                                                  sw_remote_priority);
            }
        } else {
            if (m_xact_mgr->isUnrollingLog()) {
                assert(!m_xact_mgr->config_lazyVM()); // LogTM
                shouldNack = true;
#if 0 // TODO: Check
        } else if (m_xact_mgr->isDoomed()) {
            shouldNack = false;
#endif
            } else if (!isTransactionalRequest(remote_trans) &&
                       !m_policy_nack_non_transactional) {
                if (!m_xact_mgr->config_lazyVM()) {
                    // LogTM
                    shouldNack = true; // Nack until old value restored
                    // Abort but keep nacking until old value restored
                    // from log (or read signature cleared before log unroll)
                    m_xact_mgr->setAbortFlag(addr, remote_id, remote_trans,
                                             remote_priority);
                    DPRINTF(RubyHTM,
                            "Abort due to non-transactional conflicting"
                            " access to address %#x from remote reader %d\n",
                            addr, machineIDToNodeID(remote_id));
                } else {
                    shouldNack = false;

                    DPRINTF(RubyHTM,
                            "Cannot nack non-transactional conflicting"
                            " access to address %#x from remote reader %d\n",
                            addr, machineIDToNodeID(remote_id));
                }
            } else { // trans-trans conflict
                shouldNack = true;
            }
        }
    } else if (m_policy == HtmPolicyStrings::committer_wins) {
        panic("committer_wins policy untested!\n");
        if (!m_xact_mgr->config_eagerCD() && // Lazy conflict detection
            m_xact_mgr->getXactLazyCommitArbiter()->validated()) {
            // Nack requests for blocks that have already been
            // validated (exclusive permissions acquired)
            shouldNack = true;
        } else {
            shouldNack = false;
        }
        return shouldNack;
    } else {
        shouldNack =
            ((ignoreNonTrans || isTransactionalRequest(remote_trans)) &&
             chooseWinnerPriority(remote_trans, remote_priority,
                                  sw_remote_priority));
    }
    DPRINTF(RubyHTM,
            "HTM: PROC %d detected conflict "
            "on address %#lx, %s remote proc %d %s (%s)\n",
            getProcID(), addr, shouldNack ? "nacks" : "aborted by",
            machineIDToNodeID(remote_id), remoteWriter ? "writer" : "reader",
            TransactionBit_to_string(remote_trans));
    DPRINTFR(FwdGraphTraceVerbose,
             "%d-Proc:%d-CONFLICT-%s-Remote:%d-%s-remote_prio:%s(%s)-local_"
             "prio:%s\n",
             curTick(), getProcID(), shouldNack ? "nacks" : "aborted by",
             machineIDToNodeID(remote_id), remoteWriter ? "writer" : "reader",
             remote_priority != NC_PRIO_VALUE ? to_string(remote_priority)
                                              : "NC",
             TransactionBit_to_string(remote_trans),
             m_xact_mgr->getTransactionPriority() != NC_PRIO_VALUE
                 ? to_string(m_xact_mgr->getTransactionPriority())
                 : "NC");

    // Finally, if req not nacked, resolve by aborting local tx
    if (!shouldNack) {
        m_xact_mgr->setAbortFlag(addr, remote_id, remote_trans, remote_priority,
                                 sw_remote_priority);
    } else {
        notifySendNack(addr, remote_timestamp, remote_id, remote_priority,
                       sw_remote_priority);
    }
    return shouldNack;
}

bool TransactionConflictManager::chooseWinnerPriority(TransactionBit remote_trans,
                                                      int remote_priority,
                                                      bool remote_sw_priority)
{
    int local_priority = m_xact_mgr->getTransactionPriority();
    bool local_sw_prio = m_xact_mgr->getSwTransactionPriority();
    switch (m_xact_mgr->getHTM()->getHwPrioMechanism())
    {
    // Hardware prioritized policies
    case HTM::HtmHwPrioMechanism::numeric:
    {
        bool shouldWin;
        // Nack if this tx is not consumer or
        // if the other transaction has lower prio
        if (isPowerTMPolicy())
        {
            if (local_sw_prio == remote_sw_priority)
            {
                // Either local and remote or not any of them have
                // sw priority, so check hw prio
                shouldWin = ((!m_xact_mgr->isConsumer() ||
                        remote_priority <= local_priority) &&
                        !remote_sw_priority);
            }
            else
            {
                // If this tx have sw priority means remote
                // does not
                shouldWin = local_sw_prio;
            }
        }
        else
        {
            shouldWin =
                ((m_xact_mgr->config_allowEarlyValueFwd() &&
                  !m_xact_mgr->isConsumer()) ||
                 (m_policy == HtmPolicyStrings::requester_stalls_pic_base &&
                  !m_xact_mgr->isStalled()) ||
                 remote_priority < local_priority);
        }
        if (m_xact_mgr->config_NCnotProducer()) {

            shouldWin = (local_priority == NC_PRIO_VALUE &&
                local_priority != remote_priority) ? false : shouldWin;
        }
        return shouldWin;
    }
    // No priorities or only software priorities
    case HTM::HtmHwPrioMechanism::no_prio:
    {
        if (isPowerTMPolicy())
        {
            if (isReqLosesPolicy())
            {
                // Send nack only if remote is not power
                if (remote_sw_priority) {
                    assert(!isPowered());
                    return false;
                } else {
                    return true;
                }
            }
            else if (isReqWinsPolicy())
            {
                // Send nack only if this tx is power
                return isPowered();
            }
        }
        else
        {
            // Req-loses, always nack
            // Req-wins, always abort
            if (isReqLosesPolicy())
            {
                return true;
            }
            else if (isReqWinsPolicy())
            {
                return false;
            }
        }
    }
        /* fall through */
    default:
        panic("Invalid fwd mechanism found!");
    }
}


bool
TransactionConflictManager::shouldNackLoad(Addr addr,
                                           MachineID remote_id,
                                           Cycles remote_timestamp,
                                           TransactionBit remote_trans,
                                           int remote_priority,
                                           bool remote_sw_priority)
{
  bool existConflict = m_xact_mgr->
    getXactIsolationManager()->isInWriteSetPerfectFilter(addr);
  if (existConflict) {
      assert(machineIDToMachineType(remote_id) == MachineType_L1Cache);
      assert(remote_timestamp > 0);
      DPRINTF(RubyHTM, "Conflict detected by shouldNackLoad,"
              " requestor=%d addr %#lx (%s)\n",
              machineIDToNodeID(remote_id), addr,
              TransactionBit_to_string(remote_trans));
      return shouldNackRemoteRequest(addr,remote_id,remote_timestamp,remote_trans,remote_priority,remote_sw_priority,false);
  }
  else { // No conflict
      return false;
  }
}

bool
TransactionConflictManager::shouldNackStore(Addr addr,
                                            MachineID remote_id,
                                            Cycles remote_timestamp,
                                            TransactionBit remote_trans,
                                            int remote_priority,
                                            bool remote_sw_priority,
                                            bool local_is_exclusive)
{
  bool local_is_writer = m_xact_mgr->getXactIsolationManager()->
      isInWriteSetPerfectFilter(addr);
  bool existConflict = local_is_writer ||
    m_xact_mgr->getXactIsolationManager()->
    isInReadSetPerfectFilter(addr);

  if (existConflict) {
      if (machineIDToMachineType(remote_id) == MachineType_L2Cache) {
          // LLC replacement
          assert(remote_timestamp == 0);
          DPRINTF(RubyHTM, "L2 cache eviction of transactional block"
                  " addr %#lx (local is writer: %d)\n", addr,
                  local_is_writer);
          if ((!local_is_writer &&
               !m_xact_mgr->config_allowReadSetL2CacheEvictions()) ||
              (local_is_writer &&
               !m_xact_mgr->config_allowWriteSetL2CacheEvictions())) {
              m_xact_mgr->setAbortFlag(addr, remote_id,
                                       remote_trans,
                                       remote_priority,
                                       remote_sw_priority);
          }
          return false;
      } else {
          assert(machineIDToMachineType(remote_id) == MachineType_L1Cache);
          assert(remote_timestamp > 0);
      }
      bool shouldNack = shouldNackRemoteRequest(addr,remote_id,remote_timestamp,
                        remote_trans,remote_priority, remote_sw_priority, true);
      
      
      if (isRequesterStallsPolicy()) {
        if (m_policy == HtmPolicyStrings::requester_stalls_pic_base) {
            // Everything is already covered in shouldNackRemoteRequest
        } else if (!isTransactionalRequest(remote_trans) &&
              !m_policy_nack_non_transactional) {
              if (!m_xact_mgr->config_lazyVM() && // LogTM
                  local_is_writer) {
                  shouldNack = true;

                  // Abort but keep nacking until old value restored
                  // from log (or read signature cleared before log unroll)
                  m_xact_mgr->setAbortFlag(addr, remote_id,
                                           remote_trans,
                                           remote_priority,
                                           remote_sw_priority);
                  DPRINTF(RubyHTM,"Abort due to non-transactional conflicting"
                          " access to address %#x from remote writer %d\n",
                          addr, machineIDToNodeID(remote_id));
              } else {
                  // Remote non-trans wins
                  shouldNack = false;

                  DPRINTF(RubyHTM,"Cannot nack non-transactional conflicting"
                          " access to address %#x from remote writer %d\n",
                          addr, machineIDToNodeID(remote_id));
              }
          } else { // trans-trans conflict
              shouldNack = true;
              if (m_policy == HtmPolicyStrings::requester_stalls_cda_hybrid &&
                  !local_is_writer &&
                  isRemoteOlder(getTimestamp(),
                                remote_timestamp, remote_id)) {
                  // See Bobba ISCA 2007: CDA hybrid allows an elder
                  // writer to simultanously abort a number of younger
                  // readers
                  shouldNack = false;
              }
          }
      }
      return shouldNack;
  } else { // No conflict
      return false;
  }
}

void TransactionConflictManager::notifySendNack(Addr addr,
                                                Cycles remote_timestamp,
                                                MachineID remote_id,
                                                int remote_priority,
                                                bool remote_sw_prio) {
    if (isRequesterStallsPolicy()) {
        if (m_policy == HtmPolicyStrings::requester_stalls_pic_base) {
            // Switch priority (PiC)
            // Note: sender side
            m_xact_mgr->switchHwPrio(remote_priority, remote_sw_prio, false);
            // Cycles are locally detected by the requester
        } else if (m_xact_mgr->getXactIsolationManager()
                       ->isInReadSetPerfectFilter(addr) ||
                   m_xact_mgr->getXactIsolationManager()
                       ->isInWriteSetPerfectFilter(addr)) {
            if (isRemoteOlder(getTimestamp(), remote_timestamp, remote_id)) {
                m_sentNack = true;
                m_sentNackAddr = addr;
                setPossibleCycle();
                DPRINTF(RubyHTM,
                        "HTM: PROC %d notifySendNack "
                        "sets possible cycle after conflict "
                        "with PROC %d for address %#x\n",
                        getProcID(), machineIDToNodeID(remote_id), addr);
            }
        }
    }
}

void
TransactionConflictManager::notifyReceiveNack(Addr addr,
                                              Cycles remote_timestamp,
                                              TransactionBit remote_trans,
                                              int remote_priority,
                                              bool remote_sw_prio,
                                              MachineID remote_id,
                                              bool writer) {
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    if (transactionLevel == 0) return;

    Cycles local_timestamp = getTimestamp();

    m_receivedNack = true;
    bool wasAborting = m_xact_mgr->isAborting();

    if (isRequesterStallsPolicy()) {
        if (m_policy == HtmPolicyStrings::requester_stalls_pic_base) {
            // Received a nack from a remote that had a higher
            // PiC or was not stalled. In any other case
            // remote would have aborted
            if (wasAborting) {
                // Transaction was already aborting
                // nothing to do here
            } else {
                // Transaction was not aborting
                // Check if it was legal NACK
                // (remote and local reqs raced in
                // protocol)
                if (m_xact_mgr->isIllegalNack(addr, remote_id, remote_priority,
                                              remote_sw_prio)) {
                    // Illegal nack
                    // for now just raise an abort signal
                    m_xact_mgr->setAbortFlag(addr, remote_id,
                                            remote_trans, remote_sw_prio,
                                            remote_priority);
                }
                // Note: receiver side
                int old_priority = m_xact_mgr->getTransactionPriority();
                m_xact_mgr->switchHwPrio(remote_priority, remote_sw_prio, true);
                DPRINTFR(FwdGraphTraceVerbose,
                         "%d-Proc:%d-RECV_NACK-Remote:%d-remote_prio:%s("
                         "%s)-local_old_prio:%s-local_new_"
                         "prio:%s\n",
                         curTick(), getProcID(),
                         machineIDToNodeID(remote_id),
                         remote_priority != NC_PRIO_VALUE
                             ? to_string(remote_priority)
                             : "NC",
                         TransactionBit_to_string(remote_trans),
                         old_priority != NC_PRIO_VALUE
                             ? to_string(old_priority)
                             : "NC",
                         m_xact_mgr->getTransactionPriority() != NC_PRIO_VALUE
                             ? to_string(m_xact_mgr->getTransactionPriority())
                             : "NC");
            }
            warn("NotifyReceiveNack - RequesterStalls\n");
        } else {
            warn("NotifyReceiveNack - RequesterStalls base not tested\n");
            if (possibleCycle() &&
                isRemoteOlder(local_timestamp, remote_timestamp, remote_id)) {
                if (m_xact_mgr->isUnrollingLog()) {
                    assert(!m_xact_mgr->config_lazyVM()); // LogTM
                    // Already aborted
                    return;
                }
                m_xact_mgr->setAbortFlag(m_sentNackAddr, remote_id,
                                         remote_trans, remote_sw_prio,
                                         remote_priority);
                DPRINTF(RubyHTM,
                        "HTM: PROC %d notifyReceiveNack "
                        "found possible cycle set after conflict "
                        "with PROC %d for address %#x, aborting "
                        "local (%d) as it not older  than remote (%d)"
                        "%s\n",
                        getProcID(), machineIDToNodeID(remote_id), addr,
                        local_timestamp, remote_timestamp,
                        (local_timestamp == remote_timestamp)
                            ? " and remote has lower proc ID"
                            : "");
            } else if (possibleCycle()) {
                DPRINTF(RubyHTM,
                        "HTM: PROC %d notifyReceiveNack "
                        "found possible cycle set after conflict "
                        "with PROC %d for address %#x, "
                        "but local (%d) is older than remote (%d) \n",
                        getProcID(), machineIDToNodeID(remote_id), addr,
                        local_timestamp, remote_timestamp);
            }
        }
        // For now we will allow several power Tx
        // And its behaviour should be imho: req-abort
    }
    if (isReqLosesPolicy() ||
          isPowerTMPolicy()) {
      // Whenever a nack is received, abort always.
      // Set abort flag will only notify cpu once. That's neat!
      m_xact_mgr->setAbortFlag(addr, remote_id,
                               remote_trans, remote_priority);
      DPRINTF(RubyHTM,"HTM: PROC %d notifyReceiveNack "
              "Received Nack from PROC %d (remote=%d, prio=%d)"
              "(remote_sw: %s, local_sw: %s) addr %#x "
              "aborting local transaction.\n",
              getProcID(), machineIDToNodeID(remote_id),
              remote_priority,
              m_xact_mgr->getTransactionPriority(),
              remote_sw_prio,
              m_xact_mgr->getSwTransactionPriority(),
              addr);
    }
    if (!wasAborting && m_xact_mgr->isAborting()) {
        m_abortedByNack = true;
    }
}


bool
TransactionConflictManager::hasHighestPriority()
{
    // Magic conflict detection at commit time
    std::vector<TransactionInterfaceManager*> mgrs =
        m_xact_mgr->getRemoteTransactionManagers();

    Cycles local_ts = getOldestTimestamp();
    for (int i=0; i < mgrs.size(); i++) {
        TransactionInterfaceManager* remote_mgr=mgrs[i];
        Cycles remote_ts = remote_mgr->
            getXactConflictManager()->getOldestTimestamp();
        if (remote_ts < local_ts)
            return false;
    }
    return true;
}

} // namespace ruby
} // namespace gem5
