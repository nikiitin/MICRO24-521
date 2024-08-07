/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_TRANSACTIONISOLATIONMANAGER_HH__
#define __MEM_RUBY_HTM_TRANSACTIONISOLATIONMANAGER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/Address.hh"

namespace gem5
{
namespace ruby
{

class TransactionIsolationManager {
public:
  TransactionIsolationManager(TransactionInterfaceManager *xact_mgr,
                              int version);
  ~TransactionIsolationManager();

  void beginTransaction();
  void commitTransaction();
  void abortTransaction();
  void releaseIsolation();
  void releaseReadIsolation();

  /* These functions query the perfect filter */
  bool isInReadSetPerfectFilter(Addr addr);
  bool isInWriteSetPerfectFilter(Addr addr);
  void addToReadSetPerfectFilter(Addr addr);
  void removeFromReadSetPerfectFilter(Addr addr);
  void removeFromWriteSetPerfectFilter(Addr addr);
  void addToWriteSetPerfectFilter(Addr addr);
  void clearReadSetPerfectFilter();
  void clearWriteSetPerfectFilter();
  void setFiltersToXactLevel(int new_xact_level = 0,
                             int old_xact_level = 1);

  bool inRetiredReadSet(Addr addr); // Profiling
  void addToRetiredReadSet(Addr addr);
  bool wasOvertakingRead(Addr addr); // Sanity checks

  bool hasConflictWith(TransactionIsolationManager* another);
  void redirectedStoreToWriteBuffer(Addr addr);
  bool isRedirectedStoreToWriteBuffer(Addr addr);

  int  getReadSetSize();
  int  getWriteSetSize();

  std::vector<Addr> *getReadSet();
  std::vector<Addr> *getWriteSet();

  void setVersion(int version);
  int getVersion() const;

private:
  int getProcID() const;

  TransactionInterfaceManager *m_xact_mgr;
  int m_version;

  std::map<Addr, char> m_readSet;
  std::map<Addr, char> m_writeSet;

  // Lazy VM ideal write buffer
  std::map<Addr, char> m_writeSetInWriteBuffer;

  std::vector<int> m_xact_readCount;
  std::vector<int> m_xact_writeCount;
  std::vector<int> m_xact_overflow_readCount;
  std::vector<int> m_xact_overflow_writeCount;
};

} // namespace ruby
} // namespace gem5

#endif

