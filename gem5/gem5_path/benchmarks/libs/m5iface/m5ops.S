
#include <gem5/asm/generic/m5ops.h>

#if defined(__x86_64__)
#include "asm/x86/m5op.S"
#include "asm/x86/m5op_nop.S"
#define M5OP_PIC
#include "asm/x86/m5op_addr.S"
#elif defined(__aarch64__)
#include "asm/arm64/m5op.S"
#include "asm/arm64/m5op_nop.S"
#define M5OP_PIC
#include "asm/arm64/m5op_addr.S"
//#include "asm/arm64/m5op_semi.S"
#elif defined(__riscv)
#include "asm/riscv/m5op.S"
#include "asm/riscv/m5op_nop.S"
#else
#error "m5ops.S not implemented"
#endif        
