/*
  Copyright (C) 2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/common/Global.hh"
#include "mem/ruby/htm/htm.hh"
#include "mem/ruby/htm/EagerTransactionVersionManager.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/profiler/XactProfiler.hh"

namespace gem5
{

namespace ruby
{

RubyHTM::RubyHTM(const Params &p)
    : HTM(p),
    rubyHTMStats(this)
{
}

void
RubyHTM::notifyPseudoInst() {
    /* Per thread "state" cycles (non-txnal, aborted, etc.)  must
       be accumulated before pseudo instruction serviced,
       simulating a state change. This solves the problem of the
       missing cycles due to the stretch between the last state
       change and the m5_exit instruction.
    */
    assert(g_system_ptr);
    g_system_ptr->getProfiler()->
        getXactProfiler()->profileCurrentAnnotatedRegion();
}

void
RubyHTM::notifyPseudoInstWork(bool begin, int cpuId, uint64_t reg) {
    if (!g_system_ptr) return;
    AnnotatedRegion_t region = (AnnotatedRegion_t)reg;
    assert(AnnotatedRegion_isValidRegion(region));
    if (g_system_ptr->getProfiler()->hasXactProfiler()) {
        if (begin) {
            g_system_ptr->getProfiler()->getXactProfiler()->
                beginRegion(cpuId, region);
        } else{
            g_system_ptr->getProfiler()->getXactProfiler()->
                endRegion(cpuId, region);
        }
    }
}

bool
RubyHTM::setupLog(int cpuId, Addr addr)
{
    // No need to setup log if lazy versioning
    if (params().lazy_vm) return false;

    assert(g_system_ptr);

    if (g_system_ptr->
        getTransactionInterfaceManager(cpuId)->
        getXactEagerVersionManager()->isReady()) {
        if (addr == 0) { // Shutdown log signal
            for (int i = 0;
                 i < g_system_ptr->params().num_of_sequencers;
                 ++i) {
                g_system_ptr->
                    getTransactionInterfaceManager(i)->
                    getXactEagerVersionManager()->shutdownLog();
            }
        } else {
            Addr baseAddr = g_system_ptr->
                getTransactionInterfaceManager(cpuId)->
                getXactEagerVersionManager()->getLogBaseVirtualAddress();
            // Sanity checks
            assert(baseAddr == addr);
        }
        return false;
    } else {
        g_system_ptr->
            getTransactionInterfaceManager(cpuId)->
            getXactEagerVersionManager()->setLogBaseVirtualAddress(addr);
        return true; // Needs walk to initialize log v2p translation table
    }
}

void
RubyHTM::endLogUnroll(int cpuId)
{
    // No need to setup log if lazy versioning
    if (params().lazy_vm) return;

    assert(g_system_ptr);

    g_system_ptr->
        getTransactionInterfaceManager(cpuId)->endLogUnroll();
}

int
RubyHTM::getLogNumEntries(int cpuId)
{
    // No need to setup log if lazy versioning
    if (params().lazy_vm) return 0;

    assert(g_system_ptr);

    return g_system_ptr->
        getTransactionInterfaceManager(cpuId)->
        getLogNumEntries();
}

int
RubyHTM::getCommitStatus(int cpuId)
{
    return 0;
}

bool
RubyHTM::isHtmFailureFaultCauseMemoryPower(int cpuId)
{
    return g_system_ptr->
        getTransactionInterfaceManager(cpuId)->
        isTransactionAbortedByRemotePower();
}

RubyHTM::
RubyHTMStats::RubyHTMStats(statistics::Group *parent)
    : statistics::Group(parent),
      ADD_STAT(cyclesInRegion, ""),
      ADD_STAT(regionChanges, ""),
      ADD_STAT(flAcquisitions, "")
{
    cyclesInRegion
        .init(AnnotatedRegion_NUM)
        .flags(statistics::pdf | statistics::total);

    regionChanges
        .init(AnnotatedRegion_NUM)
        .flags(statistics::pdf | statistics::total);

    for (int i = 0; i < AnnotatedRegion_NUM; i++) {
        cyclesInRegion
            .subname(i,
                     AnnotatedRegion_to_string(AnnotatedRegion(i)))
            .flags(statistics::nozero)
            ;
        regionChanges
            .subname(i,
                     AnnotatedRegion_to_string(AnnotatedRegion(i)))
            .flags(statistics::nozero)
            ;
    }
}

void
RubyHTM::profileRegion(AnnotatedRegion region, uint64_t cycles)
{
    rubyHTMStats.cyclesInRegion[region] += cycles;
    rubyHTMStats.regionChanges[region]++;
}

void
RubyHTM::notifyFallbackLockAcquired(int cpuId, long htmXid)
{
    rubyHTMStats.flAcquisitions++;
    g_system_ptr->
        getTransactionInterfaceManager(cpuId)->
        profileFlAcquisition(htmXid);
}

void
RubyHTM::notifyFallbackLockRelease(int cpuId, long htmXid)
{
    g_system_ptr->
        getTransactionInterfaceManager(cpuId)->
        profileFlRelease(htmXid);
}


} // namespace ruby
} // namespace gem5
