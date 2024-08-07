#include "env_globals.h"
#include "m5iface.h"

#if ! defined(ENABLE_M5OPS)
void m5_init() {}
void simBeginRegionOfInterest() {}
void simEndRegionOfInterest() {}
void simSetLogBase(void *ptr) {}
void simWorkBegin() {}
void simWorkEnd() {}
void simBarrierBegin() {}
void simBarrierEnd() {}
void simBackoffBegin() {}
void simBackoffEnd() {}
void simCodeRegionBegin(unsigned long int codeRegionId) {}
void simCodeRegionEnd(unsigned long int codeRegionId) {}
void simFallbackLockAcquired(long xid) {}
void simFallbackLockRelease(long xid) {}

#ifdef ANNOTATE_FALLBACKLOCK_ADDR 
#error "ANNOTATE_FALLBACKLOCK_ADDR requires ENABLE_M5OPS"
#endif

#ifdef ANNOTATE_PROC_MAPS
#error "ANNOTATE_PROC_MAPS requires ENABLE_M5OPS"
#endif

#ifdef ANNOTATE_CODE_REGIONS
#error "ANNOTATE_CODE_REGIONS requires ENABLE_M5OPS"
#endif

#ifdef NOTIFY_FALLBACKLOCK
#error "NOTIFY_FALLBACKLOCK requires ENABLE_M5OPS"
#endif

#else // # defined(ENABLE_M5OPS)

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "include/gem5/m5ops.h"
#include "util/m5/src/m5_mmap.h"

void dumpValueToHostFileSystem(long value, const char *out_filename);
void catProcMaps(const char* out_filename);


#ifdef X86
// on x86, the default mechanisms is magic address

#define m5_write_file m5_write_file_addr
#define m5_work_begin m5_work_begin_addr
#define m5_work_end m5_work_end_addr
#define m5_sum m5_sum_addr
#define m5_dump_stats  m5_dump_stats_addr
#define m5_reset_stats  m5_reset_stats_addr

#else // call type: magic instruction

#define m5_write_file m5_write_file
#define m5_work_begin m5_work_begin
#define m5_work_end m5_work_end
#define m5_sum m5_sum
#define m5_dump_stats  m5_dump_stats
#define m5_reset_stats  m5_reset_stats

#endif

#ifdef ANNOTATE_FALLBACKLOCK_ADDR 
#include "spinlock.h"
#define FALLBACKLOCKADDR_FILENAME ("fallback_lock")
#endif

void
m5_init()
{
#if X86
    if (inSimulator()) {
        map_m5_mem();
    } else {
        // When running in real hardware, point m5_mem to a region of
        // memory filled with zeros, no need to mmap /dev/mem
        m5_mem = calloc( 0x10000, sizeof(char));
    }
#endif

#ifdef ANNOTATE_FALLBACKLOCK_ADDR
    if (inSimulator()) {
        long fallBackLockAddr = (long)spinlock_getAddress();
        // Max fallback lock filename length is 63 characters
        char fallbackLockAddrFilename[CACHE_LINE_SIZE_BYTES]
            __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) =
            FALLBACKLOCKADDR_FILENAME;
        dumpValueToHostFileSystem(fallBackLockAddr,
                                  fallbackLockAddrFilename);
    }
#endif

#ifdef ANNOTATE_PROC_MAPS
    if (inSimulator())
        catProcMaps("proc_maps");
#endif
}

#ifdef NOTIFY_FALLBACKLOCK
void simFallbackLockAcquired(long xid) {
    m5_sum(M5_SUM_HACK_ARGS, M5_SUM_HACK_FL_ACQ, xid);
}

void simFallbackLockRelease(long xid) {
    m5_sum(M5_SUM_HACK_ARGS, M5_SUM_HACK_FL_COMMIT, xid);
}
#else // #ifdef NOTIFY_FALLBACKLOCK
void simFallbackLockAcquired(long xid) {}
void simFallbackLockRelease(long xid) {}
#endif // #ifdef NOTIFY_FALLBACKLOCK

void simBeginRegionOfInterest() {
    if (m5_sum(1,2,3,4,5,6)) { // i.e., inSimulator()
        // Force writing all buffered user space data (messages printed to
        // terminal) by calling fflush before GOTO_SIM (init checkpoint
        // taken), to prevent noise during the ROI. NOTE: It is assumed
        // GOTO_SIM only called by master thread
        fflush(NULL);
        fsync(1);
        sleep(1); // hack! wait for the OS to drain all buffers...
    }
    // IMPORTANT NOTICE (support for fast-forward using KVM): See
    // $(GEM5_ROOT)/src/sim/System.py for documentation. Basically, we
    // need to ensure that we checkpoint the state of the guest
    // immediately after the execution of m5_work_begin (GOTO_SIM),
    // which is not always the case when running with KVM. To ensure
    // proper checkpointing when running in the simulator (we can tell
    // by looking at the return value of m5_sum_addr), we enter a
    // "dummy loop" that may cause the program to "spin" indefinitely,
    // depending on the value returned by the m5_sum for a specific
    // set of arguments (HEXSPEAK below). When running in real
    // hardware, we point m5_mem to a zero-filled memory region so
    // that all loads to that area are safe and simply return 0.  (see
    // init_m5_mem() in abort_handlers.c)

    // When running in gem5, the command-line option
    // 'checkpoint-m5sum-kvm-hack' will determine whether the return value
    // of m5_sum is tweaked (0) instead of the expected sum of arguments

    m5_work_begin(0,0);
    if (m5_sum(1,2,3,4,5,6)) { // i.e., inSimulator()
        while (m5_sum(M5_SUM_HACK_ARGS, M5_SUM_HACK_TYPE_KVM_CKPT_SYNC, 0) == 0);
    }
    m5_reset_stats(0,0);
}

void simEndRegionOfInterest() {
    m5_work_end(0,0);
    m5_dump_stats(0,0);
}

void simWorkBegin() {
    m5_work_begin(0,0);
}
void simWorkEnd() {
    m5_work_end(0,0);
}


void walk_log(unsigned long *log);

void simSetLogBase(void *logptr) {
    if (m5_sum(M5_SUM_HACK_ARGS, M5_SUM_HACK_TYPE_LOGTM_SETUP_LOG,
               (unsigned long int )logptr) == 0) {
        // simulator returns 0 if v2p translation table already set
        // up. Otherwise, walk the log, simulator will intercept
        // accesses and fill log v2p translation table
        walk_log(logptr);
    }
}

#if defined(ANNOTATE_CODE_REGIONS)

#include "annotated_regions.h"

// The m5_sum op is reused for communication between benchmark and
// simulator, in this case to let the simulator now in which region of
// the abort the benchmark is at a given moment.
void simCodeRegionBegin(unsigned long int codeRegionId)
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_BEGIN, codeRegionId);
}
void simCodeRegionEnd(unsigned long int codeRegionId)
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_END, codeRegionId);
}

void simBarrierBegin()
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_BEGIN, AnnotatedRegion_BARRIER);
}
void simBarrierEnd()
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_END, AnnotatedRegion_BARRIER);
}

void simBackoffBegin()
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_BEGIN, AnnotatedRegion_BACKOFF);
}
void simBackoffEnd()
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_END, AnnotatedRegion_BACKOFF);
}

#else // # defined(ANNOTATE_CODE_REGIONS)


void annotateCodeRegionBegin(unsigned long int codeRegionId) {}
void annotateCodeRegionEnd(unsigned long int codeRegionId) {}

void annotateBarrierRegionBegin() {}
void annotateBarrierRegionEnd() {}

#endif // # defined(ANNOTATE_CODE_REGIONS)


#define BUFF_SIZE 16386

void catProcMaps(const char* out_filename) {
    char filename[32];
    sprintf(filename,"/proc/%d/maps",getpid());

    int fd = open(filename, 0);
    assert(fd >= 0);

    char buf[BUFF_SIZE];
    size_t bytesRead = 0;
    size_t size = 0;
    do {
        bytesRead = read(fd, &buf[size], BUFF_SIZE);
        size += bytesRead;
        assert(size < BUFF_SIZE);
    } while (bytesRead != 0);
    close(fd);
    assert(size > 0);
    int bytesWritten =  m5_write_file(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

void dumpValueToHostFileSystem(long value, const char *out_filename) {
    char buf[BUFF_SIZE];
    sprintf(buf,"%#lx\n", value);
    assert(strlen(out_filename) > 0);
    int size = strlen(buf);
    int bytesWritten =  m5_write_file(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

#endif // #if ! defined(ENABLE_M5OPS)
