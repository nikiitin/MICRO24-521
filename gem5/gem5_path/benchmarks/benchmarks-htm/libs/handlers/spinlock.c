#include "spinlock.h"

unsigned int numLock = 0;
#if defined (AARCH64)
spinlock_t fallbackLock;
volatile char lock_array[(NUM_GLOBAL_LOCKS-2)*CACHE_LINE_SIZE_BYTES]
  __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;
#elif defined (X86)
volatile char lock_array[PADDED_ARRAY_SIZE_BYTES]
  __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;
lockPtr_t locks;
#endif
