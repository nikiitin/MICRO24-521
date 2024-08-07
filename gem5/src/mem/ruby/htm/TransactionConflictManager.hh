/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_TRANSACTIONCONFLICTMANAGER_HH__
#define __MEM_RUBY_HTM_TRANSACTIONCONFLICTMANAGER_HH__

#include <map>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/MachineID.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"

namespace gem5
{
namespace ruby
{

using namespace std;

class TransactionConflictManager {
public:
  TransactionConflictManager(TransactionInterfaceManager *xact_mgr,
                             int version);
  ~TransactionConflictManager();

  void beginTransaction(bool power_mode = false);
  void commitTransaction();
  void restartTransaction();

  bool shouldNackLoad(Addr addr,
                      MachineID remote_id,
                      Cycles remote_timestamp,
                      TransactionBit remote_trans,
                      int remote_priority,
                      bool remote_sw_priority);
  bool shouldNackStore(Addr addr,
                       MachineID remote_id,
                       Cycles remote_timestamp,
                       TransactionBit remote_trans,
                       int remote_priority,
                       bool remote_sw_priority,
                       bool local_is_exclusive);
  bool possibleCycle();
  void setPossibleCycle();
  void clearPossibleCycle();
  bool nackReceived();
  bool abortedByNack();

  bool doomed();
  void setDoomed();

  void notifySendNack(Addr physicalAddress, Cycles remote_timestamp,
                      MachineID remote_id, int remote_priority = NC_PRIO_VALUE,
                      bool sw_remote_priority = false);
  void notifyReceiveNack(Addr addr, Cycles remote_timestamp,
                         TransactionBit remote_trans,
                         int remote_priority,
                         bool remote_sw_priority,
                         MachineID remote_id, bool writer);
  bool hasHighestPriority();

  Cycles getTimestamp();
  Cycles getOldestTimestamp();
  bool isRequesterStallsPolicy();
  bool isReqLosesPolicy();
  bool isReqWinsPolicy();
  bool isPowerTMPolicy();
  bool isProducerPolicy();
  bool isWoperTMPolicy();
  bool isPowered();
  bool isDataSpecConsumerValid();

  Addr getNackedPossibleCycleAddr() {
      assert(isRequesterStallsPolicy());
      assert(m_sentNack);
      return m_sentNackAddr; };

  int getNumRetries();

  void setVersion(int version);
  int getVersion() const;
  bool isRemoteOlder(Cycles local_timestamp,
                     Cycles remote_timestamp,
                     MachineID remote_id);

private:
  int getProcID() const;
  bool shouldNackRemoteRequest(Addr addr,
                               MachineID remote_id,
                               Cycles remote_timestamp,
                               TransactionBit remote_trans,
                               int remote_priority,
                               bool remote_sw_priority,
                               bool remoteWriter);
  bool chooseWinnerPriority(TransactionBit remote_trans,
                            int remote_priority,
                            bool remote_sw_priority);

  TransactionInterfaceManager *m_xact_mgr;
  int m_version;

  Cycles m_timestamp;
  bool   m_possible_cycle;
  bool   m_lock_timestamp;
  int    m_numRetries;
  bool   m_receivedNack;
  bool   m_abortedByNack;
  bool   m_sentNack;
  Addr   m_sentNackAddr;
  bool   m_doomed;
  bool   m_powered;
  HTM::ResolutionPolicy  m_default_policy;
  std::string  m_policy;
  std::string  m_lazy_validated_policy;
  bool m_policy_is_req_stalls_cda;
  bool m_policy_nack_non_transactional;
  bool m_policy_power;
};

} // namespace ruby
} // namespace gem5

#endif


