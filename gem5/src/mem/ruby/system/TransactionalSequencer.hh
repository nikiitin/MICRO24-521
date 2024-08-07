#ifndef __MEM_RUBY_SYSTEM_TRANSACTIONALSEQUENCER_HH__
#define __MEM_RUBY_SYSTEM_TRANSACTIONALSEQUENCER_HH__

#include <cassert>
#include <iostream>

#include "mem/htm.hh"
#include "mem/ruby/profiler/annotated_regions.h"
#include "mem/ruby/system/RubyPort.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "params/RubyTransactionalSequencer.hh"

namespace gem5
{

namespace ruby
{

class TransactionInterfaceManager;

// LogTM
struct LogRequestInfo
{
    PacketPtr logAddrPkt = NULL;
    PacketPtr logDataPkt = NULL;
    Addr logAddr = 0;
    Addr logData = 0;
    RequestStatus logAddrPktStatus = RequestStatus_NULL;
    RequestStatus logDataPktStatus = RequestStatus_NULL;
    //WriteCallbackArgs callbackArgs;
    /** Number of outstanding associated access to complete (LogTM) */
    int outstanding = 0;
    int completed = 0;
    int logIndex = -1;
    Addr vaddr = 0; // Program store
    Addr paddr = 0; // Program store
    LogRequestInfo(PacketPtr _logAddrPkt,
                   PacketPtr _logDataPkt,
                   uint64_t _htmUid,
                   int _logIndex,
                   Addr va, Addr pa)
        : logAddrPkt(_logAddrPkt),
          logDataPkt(_logDataPkt),
          logAddr(_logAddrPkt->getAddr()),
          logData(_logDataPkt->getAddr()),
          logIndex(_logIndex),
          vaddr(va), paddr(pa)
    {}
};

class TransactionalSequencer : public Sequencer
{
  public:
    PARAMS(RubyTransactionalSequencer);
    TransactionalSequencer(const Params &p);
    ~TransactionalSequencer();

    void print(std::ostream& out) const override;

        /* HTM extensions */
    RequestStatus insertRequest(PacketPtr pkt,
                                RubyRequestType primary_type,
                                RubyRequestType secondary_type) override;
    bool canMakeRequest(PacketPtr pkt) override;
    RequestStatus makeRequest(PacketPtr pkt);
    void setTransactionManager(TransactionInterfaceManager* xact_mgr);
    void setController(AbstractController* _cntrl);

    void failedCallback(Addr address, DataBlock& data,
                        Cycles remote_timestamp,
                        TransactionBit remote_trans,
                        int remote_priority,
                        bool remote_sw_priority,
                        MachineID nacker, bool write);
    void handleFailedCallback(SequencerRequest* srequest);

    void writeCallback(Addr address,
                       DataBlock& data,
                       const bool externalHit = false,
                       const MachineType mach = MachineType_NUM,
                       const Cycles initialRequestTime = Cycles(0),
                       const Cycles forwardRequestTime = Cycles(0),
                       const Cycles firstResponseTime = Cycles(0),
                       const bool noCoales = false) override;
    void readCallback(Addr address,
                      DataBlock& data,
                      const bool externalHit = false,
                      const MachineType mach = MachineType_NUM,
                      const Cycles initialRequestTime = Cycles(0),
                      const Cycles forwardRequestTime = Cycles(0),
                      const Cycles firstResponseTime = Cycles(0)) override;
    void invalidCallback(Addr address, DataBlock& data, bool write);
    bool isStalled() const { return m_stalled; };

    PacketPtr getPacketFromRequestTable(Addr address);
    // Value prediction via early forwarding of data
    void validateConsumedData(Addr addr);
private:
    // Private copy constructor and assignment operator
    TransactionalSequencer(const TransactionalSequencer& obj);
    TransactionalSequencer& operator=(const TransactionalSequencer& obj);

    void abortTransaction(PacketPtr pkt);
    void rubyHtmCallback(PacketPtr pkt);
    void hitCallback(SequencerRequest* srequest, DataBlock& data,
                     bool llscSuccess,
                     const MachineType mach, const bool externalHit,
                     const Cycles initialRequestTime,
                     const Cycles forwardRequestTime,
                     const Cycles firstResponseTime,
                     const bool was_coalesced) override;

    bool isWriteBufferHitEventScheduled() const
    { return writeBufferHitEvent.scheduled(); }

    bool notifyXactionEvent(PacketPtr pkt);
    void handleTransactionalRead(SequencerRequest *request,
                                   DataBlock& data, bool externalHit,
                                         const MachineType respondingMach);
    void handleTransactionalWrite(SequencerRequest *request,
                                  DataBlock& data, bool externalHit,
                                  const MachineType respondingMach);
    bool logTransactionalStores(SequencerRequest &seq_req,
                                Addr address, DataBlock& data);
    LogRequestInfo buildLogPackets(PacketPtr mainPkt, DataBlock& data);
    bool makeLogRequests(LogRequestInfo &logreqinfo);
    void handleStoresToLog(Addr address, PacketPtr pkt,
                        DataBlock& data);
    void failedCallbackCleanup(PacketPtr pkt);
    void handleLoggedStore(Addr address,
                           PacketPtr pkt,
                           DataBlock& data);
    bool interceptLogAccess(PacketPtr pkt);
    int numOutstandingWrites(Addr address);
    void suppressOutstandingRequests();
    int getNumReissuedRequests() const;
    void checkForStall(PacketPtr pkt);
    void updateReissueTime(Addr addr);
    void setFlagsPreIssueRequest(PacketPtr pkt,
                                 std::shared_ptr<RubyRequest>& msg);

    HTM * m_htm = NULL;
    TransactionInterfaceManager* m_xact_mgr = NULL;
    // LL (lazy CD) support

    bool m_commitPending = false;
    PacketPtr m_commitPendingPkt = NULL;
    bool m_failedCallback = false;
    bool m_specCallback = false;
    PacketPtr m_failedStorePkt = NULL;
    bool m_stalled = false;
    AnnotatedRegion m_lastStateBeforeStall = AnnotatedRegion_INVALID;

    uint64_t m_lastAbortHtmUid = 0;
    // Support for handling Locked RMW inside transactions
    std::map<Addr,int> outstandingTransLockedRMWAccesses;

    // LogTM (eager VM) RequestTable contains outstanding log requests
    // for pending program stores (per line address)
    std::unordered_map<Addr, std::list<LogRequestInfo>> m_logRequestTable;
    std::unordered_map<Addr, bool> m_pendingLogging;

    // Lazy-lazy HTM:
    class WriteBufferHitEvent : public Event
    {
      private:
        TransactionalSequencer *m_sequencer_ptr;
        PacketPtr m_pkt;

      public:
        WriteBufferHitEvent(TransactionalSequencer *_seq) :
            m_sequencer_ptr(_seq), m_pkt(NULL) {}
        void setPacket(PacketPtr _pkt) {
            assert(m_pkt ==  NULL);
            m_pkt = _pkt;
        }
        void clearPacket() {
            assert(m_pkt !=  NULL);
            m_pkt = NULL;
        }
        void process() {
            m_sequencer_ptr->writeBufferEvent(m_pkt);
        }
    };
    WriteBufferHitEvent writeBufferHitEvent;
    // WriteBufferHitEvent models access latency of the transactional
    // write buffer
    void writeBufferEvent(PacketPtr _pkt);
    void lazyCommitEvent(PacketPtr _pkt);

    class LazyCommitCheckEvent : public Event
    {
      private:
        TransactionalSequencer *m_sequencer_ptr;
        PacketPtr m_pkt;

      public:
        LazyCommitCheckEvent(TransactionalSequencer *_seq) :
            m_sequencer_ptr(_seq), m_pkt(NULL) {}
        void setPacket(PacketPtr _pkt) {
            assert(m_pkt ==  NULL);
            m_pkt = _pkt;
        }
        void clearPacket() {
            assert(m_pkt !=  NULL);
            m_pkt = NULL;
        }
        void process() {
            m_sequencer_ptr->lazyCommitEvent(m_pkt);
        }
    };
    LazyCommitCheckEvent lazyCommitCheckEvent;
};


} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_SYSTEM_TRANSACTIONALSEQUENCER_HH__
