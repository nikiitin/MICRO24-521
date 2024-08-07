#ifndef ENV_GLOBALS_H
#define ENV_GLOBALS_H 1

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L // definition of pthread_barrier_t (-std=c11)
#endif

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "thread_context.h"

#define DEFAULT_HTM_MAX_RETRIES 8

#define ENV_VAR_IN_SIMULATOR "M5_SIMULATOR"
#define ENV_VAR_HTM_MAX_RETRIES "HTM_MAX_RETRIES"
#define ENV_VAR_HTM_HEAP_PREFAULT "HTM_HEAP_PREFAULT"
#define ENV_VAR_HTM_BACKOFF "HTM_BACKOFF"

/* For performance reasons (avoid additional cache misses), keep in
   the same cache line all those read-only variables that act as
   "knobs" to switch on/off certain behaviours (passed via
   environment) when running a benchmark, either natively and in
   simulator. Some of them are heavily accessed (e.g. TM begin/end),
 */
typedef struct {
    bool inSimulator; /* M5 pseudo instructions disabled by default */
    bool heapPrefault;
    uint8_t numCPUs;
    uint8_t htm_max_retries;
    bool backoff;
} _env_config_t;

typedef struct {
    _env_config_t config;
    char padding[CACHE_LINE_SIZE_BYTES - sizeof(_env_config_t)];
} _env_globals_t;

typedef struct {
    /* Barrier for synchronizing threads at ROI begin/end */
    pthread_barrier_t *barrier;
} _shared_globals_t;

extern _env_globals_t env  __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;

extern _shared_globals_t sh_globals
__attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;

static inline int inSimulator() {
    return (int)env.config.inSimulator;
}

static inline int getNumCPUs() {
    return (int)env.config.numCPUs;
}

static inline int useHeapPrefault() {
    return (int)env.config.heapPrefault;
}

static inline int useBackoff() {
    return env.config.backoff;
}


#endif
