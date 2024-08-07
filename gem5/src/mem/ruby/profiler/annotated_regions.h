#ifndef ANNOTATED_REGIONS_H
#define ANNOTATED_REGIONS_H

#include <stdbool.h>
#include <assert.h>

typedef enum AnnotatedRegion {
    AnnotatedRegion_FIRST = 0, // Default region
    AnnotatedRegion_DEFAULT = AnnotatedRegion_FIRST, // Default region
    AnnotatedRegion_BARRIER,
    AnnotatedRegion_BACKOFF,
    AnnotatedRegion_TRANSACTIONAL, // For visualization only, stats split into aborted vs committed
    AnnotatedRegion_TRANSACTIONAL_COMMITTED,
    AnnotatedRegion_TRANSACTIONAL_ABORTED,
    AnnotatedRegion_TRANSACTIONAL_POWER,
    AnnotatedRegion_TRANSACTIONAL_SPECULATING,
    AnnotatedRegion_COMMITTING,
    AnnotatedRegion_ABORTING,
    AnnotatedRegion_ABORT_HANDLER_HASLOCK,
    AnnotatedRegion_ABORT_HANDLER,
    AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_THRESHOLD,
    AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_EXCEPTION,
    AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_SIZE,
    AnnotatedRegion_KERNEL,
    AnnotatedRegion_KERNEL_HASLOCK,
    AnnotatedRegion_STALLED, // For visualization only
    AnnotatedRegion_STALLED_COMMITTED,
    AnnotatedRegion_STALLED_ABORTED,
    AnnotatedRegion_STALLED_NONTRANS,
    AnnotatedRegion_ARBITRATION, // For visualization only
    AnnotatedRegion_ARBITRATION_COMMITTED,
    AnnotatedRegion_ARBITRATION_ABORTED,
    AnnotatedRegion_INVALID,
    AnnotatedRegion_NUM
} AnnotatedRegion_t;

static inline
bool
AnnotatedRegion_isValidRegion(uint64_t val) {
    return ((val >= AnnotatedRegion_FIRST) &&
            (val < AnnotatedRegion_INVALID));
}

static inline
bool
AnnotatedRegion_hasDualOutcome(AnnotatedRegion_t region) {
    switch (region) {
    case AnnotatedRegion_TRANSACTIONAL:
    case AnnotatedRegion_STALLED:
    case AnnotatedRegion_ARBITRATION:
        return true;
    default: 
        return false;
    }
}

static inline
AnnotatedRegion_t
AnnotatedRegion_getOutcome(AnnotatedRegion_t region, 
                           bool commit,
                           bool hasCycles) {
    switch (region) {
    case AnnotatedRegion_TRANSACTIONAL:
        assert(AnnotatedRegion_hasDualOutcome(region));
        return commit ? AnnotatedRegion_TRANSACTIONAL_COMMITTED :
            AnnotatedRegion_TRANSACTIONAL_ABORTED;
    case AnnotatedRegion_STALLED:
        assert(AnnotatedRegion_hasDualOutcome(region));
        return commit ? AnnotatedRegion_STALLED_COMMITTED :
            AnnotatedRegion_STALLED_ABORTED;
    case AnnotatedRegion_ARBITRATION:
        assert(AnnotatedRegion_hasDualOutcome(region));
        return commit ? AnnotatedRegion_ARBITRATION_COMMITTED :
            AnnotatedRegion_ARBITRATION_ABORTED;
    case AnnotatedRegion_COMMITTING:
    case AnnotatedRegion_ABORTING:
        assert(!AnnotatedRegion_hasDualOutcome(region));
        return region;
    case AnnotatedRegion_ABORT_HANDLER_HASLOCK:
    case AnnotatedRegion_ABORT_HANDLER:
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_THRESHOLD:
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_EXCEPTION:
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_SIZE:
        assert(!hasCycles);
    default:
        assert(!AnnotatedRegion_hasDualOutcome(region));
        return region;
    }
}

static inline
bool
AnnotatedRegion_isHardwareTransaction(AnnotatedRegion_t region)
{
    switch (region) {
    case AnnotatedRegion_TRANSACTIONAL:
    case AnnotatedRegion_TRANSACTIONAL_POWER:
    case AnnotatedRegion_TRANSACTIONAL_SPECULATING:
    case AnnotatedRegion_COMMITTING:
    case AnnotatedRegion_ABORTING:
        return true;
    default:
        return false;
    }
}

static inline
bool
AnnotatedRegion_isAbortHandlerRegion(AnnotatedRegion_t region)
{
    switch (region) {
    case AnnotatedRegion_ABORT_HANDLER:
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_THRESHOLD:
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_EXCEPTION:
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_SIZE:
        return true;
    default:
        return false;
    }
}

static inline const char* AnnotatedRegion_to_string(AnnotatedRegion_t region)
{
    switch(region) {
    case AnnotatedRegion_DEFAULT: return "DEFAULT";
    case AnnotatedRegion_BARRIER: return "BARRIER";
    case AnnotatedRegion_BACKOFF: return "BACKOFF";
    case AnnotatedRegion_TRANSACTIONAL: return "TRANSACTIONAL";
    case AnnotatedRegion_TRANSACTIONAL_COMMITTED: return "TRANSACTIONAL_COMMITTED";
    case AnnotatedRegion_TRANSACTIONAL_ABORTED: return "TRANSACTIONAL_ABORTED";
    case AnnotatedRegion_TRANSACTIONAL_POWER: return "TRANSACTIONAL_POWER";
    case AnnotatedRegion_TRANSACTIONAL_SPECULATING: return "TRANSACTIONAL_SPECULATING";
    case AnnotatedRegion_COMMITTING: return "COMMITTING";
    case AnnotatedRegion_ABORTING: return "ABORTING";
    case AnnotatedRegion_ABORT_HANDLER_HASLOCK: return "HASLOCK";
    case AnnotatedRegion_ABORT_HANDLER: return "ABORT_HANDLER";
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_THRESHOLD:
        return "WAITFORRETRY_THRESHOLD";
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_EXCEPTION:
        return "WAITFORRETRY_EXCEPTION";
    case AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_SIZE:
        return "WAITFORRETRY_SIZE";
    case AnnotatedRegion_KERNEL: return "KERNEL";
    case AnnotatedRegion_KERNEL_HASLOCK: return "KERNEL_HASLOCK";
    case AnnotatedRegion_STALLED: return "STALLED";
    case AnnotatedRegion_STALLED_COMMITTED: return "STALLED_COMMITTED";
    case AnnotatedRegion_STALLED_ABORTED: return "STALLED_ABORTED";
    case AnnotatedRegion_STALLED_NONTRANS: return "STALLED_NONTRANS";
    case AnnotatedRegion_ARBITRATION: return "ARBITRATION";
    case AnnotatedRegion_ARBITRATION_COMMITTED: return "ARBITRATION_COMMITTED";
    case AnnotatedRegion_ARBITRATION_ABORTED: return "ARBITRATION_ABORTED";
    case AnnotatedRegion_INVALID: return "INVALID";
    default: assert(0);
    }
    return NULL;
}
#endif
