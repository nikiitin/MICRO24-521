/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __CPU_VALUE_RECORD_HH__
#define __CPU_VALUE_RECORD_HH__

#include "base/types.hh"

class ValueRecord {
public:
    ValueRecord(bool store,
                uint64_t _pc, uint64_t _upc,
                gem5::Addr addr, uint64_t val,
                uint64_t seqno = 0)
        : wasStore(store),
          pc(_pc), upc(_upc),
          address(addr), value(val),
          seqNum(seqno)
    {}
    bool wasStore;
    uint64_t pc;
    uint64_t upc;
    gem5::Addr address;
    uint64_t value;
    uint64_t seqNum;
};

#endif
