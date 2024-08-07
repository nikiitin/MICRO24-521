/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_XACTVALUECHECKER_HH__
#define __MEM_RUBY_HTM_XACTVALUECHECKER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/structures/CacheMemory.hh"

namespace gem5
{
namespace ruby
{

typedef std::map<Addr, DataBlock> WriteSetValueMap;

class XactValueChecker {
public:
  XactValueChecker(RubySystem *rs);
  ~XactValueChecker();
  void notifyWrite(int proc, bool trans, Addr addr,
                   int size, uint8_t *data_ptr);

  void restartTransaction(int proc);
  void commitTransaction(int proc, TransactionInterfaceManager *xact_mgr,
                         CacheMemory *dataCache_ptr);
  bool xactValueCheck(int thread, Addr addr, int size,
                      const uint8_t *ptr);
   void notifyLoggedDataBlock(int proc, Addr addr, DataBlock &data);
  void notifyUnrolledDataBlock(int proc,Addr addr, DataBlock &data);

private:
  uint8_t readGlobalValue(Addr addr);
  bool existGlobalValue(Addr addr);
  void writeGlobalValue(Addr addr, uint8_t value);

  bool existBlockInWriteBuffer(int proc, Addr addr);
  void discardWriteBuffer(int proc);
  bool existInWriteBuffer(int proc, Addr addr);
  uint8_t getDataFromWriteBuffer(int proc, Addr addr);

  RubySystem *m_ruby_system;
  HTM *m_htm;

    /*
     */
  std::map<Addr, uint8_t> m_xact_data;
    /* Per-core map of data values written by ongoing transactions.
     */
  std::vector< std::map<Addr, uint8_t> > m_writeBuffer;
    /* Per-core map of block addresses written by ongoing transactions.
     */
  std::vector< std::map<Addr, uint8_t> > m_writeBufferBlocks;
    /* Per-core map of logged values */
  std::vector<std::map<Addr, DataBlock>> m_loggedValues;
    /* Per-core map of unrolled values */
  std::vector<std::map<Addr, DataBlock>> m_unrolledValues;

};

} // namespace ruby
} // namespace gem5

#endif

