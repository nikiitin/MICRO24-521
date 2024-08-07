/*
 * Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
 * Universidad de Murcia
 */

#ifndef __MEM_RUBY_HTM_XACTISOLATIONCHECKER_HH__
#define __MEM_RUBY_HTM_XACTISOLATIONCHECKER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"

namespace gem5
{
namespace ruby
{
class RubySystem;

class XactIsolationChecker
{
public:
  XactIsolationChecker(RubySystem *rs);
  ~XactIsolationChecker();

  bool checkXACTIsolation(int proc, Addr addr, bool trans,
                          RubyRequestType accessType);
  void addToReadSet(int proc, Addr addr);
  void addToWriteSet(int proc, Addr addr);
  void clearReadSet(int proc);
  void clearWriteSet(int proc);
  void removeFromReadSet(int proc, Addr addr);
  void removeFromWriteSet(int proc, Addr addr);
  bool existInReadSet(int proc, Addr addr, Tick &since);
  bool existInWriteSet(int proc, Addr addr, Tick &since);
  void setAbortingProcessor(int proc);
  void clearAbortingProcessor(int proc);
  void printReadWriteSets(int proc);

private:
  RubySystem *m_ruby_system;
  HTM *m_htm;

  std::vector< std::map<Addr, Tick> > m_readSet;
  std::vector< std::map<Addr, Tick> > m_writeSet;
  std::vector<bool> m_abortingProcessor;
  int m_num_sequencers;
};


} // namespace ruby
} // namespace gem5

#endif

