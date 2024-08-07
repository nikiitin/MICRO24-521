/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/LazyTransactionVersionManager.hh"

#include <iomanip>
#include <iostream>
#include <string>

#include "debug/RubyHTM.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/packet.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/Sequencer.hh"

#define CLASS_NS LazyTransactionVersionManager::

namespace gem5
{
namespace ruby
{


CLASS_NS
LazyTransactionVersionManager(TransactionInterfaceManager *xact_mgr,
                              int version, CacheMemory* dataCache_ptr) {
    m_version = version;
    m_xact_mgr = xact_mgr;
    m_dataCache_ptr = dataCache_ptr;
    m_aborting = false;
    m_committed = false;
    m_committing = false;
    m_flushPending = false;
    m_requestorID = Request::invldRequestorId;

    m_issuedWriteBufferRequest = 0;
}

CLASS_NS ~LazyTransactionVersionManager() {
}

void
CLASS_NS beginTransaction(PacketPtr pkt)
{
    m_committed = false;
    m_committing = false;
    m_requestorID = pkt->req->requestorId();
    assert(pkt->getHtmTransactionUid() > m_currentHtmUid);
    m_currentHtmUid = pkt->getHtmTransactionUid();
    assert(!m_aborting);
}

void
CLASS_NS restartTransaction(){
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);
    if (m_committing) {
        // Aborted while committing
        assert(m_aborting);
    }
    else {
        assert(!m_aborting);
    }
    m_committing = false;
    m_committed = false;
    m_aborting = false;
    assert(m_issuedWriteBufferRequest == 0);
    discardWriteBuffer();
    // Profiling
    m_readBytes.clear();
    m_writtenBytes.clear();
    m_numReadBytesWrittenRemotely = 0;
    m_numWrittenBytesWrittenRemotely = 0;
}

void
CLASS_NS notifyCommittedTransaction(){
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);
    _unused(transactionLevel);
    assert(m_committed);
    assert(m_committing);
    assert(!m_aborting);
    m_committing = false;
    m_committed = false;
    assert(m_issuedWriteBufferRequest == 0);
    assert(m_writeBufferBlocks.empty());
    assert(m_writeBuffer.empty());
    m_readBytes.clear();
    m_writtenBytes.clear();
    m_numReadBytesWrittenRemotely = 0;
    m_numWrittenBytesWrittenRemotely = 0;
}

void
CLASS_NS commitTransaction()
{
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);
    _unused(transactionLevel);
    assert(!m_committed);
    if (!m_committing) {
        // Only profile once
        // Byte-level conflict detection for lazy-lazy HTMs
        std::vector<TransactionInterfaceManager*> mgrs =
            m_xact_mgr->getRemoteTransactionManagers();
        for (int i=0; i < mgrs.size(); i++) {
            TransactionInterfaceManager* mgr=mgrs[i];
            if (m_xact_mgr == mgr) continue;
            // Mark conflicts at byte-level for all remote transaction
            for (map<Addr, uint8_t>::iterator it =
                     m_writeBuffer.begin();
                 it != m_writeBuffer.end();
                 ++it) {
                Addr addr =(*it).first;
                mgr->getXactLazyVersionManager()->
                    profileRemotelyWrittenByte(addr);
            }
        }
    }
    m_committing = true;
    m_flushPending = false;

    flushWriteBuffer();
}

int CLASS_NS getProcID() const{
    return m_version;
}

bool CLASS_NS existInWriteBuffer(Addr addr){
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);

    return (m_writeBuffer.find(addr) !=
            m_writeBuffer.end());
}

uint8_t
CLASS_NS getDataFromWriteBuffer(Addr addr){
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);
    assert(m_writeBuffer.find(addr) !=
           m_writeBuffer.end());
    return m_writeBuffer[addr];
}

void
CLASS_NS addToWriteBuffer(Addr addr, int size, uint8_t *data){

    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);

    for (int i = 0; i < size; i++){
        m_writeBuffer[addr + i] = data[i];
    }
    // Ensure all data falls in the same block
    assert(makeLineAddress(addr) == makeLineAddress(addr + size - 1));

    m_writeBufferBlocks[makeLineAddress(addr)] = Pending;
    uint64_t value = 0;
    _unused(value);
    switch(size) {
    case sizeof(uint8_t):
        value = (uint64_t)*((uint8_t *)data);
        break;
    case sizeof(uint16_t):
        value = (uint64_t)*((uint16_t *)data);
        break;
    case sizeof(uint32_t):
        value = (uint64_t)*((uint32_t *)data);
        break;
    case sizeof(uint64_t):
        value = (uint64_t)*((uint64_t *)data);
        break;
    default:
        panic("Unsupported packet size\n");
    }
    DPRINTF(RubyHTMverbose,
            "Adding to write buffer,"
            " addr %#x size %d value %#x\n",
            addr, size, value);
}

vector<uint8_t>
CLASS_NS forwardData(Addr addr, int size,
                     DataBlock& cacheBlock, bool& forwarding){

    vector<uint8_t> data;
    forwarding = false;
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    if (transactionLevel == 0) {
        // The O3CPU may issue speculative transactional loads that
        // reach memory before xbegin has retired
        // No forwarding in this case
        return data;
    }

    assert(transactionLevel > 0);

    data.resize(size);
    uint8_t buffer[64];

    for (int i = 0; i < size; i++){
        if (existInWriteBuffer(addr + i)){
            data[i] = getDataFromWriteBuffer(addr + i);
            forwarding = true;
        } else {
            data[i] = *(cacheBlock.getData(getOffset(addr + i), 1));
        }
        buffer[i] = data[i];
        // Profiling
        m_readBytes[addr + i] = false; // Will be set to true if
                                       // conflict detected
    }
    if (forwarding) {
        uint64_t value = 0;
        bool trace = true;
        _unused(value);
        switch(size) {
        case sizeof(uint8_t):
            value = (uint64_t)*((uint8_t *)buffer);
            break;
        case sizeof(uint16_t):
            value = (uint64_t)*((uint16_t *)buffer);
            break;
        case sizeof(uint32_t):
            value = (uint64_t)*((uint32_t *)buffer);
            break;
        case sizeof(uint64_t):
            value = (uint64_t)*((uint64_t *)buffer);
            break;
        default:
            trace = false;
            break;

        }
        if (trace) {
            DPRINTF(RubyHTMverbose,
                    "Forwarding from write buffer,"
                    " addr %#x size %d value %#x\n",
                    addr, size, value);
        } else {
            DPRINTF(RubyHTMverbose,
                    "Forwarding from write buffer,"
                    " addr %#x (unexpected size: %d,"
                    " value ommitted)\n",
                    addr, size);
        }
    }

    return data;
}

void
CLASS_NS flushWriteBuffer(){
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);
    assert(!m_aborting);

    if (m_writeBufferBlocks.empty()) {
        m_committed = true;
        assert(m_issuedWriteBufferRequest == 0);
        return;
    }
    for (map<Addr,WriteBufferBlockStatus>::iterator it =
             m_writeBufferBlocks.begin();
         it != m_writeBufferBlocks.end();
         ++it) {
        if (m_issuedWriteBufferRequest ==
            m_xact_mgr->config_lazyCommitWidth()) {
            m_flushPending = true;
            break;
        };
        WriteBufferBlockStatus status =(*it).second;
        if (status == Pending) {
            Addr addr =(*it).first;

            // Create request and packet
            Request::Flags flags;
            assert(m_requestorID != Request::invldRequestorId);
            RequestPtr req = std::make_shared<Request>(
                         addr, RubySystem::getBlockSizeBytes(),
                         flags,  m_requestorID);
            PacketPtr pkt =  new Packet(req, MemCmd::WriteReq,
                                        RubySystem::getBlockSizeBytes());
            pkt->allocate();
            pkt->setHtmTransactional(m_currentHtmUid);

            RequestStatus requestStatus =
                m_xact_mgr->getSequencer()->makeRequest(pkt);
            if (requestStatus != RequestStatus_Issued) {
                DPRINTF(RubyHTM, "Write buffer flush pending, makeRequest"
                        " could not issue request paddr %#x\n", addr);
                m_flushPending = true;
                continue; // Try to issue request for another block
            }
            // Mark as issued
            (*it).second = Issued;
            ++m_issuedWriteBufferRequest;
        }
    }
    if (m_flushPending) {
        // Ensure there are issued reqs that will wake up the flush
        // again when calling mergedDataFromWriteBuffer
        for (map<Addr,WriteBufferBlockStatus>::iterator it =
                 m_writeBufferBlocks.begin();
             it != m_writeBufferBlocks.end();
             ++it) {
            WriteBufferBlockStatus status =(*it).second;
            if (status == Issued) {
                // OK: At least one issued request will wake us up
                return;
            }
        }
    }

}

void
CLASS_NS mergeDataFromWriteBuffer(Addr address, DataBlock& data)
{
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);

    // Block address must exist
    assert(m_writeBufferBlocks.find(address) !=
           m_writeBufferBlocks.end());
    // Request for this block has been issued
    WriteBufferBlockStatus status = m_writeBufferBlocks[address];
    assert(status == Issued);
    _unused(status);
    int mergedBytes = 0;
    DPRINTF(RubyHTMverbose, "Data block before merge, addr %#x:\n%s\n",
            address, data.toString());
    assert(address == makeLineAddress(address));
    map<Addr,uint8_t>::iterator it;
    for (int i=0; i < RubySystem::getBlockSizeBytes(); ++i) {
        Addr byteAddr = Addr(address+i);
        it = m_writeBuffer.find(byteAddr);
        if (it != m_writeBuffer.end()) {
            // Merge byte
            assert(byteAddr ==(*it).first);
            uint8_t val =(*it).second;
            int offset = getOffset(byteAddr);
            data.setData(&val, offset, 1);
            DPRINTF(RubyHTMverbose, "Merged byte val %#x at addr"
                    " %#x (offset %#x) from write buffer into"
                    " block paddr %#x\n",
                    val, byteAddr, offset, address);
            mergedBytes++;
        }
    }
    DPRINTF(RubyHTMverbose, "Data block after merge, addr %#x:\n%s\n",
            address, data.toString());
    // Delete block address from write buffer blocks
    m_writeBufferBlocks.erase(address);
    --m_issuedWriteBufferRequest;
    DPRINTF(RubyHTM, "Merged %d bytes from write buffer"
            " into block paddr %#x\n",
            mergedBytes, address);

    // Check if we are done flushing the write buffer
    if (m_writeBufferBlocks.empty()) {
        if (m_aborting) {
            DPRINTF(RubyHTM, "Write buffer flush terminated"
                    " prematurely due to abort\n");
        }
        else {
            DPRINTF(RubyHTM, "Write buffer flush completed\n");
        }
        m_committed = true; // Now canCommitTransaction will be true,
                            // allowing commit/abort to complete
        // Discard write buffer after all contents merged into
        // cache blocks
        m_writeBuffer.clear();
    }
    else {
        // If flush pending due to too many outstanding misses, resume
        if (m_aborting) {
            DPRINTF(RubyHTM, "Write buffer flush will be cancelled once all"
                    " outstanding writes complete, transaction is aborting\n");
        }
        else if (m_flushPending) {
            m_flushPending = false;
            DPRINTF(RubyHTM, "Write buffer flush resumed"
                    " after miss completed\n");
            flushWriteBuffer();
        }
    }
}

void
CLASS_NS cancelWriteBufferFlush()
{
    assert(m_committing);
    assert(!m_aborting);
    m_aborting = true;
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);
    map<Addr,WriteBufferBlockStatus>::iterator it =
        m_writeBufferBlocks.begin();
    for (auto next_it = it;
         it != m_writeBufferBlocks.end(); it = next_it) {
        ++next_it;
        WriteBufferBlockStatus status =(*it).second;
        if (status == Pending) {
            Addr addr = (*it).first;
            DPRINTF(RubyHTM, "Cancelled pending "
                    "write buffer flush block paddr %#x\n",
                    addr);
            bool found = m_writeBufferBlocks.erase(addr);
            assert(found);
        }
    }
}

void
CLASS_NS discardWriteBuffer(){
    int transactionLevel = m_xact_mgr->getTransactionLevel();
    assert(transactionLevel == 1);

    m_writeBuffer.clear();
    m_writeBufferBlocks.clear();
    DPRINTF(RubyHTM, "Discarding contents of write buffer upon abort\n");
}

void
CLASS_NS profileRemotelyWrittenByte(Addr addr)
{
    if (m_readBytes.find(addr) !=
        m_readBytes.end()) {
        if (!m_readBytes[addr]) { // Conflict not yet signaled
            ++m_numReadBytesWrittenRemotely;
            m_readBytes[addr] = true;
        }
    }
    if (m_writeBuffer.find(addr) !=
        m_writeBuffer.end()) {
        if (!m_writtenBytes[addr]) {
            ++m_numWrittenBytesWrittenRemotely;
            m_writtenBytes[addr] = true;
        }
    }
}

} // namespace ruby
} // namespace gem5
