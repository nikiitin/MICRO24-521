#include <stdbool.h>
#include <stdint.h>

#define CACHE_LINE_SIZE_BYTES 64
#if ! defined(PAGE_SIZE_BYTES)
#define PAGE_SIZE_BYTES 4096
#endif
