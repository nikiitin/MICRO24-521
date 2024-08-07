/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_HTM_HH__
#define __MEM_RUBY_HTM_HTM_HH__

#include <map>
#include <string>

#include "base/statistics.hh"
#include "mem/htm.hh"
#include "mem/ruby/profiler/annotated_regions.h"
#include "params/RubyHTM.hh"

namespace gem5
{

namespace ruby
{

class RubyHTM : public HTM
{
  public:
    PARAMS(RubyHTM);
    RubyHTM(const Params &p);
    void notifyPseudoInst() override;
    void notifyPseudoInstWork(bool begin, int cpuId, uint64_t workid) override;
    bool setupLog(int cpuId, Addr addr) override;
    void endLogUnroll(int cpuId) override;
    int getLogNumEntries(int cpuId) override;
    int getCommitStatus(int cpuId) override;
    bool isHtmFailureFaultCauseMemoryPower(int cpuId) override;

    void profileRegion(AnnotatedRegion region, uint64_t cycles);
    void notifyFallbackLockAcquired(int cpuId, long htmXid) override;
    void notifyFallbackLockRelease(int cpuId, long htmXid) override;
    private:
      struct RubyHTMStats : public statistics::Group
      {
          RubyHTMStats(statistics::Group *parent);

          statistics::Vector cyclesInRegion;
          statistics::Vector regionChanges;
          statistics::Scalar flAcquisitions;
      } rubyHTMStats;

};

} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_HTM_HTM_HH__
