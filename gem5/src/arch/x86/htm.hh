/*
    Copyright (C) 2021 Rubén Titos <rtitos@um.es>
    Universidad de Murcia
    GPLv2, see file LICENSE.
*/
#ifndef __ARCH_X86_HTM_HH__
#define __ARCH_X86_HTM_HH__

/**
 * @file
 *
 * ISA-specific types for hardware transactional memory.
 */

#include "arch/generic/htm.hh"
#include "arch/x86/regs/float.hh"
#include "arch/x86/regs/int.hh"
#include "base/types.hh"
#include "insts/static_inst.hh"

namespace gem5
{

namespace X86ISA
{

class HTMCheckpoint : public BaseHTMCheckpoint
{
  public:
    HTMCheckpoint()
      : BaseHTMCheckpoint()
    {}
    const static int MAX_HTM_DEPTH = 255;

    void reset() override;
    void save(ThreadContext *tc) override;
    void restore(ThreadContext *tc, HtmFailureFaultCause cause) override;
    void setAbortReason(uint16_t reason);

  private:
    Addr nPc; // Fallback instruction address
    std::array<RegVal, NumIntRegs> shadowIntRegs;
    std::array<RegVal, NumFloatRegs> shadowFPRegs;
    //Addr sp; // Stack Pointer at current EL
    uint16_t abortReason; // XABORT reason
    PCState pcstateckpt;

};

} // namespace X86ISA
} // namespace gem5

#endif
