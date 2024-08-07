#ifndef __GEM5_M5IFACE_H__
#define __GEM5_M5IFACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
  
#define ENV_VAR_IN_SIMULATOR "M5_SIMULATOR"

void simInit(void);
bool simInSimulator(void);
void simBeginRegionOfInterest(void);
void simEndRegionOfInterest(void);
bool simSetLogBase(void *ptr);
void simWorkBegin(void);
void simWorkEnd(void);
void simBarrierBegin(void);
void simBarrierEnd(void);
void simBackoffBegin(void);
void simBackoffEnd(void);
void simCodeRegionBegin(unsigned long int codeRegionId);
void simCodeRegionEnd(unsigned long int codeRegionId);
void simDumpValueToHostFileSystem(long value, const char *out_filename);

// Instead of adding a new gem5op, we let the CPU know that the log
// has been unrolled successfully by writing a 64-bit magic value into
// the log base address. NOTE: This is essential for functional
// correctness since the abort does not complete until the HTM logic
// receives this signal (to release isolation over write-set, etc.)

#if defined __aarch64__

#define simEndLogUnroll(ptr) ({                                         \
            __asm__ volatile ("mov    x1, %0\n\t"                       \
                              "movz x0, #0xcafe \n\t"                    \
                              "movk x0, #0xbaad, lsl 16 \n\t"            \
                              "movk x0, #0xc0de, lsl 32 \n\t"            \
                              "movk x0, #0xdead, lsl 48 \n\t"            \
                              "str    x0, [x1]\n\t"                     \
                              :                                         \
                              : "r"(ptr)                                \
                              : "x0", "x1");                            \
        })

#elif defined __x86_64__

#define simEndLogUnroll(ptr) ({                                         \
            __asm__ volatile ("mov    %0,%%rdi\n\t"                     \
                              "movabs $0xdeadc0debaadcafe,%%rax\n\t"    \
                              "mov    %%rax,(%%rdi)\n\t"                \
                              :                                         \
                              : "r"(ptr)                                \
                              : "%rdi", "rax");                         \
        })
#endif

#ifdef __cplusplus
}
#endif

#endif
