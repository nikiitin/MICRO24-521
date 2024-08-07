#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "logtm.h"
#include "thread_context.h"

#define _unused(x) ((void)(x))


#define znew(g_rand)  ((g_rand->z=36969*(g_rand->z&65535)+(g_rand->z>>16))<<16)
#define wnew(g_rand)  ((g_rand->w=18000*(g_rand->w&65535)+(g_rand->w>>16))&65535)
#define MWC(g_rand)   (znew+wnew)
#define SHR3(g_rand)  (g_rand->jsr=(g_rand->jsr=(g_rand->jsr=g_rand->jsr^(g_rand->jsr<<17))^(g_rand->jsr>>13))^(g_rand->jsr<<5))
#define CONG(g_rand)  (g_rand->jcong=69069*g_rand->jcong+1234567)
#define KISS(g_rand)  ((MWC(g_rand)^CONG(g_rand))+SHR3(g_rand))

/**********************/

unsigned long power_of_2(unsigned long a){
    unsigned long val;
    val = 1;
    val <<= a;
    return val;
}

unsigned long compute_backoff(unsigned long num_retries){
    unsigned long backoff = 0;
    unsigned long max_backoff;

    if (num_retries > 16)
        max_backoff = 64 * 1024 + (num_retries - 16);
    else
        max_backoff = power_of_2(num_retries);

    backoff = max_backoff;

    return backoff;
}

long randomized_backoff(unsigned long num_retries, void *_thread_contexts){
    _tm_thread_context_t *thread_contexts = (_tm_thread_context_t *)_thread_contexts;
    rand_t *rand = &thread_contexts->rand;
    volatile long a[32];
    volatile long b;
    long j;
    long backoff = (unsigned long) (CONG(rand)) % compute_backoff(num_retries);
    for (j = 0; j < backoff; j++){
        b += a[j % 32];
    }
    return b;
}


void walk_log(unsigned long *log){
    int i;
    char *ptr = (char *)log;
    for (i = 0; i < MAX_LOG_SIZE_BYTES; i+= LOG_PAGE_SIZE_BYTES)
        ptr[i] = '\0';
}


void init_log(void *_thread_contexts){
    _tm_thread_context_t *thread_contexts = (_tm_thread_context_t *)_thread_contexts;
    long i,j;
    _unused(j);

    for (i = 0; i < thread_contexts->info.numThreads; i++) {
        // Point transaction log in thread context used by "standard" abort handler
        _tm_thread_context_t *ctx = &thread_contexts[i];
        ctx->info.logtm_transactionLog = aligned_alloc(LOG_PAGE_SIZE_BYTES,
                                                       MAX_LOG_SIZE_BYTES);
        if (ctx->info.logtm_transactionLog == NULL) {
            perror("Cannot allocate aligned memory for transaction log");
        }
        walk_log(ctx->info.logtm_transactionLog);
    }
}

void init_random_gen(void *_thread_contexts){
    _tm_thread_context_t *thread_contexts = (_tm_thread_context_t *)_thread_contexts;
    int i;
    for (i = 0; i < thread_contexts->info.numThreads; i++) {
        // Point transaction log in thread context used by "standard" abort handler
        _tm_thread_context_t *ctx = &thread_contexts[i];
        ctx->rand.z=362436069;
        ctx->rand.w=521288629;
        ctx->rand.jsr=123456789;
        ctx->rand.jcong=380116160;
    }
}
