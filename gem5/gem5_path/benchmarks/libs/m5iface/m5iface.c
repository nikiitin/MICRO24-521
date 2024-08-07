#include "m5iface.h"

#if ! defined(ENABLE_M5OPS)
void simInit(void) {}
void simBeginRegionOfInterest(void) {}
void simEndRegionOfInterest(void) {}
void simSetLogBase(void *ptr) {}
void simWorkBegin(void) {}
void simWorkEnd(void) {}
void simBarrierBegin(void) {}
void simBarrierEnd(void) {}
void simBackoffBegin(void) {}
void simBackoffEnd(void) {}
void simCodeRegionBegin(unsigned long int codeRegionId) {}
void simCodeRegionEnd(unsigned long int codeRegionId) {}
void simFallbackLockAcquired(long xid) {}
void simFallbackLockRelease() {}

#ifdef ANNOTATE_PROC_MAPS
#error "ANNOTATE_PROC_MAPS requires ENABLE_M5OPS"
#endif

#ifdef ANNOTATE_CODE_REGIONS
#error "ANNOTATE_CODE_REGIONS requires ENABLE_M5OPS"
#endif

#else // # defined(ENABLE_M5OPS)

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gem5/m5ops.h>
#include "m5ops_indirect.h"
#include "m5_mmap.h"

#define MSG_PREFIX "m5iface: "

static void catProcMaps(const char* out_filename);

// Use the _indirect version of magic calls to allow for runtime configurability
#define m5_write_file m5_write_file_indirect
#define m5_work_begin m5_work_begin_indirect
#define m5_work_end m5_work_end_indirect
#define m5_sum m5_sum_indirect
#define m5_dump_stats  m5_dump_stats_indirect
#define m5_reset_stats  m5_reset_stats_indirect

// Sets envVar if "envVarName" defined and set to a valid int value. Returns true if the variable was defined.
static bool parseBoolEnv(char *envVarName, bool *envVar) {
    char *envString = getenv(envVarName);
    if (envString != NULL) {
      if (!strcmp(envString, "1") || !strcmp(envString, "yes") || !strcmp(envString, "true")) {
        *envVar = true;
      } else if (!strcmp(envString, "0") || !strcmp(envString, "no") || !strcmp(envString, "false")) {
        *envVar = false;
      } else {
        fprintf(stderr, MSG_PREFIX "Invalid value for %s env var\n", envVarName);
        exit(1);
      }
      return true;
    } else {
      return false;
    }
}

#define ENV_VAR_IN_SIMULATOR "M5_SIMULATOR"

static bool inSimulator = false;

bool simInSimulator(void) {
  return inSimulator;
}

void
simInit(void)
{
  bool set = parseBoolEnv(ENV_VAR_IN_SIMULATOR, &inSimulator);
  if (!set) {
    fprintf(stderr, MSG_PREFIX "WARNING %s env var is unset! Assuming false\n", ENV_VAR_IN_SIMULATOR);
    inSimulator = false;
  }
  printf(MSG_PREFIX " %s %d \n", ENV_VAR_IN_SIMULATOR, inSimulator);
  
  if (inSimulator) {
    if (map_m5_mem()) { // Prefer magic memory accesses for compatibility with KVM, unless unavailable
      m5_op_indirect_set_mode(M5_OP_INDIRECT_MODE_ADDRESS);
      printf(MSG_PREFIX " Using magic memory accesses for gem5 interface\n");
    } else {
      m5_op_indirect_set_mode(M5_OP_INDIRECT_MODE_INSTRUCTION);
      printf(MSG_PREFIX " Using magic instructions for gem5 interface\n");
    }
  } else {
    m5_op_indirect_set_mode(M5_OP_INDIRECT_MODE_NOP);
    printf(MSG_PREFIX " Disabled gem5 interface\n");
  }

#ifdef ANNOTATE_PROC_MAPS
    if (inSimulator)
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

void simBeginRegionOfInterest(void) {
    if (m5_sum(1,2,3,4,5,6)) { // i.e., in the simulator
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
    if (m5_sum(1,2,3,4,5,6)) { // i.e., in the simulator
        while (m5_sum(M5_SUM_HACK_ARGS, M5_SUM_HACK_TYPE_KVM_CKPT_SYNC, 0) == 0);
    }
    m5_reset_stats(0,0);
}

void simEndRegionOfInterest(void) {
    m5_work_end(0,0);
    m5_dump_stats(0,0);
}

void simWorkBegin(void) {
    m5_work_begin(0,0);
}
void simWorkEnd(void) {
    m5_work_end(0,0);
}


bool simSetLogBase(void *logptr) {
    return m5_sum(M5_SUM_HACK_ARGS, M5_SUM_HACK_TYPE_LOGTM_SETUP_LOG,
                  (unsigned long int )logptr) == 0;
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

void simBarrierBegin(void)
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_BEGIN, AnnotatedRegion_BARRIER);
}
void simBarrierEnd(void)
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_END, AnnotatedRegion_BARRIER);
}

void simBackoffBegin(void)
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_BEGIN, AnnotatedRegion_BACKOFF);
}
void simBackoffEnd(void)
{
    m5_sum(M5_SUM_HACK_ARGS,
           M5_SUM_HACK_TYPE_REGION_END, AnnotatedRegion_BACKOFF);
}

#else // # defined(ANNOTATE_CODE_REGIONS)


void annotateCodeRegionBegin(unsigned long int codeRegionId) {}
void annotateCodeRegionEnd(unsigned long int codeRegionId) {}

void annotateBarrierRegionBegin(void) {}
void annotateBarrierRegionEnd(void) {}

#endif // # defined(ANNOTATE_CODE_REGIONS)


#define BUFF_SIZE 16386

void catProcMaps(const char* out_filename) {
    char filename[32];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", getpid());

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

void simDumpValueToHostFileSystem(long value, const char *out_filename) {
    char buf[BUFF_SIZE];
    sprintf(buf,"%#lx\n", value);
    assert(strlen(out_filename) > 0);
    int size = strlen(buf);
    int bytesWritten =  m5_write_file(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

#endif // #if ! defined(ENABLE_M5OPS)
