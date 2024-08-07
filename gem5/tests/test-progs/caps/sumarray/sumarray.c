#define _POSIX_C_SOURCE 200112L /* barrier */
#define _XOPEN_SOURCE 600 /* random */

#include <assert.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "env.h"
#include "timer.h"

#define ENV_VAR_IN_SIMULATOR "M5_SIMULATOR"

#if ENABLE_M5_TRIGGER
#include "include/gem5/m5ops.h"
#include "util/m5/src/m5_mmap.h"

void m5_init(bool inSimulator) {
    // In order to run same binary in real hardware (prevent
    // segmentation fault caused by m5_xxx_addr)
    if (inSimulator) {
        map_m5_mem();
    } else {
        fprintf(stderr, "%s env var is unset! m5 ops will be ignored\n",
                ENV_VAR_IN_SIMULATOR);
        // When running in real hardware, point m5_mem to a region of
        // memory filled with zeros, no need to mmap /dev/mem
        m5_mem = calloc( 0x10000, sizeof(char));
    }
}
#endif


enum param_types
{
    PARAM_ARRAY = (unsigned char)'a',
    PARAM_THREAD = (unsigned char)'t',
};

enum param_defaults
{
    PARAM_DEFAULT_ARRAY = 16384,
    PARAM_DEFAULT_THREAD = 1,
};

long global_params[256] = { /* 256 = ascii limit */
    [PARAM_ARRAY] = PARAM_DEFAULT_ARRAY,
    [PARAM_THREAD] = PARAM_DEFAULT_THREAD,
};

#define CACHE_LINE_BYTES  (64L)
#define PAGE_SIZE        (4096L)

/*** Globals **/
typedef struct globals
{
    long *sharedArray;
    char padding1[CACHE_LINE_BYTES-sizeof(long *)];
    long *results;
    char padding2[CACHE_LINE_BYTES-sizeof(long *)];
    pthread_barrier_t barrier;
} globals_t;

globals_t globals __attribute__ ((aligned (CACHE_LINE_BYTES))) ;

typedef long (*func_t)(void *);

static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                            (defaults)\n");
    printf("    a <UINT>   Number of [a]rray elements  (%i)\n", PARAM_DEFAULT_ARRAY);
    printf("    t <UINT>   Number of [t]hreads       (%i)\n", PARAM_DEFAULT_THREAD);
    exit(1);
}


void *
sumArray(void* argptr)
{
    pthread_barrier_wait(&globals.barrier);

    assert(globals.sharedArray != NULL);

    long myId = (long)argptr;
    long numThreads = global_params[PARAM_THREAD];
    long numElementsArray = global_params[PARAM_ARRAY];
    long numElementsPerThread = numElementsArray / numThreads;
    long startIndex = myId * numElementsPerThread;
    long i, arraySum = 0;

    for (i = startIndex; i < startIndex + numElementsPerThread; i++) {
        arraySum += globals.sharedArray[i];
    }

    printf("sumArray, I am thread %ld, arraySum = %ld\n",
           myId, arraySum);
    globals.results[myId] = arraySum;
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, "a:x:l:i:d:k:t:")) != -1) {
        switch (opt) {
        case 'a':
        case 'l':
        case 'd':
        case 'i':
        case 'k':
        case 'x':
        case 't':
            global_params[(unsigned char)opt] = atol(optarg);
            break;
        case '?':
        default:
            opterr++;
            break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}

void
initGlobalVars ()
{
    long numThreads = global_params[PARAM_THREAD];
    // init barrier
    int s = pthread_barrier_init(&globals.barrier, NULL, numThreads);
    assert(s == 0);

    // Allocate partial sums vector and init to 0
    globals.results = (long *)malloc(numThreads*sizeof(long));
    memset(globals.results, 0, numThreads*sizeof(long));

    // Allocate shared array and fill with random values
    globals.sharedArray = (long *)malloc(global_params[PARAM_ARRAY]*sizeof(long));
    unsigned int seed = 0;
    srandom(seed);
    int i;
    for (i = 0 ; i < global_params[PARAM_ARRAY]; ++i) {
        globals.sharedArray[i] = random() & 0xFFFF;
    }
}

void
checkSolution ()
{
    int i;
    long sum1 = 0;
    long sum2 = 0;
    for (i = 0 ; i < global_params[PARAM_ARRAY]; ++i) {
        sum1 += globals.sharedArray[i];
    }
    for (i = 0 ; i < global_params[PARAM_THREAD]; ++i) {
        sum2 += globals.results[i];
    }
    assert(sum1 == sum2);
}

int main (int argc, char** argv)
{
    bool inSimulator = parseBoolEnv(ENV_VAR_IN_SIMULATOR,
                                    &inSimulator);
#if ENABLE_M5_TRIGGER
    m5_init(inSimulator);
#endif
    parseArgs(argc, argv);

    long numThreads = global_params[PARAM_THREAD];
    long numElementsArray = global_params[PARAM_ARRAY];
    long numElementsPerThread = numElementsArray / numThreads;
    assert((numElementsArray % numThreads) == 0);

    initGlobalVars();

    printf("\nRunning %ld threads...\n", numThreads);

    pthread_t *threads = (pthread_t*) malloc(sizeof(pthread_t)*numThreads);
    long i, rc;
    for (i=1; i < numThreads; i++) {
        rc = pthread_create(&threads[i], NULL, sumArray, (void*)i);
        assert(rc==0);
    }

    TIMER_T startTime;
    TIMER_READ(startTime);

#if ENABLE_M5_TRIGGER
    // In order to run same binary in real hardware (prevent
    // segmentation fault caused by m5_xxx_addr)
    m5_work_begin_addr(0,0);

    // Hack to stall vCPUs for correct checkpoint after fast-forward
    // Required if using KVM to fast-forward until beginning of ROI
    if (m5_sum_addr(1,2,3,4,5,6)) {
        while (m5_sum_addr(0xCAFE,0xBEEF, 0xDEAD,
                           0xBABE, 0xBAAD, 0xC0DE) == 0);
    }

#endif

    sumArray((void*)0);

#if ENABLE_M5_TRIGGER
    m5_work_end_addr(0,0);
#endif

    TIMER_T stopTime;
    TIMER_READ(stopTime);
    fprintf(stderr, "Elapsed time in region of interest = %f seconds\n",
           TIMER_DIFF_SECONDS(startTime, stopTime));

    // wait for worker threads
    for (i=1; i < numThreads-1; i++) {
        rc = pthread_join(threads[i], NULL);
        assert(rc==0);
    }
    checkSolution();
}


/* =============================================================================
 *
 * End of toy.c
 *
 * =============================================================================
 */
