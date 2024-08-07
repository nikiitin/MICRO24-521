#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L // definition of pthread_barrier_t (-std=c11)
#endif

#include "env_globals.h"

_env_globals_t env  __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;

_shared_globals_t sh_globals
__attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;