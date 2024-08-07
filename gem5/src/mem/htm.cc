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

#include "mem/htm.hh"

#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/profiler/XactProfiler.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

const std::string HtmPolicyStrings::requester_wins = "requester_wins";
const std::string HtmPolicyStrings::requester_loses = "requester_loses";
const std::string HtmPolicyStrings::power_tm = "requester_wins_power";
const std::string HtmPolicyStrings::woper_tm = "requester_loses_power";
const std::string HtmPolicyStrings::power_tm_consumer_wins = "requester_wins_power_consumer";
const std::string HtmPolicyStrings::woper_tm_consumer_wins = "requester_loses_power_consumer";
const std::string HtmPolicyStrings::committer_wins = "committer_wins";
const std::string HtmPolicyStrings::requester_stalls = "requester_stalls";
const std::string HtmPolicyStrings::requester_stalls_pic_base = "requester_stalls_pic_base";
const std::string HtmPolicyStrings::magic = "magic";
const std::string HtmPolicyStrings::token = "token";
const std::string HtmPolicyStrings::requester_stalls_cda_base =
                                   "requester_stalls_cda_base";
const std::string HtmPolicyStrings::requester_stalls_cda_base_ntx =
                                   "requester_stalls_cda_base_ntx";
const std::string HtmPolicyStrings::requester_stalls_cda_hybrid =
                                   "requester_stalls_cda_hybrid";
const std::string HtmPolicyStrings::requester_stalls_cda_hybrid_ntx =
                                   "requester_stalls_cda_hybrid_ntx";

HtmFailureFaultCause
getIsaVisibleHtmFailureCause(HtmFailureFaultCause cause)
{
    HtmFailureFaultCause isaVisibleCause = cause;
    switch(cause) {
    case HtmFailureFaultCause::EXPLICIT:
    case HtmFailureFaultCause::NEST:
    case HtmFailureFaultCause::SIZE:
    case HtmFailureFaultCause::EXCEPTION:
    case HtmFailureFaultCause::MEMORY:
    case HtmFailureFaultCause::OTHER:
    case HtmFailureFaultCause::DISABLED:
        break;
    case HtmFailureFaultCause::INTERRUPT:
        isaVisibleCause = HtmFailureFaultCause::OTHER;
        break;
    case HtmFailureFaultCause::LSQ:
    case HtmFailureFaultCause::EXPLICIT_FALLBACKLOCK:
    case HtmFailureFaultCause::MEMORY_FALLBACKLOCK:
    case HtmFailureFaultCause::MEMORY_STALEDATA:
    case HtmFailureFaultCause::MEMORY_FALSESHARING:
    case HtmFailureFaultCause::MEMORY_POWER:
    case HtmFailureFaultCause::SPEC_VALIDATION:
    case HtmFailureFaultCause::SPEC_DEPTH:
    case HtmFailureFaultCause::SPEC_MAX_CAPACITY:
    case HtmFailureFaultCause::SPEC_WRITE_IN_FLIGHT:
    case HtmFailureFaultCause::SPEC_READ_CONFLICT:
    case HtmFailureFaultCause::SPEC_ILLEGAL:
        isaVisibleCause = HtmFailureFaultCause::MEMORY;
        break;
    case HtmFailureFaultCause::SIZE_RSET:
    case HtmFailureFaultCause::SIZE_WSET:
    case HtmFailureFaultCause::SIZE_L1PRIV:
    case HtmFailureFaultCause::SIZE_LLC:
        isaVisibleCause = HtmFailureFaultCause::SIZE;
        break;
    case HtmFailureFaultCause::SIZE_WRONG_CACHE:
        isaVisibleCause = HtmFailureFaultCause::OTHER;
        break;
    default:
        panic("Unexpected HtmFailureFault cause %s\n",
              htmFailureToStr(cause));
    }
    return isaVisibleCause;
};


std::string
htmFailureToStr(HtmFailureFaultCause cause)
{
    static const std::map<HtmFailureFaultCause, std::string> cause_to_str = {
        { HtmFailureFaultCause::EXPLICIT, "explicit" },
        { HtmFailureFaultCause::NEST, "nesting_limit" },
        { HtmFailureFaultCause::SIZE, "transaction_size" },
        { HtmFailureFaultCause::EXCEPTION, "exception" },
        { HtmFailureFaultCause::INTERRUPT, "interrupt" },
        { HtmFailureFaultCause::DISABLED, "htm_disabled" },
        { HtmFailureFaultCause::MEMORY, "memory_conflict" },
        { HtmFailureFaultCause::LSQ, "lsq_conflict" },
        { HtmFailureFaultCause::SIZE_RSET, "transaction_size_rset" },
        { HtmFailureFaultCause::SIZE_WSET, "transaction_size_wset" },
        { HtmFailureFaultCause::SIZE_LLC, "transaction_size_llc" },
        { HtmFailureFaultCause::SIZE_L1PRIV, "transaction_size_l1priv" },
        { HtmFailureFaultCause::SIZE_WRONG_CACHE, "transaction_size_wrongcache" },
        { HtmFailureFaultCause::EXPLICIT_FALLBACKLOCK, "explicit_fallbacklock" },
        { HtmFailureFaultCause::MEMORY_FALLBACKLOCK,
          "memory_conflict_fallbacklock" },
        { HtmFailureFaultCause::MEMORY_STALEDATA,
          "memory_conflict_staledata" },
        { HtmFailureFaultCause::MEMORY_FALSESHARING,
          "memory_conflict_falsesharing" },
        { HtmFailureFaultCause::SPEC_VALIDATION,
          "spec_validation_failure" },
        { HtmFailureFaultCause::SPEC_DEPTH, "spec_depth_failure" },
        { HtmFailureFaultCause::SPEC_MAX_CAPACITY, "spec_max_capacity" },
        { HtmFailureFaultCause::SPEC_WRITE_IN_FLIGHT, "spec_write_in_flight" },
        { HtmFailureFaultCause::SPEC_READ_CONFLICT, "spec_read_conflict" },
        { HtmFailureFaultCause::SPEC_ILLEGAL, "spec_illegal" },
        { HtmFailureFaultCause::MEMORY_POWER, "memory_conflict_power" },
        { HtmFailureFaultCause::OTHER, "other" }
    };

    auto it = cause_to_str.find(cause);
    return it == cause_to_str.end() ? "Unrecognized Failure" : it->second;
}

std::string
htmFailureToStr(HtmCacheFailure rc)
{
    static const std::map<HtmCacheFailure, std::string> rc_to_str = {
        { HtmCacheFailure::NO_FAIL, "NO_FAIL" },
        { HtmCacheFailure::FAIL_SELF, "FAIL_SELF" },
        { HtmCacheFailure::FAIL_REMOTE, "FAIL_REMOTE" },
        { HtmCacheFailure::FAIL_SPEC, "FAIL_SPEC" },
        { HtmCacheFailure::FAIL_OTHER, "FAIL_OTHER" }
    };

    auto it = rc_to_str.find(rc);
    return it == rc_to_str.end() ? "Unrecognized Failure" : it->second;
}


HTM::HTM(const Params &p)
    : ClockedObject(p),
      _params(p),
      m_fallbackLockPhysicalAddress(0),
      m_fallbackLockVirtualAddress(0)
{
    if (p.fallbacklock_addr != "") {
        try {
            m_fallbackLockVirtualAddress =
                std::stol(p.fallbacklock_addr, NULL, 0);
        }
        catch (const std::invalid_argument& ia) {
            panic("Illegal fallback lock address %s\n",
                  p.fallbacklock_addr);
            m_fallbackLockVirtualAddress = 0;
        }
    } else {
        warn("Fallback lock address not specified!\n");
    }
}

void
HTM::requestCommitToken(int proc_no)
{
    for (int i = 0 ; i < m_commitTokenRequestList.size(); i++)
        assert(m_commitTokenRequestList[i] != proc_no);

    m_commitTokenRequestList.push_back(proc_no);
}

void HTM::releaseCommitToken(int proc_no)
{
    assert(m_commitTokenRequestList.size() > 0);
    assert(proc_no == m_commitTokenRequestList[0]);
    m_commitTokenRequestList.erase(m_commitTokenRequestList.begin());
}

void HTM::removeCommitTokenRequest(int proc_no)
{
  bool found = false;

  if (getTokenOwner() == proc_no){
    releaseCommitToken(proc_no);
    return;
  }

  for (std::vector<int>::iterator it = m_commitTokenRequestList.begin() ;
       it != m_commitTokenRequestList.end() && !found; ++it) {
    if (*it == proc_no){
      m_commitTokenRequestList.erase(it);
      found = true;
    }
  }
  assert(found);
}

bool HTM::existCommitTokenRequest(int proc_no)
{
  bool found = false;
  int  curr_size = m_commitTokenRequestList.size();
  for (int i = 0; (i < curr_size) && !found; i++){
    if (m_commitTokenRequestList[i] == proc_no){
      found = true;
    }
  }
  return found;
}

int HTM::getTokenOwner()
{
  int owner = -1;
  if (m_commitTokenRequestList.size() > 0)
   owner = m_commitTokenRequestList[0];

  return owner;
}

int HTM::getNumTokenRequests()
{
 return m_commitTokenRequestList.size();
}

HTM::ResolutionPolicy
HTM::getResolutionPolicy() {
    std::string policy = _params.conflict_resolution;
    if (policy.rfind("requester_wins", 0) == 0) {
        return ResolutionPolicy::RequesterWins;
    } else if (policy.rfind("requester_loses", 0) == 0) {
        return ResolutionPolicy::RequesterLoses;
    } else if (policy.rfind("requester_stalls", 0) == 0) {
        return ResolutionPolicy::RequesterStalls;
    } else {
        return ResolutionPolicy::Undefined;
    }
}

HTM::HtmHwPrioMechanism
HTM::getHwPrioMechanism() {
    std::string mechanism = _params.prio_mechanism;
    if (mechanism == "no_prio") {
        return HtmHwPrioMechanism::no_prio;
    } else if (mechanism == "numeric") {
        return HtmHwPrioMechanism::numeric;
    }
    return HtmHwPrioMechanism::no_prio;
}

HTM::HtmFwdMechanism
HTM::getFwdMechanism() {
    std::string mechanism = _params.fwd_mechanism;
    if (mechanism == "no_fwd") {
        return HtmFwdMechanism::no_fwd;
    } else if (mechanism == "simple") {
        return HtmFwdMechanism::simple;
    } else if (mechanism == "naive") {
        return HtmFwdMechanism::naive;
    } else if (mechanism == "W1_D1") {
        return HtmFwdMechanism::W1_D1;
    } else if (mechanism == "WN_D1") {
        return HtmFwdMechanism::WN_D1;
    } else if (mechanism == "W1_DN") {
        return HtmFwdMechanism::W1_DN;
    } else if (mechanism == "WN_DM") {
        return HtmFwdMechanism::WN_DM;
    } else if (mechanism == "WX_DX") {
        return HtmFwdMechanism::WX_DX;
    }
    return HtmFwdMechanism::no_fwd;
}

HTM::HtmFwdCycleSolveStrategy
HTM::getFwdCycleSolveStrategy() {
    std::string strategy = _params.fwd_cycle_solve_strategy;
    if (strategy == "no_strategy") {
        return HtmFwdCycleSolveStrategy::no_strategy;
    } else if (strategy == "abort") {
        return HtmFwdCycleSolveStrategy::abort;
    } else if (strategy == "retry") {
        return HtmFwdCycleSolveStrategy::retry;
    }
    return HtmFwdCycleSolveStrategy::no_strategy;
}

HTM::HtmAllowedDataFwd
HTM::getAllowedDataFwd() {
    std::string strategy = _params.allowed_data_fwd;
    if (strategy == "all_data") {
        return HtmAllowedDataFwd::all_data;
    } else if (strategy == "only_written") {
        return HtmAllowedDataFwd::only_written;
    } else if (strategy == "no_write_in_flight") {
        return HtmAllowedDataFwd::no_write_in_flight;
    }
    return HtmAllowedDataFwd::all_data;
}

// HTM::HtmNumericPrioAcq
// HTM::getSwNumAcq() {
//     // Must be sw prio system
//     assert(_params.conflict_resolution.find("power", 0) !=
//         _params.conflict_resolution.npos);
//     // Only valid with numeric prio mechanism
//     assert(getHwPrioMechanism() == HtmHwPrioMechanism::numeric);
//     std::string acq = _params.sw_num_acq;
//     if (acq == "positive") {
//         return HtmNumericPrioAcq::positive;
//     } else if (acq == "negative") {
//         return HtmNumericPrioAcq::negative;
//     } else {
//         panic("Undefined numeric acquisition specified for sw priorities");
//     }
// }

// HTM::HtmNumericPrioAcq
// HTM::getHwNumAcq() {
//     // Must be sw prio system
//     assert(_params.conflict_resolution.find("requester_loses", 0) !=
//         _params.conflict_resolution.npos);
//     // Only valid with numeric prio mechanism
//     assert(getHwPrioMechanism() == HtmHwPrioMechanism::numeric);
//     std::string acq = _params.hw_num_acq;
//     if (acq == "positive") {
//         return HtmNumericPrioAcq::positive;
//     } else if (acq == "negative") {
//         return HtmNumericPrioAcq::negative;
//     } else {
//         panic("Undefined numeric acquisition specified for sw priorities");
//     }
// }

} // namespace gem5
