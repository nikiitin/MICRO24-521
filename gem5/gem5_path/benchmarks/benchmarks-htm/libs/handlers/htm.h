#include <stdbool.h>
#include <stdint.h>

#if defined AARCH64

#include "../isa/aarch64/abort_status.h"

#define htm_start(arg) ({                                      \
            uint64_t ret;                                       \
            __asm__ volatile ("tstart %0 \n\t"                     \
                              : "=r"(ret)                       \
                              :                                 \
                              : );                         \
            ret;                                                \
        })

#define htm_commit(arg) ({                                \
            __asm__ volatile ("tcommit\n\t"                \
                              :                            \
                              :                            \
                              : );                         \
        })

// TODO: pass as imm CANCEL_TRANSACTION_DEFAULT_CODE
#define htm_cancel(code) ({                                \
    assert(code == CANCEL_TRANSACTION_DEFAULT_CODE);       \
            __asm__ volatile ("tcancel  #254\n\t"          \
                              :                            \
                              :                            \
                              : );                         \
        })

// TODO: pass as imm TME_CODE_FALLBACK_LOCK_LOCKED
#define htm_cancel_lock_acquired() ({                      \
            __asm__ volatile ("tcancel  #32767\n\t"        \
                              :                            \
                              :                            \
                              : );                         \
        })

#define htm_started(status) (status == 0)
#define htm_abort_undo_log(status) (status & _TMFAILURE_UNDO_LOG)
#define htm_abort_cause_conflict(status) (status & _TMFAILURE_MEM)
#define htm_abort_cause_explicit(status) (status & _TMFAILURE_CNCL)
#define htm_abort_cause_explicit_code(status) ( TME_FAILURE_REASON_DECODE(status))
#define htm_abort_code_is_lock_acquired(abort_code) ( abort_code == TME_CODE_FALLBACK_LOCK_LOCKED)
#define htm_abort_code_is_default(abort_code) (abort_code == CANCEL_TRANSACTION_DEFAULT_CODE)
#define htm_may_succeed_on_retry(status) ( status & _TMFAILURE_RTRY)
#define htm_abort_cause_disabled(status) (status & _TMFAILURE_DISABLED)

#elif defined X86

#include "../isa/x86/abort_status.h"

/* IMPORTANT NOTE: Eager HTM systems (log-based) abort in two steps:
 *  first, the register checkpoint is restored; then, the log is
 *  unrolled to restore memory locations. Because the current stack
 *  frame is garbage after the checkpoint is restored on abort (likely
 *  overwritten by the call stack of the transaction that just
 *  aborted), the code following xbegin must NOT make use of the
 *  stack: Bear in mind that as part of the log unroll, this stack
 *  frame will be restored to its state when xbegin above executed, so
 *  we can't "call" any method after xbegin, or else the return
 *  address will be lost when the log restores the current stack
 *  frame. Hence logtm_log_unroll must be declared "static inline".
 */
/* Apart from performance reasons, using inline assembly for various
 * checks on the returned abort status code is essential for
 * functional correctness of the eager versioning implementation.
 */
#define htm_start(tag, flags) ({                                \
            uint64_t ret;                                       \
            __asm__ volatile ("mov %1, %%rdi\n\t"               \
                              "shl $16, %%rdi\n\t"               \
                              "or %2, %%rdi\n\t"                 \
                              "mov $0xffffffff,%%eax\n\t"       \
                              "xbegin   .+6 \n\t"               \
                              "mov %%rax, %0\n\t"               \
                              : "=r"(ret)                       \
                              : "r"(tag), "r"(flags)            \
                              : "%rdi", "rax");          \
            ret;                                                \
        })
    // NOTE: Abort handler offset fixed to 0 (invariably begins at the
    // next instruction after xbegin). This doesn't need to be the
    // case (xbegin takes a rel32 offset as immediate operand).
    // NOTE 2: Do not use intrinsics, as we want to return a 64-bit
    // value (RAX) rather than 32 bits (EAX) in order to return
    // virtual addresses

#define htm_started(status) (status == _XBEGIN_STARTED)

#define htm_abort_undo_log(status) (status & _XABORT_UNDO_LOG)

#define htm_commit(arg) ({                              \
            uint64_t ret;                                       \
            __asm__ volatile ("mov %0,%%rdi\n\t"        \
                              "xend\n\t"                \
                              "mov %%rax, %0\n\t"               \
                              : "=r"(ret)                       \
                              : "r"(arg)                        \
                              : "%rdi", "rax");                 \
            ret;                                                \
        })

#define htm_cancel(code) ({                             \
            __asm__ volatile ("mov %0,%%rdi\n\t"        \
                              "xabort $0x0\n\t"         \
                              :                         \
                              : "r"((uint64_t)code)    \
                              : "%rdi");                \
        })
    // TODO: Pass abort code to simulator via imm instead of RDI.

#define htm_cancel_lock_acquired() ({                   \
            __asm__ volatile ("mov $0xff,%%rdi\n\t"        \
                              "xabort $0x0\n\t"         \
                              :                         \
                              :                         \
                              : "%rdi");                \
        })
    // TODO: Pass abort code to simulator via imm instead of RDI.
    // NOTE: TODO: use XABORT_CODE_FALLBACK_LOCK_LOCKED immediate ("I")

#define htm_abort_cause_conflict(status) (status & _XABORT_CONFLICT)
#define htm_abort_cause_conflict_power(status) (status & _XABORT_CONFLICT_POWER)
#define htm_abort_cause_explicit(status) (status & _XABORT_EXPLICIT)
#define htm_abort_cause_explicit_code(status) (_XABORT_CODE_DECODE(status))
#define htm_abort_code_is_lock_acquired(abort_code) (abort_code == XABORT_CODE_FALLBACK_LOCK_LOCKED)
#define htm_abort_code_is_default(abort_code) (abort_code == CANCEL_TRANSACTION_DEFAULT_CODE)
#define htm_may_succeed_on_retry(status) (status & _XABORT_RETRY)
#define htm_abort_cause_disabled(status) (status & _XABORT_DISABLED)

#endif
