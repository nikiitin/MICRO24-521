// Abort handlers
#define _GNU_SOURCE             /* See feature_test_macros(7) */

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "htmrt.h"
#include "abort_codes.h"
#include "env_globals.h"
#include "htmrt-arch.h"
#include "spinlock.h"
#include "util.h"
#include "annotated_regions.h"
#include "m5iface.h"
#include "logtm.h"
#include "mt19937ar_1.h"
#include "xbeginFlags.h"

//Initialization. Called from STAMP to initialize common variables
void initTransactionsGlobals(int nthreads)
{
    spinlock_init();

    setEnvGlobals(nthreads);
    if (useBackoff()) {
        unsigned long seed = 1;
        init_genrand_1(mt, &mti, seed);
    }

    simInit();
#define FALLBACKLOCKADDR_FILENAME ("fallback_lock")
    
#ifdef ANNOTATE_FALLBACKLOCK_ADDR
    if (simInSimulator()) {
        long fallBackLockAddr = (long)spinlock_getAddress();
        // Max fallback lock filename length is 63 characters
        char fallbackLockAddrFilename[CACHE_LINE_SIZE_BYTES]
            __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) =
            FALLBACKLOCKADDR_FILENAME;
        simDumpValueToHostFileSystem(fallBackLockAddr,
                                     fallbackLockAddrFilename);
    }
#endif

    /*Set up thread contexts */
    thread_contexts = initThreadContexts(nthreads, inSimulator());
}

// Call at the end of the workload, outside parallel section
void deleteTransactionsGlobals () {
    // Calling simSetLogBase with a NULL ptr when the log is ready is
    // used to shutdown the log in all CPUs to stop stop monitoring
    // the virtual addresses allocated to the log (sanity checks)
    if (inSimulator()) {
      if (simSetLogBase(NULL)) {
        // simulator returns 0 if v2p translation table already set
        // up. Otherwise, walk the log, simulator will intercept
        // accesses and fill log v2p translation table
        walk_log(thread_context_get()->info.logtm_transactionLog);        
      }
    }
}

// "Touch" function in STAMPs memory allocator
extern void memory_touch (long threadId, size_t numByte);
#define PREFAULT_TOUCH_BYTES (5000) /* Estimated number of bytes malloc'ed next */

void handleHeapPrefault(int threadId) {
    if (useHeapPrefault()) {
        if (threadId < 0) return;
      /* Force the memory allocator to touch the memory locations that
         will be returned by subsequent calls to TM_MALLOC/P_MALLOC
         (memory_get) before the transaction starts
      */
      //SimAnnotateRegionEntry(threadId, AnnotatedRegion_HEAP_TOUCH_PREFAULT);
        
      // TODO: reimplement this:
       // memory_touch (threadId, PREFAULT_TOUCH_BYTES);
        //assert(false);

      //SimAnnotateRegionExit(threadId, AnnotatedRegion_HEAP_TOUCH_PREFAULT);
  }
}

void doBackoff(int nretries, _tm_thread_context_t *ctx) {
    simBackoffBegin();
    randomized_backoff(nretries, ctx);
    simBackoffEnd();
}

#if defined(HANDLER_FALLBACKLOCK)

static inline
void beginTransaction_fallbackLock(long tag,
                                   _tm_thread_context_t *ctx) {
    u_int64_t ret, retryWithLock = 0;
    int nretries = 0;
    u_int64_t flags = 0x0;
#if defined(HANDLER_POWERTM)
    bool txExecOnPower = false;
    int nRetriesPowerMode = 0;
#endif
    assert(ctx == &thread_contexts[ctx->info.threadId]);
    handleHeapPrefault(ctx->info.threadId);
    if (simSetLogBase(ctx->info.logtm_transactionLog)) {
      // simulator returns 0 if v2p translation table already set
      // up. Otherwise, walk the log, simulator will intercept
      // accesses and fill log v2p translation table
      walk_log(ctx->info.logtm_transactionLog);        
    }
    do {
        ++nretries;
#if defined(HANDLER_POWERTM)
        txExecOnPower = false;
        if (*(locks.powerFlag) == ctx->info.threadId) {
            txExecOnPower = true;
            flags |= POWER_TM_FLAG;
        }
#else
        flags = 0x0;
#endif
        ret = htm_start(flags);

        if (htm_started(ret)) {
            if (!spinlock_isLocked()) return; /* Start transaction */
            else { /* started transaction but someone has grabbed lock */
                htm_cancel_lock_acquired();
            }
        }
        if (htm_abort_undo_log(ret)) {
            uint32_t log_size = M5_ABORTSTATUS_LOGSIZE_DECODE(ret);
            uint8_t *log_base = (uint8_t *)((_tm_thread_context_t *)ctx)->info.logtm_transactionLog;
            logtm_log_unroll(log_base, log_size);
            simEndLogUnroll(ctx->info.logtm_transactionLog);
        }
        if (htm_abort_cause_conflict(ret) &&
            spinlock_isLocked()) {
            /* Heuristic: If conflict-induced abort and lock held,
               likely this was a fallbacklock-induced abort, so we do
               not count it as a retry, to avoid the lemming effect
            */
            --nretries;
        }
        /* Wait until lock is free */
        spinlock_whileIsLocked();

        /* Grab lock on one these conditions:
         * a) Too many retries
         * b) Transaction cannot succeed on retry (e.g. page fault or
         * capacity abort)
         */
#if defined(HANDLER_POWERTM)
        if (txExecOnPower) {
            ++nRetriesPowerMode;
            if (htm_may_succeed_on_retry(ret)) {
                // Retry in power mode unless retry bit set, without
                // backoff. NOTE: avoid retrying indefinitely in case
                // we have repeated conflict-induced aborts with
                // non-transactional code
                if (nRetriesPowerMode > env.config.htm_max_retries) {
                    retryWithLock=1;
                } else {
                    continue;
                }
            }
        }
#endif
        bool explicit = htm_abort_cause_explicit(ret);
        if (explicit) {
            if (htm_abort_code_is_lock_acquired
                (htm_abort_cause_explicit_code(ret))) {
                // Explicit aborts because of fallback lock acquired
                --nretries; // Not counted towards max retries
            } else {
                assert(htm_abort_code_is_default
                       (htm_abort_cause_explicit_code(ret)));
            }
        }
        else if (htm_abort_cause_disabled(ret)) {
            // Support for lockstep debugging: replayer spins on
            // xbegin until recorder grants permission to begin the
            // next transaction
            if (htm_may_succeed_on_retry(ret)) {
                /* Simulator-only: Abort was due to HTM speculation
                   disabled. Keep retrying transaction (spinning) until
                   retry bit unset (lockstep replay support)
                */
                --nretries;
                assert((nretries < env.config.htm_max_retries) &&
                       (nretries >= 0));
            } else {
                // Lockstep replayer will acquire lock below and
                // execute the transaction non-speculatively
            }
#if defined(HANDLER_POWERTM_PRECISEABORTS)
        } else if (htm_abort_cause_conflict_power(ret)) {
            // Abort status code says this tx was killed by powered
            // transaction: Avoid lemming effect
            nretries--;
#elif defined(HANDLER_POWERTM)
        } else if (!txExecOnPower && *(locks.powerFlag) != -1) {
            // Probably killed by powered transaction
            // Avoid lemming effect and do not count as retry

            // NOTE: This is just an heuristic and may increase aborts
            // and degrade performance in high contention as
            // transactions may wrongly interpret "regular" aborts as
            // "power-induced" and thus take longer to become power
            nretries--;
#endif
        }
        if ((!explicit && // Ignore retry bit for explicit aborts
            !htm_may_succeed_on_retry(ret))
           ) {
            // Transaction may not succeed on retry
            retryWithLock=1;
        } else if ((nretries >= env.config.htm_max_retries) &&
                   !retryWithLock) {
#if defined(HANDLER_POWERTM)
            assert(!txExecOnPower);
            /* Go into power mode  */
            txExecOnPower = __sync_bool_compare_and_swap((locks.powerFlag), -1, ctx->info.threadId);
#else
            /* Grab the lock  */
            retryWithLock=1;   /* Execute non-speculatively */
#endif
        }
#if defined(HANDLER_FALLBACKLOCK_2PHASE)
        /* Wait until nobody is trying to acquire the fallback lock: do
           not retry if another thread will shortly acquire the fallback
           lock. This avoids repeated fallback-lock-induced aborts that
           may trigger the lemming effect */
        while (spinlock_prefb_isLocked())_mm_pause();
#endif
        if (!retryWithLock && useBackoff()
#if defined(HANDLER_POWERTM)
            && !txExecOnPower // In case we just acquired the power
#endif
            ) {
            doBackoff(nretries, ctx);
        }
    } while (retryWithLock == 0);

#if defined(HANDLER_POWERTM)
    // Release power flag - Dice et. al
    if (*(locks.powerFlag) == ctx->info.threadId) {
        assert(txExecOnPower);
        assert(!htm_may_succeed_on_retry(ret) ||
               (nRetriesPowerMode > env.config.htm_max_retries));
         *(locks.powerFlag) = -1;
    }
#endif

#if defined(HANDLER_FALLBACKLOCK_2PHASE)
    /* Acquire "pre-fallback lock" to signal "retrying threads" that
       another thread is trying to acquire the lock (stop them from
       fruitlessly retrying and exhaust their max number of attempts)
    */
    spinlock_prefb_lock();
    spinlock_lock();
    /* Release "pre-fallback lock" after fallback lock has been
       acquired: if any "retrying thread" observes a free
       preFallbackLock before another "acquiring thread" locks it, it
       may retry speculation but when subscribing to the fallback lock
       it will necessarily see that is locked, and then explicitly abort
       (and it will not increment nretries -> no lemming effect)
    */
    spinlock_prefb_unlock();
#else
    spinlock_lock();
#endif
    simFallbackLockAcquired(tag);
    simCodeRegionBegin(AnnotatedRegion_ABORT_HANDLER_HASLOCK);
}


static inline
void commitTransaction_fallbackLock(long tag, _tm_thread_context_t *ctx)
{
    if (spinlock_isLocked()){
        /* unlock */
        spinlock_unlock();
        simFallbackLockReleased(tag);
        simCodeRegionEnd(AnnotatedRegion_ABORT_HANDLER_HASLOCK);
    }
    else {
        u_int64_t commitStatus = htm_commit(tag);
        /* TODO: Commit status not returned properly in RAX when using
           O3CPU.
         */
        //assert(commitStatus == 0); // TODO: Act based on commit status
#if defined(HANDLER_POWERTM)
        if (*(locks.powerFlag) == ctx->info.threadId) {
            *(locks.powerFlag) = -1;
        }
#endif
    }
}

#endif

// Interface to STAMP benchmarks (see lib/tm.h)
void beginTransaction(long tag, _tm_thread_context_t *ctx) {
#if defined(HANDLER_FALLBACKLOCK)
    beginTransaction_fallbackLock(tag, ctx);
#elif defined(HANDLER_EMPTY)
    // No synchronization
#elif defined(HANDLER_SGL)
    // Single global lock
    spinlock_lock();
#else
#error "Undefined abort handler. Must set CFLAGS Makefile"
#endif
}


void commitTransaction(long tag, _tm_thread_context_t *ctx) {
#if defined(HANDLER_FALLBACKLOCK)
    commitTransaction_fallbackLock(tag, ctx);
#elif defined(HANDLER_EMPTY)
    // No synchronization
#elif defined(HANDLER_SGL)
    // Single global lock
    spinlock_unlock();
#else
#error "Undefined abort handler. Must set CFLAGS Makefile"
#endif
}


void cancelTransactionWithAbortCode(long abort_code) {
    htm_cancel(abort_code);
}
void cancelTransaction() {
    htm_cancel(CANCEL_TRANSACTION_DEFAULT_CODE);
}

