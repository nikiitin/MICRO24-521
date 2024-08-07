/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_LAZYTRANSACTIONCOMMITARBITER_HH__
#define __MEM_RUBY_HTM_LAZYTRANSACTIONCOMMITARBITER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/structures/CacheMemory.hh"

namespace gem5
{
namespace ruby
{

using namespace std;

class TransactionInterfaceManager;

class LazyTransactionCommitArbiter {
public:
  LazyTransactionCommitArbiter(TransactionInterfaceManager *xact_mgr,
                               int version,
                               string lazy_validation_policy);
  ~LazyTransactionCommitArbiter();

  void beginTransaction();
  void commitTransaction();
  void restartTransaction();
  bool shouldValidateTransaction();
  void initiateValidateTransaction();
  bool validated() { return m_validated; };
  bool validating() { return m_validating; };


private:
  TransactionInterfaceManager *m_xact_mgr;
  int m_version;
  bool m_validated;
  bool m_validating;
  string m_policy;
};


} // namespace ruby
} // namespace gem5

#endif

