#ifndef LOGTM_H
#define LOGTM_H

#include <assert.h>
#include <stdint.h>

#define LOG_PAGE_SIZE_BYTES (4096L)
#define LOG_CACHE_BLOCK_SIZE 64
#define LOG_ENTRIES_PER_PAGE (56) //56*(64 + 8) = 4032

#define MAX_LOG_SIZE_PAGES (1 << 8) // ~16MB max log size
#define MAX_LOG_SIZE_BYTES (LOG_PAGE_SIZE_BYTES * MAX_LOG_SIZE_PAGES)

#define PAGE_MASK  (~(LOG_PAGE_SIZE_BYTES - 1))
#define PAGE_OFFSET_MASK  (LOG_PAGE_SIZE_BYTES - 1)
#define GET_PAGE_OFFSET(addr)  (((uint64_t)addr) & PAGE_OFFSET_MASK)
#define GET_PAGE_ADDR(addr)  (((uint64_t)addr) & PAGE_MASK)

/* Abort status (logtm), log size in cache blocks
******************************************
 *   log size       |      abort status  |
 *****************************************
 *    32b                   32b
 */

#define M5_ABORTSTATUS_LOGSIZE_ENCODE(logsize) ((logsize) << 32)
#define M5_ABORTSTATUS_LOGSIZE_DECODE(ret) ((ret) >> 32)


/******************************************************************************

Organization of Transaction Log: Each speculatively modified cache
block is copied at cache-line-size aligned addresses starting at the
botton of each log page, while its address is inserted at the top of
that page: "data" log grows from the log bottom towards the log top, while
"address" log grows from the log top towards the log bottom, for each page.

[Log base address]
  ************************************
  *              OLD DATA A          *
  ************************************
  *              OLD DATA B          *
  ************************************
  *              OLD DATA C          *
  ************************************
  < ------------ 64B ---------------->

....


  < ------------ 8B ----------------->
  ************************************
  *              ADDRESS C           *
  ************************************
  *              ADDRESS B           *
  ************************************
  *              ADDRESS A           *
  ************************************
[Log top address]

******************************************************************************/

void init_random_gen(void *thread_contexts);
void init_log(void *thread_contexts);
long randomized_backoff(unsigned long num_retries, void *thread_contexts);

/* Magic number used for sanity checks to signal completion of log unroll to simulator */
#define LOG_UNROLL_END_SIGNAL (0xDEADC0DEBAADCAFE)

static inline uint64_t logtm_compute_addr_ptr_from_data_ptr(uint64_t dataLogPtr)  {
    uint64_t pageAddr = dataLogPtr & PAGE_MASK;
    uint64_t pageOffset = dataLogPtr & PAGE_OFFSET_MASK;
    uint64_t entryIndex = pageOffset / LOG_CACHE_BLOCK_SIZE;
    assert(pageOffset < LOG_ENTRIES_PER_PAGE*LOG_CACHE_BLOCK_SIZE);
    assert((pageOffset % LOG_CACHE_BLOCK_SIZE) == 0);

    // Address log grows downwards from the page top, while data log
    // grows upwards from the page bottom. They should never meet!

    // Log pages of 4096 bytes (64 blocks) organized as follows:
    // 56 data blocks + 1 sentinel block + 7 address blocks (56 addresses)
    return (pageAddr | (LOG_PAGE_SIZE_BYTES - (entryIndex+1)*sizeof(uint64_t)));
}


static inline uint64_t logtm_compute_data_log_pointer(uint64_t logBase, int numEntries) {
    uint8_t *logBaseAddr = (uint8_t *)logBase;
    uint64_t pageIndex = numEntries / LOG_ENTRIES_PER_PAGE;
    uint64_t entryIndex = numEntries % LOG_ENTRIES_PER_PAGE;
    return (uint64_t)(logBaseAddr +
                      pageIndex*LOG_PAGE_SIZE_BYTES +
                      entryIndex*LOG_CACHE_BLOCK_SIZE);
}

static inline uint64_t logtm_compute_addr_log_pointer(uint64_t logBase, int numEntries) {
    uint8_t *logBaseAddr = (uint8_t *)logBase;
    uint64_t pageIndex = numEntries / LOG_ENTRIES_PER_PAGE;
    uint64_t entryIndex = numEntries % LOG_ENTRIES_PER_PAGE;
    return (uint64_t)(logBaseAddr +
                      pageIndex*LOG_PAGE_SIZE_BYTES +
                      (LOG_PAGE_SIZE_BYTES - (entryIndex+1)*sizeof(uint64_t)));
}

// Converted the "compute" functions above to macros in order to avoid
// having functions calls as part of the log unroll (issues with
// inline and always inline)
#define LOGTM_COMPUTE_DATA_LOG_POINTER_OFFSET(numEntries)             \
    ((numEntries) / LOG_ENTRIES_PER_PAGE)*LOG_PAGE_SIZE_BYTES +           \
    ((numEntries) % LOG_ENTRIES_PER_PAGE)*LOG_CACHE_BLOCK_SIZE

#define LOGTM_COMPUTE_ADDR_LOG_POINTER_OFFSET(numEntries)             \
    ((numEntries) / LOG_ENTRIES_PER_PAGE)*LOG_PAGE_SIZE_BYTES +             \
    LOG_PAGE_SIZE_BYTES - (((numEntries) % LOG_ENTRIES_PER_PAGE)+1)*sizeof(uint64_t)

static inline __attribute__((always_inline)) void logtm_log_unroll(uint8_t *log_base, int log_size)
{
    //assert((((uint64_t)log_base) % LOG_CACHE_BLOCK_SIZE) == 0);
    if (log_size == 0) return;
    // log_size points to the next available entry, so we must first
    // move the data pointer down to point to the last logged block
    --log_size;
    uint64_t data_offset = LOGTM_COMPUTE_DATA_LOG_POINTER_OFFSET(log_size);
    uint8_t *data_log_ptr = log_base + data_offset;

    uint64_t addr_offset = LOGTM_COMPUTE_ADDR_LOG_POINTER_OFFSET(log_size);
    uint8_t *addr_log_ptr = log_base + addr_offset;
#if 0
    uint8_t *data_log_ptr1 =
        (uint8_t *)logtm_compute_data_log_pointer((uint64_t)log_base,
                                                  log_size);
    uint8_t *addr_log_ptr1 =
        (uint8_t *)logtm_compute_addr_log_pointer((uint64_t)log_base,
                                                  log_size);

    assert(data_log_ptr == data_log_ptr1);
    assert(addr_log_ptr == addr_log_ptr1);
    (void)data_log_ptr1;
    (void)addr_log_ptr1;
#endif
    while (1) {
        uint64_t *address = (uint64_t *) *((uint64_t *)addr_log_ptr);
        //assert(((uint64_t)address % LOG_CACHE_BLOCK_SIZE) == 0);

        int k;
        for (k = 0; k < (LOG_CACHE_BLOCK_SIZE/sizeof(uint64_t)); k++){
            uint64_t *ptr = (address + k );
            *ptr = *(((long *)data_log_ptr) + k);
        }

        if (log_size == 0) break;

        if (GET_PAGE_OFFSET(data_log_ptr) == 0) {
            // Just unrolled block at the beginning of the page:
            // Log size must be a multiple of entries per page
            //assert((log_size % LOG_ENTRIES_PER_PAGE) == 0);
            // Move pointers to the previous page
            data_offset = LOGTM_COMPUTE_DATA_LOG_POINTER_OFFSET(log_size-1);
            data_log_ptr = log_base + data_offset;

            addr_offset = LOGTM_COMPUTE_ADDR_LOG_POINTER_OFFSET(log_size-1);
            addr_log_ptr = log_base + addr_offset;
#if 0
            data_log_ptr1 =
                (uint8_t *)logtm_compute_data_log_pointer((uint64_t)log_base,
                                                          log_size-1);
            addr_log_ptr1 =
                (uint8_t *)logtm_compute_addr_log_pointer((uint64_t)log_base,
                                                          log_size-1);

            assert(data_log_ptr == data_log_ptr1);
            assert(addr_log_ptr == addr_log_ptr1);
#endif
        }
        else {
            //assert((log_size % LOG_ENTRIES_PER_PAGE) != 0);
            data_log_ptr  -= LOG_CACHE_BLOCK_SIZE;
            addr_log_ptr  += sizeof(long);
        }
        log_size--;
    }
    //assert(data_log_ptr == log_base);
    //assert(addr_log_ptr == (log_base + LOG_PAGE_SIZE_BYTES - sizeof(uint64_t)));

}

#endif
