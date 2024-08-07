#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H 1

#include "defs.h"

typedef struct {
    // Careful: compiler will padd struct so that 8-byte types
    // (pointers, longs) sit at 8-byte aligned offsets. So we moved it
    // to offset 0,8
    // Non-speculative executions of critical sections
    void* logtm_transactionLog; // LogTM transaction log
    long nonSpecExecutions;
    long workUnits;
    int numThreads;
    int threadId;
    int inSimulator; // Local copy in same block as inFF
} context_info_t;

typedef struct {
    unsigned long z, w, jsr, jcong;
} rand_t;

typedef struct {
    context_info_t info;
    rand_t rand;
    char padding[2*CACHE_LINE_SIZE_BYTES - sizeof(context_info_t)  - sizeof(rand_t)];
} _tm_thread_context_t;

_tm_thread_context_t * initThreadContexts(int numThreads, int inSimulator);
void printThreadContexts(_tm_thread_context_t *thread_contexts);

#endif
