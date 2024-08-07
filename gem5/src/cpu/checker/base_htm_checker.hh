/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Copyright (C) 2021 Eduardo José Gómez Hernández <eduardojose.gomez@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __CPU_HTM_EMPTY_CHECKER_HH__
#define __CPU_HTM_EMPTY_CHECKER_HH__

#include <iostream>
#include <utility>
#include <vector>

#include "base/trace.hh"
#include "config/the_isa.hh"
#include "cpu/base.hh"
#include "cpu/o3/comm.hh"
#include "sim/lockstep.hh"
#include "sim/value_record.hh"

namespace gem5
{

class BaseHTMChecker
{
public:
  BaseHTMChecker(const std::string &_my_name,
             BaseCPU *_cpu) {};

  /** Destructor. */
  ~BaseHTMChecker() {}

  virtual bool canReplay(uint64_t xid) { return true; }
  virtual void begin(uint64_t xid){}
  virtual void commit(uint64_t xid){}
  virtual void abort(){}
  virtual void retireInst(bool isMemRef,
                          bool isTransactional,
                          Trace::InstRecord *traceData){}
  virtual void open(){}
  virtual void create(){}

};

} // namespace gem5

#endif //  __CPU_HTM_EMPTY_CHECKER_HH__
