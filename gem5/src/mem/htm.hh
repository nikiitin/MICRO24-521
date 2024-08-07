/*
 * Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
 * Universidad de Murcia
 *
 * Copyright (c) 2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_HTM_HH__
#define __MEM_HTM_HH__

#include <cassert>
#include <map>
#include <string>
#include <vector>

#include "params/HTM.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

class HTMStats
{
public:
    enum AbortCause
    {
        Conflict = 0,
        L0Capacity,
        L1Capacity,
        L2Capacity,
        PageFault,
        Syscall,
        Interrupt,
        Explicit,
        FallbackLock,
        ExplicitFallbackLock,
        WrongL0,
        ConflictStale,
        SpecValidation,
        SpecDepthOverflow,
        SpecMaxCapacityOverflow,
        SpecWriteInFlight,
        SpecReadConflict,
        IllegalData,
        Undefined,
        ConflictPower,
        NumAbortCauses
    };

    enum XBeginArgsType
    {
        Xid,
        NumRetries,
        NumXBeginArgsTypes
    };

    static int MaxXidStats;
    static const int MaxLogFilterSize = 8; // Max log filter addresses

    static int to_stats_xid(int xid) {
      // Set the xid in m_stats according to the maximum number of xid
      // passed as command line option to the simulator. By default:
      // per-xid statistics subsumed into a single xid (0)
        if (xid < HTMStats::MaxXidStats)
            return xid;
        else {
            return HTMStats::MaxXidStats - 1;
        }
    }
    static const std::string AbortCause_to_string(AbortCause cause) {
        switch(cause) {
        case Conflict:
            return "Conflict";
        case L0Capacity:
            return "L0Capacity";
        case L1Capacity:
            return "L1Capacity";
        case L2Capacity:
            return "L2Capacity";
        case PageFault:
            return "PageFault";
        case Syscall:
            return "Syscall";
        case Interrupt:
            return "Interrupt";
        case Explicit:
            return "Explicit";
        case FallbackLock:
            return "FallbackLock";
        case ExplicitFallbackLock:
            return "ExplicitFallbackLock";
        case WrongL0:
            return "WrongL0";
        case ConflictStale:
            return "ConflictStale";
        case ConflictPower:
            return "ConflictPower";
        case SpecValidation:
            return "SpecValidation";
        case SpecDepthOverflow:
            return "SpecDepthOverflow";
        case SpecMaxCapacityOverflow:
            return "SpecMaxCapacityOverflow";
        case SpecWriteInFlight:
            return "SpecWriteInFlight";
        case SpecReadConflict:
            return "SpecReadConflict";
        case IllegalData:
            return "IllegalData";
        case Undefined:
            return "Undefined";
        case NumAbortCauses:
        default:
            {
            assert(0);
            return "Error";
            }
        }
    }
};


enum class HtmFailureFaultCause : int
{
    INVALID = -1,
    EXPLICIT,
    NEST,
    SIZE,
    EXCEPTION,
    MEMORY,
    OTHER,
    DISABLED, // htm speculation disabled: lockstep debugging facility
    // The following causes are not visible to the ISA, only used for
    // statistics collection. Each must fall in one of the above
    // categories
    INTERRUPT,
    /* LSQ: conflicting snoop seen by CPU for trans load not yet in
       Rset, caused either by remote requests or local replacements */
    LSQ,
    /* Precise abort cause, set by xact mgr based on abortcause */
    SIZE_RSET,
    SIZE_WSET,
    SIZE_L1PRIV,
    SIZE_LLC,
    SIZE_WRONG_CACHE,
    EXPLICIT_FALLBACKLOCK,
    MEMORY_FALLBACKLOCK,
    MEMORY_STALEDATA,
    MEMORY_FALSESHARING,
    MEMORY_POWER,
    SPEC_ILLEGAL,
    SPEC_VALIDATION,
    SPEC_MAX_CAPACITY,
    SPEC_DEPTH,
    SPEC_WRITE_IN_FLIGHT,
    SPEC_READ_CONFLICT,
    NUM_CAUSES
};

enum class HtmCacheFailure
{
    NO_FAIL,     // no failure in cache
    FAIL_SELF,   // failed due local cache's replacement policy
    FAIL_REMOTE, // failed due remote invalidation
    FAIL_REMOTE_POWER, // failed due remote power transaction
    FAIL_SPEC,   // failed due validation
    FAIL_OTHER,  // failed due other circumstances
};

/** Convert precise failure cause (stats) to ISA visible cause  */
HtmFailureFaultCause
getIsaVisibleHtmFailureCause(HtmFailureFaultCause cause);

/** Convert enum into string to be used for debug purposes */
std::string htmFailureToStr(HtmFailureFaultCause cause);

/** Convert enum into string to be used for debug purposes */
std::string htmFailureToStr(HtmCacheFailure rc);

class HtmPolicyStrings
{
public:
  static const std::string requester_wins;
  static const std::string requester_loses;
  static const std::string power_tm;
  static const std::string woper_tm;
  static const std::string power_tm_consumer_wins;
  static const std::string woper_tm_consumer_wins;
  static const std::string committer_wins;
  static const std::string requester_stalls;
  static const std::string magic;
  static const std::string token;
  static const std::string requester_stalls_pic_base;
  static const std::string requester_stalls_cda_base;
  static const std::string requester_stalls_cda_base_ntx;
  static const std::string requester_stalls_cda_hybrid;
  static const std::string requester_stalls_cda_hybrid_ntx;
};



class HTM : public ClockedObject
{
  public:
    const HTMParams &_params;
    Addr m_fallbackLockPhysicalAddress;
    Addr m_fallbackLockVirtualAddress;

    PARAMS(HTM);
    HTM(const Params &p);
    Addr getFallbackLockPAddr() { return m_fallbackLockPhysicalAddress; }
    void setFallbackLockPAddr(Addr addr) {
        if (m_fallbackLockPhysicalAddress == Addr(0)) {
            m_fallbackLockPhysicalAddress = addr;
        } else {
            assert(m_fallbackLockPhysicalAddress == addr);
        }
    }
    Addr getFallbackLockVAddr() { return m_fallbackLockVirtualAddress; }
    virtual void notifyFallbackLockAcquired(int cpuId, long htmXid) {};
    virtual void notifyFallbackLockRelease(int cpuId, long htmXid) {};
    virtual void notifyPseudoInst() {};
    virtual void notifyPseudoInstWork(bool begin, int cpuId, uint64_t workid) {};
    virtual bool setupLog(int cpuId, Addr addr) {
        panic("Not implemented");
    };
    virtual void endLogUnroll(int cpuId) {
        panic("Not implemented");
    };
    virtual int getLogNumEntries(int cpuId) {
        panic("Not implemented");
    };
    virtual int getCommitStatus(int cpuId) {
        panic("Not implemented");
    };
    virtual bool isHtmFailureFaultCauseMemoryPower(int cpuId) {
        panic("Not implemented");
    };
    void requestCommitToken(int cpuId);
    void releaseCommitToken(int cpuId);
    void removeCommitTokenRequest(int cpuId);
    bool existCommitTokenRequest(int cpuId);
    int getTokenOwner();
    int getNumTokenRequests();
  enum ResolutionPolicy
  {
      RequesterWins,
      RequesterLoses,
      RequesterStalls,
      RequesterStallsPiCBased,
      Undefined
  };

  enum HtmFwdMechanism
  {
    no_fwd,
    naive,
    simple,
    W1_D1,
    WN_D1,
    W1_DN,
    WN_DM,
    WX_DX
  };
  HtmFwdMechanism getFwdMechanism();

  enum HtmHwPrioMechanism
  {
    no_prio,
    numeric
  };
  HtmHwPrioMechanism getHwPrioMechanism();

  enum HtmFwdCycleSolveStrategy
  {
    no_strategy,
    abort,
    retry
  };

  enum HtmAllowedDataFwd
  {
    all_data,
    only_written,
    no_write_in_flight
  };

  enum HtmNumericPrioAcq
  {
    positive,
    negative
  };
  HtmNumericPrioAcq getSwNumAcq();
  HtmNumericPrioAcq getHwNumAcq();
  HtmFwdCycleSolveStrategy getFwdCycleSolveStrategy();
  HtmAllowedDataFwd getAllowedDataFwd();
  ResolutionPolicy getResolutionPolicy();

private:
    std::vector<int>  m_commitTokenRequestList;
};

} // namespace gem5

#endif // __MEM_HTM_HH__
