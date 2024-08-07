#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "thread_context.h"
#include "logtm.h"

extern int inSimulator();

_tm_thread_context_t * initThreadContexts(int numThreads, int inSimulator)
{
    void *ptr;
    int ret = posix_memalign (&ptr,
                              CACHE_LINE_SIZE_BYTES,
                              numThreads * sizeof(_tm_thread_context_t));
    assert(ret == 0);
    _tm_thread_context_t *thread_contexts = (_tm_thread_context_t *)ptr;
    assert(sizeof(_tm_thread_context_t) == 2*CACHE_LINE_SIZE_BYTES);
    int i;
    for (i = 0; i < numThreads; i++)  {
        thread_contexts[i].info.threadId = i;
        thread_contexts[i].info.numThreads = numThreads;
        thread_contexts[i].info.inSimulator = inSimulator;
        thread_contexts[i].info.nonSpecExecutions = 0;
        thread_contexts[i].info.workUnits = 0;
    }
    init_log(thread_contexts);
    init_random_gen(thread_contexts);
    return thread_contexts;
}

void printThreadContexts(_tm_thread_context_t *thread_contexts)
{
    /* Print cummulative stats collected */
    int i;
    _tm_thread_context_t totals;
    memset(&totals, 0, sizeof(totals));
    for (i = 0; i < thread_contexts->info.numThreads; i++) {
        _tm_thread_context_t *ctx = &thread_contexts[i];
        totals.info.nonSpecExecutions += ctx->info.nonSpecExecutions;
        fprintf(stderr, "Work unit count [tid:%02d]: %lu\n",
                i, ctx->info.workUnits);
    }
    fprintf(stderr, "Total non-speculative executions: %lu\n",
            totals.info.nonSpecExecutions);
}
