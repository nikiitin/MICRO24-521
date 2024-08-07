/*
 * Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
 * Universidad de Murcia
 */

#include "mem/ruby/htm/XactIsolationChecker.hh"

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "debug/RubyHTM.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{
namespace ruby
{

XactIsolationChecker::XactIsolationChecker(RubySystem *rs) {
  m_ruby_system = rs;
  m_htm = rs->params().system->getHTM();
  int num_sequencers = rs->params().num_of_sequencers;
  m_num_sequencers = num_sequencers;
  m_readSet.resize(num_sequencers);
  m_writeSet.resize(num_sequencers);
  m_abortingProcessor.resize(num_sequencers);
  for (int i = 0; i < num_sequencers; i++){
    m_abortingProcessor[i] = false;
  }
  if (!m_htm->params().eager_cd) {
      panic("Transaction isolate checker does not support"
            " lazy conflict detection!");
  }
}

XactIsolationChecker::~XactIsolationChecker() {
}

bool XactIsolationChecker::existInReadSet(int proc, Addr addr, Tick &since){
  bool found = false;

  if (m_readSet[proc].find(addr) != m_readSet[proc].end()) {
      found = true;
      since = m_readSet[proc][addr];
  }
  return found;
}

 bool XactIsolationChecker::existInWriteSet(int proc, Addr addr, Tick &since){
  bool found = false;

  if (m_writeSet[proc].find(addr) != m_writeSet[proc].end()){
      found = true;
      since = m_writeSet[proc][addr];
  }
  return found;
}

bool XactIsolationChecker::checkXACTIsolation(int proc, Addr addr, bool trans,
                                              RubyRequestType accessType){
   addr = makeLineAddress(addr);
  bool ok = true;

  for (int i = 0; i < m_num_sequencers; i++){
    if (i == proc) continue;
    // Processors that are in the process of aborting their
    // transactions.  It is ok to access the read/write sets belonging
    // to these transactions.
    Tick since, sinceR;
    if (m_abortingProcessor[i]) continue;
    switch(accessType){
      case RubyRequestType_LD:
          if (existInWriteSet(i, addr, since)){
              if (existInReadSet(proc, addr, sinceR)){
                  assert(trans);
                  DPRINTF(RubyHTM, "HTM: Isolation check failed addr %#x"
                          " read from proc %d since %ld in"
                          " write set of proc %d since %ld\n",
                          addr, proc, sinceR,i, since);
                  ok = false;
              } else {
                  if (trans) {
                      // Address not in read set: this is a
                      // speculative load that may have got Data_Stale
                      // and wil be eventually squashed
                      DPRINTF(RubyHTM, "HTM: Speculative transactional"
                              " load to addr %#x from proc %d while "
                              " addr in write set of proc %d since %ld\n",
                              addr, proc, i, since);
                  }
                  else { // It is ok for non-transactional loads to use
                      // Data_Stale (inv seen while outstanding load
                      // miss)
                      DPRINTF(RubyHTM, "HTM: Non-transactional load"
                              " to addr %#x from proc %d can use Data_Stale,"
                              " now in write set of proc %d since %ld\n",
                              addr, proc, i, since);
                  }
              }
          }
          break;
      case RubyRequestType_ST:
      case RubyRequestType_ATOMIC:
          if (existInReadSet(i, addr, since)){
              DPRINTF(RubyHTM, "HTM: Isolation check failed addr %#x"
                      " write from proc %d in"
                      " read set of proc %d since %ld\n",
                      addr, proc, i, since);
             ok = false;
          }
          if (existInWriteSet(i, addr, since)){
              DPRINTF(RubyHTM, "HTM: Isolation check failed addr %#x"
                      " write from proc %d in"
                      " write set of proc %d since %ld\n",
                      addr, proc, i, since);
             ok = false;
          }
          break;
       default:
           break;
     }
  }
  return ok;
}

void XactIsolationChecker::addToReadSet(int proc, Addr addr){
  if (m_readSet[proc].find(addr) ==
      m_readSet[proc].end()) {
      m_readSet[proc].
          insert(std::pair<Addr,Tick>(addr, curTick()));
      DPRINTF(RubyHTMverbose, "HTM: Isolation checker adds addr %#x"
              " to read set of proc %d \n",
              addr, proc);

  }
}

void XactIsolationChecker::addToWriteSet(int proc, Addr addr){
  if (m_writeSet[proc].find(addr) ==
      m_writeSet[proc].end()){
      m_writeSet[proc].
          insert(std::pair<Addr,Tick>(addr, curTick()));
      DPRINTF(RubyHTMverbose, "HTM: Isolation checker adds addr %#x"
              " to write set of proc %d \n",
              addr, proc);
  }
}

void XactIsolationChecker::removeFromReadSet(int proc, Addr addr){
  if (m_readSet[proc].find(addr) !=
      m_readSet[proc].end()){
    m_readSet[proc].erase(addr);
  }
}

void XactIsolationChecker::removeFromWriteSet(int proc, Addr addr){
  if (m_writeSet[proc].find(addr) !=
      m_writeSet[proc].end()){
    m_writeSet[proc].erase(addr);
  }
}

void XactIsolationChecker::clearWriteSet(int proc){
    m_writeSet[proc].clear();
}

void XactIsolationChecker::clearReadSet(int proc){
    m_readSet[proc].clear();
}

void XactIsolationChecker::setAbortingProcessor(int proc) {
  m_abortingProcessor[proc] = true;
}

void XactIsolationChecker::clearAbortingProcessor(int proc) {
  m_abortingProcessor[proc] = false;
}

void XactIsolationChecker::printReadWriteSets(int proc) {
  std::cout << " PROCESSOR: " << proc << std::endl;
  std::cout << " READ SET: ";
  for (int i = 0; i < m_readSet[proc].size(); i++){
    for (std::map<Addr,Tick>::iterator it =
           m_readSet[proc].begin();
         it!=m_readSet[proc].end();
         ++it) {
      Addr addr = it->first;
      std::cout << addr << " ";
    }
  }
  std::cout << std::endl;
  std::cout << " WRITE SET: ";
  for (int i = 0; i < m_writeSet[proc].size(); i++){
    for (std::map<Addr,Tick>::iterator it =
           m_writeSet[proc].begin();
         it!=m_writeSet[proc].end();
         ++it) {
      Addr addr = it->first;
      std::cout << addr << " ";
    }
  }
  std::cout << std::endl;
}


} // namespace ruby
} // namespace gem5

