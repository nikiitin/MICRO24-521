#ifndef SPINLOCK_H
#define SPINLOCK_H 1

#include <stdint.h>

#define CACHE_LINE_SIZE_BYTES 64
#define NUM_GLOBAL_LOCKS 10
#define PADDED_ARRAY_SIZE_BYTES (CACHE_LINE_SIZE_BYTES * NUM_GLOBAL_LOCKS)

#if defined (AARCH64)

#include <assert.h>

extern unsigned int numLock;
extern volatile char lock_array[(NUM_GLOBAL_LOCKS-2)*CACHE_LINE_SIZE_BYTES]
  __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;

#if defined (AARCH64_NAIVE_SPINLOCK)

// Naive spinlock implementation from:
//  https://community.arm.com/arm-research/b/articles/posts/arms-transactional-memory-extension-support-
#include <stdatomic.h>

typedef atomic_int lock_t;

typedef struct {
    lock_t lock;
    char padding[CACHE_LINE_SIZE_BYTES - sizeof(lock_t)];
} spinlock_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

extern spinlock_t fallbackLock;

static inline void * spinlock_getAddress()
{
    return (void *) &fallbackLock.lock;
}

static inline void spinlock_init()
{
    atomic_init(&fallbackLock.lock, 0);
}

static inline long spinlock_isLocked()
{
    return atomic_load_explicit(&fallbackLock.lock,
                                memory_order_acquire);
}

static inline void spinlock_whileIsLocked()
{
  while (spinlock_isLocked()) {
      // __yield();
  }
}

static inline void spinlock_lock()
{
    do {
        spinlock_whileIsLocked();
    }
    while (atomic_exchange_explicit(&fallbackLock.lock, 1,
                                    memory_order_acquire));

    // This will generate a full memory barrier, e.g. DMB SY
    atomic_thread_fence(memory_order_seq_cst);
}

static inline void spinlock_unlock()
{
    atomic_store_explicit(&fallbackLock.lock, 0, memory_order_release);
}

#else // ARM64_NAIVE SPINLOCK

// Ticket spinlock implementation found in the linux kernel, adapted from
// https://github.com/siemens/jailhouse/blob/master/hypervisor/arch/arm64/include/asm/spinlock.h

#define TICKET_SHIFT	16

typedef struct {
    uint16_t owner;
    uint16_t next;
} spinlock_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

extern spinlock_t fallbackLock;

static inline void * spinlock_getAddress()
{
    return (void *) &fallbackLock;
}
static inline void spinlock_init()
{
    fallbackLock.owner = 0;
    fallbackLock.next = 0;
}

static inline long spinlock_isLocked()
{
    spinlock_t lockval;
    __asm__ __volatile__(
"        ldr    %[result], %[input]\n"
: [result] "=r" (lockval)
: [input] "Q" (fallbackLock)
: );
    return lockval.owner != lockval.next;
}

static inline void spinlock_whileIsLocked()
{
  while (spinlock_isLocked()) {
      // __yield();
  }
}

static inline void spinlock_lock()
{
    unsigned int tmp;
    spinlock_t lockval, newval;

        __asm__ __volatile__(
    /* Atomically increment the next ticket. */
"	prfm	pstl1strm, %3\n"
"1:	ldaxr	%w0, %3\n"
"	add     %w1, %w0, #0x10, lsl #12\n"
"	stxr	%w2, %w1, %3\n"
"	cbnz	%w2, 1b\n"
    /* Did we get the lock? */
"	eor	%w1, %w0, %w0, ror #16\n"
"	cbz	%w1, 3f\n"
    /*
     * No: spin on the owner. Send a local event to avoid missing an
     * unlock before the exclusive load.
     */
"	sevl\n"
"2:	wfe\n"
"	ldaxrh	%w2, %3\n"
"	eor	%w1, %w2, %w0, lsr #16\n"
"	cbnz	%w1, 2b\n"
    /* We got the lock. Critical section starts here. */
"3:"
    : "=&r" (lockval), "=&r" (newval), "=&r" (tmp), "+Q" (fallbackLock)
    :
    : "memory");
}

static inline void spinlock_unlock()
{
        __asm__ __volatile__(
"	stlrh	%w1, %0\n"
    : "=Q" (fallbackLock.owner)
    : "r" (fallbackLock.owner + 1)
    : "memory");
}

static inline void spinlock_basic_init(spinlock_t * basic_lock)
{
    basic_lock->owner = 0;
    basic_lock->next = 0;
}

static inline long spinlock_basic_isLocked(spinlock_t * basic_lock)
{
    spinlock_t lockval;
    __asm__ __volatile__(
"        ldr    %[result], %[input]\n"
: [result] "=r" (lockval)
: [input] "Q" (*basic_lock)
: );
    return lockval.owner != lockval.next;
}

static inline void spinlock_basic_whileIsLocked(spinlock_t * basic_lock)
{
  while (spinlock_basic_isLocked(basic_lock)) {
      // __yield();
  }
}

static inline void spinlock_basic_lock(spinlock_t * basic_lock)
{
    unsigned int tmp;
    spinlock_t lockval, newval;

        __asm__ __volatile__(
	/* Atomically increment the next ticket. */
"	prfm	pstl1strm, %3\n"
"1:	ldaxr	%w0, %3\n"
"	add     %w1, %w0, #0x10, lsl #12\n"
"	stxr	%w2, %w1, %3\n"
"	cbnz	%w2, 1b\n"
	/* Did we get the lock? */
"	eor	%w1, %w0, %w0, ror #16\n"
"	cbz	%w1, 3f\n"
	/*
	 * No: spin on the owner. Send a local event to avoid missing an
	 * unlock before the exclusive load.
	 */
"	sevl\n"
"2:	wfe\n"
"	ldaxrh	%w2, %3\n"
"	eor	%w1, %w2, %w0, lsr #16\n"
"	cbnz	%w1, 2b\n"
	/* We got the lock. Critical section starts here. */
"3:"
    : "=&r" (lockval), "=&r" (newval), "=&r" (tmp), "+Q" (*(basic_lock))
    :
    : "memory");
}

static inline void spinlock_basic_unlock(spinlock_t * basic_lock)
{
        __asm__ __volatile__(
"	stlrh	%w1, %0\n"
    : "=Q" (basic_lock->owner)
    : "r" (basic_lock->owner + 1)
    : "memory");
}

static inline spinlock_t * spinlock_basic_get()
{
    assert(numLock < NUM_GLOBAL_LOCKS);
    spinlock_t * basic_lock = &((spinlock_t *)(lock_array))[numLock];
    spinlock_basic_init(basic_lock);
    ++numLock;
    return basic_lock;
}

#endif

#elif defined (X86)


#include <assert.h>
#include <stdio.h>
#include <xmmintrin.h>

extern volatile char lock_array[PADDED_ARRAY_SIZE_BYTES]
__attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;

extern unsigned int numLock;

typedef struct {
    long lock;
    char padding[CACHE_LINE_SIZE_BYTES-8];
} spinlock_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

typedef struct {
    volatile long * powerFlag;
    volatile long * fallbackLock;
    volatile long * preFallbackLock; // see HANDLER_FALLBACKLOCK_2PHASE
} lockPtr_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

extern lockPtr_t locks;

static inline void * spinlock_getAddress()
{
    return (void *)locks.fallbackLock;
}
static inline void spinlock_init()
{
  /* If we ever need to use more than one lock, make sure to make room
     in each one maps to a different cache line in the lock array,
     which should be adequately sized using NUM_GLOBAL_LOCKS */
  assert(CACHE_LINE_SIZE_BYTES*numLock < sizeof(lock_array));
  (locks.fallbackLock) = (long *)&lock_array[CACHE_LINE_SIZE_BYTES*numLock];
  *(locks.fallbackLock) = 0;

  ++numLock;
  assert(numLock < NUM_GLOBAL_LOCKS);
  (locks.preFallbackLock) = (long *)&lock_array[CACHE_LINE_SIZE_BYTES*numLock];
  *(locks.preFallbackLock) = 0;

  ++numLock;

  (locks.powerFlag) = (long *)&lock_array[CACHE_LINE_SIZE_BYTES*numLock];
  *(locks.powerFlag) = -1;
  ++numLock;
  // Sanity check: remember to increase lock_array size accordingly
  assert(numLock < NUM_GLOBAL_LOCKS);
}

static inline long spinlock_isLocked()
{
  return *(locks.fallbackLock) != 0;
}

static inline void spinlock_whileIsLocked()
{
  while (spinlock_isLocked()) {
    _mm_pause();
  }
}

static inline void spinlock_lock()
{
    do {
        spinlock_whileIsLocked();
    }
    while (!__sync_bool_compare_and_swap((locks.fallbackLock), 0, 1));
}


static inline void spinlock_unlock()
{
    __asm__ volatile (""); // acts as a memory barrier.
    *(locks.fallbackLock) = 0;
}

static inline long spinlock_prefb_isLocked()
{
  return *(locks.preFallbackLock) != 0;
}

static inline void spinlock_prefb_whileIsLocked()
{
  while (spinlock_prefb_isLocked()) {
    _mm_pause();
  }
}

static inline void spinlock_prefb_lock()
{
    do {
        spinlock_prefb_whileIsLocked();
    }
    while (!__sync_bool_compare_and_swap((locks.preFallbackLock), 0, 1));
}


static inline void spinlock_prefb_unlock()
{
    __asm__ volatile (""); // acts as a memory barrier.
    *(locks.preFallbackLock) = 0;
}

static inline spinlock_t * spinlock_basic_get()
{
    assert(numLock < NUM_GLOBAL_LOCKS);
    spinlock_t * basic_lock = &((spinlock_t *)(lock_array))[numLock];
    assert((long *)basic_lock != (long *)locks.preFallbackLock);
    assert((long *)basic_lock != (long *)locks.fallbackLock);
    basic_lock->lock = 0;
    ++numLock;
    return basic_lock;
}

static inline long spinlock_basic_isLocked(spinlock_t * basic_lock)
{
  return basic_lock->lock != 0;
}

static inline void spinlock_basic_whileIsLocked(spinlock_t * lock)
{
  while (spinlock_basic_isLocked(lock)) {
    _mm_pause();
  }
}

static inline void spinlock_basic_lock(spinlock_t * basic_lock){
    assert((long *)basic_lock != (long *)locks.preFallbackLock);
    assert((long *)basic_lock != (long *)locks.fallbackLock);
    do {
        spinlock_basic_whileIsLocked(basic_lock);
    }
    while (!__sync_bool_compare_and_swap(&(basic_lock->lock), 0, 1));
}

static inline void spinlock_basic_unlock(spinlock_t * basic_lock)
{
    assert((long *)basic_lock != (long *)locks.preFallbackLock);
    assert((long *)basic_lock != (long *)locks.fallbackLock);
    __asm__ volatile (""); // acts as a memory barrier.
    basic_lock->lock= 0;
}

#endif // ARCH

#endif /* SPINLOCK_H */
