/*
    Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
    Universidad de Murcia

    GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_XACTVISUALIZER_HH__
#define __MEM_RUBY_HTM_XACTVISUALIZER_HH__

#include <ostream>
#include <vector>

#include "mem/ruby/profiler/annotated_regions.h"
#include "mem/ruby/system/Sequencer.hh"

namespace gem5
{

namespace ruby
{

class XactProfiler;

class XactVisualizer {
public:
  XactVisualizer(XactProfiler* profiler, std::ostream* output);
  ~XactVisualizer();

  void printAnnotatedRegions(std::string& extraStr);

  XactProfiler *m_xact_profiler;
  Cycles lastPrintCycle;
  std::ostream*  m_xact_visualizer_output_file_ptr;
};

} // namespace ruby
} // namespace gem5

#endif


