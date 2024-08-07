/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "debug/RubyHTM.hh"
#include "mem/ruby/htm/TransactionConflictManager.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/TransactionIsolationManager.hh"

//#include "XactIsolationChecker.h"

namespace gem5
{
namespace ruby
{


using namespace std;

#define XACT_MGR m_xact_mgr

#define EXECUTED_LOAD ('x')
#define RETIRED_LOAD ('r')
#define OVERTAKING_LOAD ('o')
#define RETIRED_STORE ('w')

TransactionIsolationManager::
TransactionIsolationManager(TransactionInterfaceManager *xact_mgr,
                            int version) {

  m_version = version;
  m_xact_mgr = xact_mgr;

}

TransactionIsolationManager::~TransactionIsolationManager() {
}

void TransactionIsolationManager::setVersion(int version) {
  m_version = version;
}

int TransactionIsolationManager::getVersion() const {
  return m_version;
}

int TransactionIsolationManager::getProcID() const{
  return m_xact_mgr->getProcID();
}

void TransactionIsolationManager::beginTransaction(){
  if (!m_readSet.empty()) {
      // Do not clear Rset on beginTransaction since in O3CPU trans
      // loads may overtake the xbegin.
      DPRINTF(RubyHTM,"HTM: PROC %d beginTransaction finds"
              " non-empty read set\n", getProcID());
  }
  //  m_readSet.clear();
  m_writeSet.clear();
}

void TransactionIsolationManager::commitTransaction(){
  for ( map<Addr, char>::iterator ii =
            m_readSet.begin();
        ii!=m_readSet.end(); ++ii) {
      Addr key=(*ii).first;
      if (XACT_MGR->config_preciseReadSetTracking()) {
          assert(ii->second == RETIRED_LOAD);
      } else if (ii->second != RETIRED_LOAD) {
          // TODO: Executed load never retired: profile?
          DPRINTF(RubyHTM,
                  "HTM: PROC %d commitTransaction finds "
                  "non-retired read set block addr %x (%c)\n",
                  getProcID(), key, ii->second);
      }
  }

  clearReadSetPerfectFilter();
  clearWriteSetPerfectFilter();

}

void TransactionIsolationManager::abortTransaction(){
    clearReadSetPerfectFilter();
    clearWriteSetPerfectFilter();
}

void TransactionIsolationManager::releaseIsolation(){

  clearReadSetPerfectFilter();
  clearWriteSetPerfectFilter();

}

void TransactionIsolationManager::releaseReadIsolation(){

  clearReadSetPerfectFilter();

  DPRINTF(RubyHTM,"HTM: PROC %d releaseReadIsolation (abort) \n",
          getProcID());

}

bool
TransactionIsolationManager::isInReadSetPerfectFilter(Addr addr) {

  map<Addr, char>::iterator it =
      m_readSet.find(makeLineAddress(addr));
  if (it != m_readSet.end()) {
      if (XACT_MGR->getTransactionLevel() == 0) {
          // Trans loads cannot retire before htm_start instruction
          assert(it->second != RETIRED_LOAD);
      }
      if (XACT_MGR->config_preciseReadSetTracking()) {
          assert(it->second == RETIRED_LOAD);
      }
      return true;
  } else {
      return false;
  }
}

bool
TransactionIsolationManager::isInWriteSetPerfectFilter(Addr addr){
  map<Addr, char>::iterator it =
      m_writeSet.find(makeLineAddress(addr));
  if (it != m_writeSet.end()) {
      assert(it->second == RETIRED_STORE);
      return true;
  } else {
      return false;
  }
}

void
TransactionIsolationManager::addToReadSetPerfectFilter(Addr address){
  Addr addr = makeLineAddress(address);
  char value = EXECUTED_LOAD;
  if (XACT_MGR->getTransactionLevel() == 0) {
      // Sanity checks: distinguish read-set blocks that have been
      // isolated before xact mgr::beginTransaction called
      value = OVERTAKING_LOAD; // (o)vertaking load
  }

  if (m_readSet.find(addr) ==
      m_readSet.end())
    m_readSet.
      insert(std::pair<Addr,char>(addr, value));

  assert(m_readSet.find(addr) !=
         m_readSet.end());

}

void
TransactionIsolationManager::addToRetiredReadSet(Addr addr)
{
    map<Addr, char>::iterator it =
        m_readSet.find(makeLineAddress(addr));
    assert(it != m_readSet.end());
    if (it->second == OVERTAKING_LOAD) {
          // TODO: Executed load never retired: profile?
        DPRINTF(RubyHTM,
                "HTM: PROC %d addToRetiredReadSet finds "
                "overtaking load to block addr %x\n",
                getProcID(), addr);
    } else {
        assert(it->second == EXECUTED_LOAD);
    }
    it->second = RETIRED_LOAD;
}

bool
TransactionIsolationManager::inRetiredReadSet(Addr addr) {
  map<Addr, char>::iterator it =
      m_readSet.find(makeLineAddress(addr));
  assert(it != m_readSet.end());
  // Trans loads cannot retire before htm_start instruction
  return (it->second == RETIRED_LOAD);
}

bool
TransactionIsolationManager::wasOvertakingRead(Addr addr) {
    map<Addr, char>::iterator it =
        m_readSet.find(addr);
    assert(it != m_readSet.end());
    return it->second == OVERTAKING_LOAD;
}

void
TransactionIsolationManager::removeFromReadSetPerfectFilter(Addr address){
  Addr addr = makeLineAddress(address);

  map<Addr, char>::iterator it =
    m_readSet.find(addr);
  if (it != m_readSet.end()) {
    m_readSet.erase(it);
  }
  else { // release address not in Rset??
    assert(false);
  }
  assert(m_readSet.find(addr) ==
         m_readSet.end());
}

void
TransactionIsolationManager::removeFromWriteSetPerfectFilter(Addr address){
  Addr addr = makeLineAddress(address);

  map<Addr, char>::iterator it =
    m_writeSet.find(addr);
  if (it != m_writeSet.end()) {
    m_writeSet.erase(it);
  }
  else { // release address not in Wset??
    assert(false);
  }
  assert(m_writeSet.find(addr) ==
         m_writeSet.end());
}


void
TransactionIsolationManager::addToWriteSetPerfectFilter(Addr address){
  Addr addr = makeLineAddress(address);

  if (m_writeSet.find(addr) ==
      m_writeSet.end())
    m_writeSet.
      insert(std::pair<Addr,char>(addr, RETIRED_STORE));

  assert(m_writeSet.find(addr) !=
         m_writeSet.end());
}

void
TransactionIsolationManager::clearReadSetPerfectFilter(){
  m_readSet.clear();
  assert(m_readSet.size() == 0);
}

void
TransactionIsolationManager::clearWriteSetPerfectFilter(){

  m_writeSet.clear();
  assert(m_writeSet .size() == 0);

  m_writeSetInWriteBuffer.clear(); // ideal lazy VM
}


int TransactionIsolationManager::getReadSetSize(){
  return m_readSet.size();
}

int TransactionIsolationManager::getWriteSetSize(){
  return m_writeSet.size();
}

vector<Addr> *
TransactionIsolationManager::getWriteSet()
{
  // Allocates a new vector<Addr> with the write set and returns it
  // Caller must delete the object after it is done with it

  vector<Addr> *wset = new vector<Addr>();
  for ( map<Addr, char>::iterator ii =
          m_writeSet.begin();
        ii!=m_writeSet.end(); ++ii) {
    Addr key=(*ii).first;
    wset->push_back(key);
  }
  return wset;
}

vector<Addr> *
TransactionIsolationManager::getReadSet()
{
  // Allocates a new vector<Addr> with the read set and returns it
  // Caller must delete the object after it is done with it

  vector<Addr> *rset = new vector<Addr>();
  for ( map<Addr, char>::iterator ii =
          m_readSet.begin();
        ii!=m_readSet.end(); ++ii) {
    Addr key=(*ii).first;
    rset->push_back(key);
  }
  return rset;
}

void 
TransactionIsolationManager::redirectedStoreToWriteBuffer(Addr addr)
{
    m_writeSetInWriteBuffer.
      insert(std::pair<Addr,char>(addr, RETIRED_STORE));
}

bool
TransactionIsolationManager::isRedirectedStoreToWriteBuffer(Addr addr)
{
    return m_writeSetInWriteBuffer.find(addr) !=
        m_writeSetInWriteBuffer.end();
}

bool
TransactionIsolationManager::hasConflictWith(TransactionIsolationManager* new_committer)
{
    // this: ongoing committer
    // Check write set of ongoing committer against read-write set of new committer
    for ( map<Addr, char>::iterator ii =
              m_writeSetInWriteBuffer.begin();
          ii!=m_writeSetInWriteBuffer.end(); ++ii) {
        Addr waddr=(*ii).first;
        if (new_committer->isInReadSetPerfectFilter(waddr)) {
            return true;
        }
        else if (new_committer->isRedirectedStoreToWriteBuffer(waddr)) {
            return true;
        }
    }
    return false;
}


} // namespace ruby
} // namespace gem5
