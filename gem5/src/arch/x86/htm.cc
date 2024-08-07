/*
    Copyright (C) 2021 Rubén Titos <rtitos@um.es>
    Universidad de Murcia
    GPLv2, see file LICENSE.
*/


#include "arch/x86/htm.hh"

#include "cpu/thread_context.hh"

namespace gem5
{

void
X86ISA::HTMCheckpoint::reset()
{
    nPc = 0;
    abortReason = 0;
    shadowIntRegs.fill(0);
    shadowFPRegs.fill(0);
    pcstateckpt = PCState();

    BaseHTMCheckpoint::reset();
}

void
X86ISA::HTMCheckpoint::save(ThreadContext *tc)
{
    for (auto n = 0; n < NumIntRegs; n++) {
        shadowIntRegs[n] = tc->readIntRegFlat(n);
    }
    for (auto n = 0; n < NumFloatRegs; n++) {
        shadowFPRegs[n] = tc->readFloatRegFlat(n);
    }
    pcstateckpt = tc->pcState();
    BaseHTMCheckpoint::save(tc);
}

void
X86ISA::HTMCheckpoint::restore(ThreadContext *tc, HtmFailureFaultCause cause)
{
    for (auto n = 0; n < NumIntRegs; n++) {
        tc->setIntRegFlat(n, shadowIntRegs[n]);
    }
    for (auto n = 0; n < NumFloatRegs; n++) {
        tc->setFloatRegFlat(n, shadowFPRegs[n]);
    }

    bool retry = false;
    uint64_t error_code = 0;
    switch (cause) {
      case HtmFailureFaultCause::EXPLICIT:
        replaceBits(error_code, 31, 24, abortReason);
        replaceBits(error_code, 0, 1);
        break;
      case HtmFailureFaultCause::MEMORY:
        replaceBits(error_code, 2, 1);
        retry = true;
        if (tc->isHtmFailureFaultCauseMemoryPower()) {
            // Power transactions: abort caused by power transaction
            // See gem5_path/benchmarks/benchmarks-htm/libs/isa/x86/abort_status.h
            replaceBits(error_code, 8, 1);
        }
        break;
      case HtmFailureFaultCause::SIZE:
        // Set if an internal buffer overflowed
        replaceBits(error_code, 3, 1);
        break;
      case HtmFailureFaultCause::EXCEPTION:
        break;
      case HtmFailureFaultCause::OTHER:
        retry = true;
        break;
        /*
      case HtmFailureFaultCause::DEBUG:
        replaceBits(error_code, 4, 1);
        break;
      case HtmFailureFaultCause::NEST:
        replaceBits(error_code, 5, 1);
        break;
        */
      case HtmFailureFaultCause::DISABLED:
        replaceBits(error_code, 6, 1);
        assert(tc->forceHtmDisabled());
        if (tc->forceHtmRetryStatusBit()) {
            // Lockstep support: Abort handler must spin on the
            // htm_start instruction when bit 6 in the abort status is
            // set, until the retry bit is unset (then acquire lock)
            retry = true;
        }
        break;
      default:
        panic("Unknown HTM failure reason\n");
    }
    if (retry)
        replaceBits(error_code, 1, 1);
    if (tc->getHtmUndoLogSize() > 0) {
        replaceBits(error_code, 7, 1);
        uint64_t logsize = tc->getHtmUndoLogSize();
        replaceBits(error_code, 63, 32, logsize);
    }
    tc->setIntReg(INTREG_EAX, error_code);
    // set next PC
    pcstateckpt.uReset();
    pcstateckpt.advance();
    tc->pcState(pcstateckpt);

    BaseHTMCheckpoint::restore(tc, cause);
}

void
X86ISA::HTMCheckpoint::setAbortReason(uint16_t reason)
{
    abortReason = reason;
}

} // namespace gem5
