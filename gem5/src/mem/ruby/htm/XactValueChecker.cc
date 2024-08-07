/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/XactValueChecker.hh"

#include <iostream>

#include "debug/RubyHTM.hh"
#include "debug/RubyHTMvalues.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{
namespace ruby
{


#define _unused(x) ((void)(x))

#define CLASS_NS XactValueChecker::

#define XACT_LAZY_VM (m_htm->params().lazy_vm)
#define XACT_EAGER_CD (m_htm->params().eager_cd)

CLASS_NS XactValueChecker(RubySystem *rs) {
    m_ruby_system = rs;
    m_htm = rs->params().system->getHTM();

    int num_sequencers = rs->params().num_of_sequencers;
    m_writeBuffer.resize(num_sequencers);
    m_writeBufferBlocks.resize(num_sequencers);
    m_loggedValues.resize(num_sequencers);
    m_unrolledValues.resize(num_sequencers);
}

CLASS_NS ~XactValueChecker() {
}

uint8_t
CLASS_NS readGlobalValue(Addr addr) {
    std::map<Addr, uint8_t>::iterator p;
  uint8_t data;

  p  = m_xact_data.find(addr);
  assert(p != m_xact_data.end());
  data = p->second;
  return data;
}

bool
CLASS_NS existGlobalValue(Addr addr) {
    std::map<Addr, uint8_t>::iterator p;

  return m_xact_data.find(addr) != m_xact_data.end();
}

void
CLASS_NS writeGlobalValue(Addr addr, uint8_t value) {
    std::map<Addr, uint8_t>::iterator p;
  p  = m_xact_data.find(addr);

  if (p == m_xact_data.end()) { // Not present
    m_xact_data.insert(std::pair<Addr,uint8_t>(addr, value));
  }
  else {
    p->second = value;     // Byte already present: update value
  }
}

bool
CLASS_NS existBlockInWriteBuffer(int proc, Addr addr){
  return m_writeBufferBlocks[proc].find(makeLineAddress(addr)) !=
    m_writeBufferBlocks[proc].end();
}

bool
CLASS_NS existInWriteBuffer(int proc, Addr addr){
  return (m_writeBuffer[proc].find(addr) !=
          m_writeBuffer[proc].end());
}

uint8_t
CLASS_NS getDataFromWriteBuffer(int proc, Addr addr){
    std::map<Addr, uint8_t>::iterator p;
  uint8_t data = 0;
  p  = m_writeBuffer[proc].find(addr);
  assert(p != m_writeBuffer[proc].end());
  data = p->second;
  return data;
}

void
CLASS_NS notifyWrite(int proc, bool trans, Addr addr,
                     int size, uint8_t *data_ptr){
    if (trans) {
        std::vector<uint8_t> data;
        for (int i=0; i < size; i++)
            data.push_back(data_ptr[i]);
        assert(size == data.size());

        bool overwrites = false;
        _unused(overwrites);
        for (int i = 0; i < size; i++){
            std::map<Addr, uint8_t>::iterator p =
                m_writeBuffer[proc].find(addr + i);
            if (p != m_writeBuffer[proc].end()) {
                assert(m_writeBuffer[proc].size() > 0);
                assert(existBlockInWriteBuffer(proc, addr+i));
                // Byte already present in write buffer: update value
                p->second = data[i];
                overwrites = true;
            }
            else {
                Addr address = makeLineAddress(addr + i);
                m_writeBuffer[proc].
                    insert(std::pair<Addr,uint8_t>(addr + i, data[i]));
                m_writeBufferBlocks[proc][address] =  0;
            }
        }
        if (size == 8) {
            unsigned long value = *((unsigned long *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x %s "
                    "(%d BYTES) VALUE %018#x\n", proc, addr,
                    overwrites ? "OVERWRITES" : "WRITES", size, value);
        }
        else if (size == 4) {
            unsigned int value = *((unsigned int *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x %s "
                    "(%d BYTES) VALUE %010#x\n", proc, addr,
                    overwrites ? "OVERWRITES" : "WRITES", size, value);
        }
        else if (size == 2) {
            unsigned short value = *((unsigned short *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x %s "
                    "(%d BYTES) VALUE %06#x\n", proc, addr,
                    overwrites ? "OVERWRITES" : "WRITES", size, value);
        }
        else if (size == 1) {
            unsigned char value = *((unsigned char *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x %s "
                    "(%d BYTES) VALUE %#x\n", proc, addr,
                    overwrites ? "OVERWRITES" : "WRITES", size, value);
        }
        else {
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x %s "
                    "(%d BYTES)\n", proc, addr,
                    overwrites ? "OVERWRITES" : "WRIRTE", size);
        }
    }
    else {
        for (int i=0; i < size; i++) {
            Addr byte_addr = addr + i;
            if (existGlobalValue(byte_addr)){
                // Writing shared data outside tx (previously
                // committed by a tx) Now make the new values visible
                // to the global value checker
                writeGlobalValue(byte_addr, data_ptr[i]);
            }
        }
        if (size == 8) {
            unsigned long value = *((unsigned long *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x WRITES %d BYTES "
                    "VALUE %018#x DIRECTLY TO SHARED MEMORY (NON-TX)\n",
                    proc, addr, size, value);
        }
        else if (size == 4) {
            unsigned int value = *((unsigned int *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x WRITES %d BYTES "
                    "VALUE %010#x DIRECTLY TO SHARED MEMORY (NON-TX)\n",
                    proc, addr, size, value);
        }
        else if (size == 2) {
            unsigned short value = *((unsigned short *)data_ptr);
            _unused(value);
            DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x WRITES %d BYTES "
                    "VALUE %06#x DIRECTLY TO SHARED MEMORY (NON-TX)\n",
                    proc, addr, size, value);
        }
        else {
            for (int i=0; i < size; i++) {
                unsigned char value = data_ptr[i];
                _unused(value);
                DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x WRITES "
                        "VALUE %#x DIRECTLY TO SHARED MEMORY (NON-TX)\n",
                        proc, addr, value);
            }
        }
    }
}

void
CLASS_NS discardWriteBuffer(int proc){
  m_writeBuffer[proc].clear();
  m_writeBufferBlocks[proc].clear();
}

bool
CLASS_NS xactValueCheck(int proc, Addr address, int size,
                        const uint8_t *ptr) {
  Addr addr = address;
  uint8_t data;
  bool reads_from_shared_mem = false,
    reads_from_wb = false, reads_untracked = false;
  _unused(reads_from_shared_mem);
  _unused(reads_from_wb);
  _unused(reads_untracked);
  for (int i = 0; i < size; i++){
    if (existInWriteBuffer(proc, addr + i)){
      data = getDataFromWriteBuffer(proc, addr + i);
      if (data != ptr[i]) {
        inform("xactValueCheck: VALUE CHECK HAS FAILED!");
        warn("xactValueCheck: VALUE CHECK HAS FAILED!");
        DPRINTF(RubyHTM, "HTM: VALUE CHECK FAILED! "
                "PROC %d address=%#x HAS READ VALUE %#x, "
                "WRITE BUFFER VALUE WAS %#x \n",
                proc, addr+i, ptr[i], data);
        return false;
      }
      else {
        reads_from_wb = true;
      }
    }
    else if (existGlobalValue(addr + i)) {
      /* Loading address that has not been yet written by this transaction,
       * but was written by an earlier committed transaction: make sure
       * we observe the value that is globally visible at this point.
       */
      data = readGlobalValue(addr + i);
      if (data != ptr[i]) {
        inform("xactValueCheck: VALUE CHECK HAS FAILED!");
        warn("xactValueCheck: VALUE CHECK HAS FAILED!");
        DPRINTF(RubyHTM, "HTM: VALUE CHECK FAILED! "
                "PROC %d address=%#x HAS READ VALUE %#x, "
                "SHARED MEMORY VALUE WAS %#x \n",
                proc, addr+i, ptr[i], data);
        return false;
      }
      else {
        reads_from_shared_mem = true;
      }
    }
    else { // Loading address that has never been written inside a transaction
      // Ignore it, for now
      reads_untracked = true;
    }
  }
  assert(!(reads_from_shared_mem && reads_from_wb && reads_untracked));
  assert(reads_from_shared_mem || reads_from_wb || reads_untracked);
  if (size == 8) {
    unsigned long value = *((unsigned long *)ptr);
    _unused(value);
    DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x READS %d BYTES "
            "VALUE %018#x FROM %s\n", proc, addr, size, value,
            reads_from_shared_mem ?  "SHARED MEMORY" :
            (reads_untracked ? "SHARED MEMORY (UNTRACKED)" :
             "WRITE BUFFER"));
  }
  else if (size == 4) {
    unsigned int value = *((unsigned int *)ptr);
    _unused(value);
    DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x READS %d BYTES "
            "VALUE %010#x FROM %s\n", proc, addr, size, value,
            reads_from_shared_mem ? "SHARED MEMORY" :
            (reads_untracked ? "SHARED MEMORY (UNTRACKED)" :
             "WRITE BUFFER"));
  }
  else if (size == 2) {
    unsigned short value = *((unsigned short *)ptr);
    _unused(value);
    DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x READS %d BYTES "
            "VALUE %06#x FROM %s\n", proc, addr, size, value,
            reads_from_shared_mem ? "SHARED MEMORY" :
            (reads_untracked ? "SHARED MEMORY (UNTRACKED)" :
             "WRITE BUFFER"));
  }
  else if (size == 1) {
    unsigned char value = *((unsigned char *)ptr);
    _unused(value);
    DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x READS %d BYTES "
            "VALUE %#x FROM %s\n", proc, addr, size, value,
            reads_from_shared_mem ? "SHARED MEMORY" :
            (reads_untracked ? "SHARED MEMORY (UNTRACKED)" :
             "WRITE BUFFER"));
  }
  else {
    DPRINTF(RubyHTMvalues, "HTM: PROC %d address=%#x READS %d BYTES "
            "FROM %s\n", proc, addr, size, reads_from_shared_mem ?
            "SHARED MEMORY" :  (reads_untracked ? "SHARED MEMORY (UNTRACKED)"
                                : "WRITE BUFFER"));
  }


  return true;
}


void
CLASS_NS commitTransaction(int proc, TransactionInterfaceManager *xact_mgr,
                           CacheMemory *dataCache_ptr)
{
  bool hit;
  if (XACT_LAZY_VM) {
      /* ACTIONS AND SANITY CHECKS AT COMMIT:
       * - Check that write buffer values match values in cache
       * - Check that L1D cache has write permissions for all written lines
       */
      std::map<Addr, uint8_t>::iterator ii;
      for (ii = m_writeBuffer[proc].begin();
           ii != m_writeBuffer[proc].end(); ++ii) {

          // Make sure the values in the write buffer match those in
          // the L1 cache

          // First get write buffer byte address and value
          Addr byte_addr=ii->first;
          uint8_t wb_value=ii->second; // Value in write buffer

          // Now get L1 cache value
          DataBlock* datablock_ptr;
          // tryCacheAccess must find a write hit for all lines in the
          // write buffer
          hit = dataCache_ptr->
              tryCacheAccess(makeLineAddress(byte_addr),
                             RubyRequestType_ST,
                             datablock_ptr, false);
          if (!hit) {
              // Overflowed data that is buffered in the transactional
              // victim cache
              assert(false);
          }
          else {
              if (XACT_EAGER_CD) {
                  uint8_t cache_value =
                      datablock_ptr->getByte(getOffset(byte_addr));
                  _unused(cache_value);
                  assert(cache_value == wb_value); // Sanity check
              }
              else { // Ideal LL system writes speculative values to L1D now
                  panic("Ideal LL system ('magic' write buffer) not tested!");
                  datablock_ptr->setData(&wb_value, getOffset(byte_addr), 1);
              }
          }

          uint8_t old_value;
          _unused(old_value);
          bool old_value_exists = false;
          if (existGlobalValue(byte_addr)) {
              old_value = readGlobalValue(byte_addr);
              old_value_exists = true;
          }
          // Now make the new values visible to the global value checker
          writeGlobalValue(byte_addr, wb_value);


          if (old_value_exists) {
              DPRINTF(RubyHTMvalues, "HTM: PROC %d COMMITTING address=%#x "
                      "VALUE %#x (OLD VALUE WAS %x)\n", proc,
                      byte_addr, wb_value, old_value);
          }
          else {
              DPRINTF(RubyHTMvalues, "HTM: PROC %d COMMITTING address=%#x "
                      "VALUE %#x (FIRST WRITE)\n", proc,
                      byte_addr, wb_value);
          }
      }
  }
  else { // LogTM
      // Additional checks on committed values??
      // Clear logged values
      m_loggedValues[proc].clear();
      assert(m_unrolledValues[proc].empty());
  }

  // Clear write buffer contents
  m_writeBuffer[proc].clear();
  m_writeBufferBlocks[proc].clear();
}

void
CLASS_NS restartTransaction(int proc){
    // Must match std::map<Addr, DataBlock> > m_loggedValues;
    //std::map<Addr, DataBlock> > m_unrolledValues;
    // if logtm
    if (!XACT_LAZY_VM) { // LogTM
        auto it = m_loggedValues[proc].cbegin();
        bool mismatch = false;
        for (auto next_it = it;
             it != m_loggedValues[proc].cend(); it = next_it) {
            ++next_it;
            DataBlock loggedData =(*it).second;
            Addr addr = (*it).first;
            auto uit = m_unrolledValues[proc].find(addr);
            if (uit == m_unrolledValues[proc].end()) {
                panic("Logged data was not unrolled!");
            }
            DataBlock unrolledData =(*uit).second;
            if (!loggedData.equal(unrolledData)) {
                DPRINTF(RubyHTMvalues, "HTM: PROC %d"
                        " value mismatch line address=%#x "
                        " logged/unrolled\n%s\n%s\n\n", proc,
                        addr, loggedData.toString(),
                        unrolledData.toString());
                mismatch = true;
            }
            bool found = m_loggedValues[proc].erase(addr);
            assert(found);
            found = m_unrolledValues[proc].erase(addr);
            assert(found);
        }
        if (mismatch) {
            panic("Value mismatch in logged/unrolled data!");
        }
    }
    discardWriteBuffer(proc);
}

void
CLASS_NS notifyLoggedDataBlock(int proc, Addr addr,
                               DataBlock &data)
{
    assert(addr == makeLineAddress(addr));
    assert(m_loggedValues[proc].find(addr) ==
           m_loggedValues[proc].end());
    DataBlock db = DataBlock(data);
    m_loggedValues[proc][addr] = db;
}
void
CLASS_NS notifyUnrolledDataBlock(int proc,Addr addr,
                                 DataBlock &data)
{
    assert(addr == makeLineAddress(addr));
    // Will be called more than once for each datablock
    DataBlock db = DataBlock(data);
    m_unrolledValues[proc][addr] = db;
}

} // namespace ruby
} // namespace gem5
