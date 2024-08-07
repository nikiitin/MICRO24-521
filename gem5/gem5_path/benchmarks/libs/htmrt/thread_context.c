#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#include "thread_context.h"
#include "logtm.h"

// global array of thread contexts
_tm_thread_context_t     *thread_contexts       = NULL;
static int num_thread_contexts = 0;

static _tm_thread_context_t *nextThreadContext = NULL;
__thread _tm_thread_context_t *thread_context = NULL;

_tm_thread_context_t * getUnusedThreadContext(void) {
  assert(nextThreadContext != NULL);
  assert(thread_contexts != NULL);
  const size_t offset = ((size_t) &nextThreadContext[1]) - ((size_t) &nextThreadContext[0]);
  _tm_thread_context_t * ret = (_tm_thread_context_t *) atomic_fetch_add((size_t*) &nextThreadContext, offset);
  int tid = ret - thread_contexts;
  assert(tid < num_thread_contexts);
  assert(tid == thread_contexts[tid].info.threadId);
  assert(tid == ret->info.threadId);
  assert(ret == &thread_contexts[ret->info.threadId]);
  return ret;
}

_tm_thread_context_t * initThreadContexts(int numThreads, int inSimulator)
{
    void *ptr = aligned_alloc(CACHE_LINE_SIZE_BYTES,
                              numThreads * sizeof(_tm_thread_context_t));       
    assert(ptr != NULL);
    _tm_thread_context_t *thread_contexts = (_tm_thread_context_t *)ptr;
    assert(sizeof(_tm_thread_context_t) == 2*CACHE_LINE_SIZE_BYTES);
    for (int i = 0; i < numThreads; i++)  {
        thread_contexts[i].info.threadId = i;
        thread_contexts[i].info.numThreads = numThreads;
        thread_contexts[i].info.inSimulator = inSimulator;
        thread_contexts[i].info.nonSpecExecutions = 0;
        thread_contexts[i].info.workUnits = 0;
    }
    init_log(thread_contexts);
    init_random_gen(thread_contexts);
    nextThreadContext = &thread_contexts[0];
    num_thread_contexts = numThreads;
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


