/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/EagerTransactionVersionManager.hh"

#include <iomanip>
#include <iostream>
#include <string>

#include "debug/RubyHTMlog.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/packet.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/logtm.h"
#include "mem/ruby/system/Sequencer.hh"

#define CLASS_NS EagerTransactionVersionManager::

namespace gem5
{
namespace ruby
{


CLASS_NS
EagerTransactionVersionManager(TransactionInterfaceManager *xact_mgr,
                              int version, CacheMemory* dataCache_ptr) {
    m_version = version;
    m_xact_mgr = xact_mgr;
    m_dataCache_ptr = dataCache_ptr;
}

CLASS_NS ~EagerTransactionVersionManager() {
}

void
CLASS_NS beginTransaction()
{
    assert(m_logNumEntries == 0);
    assert(m_initStatus == LogInitStatus::Ready);
    assert(!m_logTLB.empty());
}

void
CLASS_NS restartTransaction(){
    m_logNumEntries = 0;
}


void
CLASS_NS commitTransaction()
{
    m_logNumEntries = 0;
}

bool
CLASS_NS isAccessToLog(Addr addr) const
{
    if (m_initStatus == LogInitStatus::Invalid)
        return false;
    return (addr >= m_logBaseVAddr &&
            addr  < (m_logBaseVAddr + MAX_LOG_SIZE_BYTES));
}

void
CLASS_NS setupLogTranslation(Addr vaddr, Addr paddr)
{
    if (m_initStatus == LogInitStatus::BaseAddress) {
        assert(m_logTLB.empty());
        // First translation must be for base address
        assert(m_logBaseVAddr == vaddr);
    } else {
        assert(m_initStatus == LogInitStatus::V2PTranslations);
    }
    m_initStatus = LogInitStatus::V2PTranslations;
    if (m_logTLB.find(vaddr) != m_logTLB.end()) {
        // if store during walk_log (simSetLogBase) retried, ensure
        // we find the same translation
        assert(m_logTLB[vaddr] == paddr);
    }
    m_logTLB[vaddr] = paddr;
    DPRINTF(RubyHTMlog,
            "Setting up log TLB vaddr %#x paddr %#x \n",
            vaddr, paddr);
    if (vaddr + LOG_PAGE_SIZE_BYTES ==
        m_logBaseVAddr + MAX_LOG_SIZE_BYTES) {
        m_initStatus = LogInitStatus::Ready;
        DPRINTF(RubyHTMlog,
                "Done setting log translations. Log ready! "
                "Log max size is %ld bytes\n",
                MAX_LOG_SIZE_BYTES);
    }
}


void
CLASS_NS setLogBaseVirtualAddress(Addr addr)
{
    if (m_initStatus == LogInitStatus::Invalid) {
        m_initStatus = LogInitStatus::BaseAddress;
        m_logBaseVAddr = addr;
        DPRINTF(RubyHTMlog,
                "Setting up log base vaddr %#x \n",
                addr);
    } else {
        assert(m_logBaseVAddr == addr);
    }
}


Addr
CLASS_NS computeLogDataPointer(int numEntries) const {
    assert(numEntries >= 0 &&
           numEntries < MAX_LOG_SIZE_PAGES * LOG_ENTRIES_PER_PAGE);
    return (Addr)logtm_compute_data_log_pointer(m_logBaseVAddr,
                                                numEntries);
}
Addr
CLASS_NS translateLogAddress(Addr vaddr) const {
    Addr vpageAddr = vaddr & PAGE_MASK;
    Addr pageOffset = vaddr & ~PAGE_MASK;
    std::map<Addr,Addr>::const_iterator it =
        m_logTLB.find(vpageAddr);
    assert(it != m_logTLB.end());
    Addr ppageAddr = it->second;
    return ppageAddr | pageOffset;
}

int
CLASS_NS addLogEntry()
{
    // Should never be called unless we have set the log base
    assert(m_initStatus == LogInitStatus::Ready);
    // Return next available index in the log
    return m_logNumEntries++;
}


bool
CLASS_NS isEndLogUnrollSignal(PacketPtr pkt)
{
    assert(pkt->req->hasVaddr());
    if (pkt->req->getVaddr() == m_logBaseVAddr) {
        if (pkt->isWrite()) {
            // No writes to the log during log unroll, except for this
            // "end unroll signal" to the log base (magic number as
            // sanity check)
            const uint64_t *data = pkt->getConstPtr<uint64_t>();
            assert(*data == 0xdeadc0debaadcafe);
            return true;
        } else {
            return false;
        }
    }
    return false;
}

void
CLASS_NS shutdownLog()
{
    assert(m_initStatus == LogInitStatus::Ready);
    m_initStatus = LogInitStatus::Invalid;
    m_logTLB.clear();
}

} // namespace ruby
} // namespace gem5
