/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_TRANSACTIONINTERFACEMANAGER_HH__
#define __MEM_RUBY_HTM_TRANSACTIONINTERFACEMANAGER_HH__

#include <stack>
#include <vector>

#include "mem/htm.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/htm/htm.hh"
#include "mem/ruby/protocol/ActionOnIllegalWriteData.hh"
#include "mem/ruby/protocol/TransactionBit.hh"
#include "mem/ruby/slicc_interface/RubySlicc_Util.hh"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "mem/ruby/system/TransactionalSequencer.hh"
#include "params/TransactionInterfaceManager.hh"
#include "sim/sim_object.hh"
#include "sim/system.hh"

namespace gem5
{
namespace ruby
{

class EagerTransactionVersionManager;
class LazyTransactionCommitArbiter;
class LazyTransactionVersionManager;
class TransactionInterfaceManager;
class TransactionConflictManager;
class TransactionIsolationManager;

#define _unused(x) ((void)(x))
#define INITIAL_PRIO_VALUE 0
#define NC_PRIO_VALUE INT_MIN

class TransactionInterfaceManager : public ClockedObject
{
public:
  PARAMS(TransactionInterfaceManager);
  TransactionInterfaceManager(const Params &p);
  ~TransactionInterfaceManager();

  void regStats() override;
  TransactionIsolationManager* getXactIsolationManager();
  TransactionConflictManager*  getXactConflictManager();
  EagerTransactionVersionManager*   getXactEagerVersionManager();
  LazyTransactionVersionManager*   getXactLazyVersionManager();
  LazyTransactionCommitArbiter* getXactLazyCommitArbiter();
  TransactionalSequencer *getSequencer();
  bool isRetryingTx();


  // Speculation checking
  bool canProduce();
  bool shouldBecomeProducer(Addr addr, TransactionBit remote_trans, int remote_priority, bool remote_sw_prio, MachineID consumer, bool writeInFlight);
  bool isBlockConsumed(Addr addr);
  bool isBlockProduced(Addr addr);
  void addProducer(Addr addr, MachineID producer);
  void addConsumer(Addr addr, MachineID consumer, int remote_priority, bool remote_sw_priority);
  void addBlockToNackReg(Addr addr, MachineID consumer);
  bool shouldForwardDataToConsumer(Addr addr, MachineID consumer, TransactionBit remote_trans, int remote_priority, bool remote_sw_prio, bool writeInFlight);
  bool shouldAcceptForwardedDataFromProducer(Addr addr, MachineID source, int remote_priority, bool remote_sw_prio);
  bool isIllegalNack(Addr addr, MachineID source, int remote_priority, bool remote_sw_priority);
  void notifyIllegalFailure() { setAbortCause(HTMStats::AbortCause::IllegalData); }
  bool isProducer();
  bool isConsumer();
  NodeID getConsumer();
  bool isStalled() {
    return getSequencer()->isStalled();
  }

  void notifyForwardedDataFromProducer(Addr addr, const DataBlock& datablock_ptr,
                                       MachineID producer,
                                       Cycles remote_timestamp,
                                       int remote_priority,
                                       bool remote_sw_prio);
  void setAccessOnForwardBlock(Addr addr, int offset, int len, bool write);
  void setAccessOnProducedBlock(Addr addr, int offset, int len, bool write);
  bool shouldNackLoad(Addr addr,
                      MachineID requestor,
                      Cycles remote_timestamp,
                      TransactionBit remote_trans,
                      int remote_priority,
                      bool remote_sw_priority);
  bool shouldNackStore(Addr addr,
                       MachineID requestor,
                       Cycles remote_timestamp,
                       TransactionBit remote_trans,
                       int remote_priority,
                       bool local_is_exclusive,
                       bool remote_sw_priority);
  void notifyReceiveNack(Addr addr, Cycles remote_timestamp,
                         TransactionBit remote_trans,
                         int remote_priority,
                         bool remote_sw_priority,
                         MachineID remote_id, bool writer);
  Cycles getOldestTimestamp();

  void setAbortFlag(Addr addr,
                    MachineID abortSource,
                    TransactionBit remote_trans,
                    int remote_priority,
                    bool remote_sw_priority = false,
                    bool capacity = false, bool wset = false,
                    bool dataStale = false, bool validation = false,
                    bool specDepthOverflow = false,
                    bool specMaxCapacityOverflow = false,
                    bool writeInFlight = false,
                    bool noFwdRead = false);
  void cancelTransaction(PacketPtr pkt);
  bool isCancelledTransaction();
  bool isTransactionAbortedByRemotePower();

  void setAbortCause(HTMStats::AbortCause cause);
  HTMStats::AbortCause getAbortCause() const;

  void profileHtmFailureFaultCause(HtmFailureFaultCause cause);
  HtmCacheFailure getHtmTransactionalReqResponseCode();
  void profileHtmFwdTransaction(bool aborted, int abort_cause_idx);

  AnnotatedRegion_t getWaitForRetryRegionFromPreviousAbortCause();
  void setController(AbstractController *_ctrl)
  { m_controller = _ctrl; }

  void beginTransaction(PacketPtr pkt);
  bool canCommitTransaction(PacketPtr pkt) const;
  void initiateCommitTransaction(PacketPtr pkt);
  bool atCommit() const {    return m_atCommit; };
  void commitTransaction(PacketPtr pkt);
  void abortTransaction(PacketPtr pkt);
  Addr getAbortAddress();
  void switchHwPrio(int remote_hw_prio, bool remote_sw_prio, bool isReceived);

  int getTransactionLevel();
  TransactionBit getTransactionBit();
  int getTransactionPriority();
  uint64_t getCurrentHtmTransactionUid() const;

  bool inTransaction();
  void isolateTransactionLoad(Addr physicalAddr);
  void addToRetiredReadSet(Addr physicalAddr);
  bool inRetiredReadSet(Addr physicalAddr);
  void isolateTransactionStore(Addr physicalAddr);

  void profileCommitCycle();

  void profileTransactionAccess(bool miss, bool isWrite,
                                MachineType respondingMach,
                                Addr addr, Addr pc, int bytes);

  bool isAborting();
  bool isDoomed(); // Aborting or bound to abort

  void setVersion(int version);
  int getVersion() const;

  void xactReplacement(Addr addr, MachineID source,
                       bool capacity = false,
                       bool dataStale = false,
                       bool spec = false);

  bool checkReadSignature(Addr addr);
  bool checkWriteSignature(Addr addr);

  void notifyMissCompleted(bool isStore, bool remoteConflict) {}
  bool hasConflictWith(TransactionInterfaceManager *another);

  void redirectStoreToWriteBuffer(PacketPtr pkt);
  void bypassLoadFromWriteBuffer(PacketPtr pkt, DataBlock& datablock);
  void mergeDataFromWriteBuffer(PacketPtr pkt, DataBlock& datablock);

  bool isLogReady();
  bool isAccessToLog(Addr addr);
  bool isEndLogUnrollSignal(PacketPtr pkt);
  void setupLogTranslation(Addr vaddr, Addr paddr);
  Addr translateLogAddress(Addr vaddr) const;
  Addr addLogEntry();
  int getLogNumEntries();
  bool isUnrollingLog();
  void endLogUnroll();

  void beginEscapeAction();
  void endEscapeAction();
  bool inEscapeAction();
  bool getSwTransactionPriority();

  std::string config_protocol() const {
      return m_ruby_system->getProtocol();
  }

  bool config_eagerCD() const {
      return m_htm->params().eager_cd;
  }

  std::string config_conflictResPolicy() const {
      return m_htm->params().conflict_resolution;
  }
  bool config_lazyVM() const {
      return m_htm->params().lazy_vm;
  }
  bool config_allowReadSetL0CacheEvictions() const {
      assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");
      return m_htm->params().allow_read_set_l0_cache_evictions;
  }
  bool config_allowReadSetL1CacheEvictions() const {
      assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");
      return m_htm->params().allow_read_set_l1_cache_evictions;
  }
  bool config_allowWriteSetL0CacheEvictions() const {
      assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");
      return m_htm->params().allow_write_set_l0_cache_evictions;
  }
  bool config_allowWriteSetL1CacheEvictions() const {
      assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");
      return m_htm->params().allow_write_set_l1_cache_evictions;
  }
  bool config_allowReadSetL2CacheEvictions() const {
      assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");
      return m_htm->params().allow_read_set_l2_cache_evictions;
  }
  bool config_allowWriteSetL2CacheEvictions() const {
      assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");
      return m_htm->params().allow_write_set_l2_cache_evictions;
  }
  bool config_transAwareL0Replacements() const {
      return m_htm->params().trans_aware_l0_replacements;
  }
  bool config_removeFalseSharingForward() const {
    return m_htm->params().remove_false_sharing_forward;
  }
  bool config_preciseReadSetTracking() const {
      return m_htm->params().precise_read_set_tracking;
  }
  bool config_reloadIfStale() const {
      return m_htm->params().reload_if_stale;
  }
  int config_maxPrioDiff() const {
      return m_htm->params().max_prio_diff;
  }

  int config_maxConsumers() const {
      return m_htm->params().max_consumers;
  }
  bool config_NCnotProducer() const {
    return m_htm->params().NC_not_producer;
  }

  int config_cyclesToValidate() const {
      return m_htm->params().cycles_to_validate;
  }
  bool config_noProduceOnRetry() const {
      return m_htm->params().no_produce_on_retry;
  }

  bool config_isReqLosesPolicy();

  bool isPowerMode();

  bool config_allowEarlyValueFwd() const {
      return fwdMechanism != HTM::HtmFwdMechanism::no_fwd;
  }

  bool shouldReloadIfStale();
  bool hasConsumedValidData() { return m_hasConsumedValidData; };
  bool hasValidatedAnyData() { return m_hasValidatedAnyData; };
  bool hasConsumedData();
  bool hasConsumedFromPower() { return m_dataConsumedFromPower; };
  MachineID hasConsumedDataFrom() {
      return m_hasReceivedDataFromProducer;
  }

  void scheduleValidationOfConsumedData(Addr addr, bool retry);

  bool validateDataBlock(Addr address, MachineID source,
                         DataBlock& cacheData,
                         const DataBlock& validData,
                         Cycles remote_timestamp,
                         bool dirty, bool isNack);

  ActionOnIllegalWriteData getActionOnIllegalData();
  int config_reloadIfStaleMaxRetries() const {
      return m_htm->params().reload_if_stale_max_retries;
  }
  bool config_enableValueChecker() const {
      return m_htm->params().value_checker;
  }
  bool config_enableIsolationChecker() const {
      return m_htm->params().isolation_checker;
  }
  int config_lazyCommitWidth() const {
      return m_htm->params().lazy_commit_width;
  }
  int config_nValFwd() const {
    return m_htm->params().n_val_fwd;
  }

  void profileFlRelease(long htmXid);
  void profileFlAcquisition(long htmXid);

  std::vector<TransactionInterfaceManager*>
     getRemoteTransactionManagers() const;

  HTM* getHTM() const { return m_htm; };

  int getProcID() const;
    class ValidateConsumedDataEvent : public Event
    {
      private:
        TransactionalSequencer *m_sequencer_ptr;
        Addr m_addr;

      public:
        ValidateConsumedDataEvent(TransactionalSequencer *_seq) :
            m_sequencer_ptr(_seq) {}
        void setAddr(Addr _addr) {
            m_addr = _addr;
        }
        void process() {
            m_sequencer_ptr->validateConsumedData(m_addr);
        }
    };

private:
  void discardWriteSetFromL1DataCache();
  void discardForwardedDataFromL1DataCache();

  const TransactionInterfaceManagerParams &_params;
  RubySystem *m_ruby_system;
  HTM * m_htm;
  int m_version;
  AbstractController *m_controller;
  TransactionalSequencer *m_sequencer;
  CacheMemory* m_dataCache_ptr;
  CacheMemory* m_l1Cache_ptr;

  TransactionIsolationManager     * m_xactIsolationManager;
  TransactionConflictManager      * m_xactConflictManager;
  EagerTransactionVersionManager  * m_xactEagerVersionManager;
  LazyTransactionVersionManager   * m_xactLazyVersionManager;
  LazyTransactionCommitArbiter    * m_xactLazyCommitArbiter;

  int      m_transactionLevel; // nesting depth, where outermost has depth 1
  uint64_t m_currentHtmUid;
  long     m_htmXid;
  int      m_escapeLevel; // nesting depth, where outermost has depth 1
  bool     m_abortFlag;
  bool     m_unrollingLogFlag;
  bool     m_atCommit;
  HTMStats::AbortCause m_abortCause;
  bool     m_abortSourceNonTransactional;
  HtmFailureFaultCause  m_lastFailureCause;  // Cause of preceding abort
  bool     m_capacityAbortWriteSet; // For capacity aborts, whether Wset/Rset
  Addr     m_abortAddress;
  int      m_prio_level;        // Emulate hardware priorities
  HTM::HtmFwdMechanism fwdMechanism;

  // Sanity checks
  std::map<Addr, char> m_writeSetDiscarded;

    Tick m_htmstart_tick;
    Counter m_htmstart_instruction;
    //! Histogram of cycle latencies of HTM transactions
    statistics::Histogram m_htm_transaction_cycles;
    //! Histogram of cycle latencies of committed HTM transactions
    statistics::Histogram m_htm_transaction_commit_cycles;
    //! Histogram of cycle latencies of committed HTM transactions
    statistics::Histogram m_htm_transaction_abort_cycles;
    //! Histogram of instruction lengths of HTM transactions
    statistics::Histogram m_htm_transaction_instructions;
    //! Causes for HTM transaction aborts
    Stats::Vector m_htm_transaction_abort_cause;
    //! Tx executed by type: aborted and committed
    Stats::Vector m_htm_transaction_executed_type_rate;
    //! Ideal times a producer modified fwd block
    Stats::Vector m_htm_transaction_power_modified;
    //! Consumer that turned into producer
    Stats::Vector m_htm_consumer_producer_transactions;
    //! Producer that consumed values
    Stats::Vector m_htm_producer_consumer_transactions;
    //! Size of both, write and read set at abort
    Stats::Distribution m_htm_accessed_mem_abort;
    Stats::Distribution m_htm_PiC_committed;
    Stats::Distribution m_htm_PiC_aborted;
    //! Consumer transactions, from how many transactions, commit or abort
    Stats::Distribution m_htm_consumer_transactions_nprod_abort;
    //! Consumer transactions, from how many transactions, commit or abort
    Stats::Distribution m_htm_consumer_transactions_nprod_commit;
    //! Producers and how many transactions they send data to
    Stats::Distribution m_htm_producer_transactions_nprod_abort;
    //! Producers and how many transactions they send data to
    Stats::Distribution m_htm_producer_transactions_nprod_commit;
    Stats::Distribution m_htm_producer_n_modified_produced_data;
    // Producers that were consumers, abort, commit and modified
    Stats::Vector m_htm_producers_consumers_tx;
    Stats::Vector m_htm_producers_tx;
    Stats::Vector m_htm_consumers_tx;
    Stats::Vector m_htm_powered_tx;
    // Transactions that consumed, validated and consumed again
    Stats::Vector m_htm_reconsumer_tx;
    Stats::Scalar m_htm_transactions_produced_consumed_same;
    // Performed forwardings
    Stats::Vector m_htm_forwardings;
    Stats::Histogram m_htm_percentage_executed_on_abort;

    Stats::Histogram m_htm_delayed_time_fwd_on_abort;

    //! Distribution of the cycles executed by the HTM per xid
    Stats::Distribution m_htm_cycles_xid;
    //! Distribution of the cycles aborted by the HTM per xid
    Stats::Distribution m_htm_aborted_cycles_xid;
    //! Distribution of the cycles committed by the HTM per xid
    Stats::Distribution m_htm_committed_cycles_xid;
    //! Distribution of the cycles executed by using fl per xid
    Stats::Distribution m_htm_fl_cycles_tx_xid;
    //! Distribution of the tx executed using fl per xid
    Stats::Distribution m_htm_executed_tx_xid;
    //! Distribution of the tx committed by the HTM per xid
    Stats::Distribution m_htm_committed_tx_xid;
    //! Distribution of the tx aborted by the HTM per xid
    Stats::VectorDistribution m_htm_aborted_tx_xid_cause;
    //! Distribution of the tx committed using fl per xid
    Stats::Distribution m_committed_fl_tx_xid;

    // value prediction
    std::set<MachineID> m_hasSentDataToConsumer;
    MachineID m_hasReceivedDataFromProducer;
    // Directory of FD blocks in L0, plus original value (data in L0
    // may have been modified by this transaction)
    struct DataForwarded
    {
        DataBlock data;
        bool modified;
        WriteMask mask;
        unsigned int nVal;
    };
    // Stats structure
    struct ConsumerInfo
    {
        // Could be more than one
        std::set<MachineID> producers;
        DataBlock data;
        Tick tickConsumed;
        Tick tickValidated;
        WriteMask wMask;
        WriteMask accessMask;
        unsigned int timesVal;
        bool locallyModified;

        ConsumerInfo(MachineID producer, DataBlock data, Tick tickConsumed) :
            producers({producer}),
            data(data),
            tickConsumed(tickConsumed),
            tickValidated(0),
            wMask(WriteMask()),
            timesVal(0),
            locallyModified(false)
            {}
    };
    struct FwdInfo
    {
        std::set<MachineID> consumers;
        bool modified;

        FwdInfo(MachineID consumer) :
            consumers({consumer})
            {}
    };
    // Stats structure
    struct ProducerInfo
    {
        std::set<MachineID> consumers;
        Tick tickProduced;
        int timesModifiedAfterSharing;
        std::vector<FwdInfo> forwardings;

        ProducerInfo(MachineID consumer, Tick tickProduced) :
            consumers({consumer}),
            tickProduced(tickProduced),
            timesModifiedAfterSharing(0),
            forwardings({FwdInfo(consumer)})
            {}
    };

    HTM::HtmHwPrioMechanism prioMechanism;
    ValidateConsumedDataEvent* validationEvent;
    std::map<Addr, DataForwarded> m_recvDataForwardedFromProducer;
    // Stats variable
    std::map<Addr, ConsumerInfo> m_allReceivedData;
    std::map<Addr, ProducerInfo> m_allSentData;
    std::stack<Tick> m_retriesTicks;
    bool m_hasConsumedValidData;
    bool m_hasValidatedAnyData;
    bool m_dataConsumedFromPower;
    bool m_consumerLowPrio;
    long m_currentValidationAddress;
    int  m_hwTransPrio;
    bool m_discardFwdData;
    bool m_illegalDataNack;
    bool m_hasModifiedProducedBlock;
    bool m_consumerTurnedProducer;
    bool m_producerTurnedConsumer;
    bool m_reconsumer;
    // Statistics for power-transactions + value prediction
    std::map<Addr, DataBlock> m_sentDataForwardedToConsumer;
    std::map<Addr, DataBlock> m_nackedAddressesReg;
};

} // namespace ruby
} // namespace gem5

#endif



