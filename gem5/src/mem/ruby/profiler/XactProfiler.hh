/*
  Copyright (C) 2016-2021 Ruben Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_PROFILER_XACTPROFILER_HH__
#define __MEM_RUBY_PROFILER_XACTPROFILER_HH__

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "mem/ruby/profiler/annotated_regions.h"
#include "mem/ruby/profiler/XactVisualizer.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "params/RubySystem.hh"

namespace gem5
{

namespace ruby
{

class XactProfiler
{
    friend class XactVisualizer;
public:
    // Constructors
    XactProfiler(RubySystem *rs);

    void print(std::ostream& out) const;
    void regStats(const std::string &name) {};
    void collateStats();
    void resetStats();
    
    void printProfilers() {};
    void moveTo(int proc_no, AnnotatedRegion newState);
    AnnotatedRegion getCurrentRegion(int proc_no) {
        return m_annotatedRegion[proc_no]; };
    void beginRegion(int proc_no, AnnotatedRegion region);
    void endRegion(int proc_no, AnnotatedRegion region);
    void profileCurrentAnnotatedRegion();
    void profileCurrentTransactionalRegion(int proc_no);

    // Destructor
    ~XactProfiler();
private:
    void profile_region(AnnotatedRegion state, int proc_num, uint64_t cycles);
    void profileRegionChange(int proc_no, AnnotatedRegion newState);

    std::vector< std::vector<uint64_t> > m_state_cycle_count;
    /* The following attributes are required to collect m_state_cycle
       count per cpu */
    std::vector<AnnotatedRegion>    m_annotatedRegion;
    std::vector<int>                m_annotatedTid;
    std::vector<uint64_t>           m_xactLastRegionProfile;
    std::vector<uint64_t>           m_xactLastRegionChange;
    std::vector< bool  >            m_inTransaction;
    std::vector< bool  >            m_inHardwareTransaction;
    std::vector< std::vector<uint64_t>  > m_currentXactCyclesPerRegion;
    AnnotatedRegion_t               m_currentWaitForRetryRegion;

    XactVisualizer* m_visualizer = NULL;
    RubySystem* m_ruby_system;
};

} // namespace ruby
} // namespace gem5

#endif


