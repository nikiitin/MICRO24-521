#include "mem/ruby/system/TransactionalSequencer.hh"

#include "arch/x86/ldstflags.hh"
#include "debug/ProtocolTrace.hh"
#include "debug/RubyHTM.hh"
#include "debug/RubyHTMlog.hh"
#include "debug/RubyHTMvaluepred.hh"
#include "debug/RubyHTMverbose.hh"
#include "debug/RubyPort.hh"
#include "mem/ruby/htm/EagerTransactionVersionManager.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/XactIsolationChecker.hh"
#include "mem/ruby/htm/XactValueChecker.hh"
#include "mem/ruby/htm/logtm.h"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/profiler/XactProfiler.hh"
#include "mem/ruby/protocol/HtmFailedInCacheReason.hh"
#include "mem/ruby/slicc_interface/RubySlicc_Util.hh"
#include "sim/system.hh"

namespace gem5
{

namespace ruby
{

TransactionalSequencer::TransactionalSequencer(const Params &p)
    : Sequencer(p),
      m_commitPending(false),
      m_failedCallback(false),
      m_specCallback(false),
      m_stalled(false),
      m_lastStateBeforeStall(AnnotatedRegion_INVALID),
      writeBufferHitEvent(this),
      lazyCommitCheckEvent(this)

{
    // TransactionalSequencer is only used by UMU protocols
    assert(m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");;
    m_htm = system->getHTM();
    assert(m_htm);
    assert(m_ruby_system->getProfiler()->hasXactProfiler());
}

TransactionalSequencer::~TransactionalSequencer()
{
}

void
TransactionalSequencer::print(std::ostream& out) const
{
    Sequencer::print(out);
}

void
TransactionalSequencer::setController(AbstractController* _cntrl)
{
    m_controller = _cntrl;
    assert(m_xact_mgr);
    m_xact_mgr->setController(m_controller);
}


void
TransactionalSequencer::
setTransactionManager(TransactionInterfaceManager* xact_mgr)
{
  m_xact_mgr = xact_mgr;
}

/* HTM extensions */
void
TransactionalSequencer::abortTransaction(PacketPtr pkt)
{
    assert(m_xact_mgr->isAborting() ||
           pkt->req->isHTMAbort());
    m_stalled = false;
    m_lastStateBeforeStall = AnnotatedRegion_INVALID;
    m_xact_mgr->abortTransaction(pkt);
    m_lastAbortHtmUid = pkt->getHtmTransactionUid();
    suppressOutstandingRequests();
    if (!m_htm->params().lazy_vm) { // LogTM
        assert(m_logRequestTable.empty());
    }
    // Unblock all blocked addresses (if abort occurs between
    // Locked_RMW_Read and Locked_RMW_Write
    if (outstandingTransLockedRMWAccesses.size() > 0) {
        assert(outstandingTransLockedRMWAccesses.size() == 1);
        Addr blockedAddr = outstandingTransLockedRMWAccesses.begin()->first;
        m_controller->unblock(blockedAddr);
        outstandingTransLockedRMWAccesses.erase(blockedAddr);
        DPRINTF(RubyHTM,
                "Abort while outstanding Locked RMW access,"
                " cache controller unblocked  addr %#x \n",
                blockedAddr);
        warn("Abort while outstanding Locked RMW instruction\n");
    }
}

bool
TransactionalSequencer::notifyXactionEvent(PacketPtr pkt)
{

  if (m_xact_mgr->isAborting()) {
      // Only abort command accepted
      if (pkt->req->isHTMAbort()) {
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   "HTM_ABORT" , "", "");
          DPRINTF(RubyHTM, "HTM_ABORT%s\n",
                  m_xact_mgr->isPowerMode() ? "_POWER" : "");
          abortTransaction(pkt);
      }
      else {
          // Other commands (e.g. HTM_COMMIT) get ignored if the abort
          // flag is found set. rubyHtmCallback next turns around
          // packet and notifies CPU that transaction has failed via
          // response code (see getHtmTransactionalReqResponseCode)
          // NOTE: The tcommit instruction appears as committed since
          // the HTM fault is triggered after the instruction retires,
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %s \n",
                   curTick(), m_version, "Seq",
                   "HTM_CMD" , "", "",  "(abort flag set)");
          if (m_commitPending) {
              assert(pkt->req->isHTMCommit());
              if (m_xact_mgr->canCommitTransaction(pkt)) {
                  // All commit actions completed (e.g. pending write
                  // misses), now rubyHtmCallback will signal abort
                  // via getHtmTransactionalReqResponseCode
                  m_commitPending = false;
              } else {
                  // If pending commit actions that prevent abort,
                  // schedule event to complete the abort the such
                  // outstanding actions are done
                  if (!lazyCommitCheckEvent.scheduled()) {
                      lazyCommitCheckEvent.setPacket(pkt);
                      schedule(lazyCommitCheckEvent,
                               clockEdge(Cycles(1)));
                      DPRINTF(RubyHTM, "Scheduled lazy commit check"
                              " event (abort)\n");
                  }
              }
          }
      }
      return true;
  }
  if (pkt->req->isHTMStart()) {
      DPRINTF(RubyHTM, "HTM_BEGIN\n");
      m_xact_mgr->beginTransaction(pkt);
      bool power = m_xact_mgr->isPowerMode();
      DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
               curTick(), m_version, "Seq",
               power ? "HTM_START_POW " : "HTM_START     " , "", "");
  } else if (pkt->req->isHTMCommit()) {
      // Store value returned by canCommit, used to signal CPU whether
      // xend must fault. Prevent calling canCommit again after
      // initiateCommitTransaction since it changes the returned value
      if (m_xact_mgr->canCommitTransaction(pkt)) {
          bool power = m_xact_mgr->isPowerMode();
          DPRINTF(RubyHTM, "HTM_COMMIT%s\n", power ? "_POWER" : "");
          m_xact_mgr->commitTransaction(pkt);
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   power ? "HTM_COMMIT_POW" : "HTM_COMMIT    "  , "", "");
          m_commitPending = false;
          m_lastStateBeforeStall = AnnotatedRegion_INVALID;
          m_stalled = false;
      } else {
          DPRINTF(RubyHTM, "HTM_COMMIT_PENDING\n");
          m_xact_mgr->initiateCommitTransaction(pkt);
          m_commitPending = true;
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   "HTM_COMMIT_PENDING" , "", "");
          // Schedule event to call makeRequest again on the next
          // cycle with this commit packet. rubyHtmCallback next will
          // observe commitPending active and thus will not delete the
          // packet nor send a response back.
          if (!lazyCommitCheckEvent.scheduled()) {
              lazyCommitCheckEvent.setPacket(pkt);
              schedule(lazyCommitCheckEvent,
                       clockEdge(Cycles(1)));
              DPRINTF(RubyHTM, "Scheduled lazy commit check event\n");
          }
      }
  } else if (pkt->req->isHTMCancel()) {
      // Explicit abort originated from a user instruction
      // (xabort/tcancel)
      DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
               curTick(), m_version, "Seq",
               "HTM_CANCEL" , "", "");
      assert(m_xact_mgr->getProcID() == m_version);
      m_xact_mgr->cancelTransaction(pkt);
  } else if (pkt->req->isHTMAbort()) {
      // CPU may only trigger HTM_ABORT before Ruby has set abort flag
      // for aborts whose cause is LSQ conflict or exception/interrupt
      if ((pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::LSQ) ||
          (pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::EXCEPTION) ||
          (pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::INTERRUPT) ||
          (pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::DISABLED)) {
          DPRINTF(RubyHTM, "HTM_ABORT due to %s\n",
                  htmFailureToStr(pkt->req->getHtmAbortCause()));
      } else {
          panic("HTM_ABORT must find abort flag set!\n");
      }
      DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
               curTick(), m_version, "Seq",
               "HTM_ABORT" , "", "");
      abortTransaction(pkt);
  } else if (pkt->req->isHTMIsolate()) {
      Addr addr = makeLineAddress(pkt->getAddr());
      if (!m_htm->params().precise_read_set_tracking) {
          /* With imprecise read sets, HTM_ISOLATE signal must always
             find the block already in the read set. This signal will
             simply add to the "retired read set", for profiling.

             NOTE: in O3CPU, store-to-load forwarding may lead to
             loads never accessing cache, so in this case the
             preceding store in program order adds the block to the
             write set, but the store may not have performed in cache
             yet..
          */
          if (!m_xact_mgr->checkReadSignature(addr)) {
              DPRINTF(RubyHTMverbose,
                      "Committed load to %#x (%#x) but block "
                      "does not belong to read set (store-to-load"
                      "forwarding?)\n",
                      pkt->getAddr(),
                      makeLineAddress(pkt->getAddr()));
              panic("Unexpected HTM_ISOLATE! (ST2LD forwarding?) \n");
          }
      }
      if (m_xact_mgr->checkReadSignature(addr)) {
          DPRINTF(RubyHTMverbose,
                  "Load to %#x (%#x) already in read set\n",
                  pkt->getAddr(),
                  makeLineAddress(pkt->getAddr()));
      } else {
          assert(m_htm->params().precise_read_set_tracking);
          /* With precise_read_set_tracking, transactional loads are
           * isolated (added to the read-set) when the load retires
           * from the processor via HTM_ISOLATE signal. If disabled,
           * the SR bit is set when the load executes, which may
           * "imprecisely" set SR bits for blocks targeted by loads
           * coming from mispredicted paths.
           */
          m_xact_mgr->isolateTransactionLoad(addr);

          // With precise read sets, if block not in the read set this
          // far, then it cannot be part of retired read set
          assert(!m_xact_mgr->inRetiredReadSet(addr));
          DPRINTF(RubyHTM,
                  "Committed load to %#x (%#x) adding block"
                  " address to read set\n",
                  pkt->getAddr(),
                  makeLineAddress(pkt->getAddr()));
      }
      if (!m_xact_mgr->inRetiredReadSet(addr)) {
          // Regardless of whether loads are isolated on issue or
          // retirement, always keep track of blocks referenced by
          // retired loads ("retired read set")
          m_xact_mgr->addToRetiredReadSet(addr);
          DPRINTF(RubyHTMverbose,
                  "Committed load to %#x (%#x) adding block"
                  " address to retired read set\n",
                  pkt->getAddr(),
                  makeLineAddress(pkt->getAddr()));
      }
  } else {
    panic("Unsupported transactional MemCmd\n");
  }

  return true;
}

void
TransactionalSequencer::failedCallback(Addr address,
                                       DataBlock& data,
                                       Cycles remote_timestamp,
                                       TransactionBit remote_trans,
                                       int remote_priority,
                                       bool remote_sw_priority,
                                       MachineID remote_nacker,
                                       bool write)
{
    m_failedCallback = true;
    auto &seq_req_list = m_RequestTable[address];
    assert(!seq_req_list.empty());
    SequencerRequest &seq_req = seq_req_list.front();
    PacketPtr pkt = seq_req.pkt;
    if (m_xact_mgr->config_allowEarlyValueFwd() &&
        m_xact_mgr->isBlockConsumed(address)) {
        // Validation over I state block!
        // Do not notify receive nack
        // If consumed data is in I the transaction
        // should be aborting
        if (!m_xact_mgr->isAborting()) {
            m_xact_mgr->setAbortFlag(address, remote_nacker,
                remote_trans, false, false, false, true);
        }

    } else if (pkt->getHtmTransactionUid() !=
                m_lastAbortHtmUid) {
        // We could end here if a nack from
        // another transaction reach this cache
        // It should not call notifyReceiveNack
        // as it is from another transaction and
        // callback should sink everything from this
        // address
        m_xact_mgr->notifyReceiveNack(address,
                                  remote_timestamp,
                                  remote_trans,
                                  remote_priority,
                                  remote_sw_priority,
                                  remote_nacker, write);
    }
    if (write) {
        // failed stores must not call hitCallback but instead be
        // retried without CPU intervention
        bool isWrite = false;
        bool isRMWRead = false;
        for (auto it=seq_req_list.begin();
             it != seq_req_list.end(); ++it) {
            if ((*it).pkt->isWrite()) {
                isWrite = true;
                break;
            } else if (((*it).m_type == RubyRequestType_RMW_Read) ||
                       ((*it).m_type == RubyRequestType_Locked_RMW_Read)) {
                isRMWRead = true;
            }
        }
        if (isWrite) {
            assert(m_failedStorePkt == NULL);
            if (seq_req.suppressed ||
                (m_xact_mgr->isAborting() &&
                 pkt->isHtmTransactional())) {
                // Remove this and all aliased reqs from Sequencer
                Sequencer::writeCallback(address, data);
                // writeCallback will eventually call
                // handleFailedCallback Sequencer::hitCallback
            } else {
                m_failedStorePkt = pkt;
                updateReissueTime(address);
                makeRequest(pkt);
            }
        } else { // No stores aliased with this RMW_Read

            // NOTE: RMW_Read are handled as stores by the protocol)
            // but must be retried following the "load path" if no
            // coalesced stores exist, since the memory request may
            // come from a speculative instruction subject to
            // squashing (do not retry indefinitely)
            assert(isRMWRead);
            // Remove reqs from Sequencer
            Sequencer::writeCallback(address, data);
        }
    } else {
        Sequencer::readCallback(address, data);
    }
    m_failedCallback = false;
}

void
TransactionalSequencer::updateReissueTime(Addr address)
{
    auto &seq_req_list = m_RequestTable[address];

    // Set reissue time for all requests aliased on this write, used
    // to keep track of failed requests and prevent deadlock event
    for (auto it=seq_req_list.begin();
         it != seq_req_list.end(); ++it) {
        (*it).reissue_time = curCycle();
    }
    // After reissue_time updated, check if there is a load aliased
    // with this store, which may have arrived at LSQ head by now (but
    // at LSQ head when sent from the CPU). For now, conservatively
    // consider a load as being at LSQ head if all outstanding
    // requests are being reissued
    if (getNumReissuedRequests() == m_RequestTable.size()) {
        for (auto it=seq_req_list.begin();
             it != seq_req_list.end(); ++it) {
            //if ((*it).pkt->isRead()) {
            // Doesn't make much sense to only
            // check for loads, since the store
            // may cause a stall too
            // which will be not detected
            (*it).pkt->setAtLSQHead(true);
            checkForStall((*it).pkt);
            //}
        }
    }
}

int
TransactionalSequencer::getNumReissuedRequests() const
{
    int count = 0;
    for (const auto &table_entry : m_RequestTable) {
        if (table_entry.second.front().reissue_time != Cycles(0)) {
            ++count;
        }
    }
    return count;
}
void
TransactionalSequencer::rubyHtmCallback(PacketPtr pkt)
{
    assert(pkt->isRequest());

    // rubyHtmCallback called by:
    //  a) HTM commands after notifyXactionEvent
    //  b) mem accesses that find abort flag set
    // Note: Only loads may signal abort back to CPU
    // Cache access for stores & ifetches simply suppressed
    if (pkt->req->isHTMCmd()) {
        DPRINTF(RubyHTMverbose, "rubyHtmcallback: start=%d, commit=%d, "
                "cancel=%d isolate=%d\n",
                pkt->req->isHTMStart(), pkt->req->isHTMCommit(),
                pkt->req->isHTMCancel(), pkt->req->isHTMIsolate());
    }
    else {
        assert(pkt->isHtmTransactional()); // Check: may fail...
        assert(m_xact_mgr->isAborting() ||
               m_lastAbortHtmUid == pkt->getHtmTransactionUid());
        if (pkt->isRead() && !pkt->req->isInstFetch()) {
            DPRINTF(RubyHTM, "rubyHtmcallback: load finds abort flag set\n");
        }
        DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %#x %s %s\n",
                 curTick(), m_version, "Seq", "Begin", "", "",
                 printAddress(pkt->req->getPaddr()),
                 "FAIL", " (abort flag set)");


    }

    // turn packet around to go back to requestor if response expected
    if (pkt->needsResponse()) {
        bool skip_response = false;
        // ArmISAInst::Tstart64::completeAcc expects that response
        // packets have data (payload is HtmFailedInCacheReason)
        uint8_t* dataptr = pkt->getPtr<uint8_t>();
        memset(dataptr, 0, pkt->getSize());

        // Turn around packet into response: if HtmCacheFailure
        // anything but NO_FAIL, will set flag FAILS_TRANSACTION in
        // packet to notify CPU that transaction has failed in cache.
        HtmCacheFailure response_code = HtmCacheFailure::NO_FAIL;
        if (pkt->isRead() && !pkt->req->isInstFetch()
            && !pkt->req->isHTMStart()) { // HTM begin cannot fault
            if (m_commitPending) {
                assert(pkt->req->isHTMCommit());
                skip_response = true;
            } else {
                response_code =
                    m_xact_mgr->getHtmTransactionalReqResponseCode();
            }
        }
        *dataptr = (uint8_t) response_code;

        if (!skip_response) {

            // First retrieve the request port from the sender State
            RubyPort::SenderState *senderState =
                safe_cast<RubyPort::SenderState *>(pkt->popSenderState());

            MemResponsePort *port =
                safe_cast<MemResponsePort*>(senderState->port);
            assert(port != nullptr);
            delete senderState;
            pkt->makeHtmTransactionalReqResponse(response_code);
            port->schedTimingResp(pkt, curTick());
        }
    } else {
        // First retrieve the request port from the sender State
        RubyPort::SenderState *senderState =
            safe_cast<RubyPort::SenderState *>(pkt->popSenderState());
        delete senderState;
        delete pkt;
    }

    trySendRetries();
}

void
TransactionalSequencer::setFlagsPreIssueRequest(PacketPtr pkt, std::shared_ptr<RubyRequest>& msg)
{
    Sequencer::setFlagsPreIssueRequest(pkt, msg);

    if (m_xact_mgr->config_allowEarlyValueFwd()) {
        Addr addr = makeLineAddress(pkt->getAddr());
        if (m_xact_mgr->isBlockConsumed(addr)) {
            if (pkt->isHtmForwardedDataValidation()) {
                assert(msg->m_Size == RubySystem::getBlockSizeBytes());
                // Validation: reset type so that they are intercepted
                // by protocol as they go through a different path as
                // regular loads/stores
                msg->m_Type = RubyRequestType_HTM_Validate;
            } else { // Program access: should find data in FM
                // Except if it is a retried store that is reissued
                // from readCallback -> yes!
                // Another case that can lead to this situation is
                // on aliasedNotIssued request that is no transactional
                // like IFETCH whenever a nack_spec was received.
                // IFETCH will be issued on the same cycle than
                // the callback was done so block will not
                // be updated until end of nack_spec event
                // This happened due to non-transactional requests
                // being able to use transactional data (makeRequest)
                // was not controlling that non-tx requests were not
                // aliased with tx requests
                bool isStore =
                    msg->m_Type != RubyRequestType_LD &&
                    msg->m_Type != RubyRequestType_Load_Linked &&
                    msg->m_Type != RubyRequestType_IFETCH;
                MemCmd cmd;
                DataBlock* dataPtr;
                bool hit = m_dataCache_ptr->
                    tryCacheAccess(addr, RubyRequestType_LD,
                                   dataPtr, false /*touch*/);
                // We should add an extra field to check that only
                // on a retried request we get here...
                assert(hit || isStore ||
                       (m_specCallback && !pkt->isHtmTransactional()));
            }
        }
    }
    if (msg->m_htmFromTransaction) {
        msg->m_Transactional = m_xact_mgr->getTransactionBit();
        msg->m_transactionalPriority = m_xact_mgr->getTransactionPriority();
        msg->m_swTransactionalPriority = m_xact_mgr->getSwTransactionPriority();
    }
}

// Insert the request in the request table. Return
// RequestStatus_Aliased if the entry was already present.
RequestStatus
TransactionalSequencer::insertRequest(PacketPtr pkt,
                                      RubyRequestType primary_type,
                                      RubyRequestType secondary_type)
{
    Addr address = makeLineAddress(pkt->getAddr());
    if (m_failedStorePkt == pkt) {
        // Clear
        m_failedStorePkt = NULL;
        // Request already inserted, can (re)issue
        return RequestStatus_Ready;
    }

    RequestStatus status = Sequencer::insertRequest(pkt,
                                                    primary_type,
                                                    secondary_type);
    if (pkt->isHtmTransactional() &&
        (primary_type == RubyRequestType_Locked_RMW_Read ||
         primary_type == RubyRequestType_Locked_RMW_Write)) {
        // This is generally the indication that something has gone
        // south and a library function calll is acquiring
        // locks... (e.g. assert that has failed, etc.)
        assert(pkt->req->isLockedRMW());
        warn("WARNING: %s access within transactional"
             " boundaries - PC %#x vaddr %#x paddr %#x\n",
             RubyRequestType_to_string(primary_type),
             pkt->req->getPC(), pkt->req->getVaddr(), address);
        DPRINTF(RubyHTM,
                "%s access within transactional boundaries"
                " - PC %#x vaddr %#x paddr %#x\n",
                RubyRequestType_to_string(primary_type),
                pkt->req->getPC(), pkt->req->getVaddr(), address);
    }
    if (!m_htm->params().lazy_vm) { // LogTM
        if (pkt->isWrite() &&
            pkt->isHtmTransactional() &&
            !m_xact_mgr->checkWriteSignature(address)) {
            // Transactional store to block not in Wset: if issued,
            // set pendingLogging, which reserves enough MSHRs to
            // ensure that log requests can issue when this store
            // completes
            if (status == RequestStatus_Ready) {
                // Store miss
                assert(m_pendingLogging.find(address) ==
                       m_pendingLogging.end());
                m_pendingLogging[address]=true;
            } else if (status == RequestStatus_Aliased) {
                // Store aliased on pending miss: log if first store
                // to block (aliased with earlier load)
                auto &seq_req_list = m_RequestTable[address];
                assert(seq_req_list.size() > 1);
                int numStoresFound = numOutstandingWrites(address);
                // At least the current store
                assert(numStoresFound > 0);
                if (numStoresFound == 1) {
                    // This is the first store, needs logging
                    assert(m_pendingLogging.find(address) ==
                           m_pendingLogging.end());
                    m_pendingLogging[address]=true;
                } else { // Coalesced into a previous store that
                         // already booked the MSHRs for logging
                    assert(m_pendingLogging.find(address) !=
                           m_pendingLogging.end() ||
                           m_logRequestTable.find(address) !=
                           m_logRequestTable.end());
                }
            } else { // Store was not issued
                assert(status == RequestStatus_AliasedNotIssued);
            }
        }
    }

    if (!m_htm->params().precise_read_set_tracking) {
        /* If no precise read-set tracking, cache blocks are added to
         * read set as soon as load access begins. This forces the
         * abort of transactions that see conflicting snoops (Invs)
         * for loads that are not yet retired from the processor.
         * Note that with an O3 CPU model, it may lead to SR bits
         * being set despite not running a transaction.
         */
        if (pkt->isHtmTransactional() && pkt->isRead()) {
            // Trans loads added to read set as soon as issued.
            assert(!pkt->isWrite());
            assert(isDataReadRequest(secondary_type) ||
                   (primary_type == RubyRequestType_RMW_Read) ||
                   (primary_type == RubyRequestType_Locked_RMW_Read));

            // It is OK to receive RequestStatus_Aliased, it can be
            // considered Issued
            if ((status == RequestStatus_Ready) ||
                (status == RequestStatus_Aliased)) {

                Addr addr = makeLineAddress(pkt->getAddr());
                if (m_xact_mgr->inTransaction()) {
                    if (m_xact_mgr->checkReadSignature(addr)) {
                        DPRINTF(RubyHTMverbose,
                                "Load to %#x (%#x) already in read set\n",
                                pkt->getAddr(), addr);
                    } else {
                        DPRINTF(RubyHTM,
                                "Issued load to %#x (%#x), adding block"
                                " address to read set\n",
                                pkt->getAddr(), addr);
                        m_xact_mgr->isolateTransactionLoad(addr);
                    }
                } else {
                    panic("Transactional store outside boundaries!");
                    DPRINTF(RubyHTM,
                            "Transactional load to %#x (%#x)"
                            " outside transaction boundaries\n",
                            pkt->getAddr(),
                            makeLineAddress(pkt->getAddr()));
                    m_xact_mgr->isolateTransactionLoad(addr);
                }
            }
        }
    }
    return status;
}
bool
TransactionalSequencer::canMakeRequest(PacketPtr pkt)
{
    // HTM abort signals must be allowed to reach the Sequencer
    // the same cycle they are issued. They cannot be retried.
    int num_reserved_mshrs = 0;
    if (!m_htm->params().lazy_vm) { // LogTM
        // Need two extra MSHRs for each store with log actions
        // pending.
        num_reserved_mshrs = 2*m_pendingLogging.size();
        if (pkt->isWrite() &&
            pkt->isHtmTransactional()) {
            Addr address = makeLineAddress(pkt->getAddr());
            if (!m_xact_mgr->checkWriteSignature(address) &&
                m_pendingLogging.find(address) == m_pendingLogging.end()) {
                // This trans store can issue if it can reserve 2 extra
                // MSHRs for its logging actions
                num_reserved_mshrs += 2;
            }
        } else if (pkt->isHtmStoreToLog()) {
            assert(!pkt->isHtmTransactional());
            Addr store_addr = makeLineAddress(pkt->getHtmLoggedStoreAddr());
            // These are the two logging requests: a pair of the
            // reserved mshrs was allocated for them, so exclude them
            assert(m_pendingLogging[store_addr]);
            num_reserved_mshrs -= 2;
        }
    }
    // Failed stores can always issue since they have already an
    // allocated MSHR (not removed  failedCallback)
    if (m_failedStorePkt == pkt) {
        return true;
    }
    if ((m_outstanding_count +
         num_reserved_mshrs >= m_max_outstanding_requests) &&
        !pkt->req->isHTMAbort()) {
        return false;
    } else {
        return true;
    }
}

RequestStatus
TransactionalSequencer::makeRequest(PacketPtr pkt)
{
    if (pkt->req->isHTMCmd()) {
        // HTM command: Intercept and notify transaction manager
        notifyXactionEvent(pkt);

        // All HTM commands need to callback CPU immediately
        rubyHtmCallback(pkt);

        // Pretend this request issued so that RubyPort does not try
        // to send it again later
        return RequestStatus_Issued;
    }
    else if (pkt->isHtmTransactional() &&
             (m_xact_mgr->isAborting() ||
              pkt->getHtmTransactionUid() == m_lastAbortHtmUid)) {
        // Transactional access that finds abort flag set or lingering
        // access after transaction has already aborted: Callback CPU
        // immediately. If access is load, abort signal sent back to
        // CPU by setting the htmReturnReason in the response packet
        // (ifetch and stores always return HtmCacheFailure::NO_FAIL)
        if (m_commitPending) {
            panic("Unexpected abort while commit pending!\n");
        }
        assert(m_failedStorePkt == NULL);
        rubyHtmCallback(pkt);
        return RequestStatus_Issued;
    } else {
        if (pkt->req->hasVaddr()){
            if (pkt->req->getVaddr() == m_htm->getFallbackLockVAddr()) {
                // Intercept access to fallback lock and obtain physical addr
                m_htm->setFallbackLockPAddr(pkt->req->getPaddr());
            } else if (!m_htm->params().lazy_vm) {
                bool noaccess = interceptLogAccess(pkt);
                if (noaccess) { // No cache access required
                    ruby_hit_callback(pkt);
                    testDrainComplete();
                    return RequestStatus_Issued;
                }
            }
        }
        uint32_t flags = pkt->req->getFlags();
        bool is_trans_rmw_read = false;
        if (pkt->isHtmTransactional() &&
            system->getArch() == Arch::X86ISA &&
            pkt->isRead() &&
            (flags & (X86ISA::StoreCheck << X86ISA::FlagShift))) {
            // Careful with rmw macroops in x86: ld microop is sent to
            // protocol as ST, but otherwise handle it as a trans load
            is_trans_rmw_read = true;

            DPRINTF(RubyHTM, "Transactional load is RMW_Read, "
                    "vaddr %#x paddr %#x\n",
                    pkt->req->getVaddr(),
                    pkt->req->getPaddr());
        }
        if (m_htm->params().lazy_vm &&
            !m_htm->params().eager_cd && // LL system
            pkt->isHtmTransactional()) {
            if (pkt->isWrite()) {
                assert(m_xact_mgr);
                if (m_xact_mgr->atCommit()) {
                    // LL: write buffer contents being written back to cache,
                    // issue write request
                    DPRINTF(RubyHTM,
                            "Store to %#x while flushing write buffer\n",
                            pkt->getAddr());
                }
                else {
                    // LL: Redirect stores to write buffer
                    // Schedule hit callback for the next cycle
                    if (isWriteBufferHitEventScheduled()) {
                        return RequestStatus_BufferFull;
                    }
                    m_xact_mgr->redirectStoreToWriteBuffer(pkt);

                    writeBufferHitEvent.setPacket(pkt);

                    // For now assume that write buffer has same latency
                    // of Dcache
                    Cycles wb_latency = m_controller->
                        mandatoryQueueLatency(RubyRequestType_ST);
                    schedule(writeBufferHitEvent,
                             clockEdge(wb_latency));
                    return RequestStatus_Issued;
                }
            } else if (pkt->req->isLockedRMW()) {
                panic("Transactional Locked RMW not tested!\n");
            } else if (is_trans_rmw_read) {
                pkt->req->clearFlags(X86ISA::StoreCheck << X86ISA::FlagShift);
                DPRINTF(RubyHTM, "Transactional load is RMW_Read, "
                        "StoreCheck flag was cleared from req "
                        "vaddr %#x paddr %#x\n",
                        pkt->req->getVaddr(),
                        pkt->req->getPaddr());
            }
        }
        return Sequencer::makeRequest(pkt);
    }
}

void
TransactionalSequencer::failedCallbackCleanup(PacketPtr pkt)
{
    assert(m_failedCallback);
    if (!m_htm->params().lazy_vm) { // LogTM: free reserved MSHRs via
                                    // "pendingLogging" map.
        // Done after ruby_hit_callback in case a request is retried
        Addr address = makeLineAddress(pkt->getAddr());
        bool needsLogging =
            m_pendingLogging.find(address) != m_pendingLogging.end();
        if (needsLogging) {
            auto &seq_req_list = m_RequestTable[address];
            if (!pkt->isWrite()) { // Aliased loads on nacked store miss
                assert(seq_req_list.size() > 1);
            } else { // Nacked write
                assert(pkt->isHtmTransactional() &&
                       !m_xact_mgr->checkWriteSignature(address));
                if (seq_req_list.size() == 1) { // Only write
                    // Clear from pending to free "reserved MSHRs"
                    // see (canMakeRequest)
                    m_pendingLogging.erase(address);
                } else { // Aliased requests: if other stores
                    // coalesced into this miss, do not erase
                    int numStoresFound = numOutstandingWrites(address);
                    if (numStoresFound == 1) {
                        // This is the only store, other aliased reqs are
                        // loads: erase from pending logging
                        m_pendingLogging.erase(address);
                    } else { // There are other stores apart from this
                        // one that failed: retain entry
                        assert(numStoresFound > 1);
                    }
                }
            }
        }
    }
}

void
TransactionalSequencer::checkForStall(PacketPtr pkt)
{
    if (pkt->isAtLSQHead() &&
        !m_xact_mgr->isAborting() &&
        !m_stalled &&
        (!pkt->isHtmTransactional() ||
         m_lastAbortHtmUid != pkt->getHtmTransactionUid())) {
        // Enter stall
        m_stalled = true;
        assert(m_lastStateBeforeStall == AnnotatedRegion_INVALID);
        m_lastStateBeforeStall = m_ruby_system->getProfiler()->
            getXactProfiler()->getCurrentRegion(m_version);
        Addr address = makeLineAddress(pkt->getAddr());
        DPRINTF(RubyHTM,
                "Stalled (nacked) thread after failing to perform"
                " access to block addr %#x\n", address);
        m_ruby_system->getProfiler()->
            getXactProfiler()->moveTo(m_version,
                                      pkt->isHtmTransactional() ?
                                      AnnotatedRegion_STALLED :
                                      AnnotatedRegion_STALLED_NONTRANS);
    }
}


void
TransactionalSequencer::handleFailedCallback(SequencerRequest* srequest)
{
    PacketPtr pkt = srequest->pkt;
    Addr address = makeLineAddress(pkt->getAddr());
    assert(m_failedCallback);
    if (pkt->isWrite() || m_xact_mgr->isAborting()) {
        // Failed writes should never go through this path unless we
        // are aborting and want to "sink" them instead of retrying
        // Set the HtmTransactionFailedInCache in the packet, the CPU
        // expects it set for writes with HtmFailedCacheAccess set
        if (pkt->isWrite()) {
            if (m_xact_mgr->config_allowEarlyValueFwd() &&
                !m_xact_mgr->isBlockConsumed(address) &&
                pkt->isHtmForwardedDataValidation()) {
                // We do not want to manage validation messages
                // on failed callback. In any case it is an eviction
                // on the current transaction will cause an abort
                // so just return to hitCallback
                return;
            }
            assert(m_xact_mgr->isAborting() ||
                m_lastAbortHtmUid == pkt->getHtmTransactionUid());
        }
        HtmCacheFailure reason =
            m_xact_mgr->getHtmTransactionalReqResponseCode();
        pkt->setHtmTransactionFailedInCache(reason);
    }
    // Handle nacking of Locked_RMW accesses.
    // address variable here is assumed to be a line address, so when
    // blocking buffers, must check line addresses.
    if (srequest->m_type == RubyRequestType_Locked_RMW_Read) {
        assert(m_controller->isBlocked(address));
        m_controller->unblock(address);
        DPRINTF(RubyHTM,
                "Failed callback for Locked_RMW_Read to addr %#x"
                " - unblocking queue\n", address);
    } else {
        assert(!m_controller->isBlocked(address));
    }
    if (pkt->req->hasVaddr() &&
        pkt->req->getVaddr() == m_htm->getFallbackLockVAddr()) {
        DPRINTF(RubyHTM,
                "Failed access to fallback lock!"
                " - PC %#x vaddr %#x\n",
                pkt->req->getPC(),
                pkt->req->getVaddr());
        // Requester-stalls policies that prevent the lock from
        // being acquired/released are subject to deadlocks
        // without adequate management of conflicts with a
        // non-transactional requester
        warn("Failed access to fallback lock!"
             " - PC %#x vaddr %#x\n",
             pkt->req->getPC(),
             pkt->req->getVaddr());
    }
    // Skip all the following actions and do not call
    // Sequencer::hitCallback
    pkt->setHtmFailedCacheAccess(true);
    checkForStall(pkt);
    if (m_xact_mgr->config_allowEarlyValueFwd() &&
        pkt->isHtmForwardedDataValidation())
    {
        // Corner case for validation over
        // invalid blocks and aliased reads or
        // writes. As we cannot handle it in
        // hit callback, do not call readCallback,
        // do a failed callback

        // If validation packet just delete it
        // and do not call ruby hit callback
        // either cleanup
        delete pkt;
        return;
    }

    ruby_hit_callback(pkt);
    failedCallbackCleanup(pkt);
    testDrainComplete();
    return;
}

void
TransactionalSequencer::hitCallback(SequencerRequest* srequest,
                                    DataBlock& data,
                                    bool llscSuccess,
                                    const MachineType mach,
                                    const bool externalHit,
                                    const Cycles initialRequestTime,
                                    const Cycles forwardRequestTime,
                                    const Cycles firstResponseTime,
                                    const bool was_coalesced)
{
    PacketPtr pkt = srequest->pkt;
    if (m_failedCallback) {
        handleFailedCallback(srequest);
        return;
    }
    if (m_xact_mgr->config_allowEarlyValueFwd()) {
        Addr addr = makeLineAddress(pkt->getAddr());
        if (pkt->isHtmForwardedDataValidation()) {
            // Validation request that got data+permissions or nack
            assert(pkt->getSize() == RubySystem::getBlockSizeBytes());
            if (m_xact_mgr->isAborting()) {
                // No need to validate/schedule events if already
                // aborting. Will be removed from the "consumed data
                // buffer" upon abort
                DPRINTF(RubyHTMvaluepred, "Validation callback ignored"
                        " for block %#x (tx aborting)\n",
                        addr);
            } else if (!m_xact_mgr->isBlockConsumed(addr)) {
                // Block was removed from "consumed data buffer":
                // either got data+permissions or transaction has
                // aborted
                if (m_xact_mgr->inTransaction() &&
                    pkt->getHtmTransactionUid() == m_xact_mgr->getCurrentHtmTransactionUid()) {
                    DPRINTF(RubyHTMvaluepred, "Validation callback for"
                            " block %#x, got data+permissions \n", addr);
                    if (!m_xact_mgr->isConsumer()) {
                        if (!m_xact_mgr->atCommit() &&
                            !m_xact_mgr->isAborting()) {
                            m_ruby_system->getProfiler()->
                                getXactProfiler()->moveTo(m_version,
                                        AnnotatedRegion_TRANSACTIONAL);
                        }
                    }
                } else {
                    assert(pkt->getHtmTransactionUid() <= m_lastAbortHtmUid);
                    // This may happen if a validation races with
                    // abort and the block is deallocated while the
                    // validation is ongoing: I think it's ok to
                    // ignore it unless we have aliased memory accesses
                    // as receiving the validation will do hitCallback
                    // for them too without noticing it is from last
                    // aborted transaction
                    warn("Lingering validation from another transaction"
                    " reached a newer transaction\n");
                    // Can happen even after removing coalesced accesses as
                    // the lingering one did not finish yet
                    DPRINTF(RubyHTMvaluepred, "Validation cannot complete"
                            " for block %#x (tx aborted)\n",
                            addr);
                }
            }
            // Regardless of validation result, no callback
            // needed: Delete packet and return
            delete pkt;
            return;
        } else if (m_xact_mgr->isBlockConsumed(addr)) {
            m_xact_mgr->setAccessOnForwardBlock(addr,
                getOffset(pkt->getAddr()),
                pkt->getSize(),
                pkt->isWrite());
            pkt->setHtmForwardedData(true);
        } else if (m_xact_mgr->isBlockProduced(addr)) {
            m_xact_mgr->setAccessOnProducedBlock(addr,
                getOffset(pkt->getAddr()),
                pkt->getSize(),
                pkt->isWrite());
        }
    }
    if (pkt->isHtmStoreToLog()) {
        ///////////
        // Intercept stores to the log
        assert(!m_failedCallback);
        // Logging stores (addr + data): one copies old value into
        // the log and the other the virtual address targeted by
        // the program store. When both are complete, will
        // directly callback Sequencer::writeCallback
        Addr address = makeLineAddress(pkt->getAddr());
        handleStoresToLog(address, pkt, data);
        // No callback needed
        delete pkt;
        return;
    }

    if (m_stalled && pkt->isAtLSQHead() &&
        !m_xact_mgr->isAborting() &&
        (!pkt->isHtmTransactional() ||
         m_lastAbortHtmUid != pkt->getHtmTransactionUid())) {

        if (pkt->isHtmTransactional()) {
            assert(m_xact_mgr->inTransaction());
            if (m_xact_mgr->config_allowEarlyValueFwd()) {
                assert(
                    m_lastStateBeforeStall == AnnotatedRegion_TRANSACTIONAL ||
                    m_lastStateBeforeStall == AnnotatedRegion_TRANSACTIONAL_SPECULATING);
            } else {
                assert(m_lastStateBeforeStall == AnnotatedRegion_TRANSACTIONAL);
            }
        }
        m_ruby_system->getProfiler()->
            getXactProfiler()->moveTo(m_version,
                                      m_lastStateBeforeStall);
        // Reset
        m_lastStateBeforeStall = AnnotatedRegion_INVALID;
        m_stalled = false;
        Addr address = makeLineAddress(pkt->getAddr());
        DPRINTF(RubyHTM,
                "Stalled (nacked) thread successfully completed"
                " access to block addr %#x\n", address);
    }

    bool bypassTransLoad = false;
    if (pkt->isHtmTransactional()) {
        bool read = ((srequest->m_type == RubyRequestType_LD) ||
                     (srequest->m_type == RubyRequestType_Load_Linked) ||
                     (srequest->m_type == RubyRequestType_Locked_RMW_Read) ||
                     (srequest->m_type == RubyRequestType_RMW_Read) ||
                     (srequest->m_type == RubyRequestType_IFETCH));

        if (read) {
            handleTransactionalRead(srequest,
                                    data,
                                    externalHit,
                                    mach);
        }
        else if (srequest->m_type == RubyRequestType_RMW_Read ||
                 srequest->m_type == RubyRequestType_Locked_RMW_Read) {
            // Handle RMW_Read's with care due to writeback of dirty data
            // before it gets speculatively modified
            panic("Not tested!\n");
            handleTransactionalRead(srequest,
                                    data,
                                    externalHit,
                                    mach);
        }
        else {
            handleTransactionalWrite(srequest,
                                     data,
                                     externalHit,
                                     mach);
        }
        if (m_htm->params().lazy_vm &&
            !m_htm->params().eager_cd) {
            // LL system
            if (m_xact_mgr->atCommit()) {
                if (read) {
                    // Corner case: in O3CPU, transactional loads from
                    // mispredicted paths can cause misses that
                    // complete in cache after the instruction was
                    // squashed while at commit
                    DPRINTF(RubyHTM,
                            "Unexpected load hit to %#x during"
                            " lazy commit (squashed load?)\n",
                            pkt->getAddr());
                } else {
                    // Write performed in cache during lazy commit:
                    // now copy data from write buffer to datablock
                    m_xact_mgr->mergeDataFromWriteBuffer(pkt, data);
                }
                // No hitCallback needed, but handle retries in cache
                // IFETCH on xend could not be issued
                trySendRetries();
                return;
            } else if (read) {
                // Bypass from write buffer: copy data into packet
                // after hitCallback (overwrite old data from cache)
                bypassTransLoad = true;
            } else { // No callback expected from writes before commit
                assert(false);
            }
        }

    }
    if (pkt->isWrite()) {
        if (m_xact_mgr->config_enableValueChecker()) {
            uint8_t *data_ptr=(pkt->getPtr<uint8_t>());
            m_ruby_system->getXactValueChecker()->
                notifyWrite(m_version, pkt->isHtmTransactional(),
                            pkt->getAddr(), pkt->getSize(), data_ptr);
        }
    } else {
        if (m_xact_mgr->config_enableValueChecker()) {
            assert(m_xact_mgr);
            Addr request_address(pkt->getAddr()); // Word address
            _unused(request_address);
            bool checkPassed =
                m_ruby_system->getXactValueChecker()->
                xactValueCheck(m_version, request_address, pkt->getSize(),
                               data.getData(getOffset(request_address),
                                            pkt->getSize()));
            if (!checkPassed) {
                DPRINTF(RubyHTM,
                        "Load to %#x (%#x)"
                        " has failed value check!\n",
                        pkt->getAddr(),
                        makeLineAddress(pkt->getAddr()));
                panic("Value check failed!\n");
            }
        }
    }
    if (m_xact_mgr->config_enableIsolationChecker() &&
        !m_xact_mgr->isAborting()) {
        bool passed = m_ruby_system->getXactIsolationChecker()->
            checkXACTIsolation(m_version, pkt->getAddr(),
                               pkt->isHtmTransactional(),
                               srequest->m_type);
        if (!passed) {
            panic("Transaction isolation check failed!\n");
        }
    }
    Sequencer::hitCallback(srequest, data,
                           llscSuccess,
                           mach, externalHit,
                           initialRequestTime,
                           forwardRequestTime,
                           firstResponseTime,
                           was_coalesced);
    if (bypassTransLoad) {
        m_xact_mgr->bypassLoadFromWriteBuffer(pkt, data);
    }

}

void
TransactionalSequencer::handleTransactionalWrite(SequencerRequest *request,
                                          DataBlock& data, bool externalHit,
                                          const MachineType respondingMach)
{
    PacketPtr pkt = request->pkt;
    Addr request_address(pkt->getAddr()); // Word address
    assert(pkt->isWrite());
    assert(pkt->isHtmTransactional());
    assert(m_xact_mgr->inTransaction());

    if (m_xact_mgr->checkWriteSignature(pkt->getAddr())) {
        DPRINTF(RubyHTMverbose,
                "Store to %#x (%#x) already in write set\n",
                pkt->getAddr(),
                makeLineAddress(pkt->getAddr()));
    } else {
        DPRINTF(RubyHTM,
                "Store to %#x (%#x) adds block"
                " address to write set\n",
                pkt->getAddr(),
                makeLineAddress(pkt->getAddr()));
        m_xact_mgr->isolateTransactionStore(request_address);
    }
    Addr pc = Addr(0);
    if (pkt->req->hasPC()) {
        pc = pkt->req->getPC();
        assert(pkt->req->hasVaddr());
    } else { // LL flushing write buffer
        assert(m_htm->params().lazy_vm &&
               !m_htm->params().eager_cd);
        assert(m_xact_mgr->atCommit());
        // Whole block
        assert(pkt->getSize() == RubySystem::getBlockSizeBytes());
    }
    m_xact_mgr->profileTransactionAccess(externalHit, true,
                                         respondingMach,
                                         pkt->getAddr(), pc,
                                         pkt->getSize());

}

void
TransactionalSequencer::handleTransactionalRead(SequencerRequest *srequest,
                                         DataBlock& data, bool externalHit,
                                         const MachineType respondingMach)
{
    assert ((srequest->m_type == RubyRequestType_LD) ||
            (srequest->m_type == RubyRequestType_Load_Linked) ||
            (srequest->m_type == RubyRequestType_Locked_RMW_Read) ||
            (srequest->m_type == RubyRequestType_RMW_Read) ||
            (srequest->m_type == RubyRequestType_IFETCH));
    PacketPtr pkt = srequest->pkt;
    assert(pkt->isHtmTransactional());
    assert(m_xact_mgr);
    assert(pkt->req->hasPC());
    Addr pc = pkt->req->getPC();
    m_xact_mgr->profileTransactionAccess(externalHit, false,
                                         respondingMach,
                                         pkt->getAddr(),
                                         pc,
                                         pkt->getSize());
    // Trans loads isolated (i.e. SR bit set) when when cache access
    // begins or when retired from ROB
    if (m_xact_mgr->isAborting()) {
        HtmCacheFailure reason =
            m_xact_mgr->getHtmTransactionalReqResponseCode();
        assert(reason != HtmCacheFailure::NO_FAIL ||
               m_xact_mgr->isCancelledTransaction());
        pkt->setHtmTransactionFailedInCache(reason);
        DPRINTF(RubyHTM, "Transactional read callback finds abort flag set\n");
    }
}

void
TransactionalSequencer::writeBufferEvent(PacketPtr pkt)
{
    writeBufferHitEvent.clearPacket();
    ruby_hit_callback(pkt);
    testDrainComplete();
}

void
TransactionalSequencer::lazyCommitEvent(PacketPtr pkt)
{
    lazyCommitCheckEvent.clearPacket();
    makeRequest(pkt);
    testDrainComplete();
}

void
TransactionalSequencer::validateConsumedData(Addr addr)
{
    // Create request and packets
    RequestPtr req =
        std::make_shared<Request>(addr,
                                  RubySystem::getBlockSizeBytes(),
                                  Request::PHYSICAL,
                                  Request::invldRequestorId/*reqId*/);
    PacketPtr pkt;
    pkt = Packet::createWrite(req);
    pkt->setHtmTransactional(m_xact_mgr->getCurrentHtmTransactionUid());
    pkt->setHtmForwardedDataValidation(true);
    pkt->allocate();
    if (m_xact_mgr->isAborting()) {
        DPRINTF(RubyHTMvaluepred, "Suppressed validation event"
                " (tx aborting)\n");
        delete pkt;
    } else {
        DPRINTF(RubyHTMvaluepred, "Making validation request for %#x\n",
                addr);
        RequestStatus status = makeRequest(pkt);
        if (status != RequestStatus_Issued &&
            status != RequestStatus_Aliased) {
             // Schedule a new event for the next cycle
            m_xact_mgr->scheduleValidationOfConsumedData(addr, true);
        }
        testDrainComplete();
    }
}

bool
TransactionalSequencer::logTransactionalStores(SequencerRequest &seq_req,
                                               Addr address,
                                               DataBlock& data) {
    assert(seq_req.pkt->isHtmTransactional());
    assert(!m_htm->params().lazy_vm); // LogTM
    assert(m_htm->params().eager_cd);

    bool needsLogging =
        m_pendingLogging.find(address) != m_pendingLogging.end();
    if (needsLogging) {
        assert(!m_xact_mgr->checkWriteSignature(address));
        assert(numOutstandingWrites(address) > 0);
        // Lock this line in cache until logging done, we need it
        // for the callback
        m_dataCache_ptr->setHtmLogPending(address, true);
        // Add to write set in order to start detecting conflicts
        m_xact_mgr->isolateTransactionStore(address);
        DPRINTF(RubyHTM,
                "Store to %#x (%#x) adds block"
                " address to write set\n",
                seq_req.pkt->getAddr(),
                makeLineAddress(seq_req.pkt->getAddr()));
        /* Generate log requests and use
           Sequencer::makeRequest(pkt) to handle them. Once both
           log requests completed, then callback CPU to complete the
           program store. Need to keep track of each
           "oustanding" store and its associated log requests.
        */
        // Check if there  outstanding log request for the
        // cache line targeted by this write.
        assert(m_logRequestTable.find(address) ==
               m_logRequestTable.end());

        // Create log requests/packets
        LogRequestInfo log_req = buildLogPackets(seq_req.pkt, data);
        // Try to issue them to cache
        if (makeLogRequests(log_req)) {
            bool found = m_pendingLogging.erase(address);
            assert(found);
            // Record their status via logReqTable
            auto &log_req_list = m_logRequestTable[address];
            assert(log_req_list.empty());
            log_req_list.emplace_back(log_req);
            DPRINTF(RubyHTMlog, "Store to paddr %#x"
                    " must wait until log requests done\n",
                    address);
            // Do not call hitCallback until log requests done. Will
            // be done by handleStoresToLog when logging completes
            return true;
        } else {
            panic("Cannot make log requests!\n");
        }
    }
    return false;
}

LogRequestInfo
TransactionalSequencer::buildLogPackets(PacketPtr mainPkt,
                                        DataBlock& datablock) {
  assert(mainPkt->isHtmTransactional());
  assert(mainPkt->req->hasVaddr());

  int logIndex = m_xact_mgr->addLogEntry();
  Addr logDataVPtr = m_xact_mgr->getXactEagerVersionManager()->
      computeLogDataPointer(logIndex);
  Addr logDataPtr = m_xact_mgr->getXactEagerVersionManager()->
      translateLogAddress(logDataVPtr);
  DPRINTF(RubyHTMlog,
          "Logging store to paddr %#x -"
          " log index %d vaddr %#x log paddr %#x \n",
          mainPkt->getAddr(), logIndex, logDataVPtr, logDataPtr);

  assert(logDataPtr == makeLineAddress(logDataPtr));
  Addr logAddressPtr = (Addr)logtm_compute_addr_ptr_from_data_ptr(logDataPtr);

  if (makeLineAddress(logDataPtr) == makeLineAddress(logAddressPtr)) {
      panic("Undo log has overflowed,"
            " address log should never overlap with data log\n");
  }

  // Log data pointer must be always aligned to cache line size
  assert(makeLineAddress(logDataPtr) == logDataPtr);

  RequestPtr logDataReq =
      std::make_shared<Request>(logDataPtr,
                                RubySystem::getBlockSizeBytes(),
                                Request::PHYSICAL,
                                mainPkt->req->requestorId());

  RequestPtr logAddrReq =
      std::make_shared<Request>(logAddressPtr,
                                sizeof(Addr),
                                Request::PHYSICAL,
                                mainPkt->req->requestorId());

  // Create request and packets
  PacketPtr logDataPkt = Packet::createWrite(logDataReq);
  PacketPtr logAddrPkt = Packet::createWrite(logAddrReq);
  // Mark these packets as stores to the log
  // Keep pointer to original packet, required to locate
  // LogRequestInfo upon callback
  logAddrPkt->setHtmStoreToLog(true, mainPkt);
  logDataPkt->setHtmStoreToLog(true, mainPkt);

  // Allocate packet data and copy values to be logged
  logDataPkt->allocate();
  logDataPkt->setData(datablock.getData(0, RubySystem::getBlockSizeBytes()));

  // Allocate packet data and copy program store's target virtual line address
  logAddrPkt->allocate();
  Addr vaddr = makeLineAddress(mainPkt->req->getVaddr());
  uint8_t *p = (uint8_t *)&vaddr;
  logAddrPkt->setData(p);
  DPRINTF(RubyHTMlog, "Generating accesses to log:"
          " - paddr %#x (addr) %#x (data)\n",
          logAddrPkt->getAddr(),
          logDataPkt->getAddr());

  return LogRequestInfo(logAddrPkt, logDataPkt,
                        mainPkt->getHtmTransactionUid(),
                        logIndex,
                        mainPkt->req->getVaddr(),
                        makeLineAddress(mainPkt->getAddr()));
}

bool
TransactionalSequencer::makeLogRequests(LogRequestInfo &logreqinfo)
{

    if (logreqinfo.logAddrPktStatus != RequestStatus_Issued) {
        logreqinfo.logAddrPktStatus =
            Sequencer::makeRequest(logreqinfo.logAddrPkt);
        if (logreqinfo.logAddrPktStatus != RequestStatus_Issued) {
            return false;
        }
        DPRINTF(RubyHTMlog, "Issued log address request:"
                " - paddr %#x (addr)\n",
                logreqinfo.logAddrPkt->getAddr());
        ++logreqinfo.outstanding;
    }
    if (logreqinfo.logDataPktStatus != RequestStatus_Issued) {
        logreqinfo.logDataPktStatus =
            Sequencer::makeRequest(logreqinfo.logDataPkt);
        if (logreqinfo.logDataPktStatus != RequestStatus_Issued) {
            return false;
        }
        DPRINTF(RubyHTMlog, "Issued log data request:"
                " - paddr %#x (addr)\n",
                logreqinfo.logDataPkt->getAddr());
        ++logreqinfo.outstanding;
    }
    return true;
}

void
TransactionalSequencer::handleStoresToLog(Addr address,
                                       PacketPtr pkt,
                                       DataBlock& data)
{
    assert(pkt->isWrite());
    Addr store_addr = makeLineAddress(pkt->getHtmLoggedStoreAddr());
    assert(m_logRequestTable.find(store_addr) !=
           m_logRequestTable.end());
    auto &log_req_list = m_logRequestTable[store_addr];
    assert(log_req_list.size() == 1);
    LogRequestInfo &log = log_req_list.back();
    assert(pkt->isHtmStoreToLog());

    --log.outstanding;
    ++log.completed;
    if (pkt == log.logAddrPkt) { // This is the callback for the log
                                 // address block
        // Will copy virtual address to log when program store
        // completes. For now, set it to zero to indicate that the
        // data log is not valid. This is essential for correctness
        // since the program store may never complete leaving "gaps",
        // and the unroll must skip restoring memory in that case
        const uint64_t *vaddrPtr = pkt->getConstPtr<uint64_t>();
        assert(*vaddrPtr == makeLineAddress(log.vaddr));
        data.setData(pkt->getConstPtr<uint8_t>(),
                     getOffset(pkt->getAddr()), pkt->getSize());
        DPRINTF(RubyHTMlog, "Log address block at paddr %#x"
                " written with vaddr %#x"
                " targeted by program store (%#x)\n",
                pkt->getAddr(), *vaddrPtr, log.paddr);
    }
    else { // This is the callback for the log data block
        assert(pkt == log.logDataPkt);
        assert(pkt->getSize() == RubySystem::getBlockSizeBytes());
        // Write non-speculative values from log buffer
        // to data log cache block
        data.setData(pkt->getConstPtr<uint8_t>(),
                     0 /*offset*/,
                     RubySystem::getBlockSizeBytes() /*len*/);
        DPRINTF(RubyHTMlog, "Successfully logged program store "
                "  vaddr %#x (paddr %#x) into log paddr %#x\n",
                log.vaddr, log.paddr, address);
    }

    if (log.outstanding == 0 && log.completed == 2) {
        DPRINTF(RubyHTMlog, "Done logging store to paddr %#x\n",
                store_addr);
        // Unlock block targeted by program store
        m_dataCache_ptr->setHtmLogPending(store_addr, false);
        DataBlock* storeDataPtr;
        bool hit = m_dataCache_ptr->
            tryCacheAccess(store_addr, RubyRequestType_ST,
                           storeDataPtr, false);
        assert(hit);
        Sequencer::writeCallback(store_addr, *storeDataPtr);
        if (m_xact_mgr->config_enableValueChecker()) {
            // Record old value we just logged
            m_ruby_system->getXactValueChecker()->
                notifyLoggedDataBlock(m_version,address, data);
        }

        // Erase log request
        bool found = m_logRequestTable.erase(store_addr);
        assert(found);
    }
}

void
TransactionalSequencer::writeCallback(Addr address, DataBlock& data,
                         const bool externalHit, const MachineType mach,
                         const Cycles initialRequestTime,
                         const Cycles forwardRequestTime,
                         const Cycles firstResponseTime,
                         const bool noCoales)
{
    assert(!m_failedCallback);
    assert(address == makeLineAddress(address));
    assert(m_RequestTable.find(address) != m_RequestTable.end());
    m_specCallback = m_xact_mgr->config_allowEarlyValueFwd() &&
        m_xact_mgr->isBlockConsumed(address);
    auto seq_req_list = m_RequestTable[address];
    assert(!seq_req_list.empty());
    SequencerRequest seq_req = seq_req_list.front();
    if (m_specCallback)
    {
        if (!seq_req.pkt->isWrite() &&
           !(seq_req.m_type == RubyRequestType_RMW_Read) &&
           !(seq_req.m_type == RubyRequestType_Locked_RMW_Read)) {
            // Request was read (not RMW) but speculative responses
            // treated as writes, modelling only one state (FD) for
            // spec blocks. Can cause extra writebacks, maybe
            // invalidations too.
            Sequencer::readCallback(address, data, externalHit, mach,
                             initialRequestTime,
                             forwardRequestTime,
                             firstResponseTime);
            // Preemptively add it to write set even if
            // it was a read.
            m_xact_mgr->isolateTransactionStore(address);
            return;
        }
        if (seq_req_list.size() > 1) {
            for (auto it = seq_req_list.begin();
                it != seq_req_list.end();
                it++)
            {
                if (it->m_type ==
                    RubyRequestType_Locked_RMW_Read)
                {
                    // Need to check if we have an aliased RMW
                    // locked to unlock in case of abort
                    seq_req = *it;
                    break;
                }
            }
        }
    }
    assert(seq_req.pkt->isWrite() ||
           (seq_req.m_type == RubyRequestType_RMW_Read) ||
           (seq_req.m_type == RubyRequestType_Locked_RMW_Read));
    bool trans = false;
    RubyRequestType type = seq_req.m_type;

    if (seq_req.pkt->isHtmTransactional()) {
        trans = true;
        if (!m_htm->params().lazy_vm) { // LogTM
            bool skipCallback = logTransactionalStores(seq_req,
                                                       address,
                                                       data);
            if (skipCallback) return;
        }
    }
    Sequencer::writeCallback(address, data, externalHit, mach,
                             initialRequestTime,
                             forwardRequestTime,
                             firstResponseTime,
                             noCoales);
    // From this point onwards, cannot use seq_req (writecallback
    // erased them from m_RequestTable)
    if (trans &&
        (type == RubyRequestType_Locked_RMW_Read ||
         type == RubyRequestType_Locked_RMW_Write)) {
        DPRINTF(RubyHTM, "HTM: Sequencer::writecallback ignores "
                "transactional %s address=%#x\n",
                RubyRequestType_to_string(type),
                address);
        warn("WARNING: writeCallback for transactional %s\n",
             RubyRequestType_to_string(type));
        if (type == RubyRequestType_Locked_RMW_Read) {
            // Keep track in order to unblock queue if abort occurs
            // before subsequent locked write
            assert(outstandingTransLockedRMWAccesses[address] == 0);
            ++outstandingTransLockedRMWAccesses[address];
        }
        else { // request->m_type == RubyRequestType_Locked_RMW_Write))
            assert(outstandingTransLockedRMWAccesses[address] == 1);
            outstandingTransLockedRMWAccesses.erase(address);
        }
    }
    if (!m_htm->params().lazy_vm) { // LogTM
        if (m_xact_mgr->config_enableValueChecker() &&
            m_xact_mgr->isUnrollingLog()) {
            if (m_xact_mgr->checkWriteSignature(address)) {
                // Restoring old value from log into wset block: Save
                // datablock and check when unroll completes. Must do
                // it after writeCallback observe written value
                m_ruby_system->getXactValueChecker()->
                    notifyUnrolledDataBlock(m_version, address, data);
            }
        }
    }
}

void
TransactionalSequencer::invalidCallback(Addr address, DataBlock& data, bool write)
{
    // We can get here either after an abort that deallocated
    // an speculative block but raced with validation event
    // or it got evicted from L0 but had the same race issue.
    // In the second case transaction should be aborting
    // as evictions of consumed blocks cause an abort
    assert(m_xact_mgr->config_allowEarlyValueFwd() &&
           (!m_xact_mgr->inTransaction() ||
           (m_xact_mgr->isBlockConsumed(address) &&
           m_xact_mgr->isAborting())));

    // This function call already assert the packet is in the table
    // it MUST be because even if a transaction aborted or the block
    // was evicted, the packet is still in flight and cache is waiting
    // for the callback
    Packet *pkt = getPacketFromRequestTable(address);
    assert(pkt->isHtmForwardedDataValidation());

    // Treat it as a failed callback to flush
    // all aliased requests from the transaction
    // and reissue the following ones (non-speculative)
    // (or at least not from the same transaction)
    m_failedCallback = true;
    warn("WARNING! validation over invalid block! "
                    "addr: %#x write: %d\n", address,
                    write ? "True" : "False");
    DPRINTF(RubyHTMvaluepred, "WARNING! validation over invalid block! "
                    "addr: %#x write: %d\n",
                    address,
                    write ? "True" : "False");
    if (write) {
        Sequencer::writeCallback(address, data);
    } else {
        Sequencer::readCallback(address, data);
    }
    // Reset flag, all lingering requests are flushed
    m_failedCallback = false;

}

void
TransactionalSequencer::readCallback(Addr address, DataBlock& data,
                                     const bool externalHit,
                                     const MachineType mach,
                                     const Cycles initialRequestTime,
                                     const Cycles forwardRequestTime,
                                     const Cycles firstResponseTime)
{
    m_specCallback = m_xact_mgr->config_allowEarlyValueFwd() &&
        m_xact_mgr->isBlockConsumed(address);
    Sequencer::readCallback(address, data, externalHit, mach,
                            initialRequestTime,
                            forwardRequestTime,
                            firstResponseTime);
}

bool
TransactionalSequencer::interceptLogAccess(PacketPtr pkt)
{
    // LogTM: Intercept access to undo log and setup
    assert(!m_htm->params().lazy_vm);
    assert(m_xact_mgr);
    assert(pkt->req->hasVaddr());
    // log TLB translations
    if (m_xact_mgr->isAccessToLog(pkt->req->getVaddr())) {
        if (!m_xact_mgr->isLogReady()) {
            m_xact_mgr->setupLogTranslation(pkt->req->getVaddr(),
                                            pkt->req->getPaddr());
        } else if (m_xact_mgr->isUnrollingLog()) {
            DPRINTF(RubyHTMlog, "Log access during unroll "
                    "vaddr %#x paddr %#x\n",
                    pkt->req->getVaddr(),
                    pkt->req->getPaddr());
            if (m_xact_mgr->isEndLogUnrollSignal(pkt)) {
                assert(pkt->isWrite());
                // "Magic value" written to logbase to
                // signal log unroll completed without
                // writing to the stack (call m5xxx)
                m_xact_mgr->endLogUnroll();
                DPRINTF(RubyHTMlog, "Log unroll completed\n");
                // No need to perform memory access in cache
                return true;
            } else {
                // We can have lingering transactional
                // loads immediately abort signal from CPU
                assert(!pkt->isWrite());
            }
        } else {
            // Speculative from mispredicted paths may
            // read from log locations immediately
            // after unroll has completed
            warn("Unexpected %s to undo log!"
                 " - PC %#x vaddr %#x\n",
                 pkt->isWrite() ? "write" : "read",
                 pkt->req->getPC(),
                 pkt->req->getVaddr());
        }
        // Sanity checks: Detect if the OS ever tries to move the log
        // after it has been set up
        Addr paddr = m_xact_mgr->
            translateLogAddress(pkt->req->getVaddr());
        if (pkt->getAddr() != paddr) {
            panic("Unexpected v2p translation for log access -"
                  " vaddr %#x paddr %#x (expected paddr %#x)\n",
                  pkt->req->getVaddr(), pkt->getAddr(),
                  paddr);
        }
    }
    return false;
}
int
TransactionalSequencer::numOutstandingWrites(Addr address)
{
    int numStoresFound = 0;
    assert(m_RequestTable.find(address) != m_RequestTable.end());
    auto &seq_req_list = m_RequestTable[address];
    for (auto it=seq_req_list.begin();
         it != seq_req_list.end(); ++it) {
        SequencerRequest seq_req = *it;
        if (seq_req.pkt->isWrite()) {
            ++numStoresFound;
        }
    }
    return numStoresFound;
}

void
TransactionalSequencer::suppressOutstandingRequests()
{
    for (auto &table_entry : m_RequestTable) {
        for (auto &seq_req : table_entry.second) {
            if (seq_req.pkt->isHtmTransactional()) {
                seq_req.suppressed = true;
            }
        }
    }
}

PacketPtr
TransactionalSequencer::getPacketFromRequestTable(Addr address)
{
    assert(address == makeLineAddress(address));
    assert(m_RequestTable.find(address) != m_RequestTable.end());
    auto &seq_req_list = m_RequestTable[address];
    while (!seq_req_list.empty()) {
        SequencerRequest &seq_req = seq_req_list.front();
	// Should only find lingering loads
        //assert(seq_req.m_type == RubyRequestType_LD);
        // Write request: reissue request to the cache hierarchy
        return seq_req.pkt;
    }
    panic("Should never get this far!");
}

} // namespace ruby
} // namespace gem5
