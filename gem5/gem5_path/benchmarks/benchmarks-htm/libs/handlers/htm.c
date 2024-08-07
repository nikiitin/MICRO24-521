#include <assert.h>

#include "abort_codes.h"
#include "htm.h"

#if defined (AARCH64)

#if 0 // TODO: Remove, no longer needed, see htm.h

#include <arm_acle.h>

uint64_t htm_start(uint64_t arg) {
    return __tstart();
}

void htm_commit(uint64_t arg) {
    __tcommit();
}

void htm_cancel(uint64_t code) {
    // __tcancel expects a 16-bit immediate, so need this hack: Make
    // sure the abort codes used by benchmarks are reflected in this
    // switch Currently, only code 0 allowed for application-triggered
    // aborts (STAMP benchmarks do not pass any abort code, so we
    // invariably pass a zero value)
    switch(code) {
    case 0: __tcancel(CANCEL_TRANSACTION_DEFAULT_CODE);
    default:
    __builtin_unreachable();
    }
}

void htm_cancel_lock_acquired() {
    __tcancel(TME_CODE_FALLBACK_LOCK_LOCKED);
    __builtin_unreachable();
}
#endif

#elif defined (X86)



#endif
