/*
    Copyright (C) 2016-2021 Ruben Titos <rtitos@um.es>
    Universidad de Murcia

    GPLv2, see file LICENSE.
*/


#include "mem/ruby/profiler/XactProfiler.hh"

#include <bits/stdc++.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

#include "debug/RubyHTM.hh"
#include "mem/ruby/common/Global.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/protocol/MachineType.hh"

namespace gem5
{

namespace ruby
{

static const bool m_showPriority = true;

XactProfiler::XactProfiler(RubySystem *rs)

{
    m_ruby_system = rs;
    int num_sequencers = rs->params().num_of_sequencers;
    m_state_cycle_count.resize(num_sequencers);
    m_annotatedRegion.resize(num_sequencers);
    m_xactLastRegionChange.resize(num_sequencers);
    m_xactLastRegionProfile.resize(num_sequencers);
    m_inTransaction.resize(num_sequencers);
    m_inHardwareTransaction.resize(num_sequencers);
    m_currentXactCyclesPerRegion.resize(num_sequencers);
    m_currentWaitForRetryRegion = AnnotatedRegion_INVALID;
    for (int i = 0; i < num_sequencers; i++){
        m_state_cycle_count[i].resize(AnnotatedRegion_NUM);
        m_annotatedRegion[i]            = AnnotatedRegion_INVALID;
        m_xactLastRegionChange[i]  =  g_system_ptr->curCycle();
        m_xactLastRegionProfile[i]  =  g_system_ptr->curCycle();
        m_inTransaction[i] = false;
        m_inHardwareTransaction[i] = false;
        m_currentXactCyclesPerRegion[i].resize(AnnotatedRegion_NUM);
    }
    assert(rs->getHTM() != nullptr);
    if (rs->getHTM()->params().visualizer) {
        std::string fname = rs->getHTM()->params().visualizer_filename;
        if (fname != "") {
            std::ofstream* out = new std::ofstream(fname.c_str(),
                                                   std::ios_base::out |
                                                   std::ios_base::trunc);
            if (out->good()) {
                m_visualizer = new XactVisualizer(this, out);
            } else {
                panic("Cannot create file for HTM visualizer");
            }
        } else { // Default to stderr
            m_visualizer = new XactVisualizer(this, &std::cerr);
        }
    }
}

XactProfiler::~XactProfiler() {
}

void
XactProfiler::resetStats()
{

    assert(g_system_ptr->curCycle() == g_system_ptr->getStartCycle());
    for (int i = 0; i < m_ruby_system->params().num_of_sequencers; i++){
        m_xactLastRegionChange[i]  =  g_system_ptr->curCycle();
        m_xactLastRegionProfile[i]  =  g_system_ptr->curCycle();
        m_annotatedRegion[i]            = AnnotatedRegion_DEFAULT;
        m_inTransaction[i] = false;
        m_inHardwareTransaction[i] = false;
        for (unsigned int j = AnnotatedRegion_FIRST;
             j < AnnotatedRegion_NUM; ++j) {
            m_state_cycle_count[i][j]  = 0;
            m_currentXactCyclesPerRegion[i][j] = 0;
        }
    }
}

void XactProfiler::profile_region(AnnotatedRegion region, int proc_num,
                                  uint64_t cycles) {
  // Make sure we don't profile regions with dual outcome directly
  assert(!AnnotatedRegion_hasDualOutcome(region));
  m_state_cycle_count[proc_num][region] += cycles;
  m_ruby_system->getHTM()->profileRegion(region,cycles);
}

void XactProfiler::beginRegion(int proc_no, AnnotatedRegion region)
{
    switch (region) {
    case AnnotatedRegion_ABORT_HANDLER_HASLOCK:
        {
            int num_sequencers = m_ruby_system->params().num_of_sequencers;
            // Handle race condition in which another thread acquires
            // the lock before the preceding lock release notification
            bool remote_has_lock = false;
            for (int i=0; i < num_sequencers; i++) {
                if (m_annotatedRegion[i] ==
                    AnnotatedRegion_ABORT_HANDLER_HASLOCK) {
                    remote_has_lock = true;
                    break;
                }
            }
            if (remote_has_lock) {
                // Sanity checks? In theory the overlap in time should
                // be negligible
            } else {
                assert(m_currentWaitForRetryRegion == AnnotatedRegion_INVALID);
            }
            TransactionInterfaceManager* mgr =
                g_system_ptr->getTransactionInterfaceManager(proc_no);

            m_currentWaitForRetryRegion = mgr->
                getWaitForRetryRegionFromPreviousAbortCause();
            // "Fix" state for those threads that have already moved
            // to ABORT_HANDLER
            for (int i=0; i < num_sequencers; i++) {
                if (i != proc_no &&
                    m_annotatedRegion[i] == AnnotatedRegion_ABORT_HANDLER) {
                    moveTo(i, m_currentWaitForRetryRegion);
                }
            }
            DPRINTF(RubyHTM, "PROC %d acquired fallback lock\n",
                    proc_no);
        }
        break;
    case AnnotatedRegion_BARRIER:
        assert(m_annotatedRegion[proc_no] == AnnotatedRegion_DEFAULT);
        break;
    case AnnotatedRegion_BACKOFF:
        assert(AnnotatedRegion_isAbortHandlerRegion(
                  m_annotatedRegion[proc_no]));
        break;
    default:
        panic("Unexpected region");
    }
    moveTo(proc_no, region);
}
void XactProfiler::endRegion(int proc_no, AnnotatedRegion region)
{
    switch (region) {
    case AnnotatedRegion_ABORT_HANDLER_HASLOCK:
        // assert(region == m_annotatedRegion[proc_no]);
        m_currentWaitForRetryRegion = AnnotatedRegion_INVALID;
        DPRINTF(RubyHTM, "PROC %d released fallback lock\n",
                proc_no);
        break;
    case AnnotatedRegion_BARRIER:
        if (m_xactLastRegionChange[proc_no] ==
            g_system_ptr->getStartCycle()) {
        // Allow mismatch at initialization (after thread created)
            assert(m_annotatedRegion[proc_no] == AnnotatedRegion_DEFAULT);
        } else {
            assert(region == m_annotatedRegion[proc_no]);
        }
        break;
    case AnnotatedRegion_BACKOFF:
        assert(region == m_annotatedRegion[proc_no]);
        break;
    default:
        panic("Unexpected region");
    }
    moveTo(proc_no, AnnotatedRegion_DEFAULT);
}

void XactProfiler::moveTo(int proc_no, AnnotatedRegion newState){
  if (m_annotatedRegion[proc_no] != newState) {
      if (newState == AnnotatedRegion_ABORT_HANDLER &&
          m_currentWaitForRetryRegion != AnnotatedRegion_INVALID) {
          // Set precise wait for retry region
          newState = m_currentWaitForRetryRegion;
      }
    profileRegionChange(proc_no, newState);

    if (m_visualizer) {
        std::vector<int> vec;
        if (m_showPriority) {
            vec = g_system_ptr->getLowestTimestampTransactionManager();
        }
        std::ostringstream oss;
        oss << std::hex;
        // Convert all but the last element to avoid a trailing ","
        std::copy(vec.begin(), vec.end(),
                  std::ostream_iterator<int>(oss, " "));
        std::string extraStr = " ["+oss.str()+"]";
        m_visualizer->printAnnotatedRegions(extraStr);
    }
  }
  else {
    // Sanity checks: do not move to committing/aborting more than once
    assert(newState != AnnotatedRegion_ABORTING &&
           newState != AnnotatedRegion_COMMITTING);
  }
}

void
XactProfiler::profileCurrentAnnotatedRegion()
{
    int num_sequencers = m_ruby_system->params().num_of_sequencers;
    for (int i=0; i < num_sequencers; i++) {
        profileRegionChange(i, m_annotatedRegion[i]);
    }
    uint64_t curExecTime = g_system_ptr->curCycle() -
        g_system_ptr->getStartCycle();
    uint64_t totalRegionCycles = 0;
    _unused(curExecTime);
    for (int i=0; i < num_sequencers; i++) {
        for (unsigned int j = AnnotatedRegion_FIRST;
             j < AnnotatedRegion_NUM; ++j) {
            totalRegionCycles += m_state_cycle_count[i][j];
            totalRegionCycles += m_currentXactCyclesPerRegion[i][j];

        }
    }

    assert((curExecTime * num_sequencers) == totalRegionCycles);
}

void
XactProfiler::profileCurrentTransactionalRegion(int proc_no)
{
    AnnotatedRegion region = m_annotatedRegion[proc_no];

    if (region == AnnotatedRegion_COMMITTING ||
        region == AnnotatedRegion_ABORTING) {
        for (unsigned int i = AnnotatedRegion_FIRST;
             i < AnnotatedRegion_NUM; ++i) {
            uint64_t regionCycles = m_currentXactCyclesPerRegion[proc_no][i];
            bool committing = region == AnnotatedRegion_COMMITTING;
            AnnotatedRegion outcomeRegion =
                AnnotatedRegion_getOutcome((AnnotatedRegion)i,
                                           committing,
                                           regionCycles != 0);
            profile_region(outcomeRegion, proc_no, regionCycles);
            // Clear cycles for this region
            m_currentXactCyclesPerRegion[proc_no][i] = 0;
        }
    }
    else { // Should never go to to non-transactional from any region
           // other than committing/aborting
        panic("Going to non-transactional from unexpected region");
    }
}

void
XactProfiler::profileRegionChange(int proc_no,
                                  AnnotatedRegion nextRegion){
  assert(g_system_ptr->curCycle() >=  m_xactLastRegionProfile[proc_no]);
  uint64_t last_state_time = g_system_ptr->curCycle() -
    m_xactLastRegionProfile[proc_no];
  AnnotatedRegion region = m_annotatedRegion[proc_no];


  if (m_inHardwareTransaction[proc_no]) {
      // Transactional cycles are not immediately profiled, but
      // clasiffied once the transaction commits/aborts

      m_currentXactCyclesPerRegion[proc_no][region] += last_state_time;

      if ((region != nextRegion) &&
          ((nextRegion == AnnotatedRegion_DEFAULT) ||
           AnnotatedRegion_isAbortHandlerRegion(nextRegion))) {
          // Going to non-transactional: profile currentXactCycles
          // depending on outcome
          profileCurrentTransactionalRegion(proc_no);

          // No longer in a hardware transaction
          m_inHardwareTransaction[proc_no] = false;

          if (region == AnnotatedRegion_COMMITTING) {
              // No longer in a transaction (from the programmer's pov)
              m_inTransaction[proc_no] = false;
          }
      }
  }
  else { // Non-transactional cycles are profiled immediately

      profile_region(region, proc_no, last_state_time);

      if (region == AnnotatedRegion_ABORT_HANDLER_HASLOCK &&
          nextRegion == AnnotatedRegion_DEFAULT) {
          // Leaving software (non-speculative) transaction
        //   assert(m_inTransaction[proc_no]);
          m_inTransaction[proc_no] = false;
      }
      else if (AnnotatedRegion_isHardwareTransaction(nextRegion)) {
          assert((region == AnnotatedRegion_DEFAULT) ||
                 AnnotatedRegion_isAbortHandlerRegion(region));

          // Entering a hardware transaction
          m_inHardwareTransaction[proc_no] = true;
          // moveTo should be called after transaction level
          // increased in xact mgr
          assert(g_system_ptr->getTransactionInterfaceManager(proc_no)->
                 getTransactionLevel() > 0);

          if (!m_inTransaction[proc_no]) {
              // Fresh start of new trasaction (first attempt)
              m_inTransaction[proc_no] = true;
          }
          else {
              // Retrying hardware transaction after
          }
      }
  }

  // Update current region and cycle of last region change
  if (m_annotatedRegion[proc_no] != nextRegion) {
      m_xactLastRegionChange[proc_no] = g_system_ptr->curCycle();
  } else { // No change
      Cycles diff = g_system_ptr->curCycle() -
          Cycles(m_xactLastRegionChange[proc_no]);
      if (diff > 2000000) { // Two million cycles without changes??
          warn_once ("htm visualizer detected anomalous freeze in cpu %d: "
                     " Last state change was %ld cycles back (%ld)."
                     " Current tick is: %ld\n",
                     proc_no, diff,
                     g_system_ptr->
                     cyclesToTicks(Cycles(m_xactLastRegionChange[proc_no])),
                     g_system_ptr->cyclesToTicks(g_system_ptr->curCycle()));
      }
  }
  m_annotatedRegion[proc_no] = nextRegion;
  m_xactLastRegionProfile[proc_no] = g_system_ptr->curCycle();
}

} // namespace ruby
} // namespace gem5
