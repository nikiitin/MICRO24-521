#ifndef ABORT_STATUS_H
#define ABORT_STATUS_H

#if defined(__x86_64__)

// TSX Abort codes
#ifndef _XBEGIN_STARTED
/* Avoid compilation redefined error: gcc 5 RTM intrinsic definitions
 * /usr/lib/gcc/x86_64-linux-gnu/5/include/rtmintrin.h
 */
// See "16.3.5 RTM Abort Status Definition" in Intel's sw dev manual
#define _XBEGIN_STARTED		(~0u)
  // Set if abort caused by XABORT instruction.
#define _XABORT_EXPLICIT	(1 << 0)
  // Set if tx may succeed on retry. Always clear if bit 0 set
#define _XABORT_RETRY		(1 << 1)
  // Another processor conflicted with a mem addr in RW set
#define _XABORT_CONFLICT	(1 << 2)
  // Set if an internal buffer to track tx state overflowed
#define _XABORT_CAPACITY	(1 << 3)
  // Set if debug or breakpoint exception hit
#define _XABORT_DEBUG		(1 << 4)
  // Set if an abort occurred while in nested tx
#define _XABORT_NESTED		(1 << 5)
  // Set if an abort was forced by HTM speculation disabled
#define _XABORT_DISABLED	(1 << 6)
  // Set if an abort was not complete (requires log unroll)
#define _XABORT_UNDO_LOG  	(1 << 7)
  // Set if an abort was caused by a conflict with a power transaction
#define _XABORT_CONFLICT_POWER  	(1 << 8)

#endif // _XBEGIN_STARTED

#elif defined(__aarch64__)

// TME Abort codes
#ifndef _TMFAILURE_REASON

#define _TMFAILURE_REASON     0x00007fffu
#define _TMFAILURE_RTRY       0x00008000u
#define _TMFAILURE_CNCL       0x00010000u
#define _TMFAILURE_MEM        0x00020000u
#define _TMFAILURE_IMP        0x00040000u
#define _TMFAILURE_ERR        0x00080000u
#define _TMFAILURE_SIZE       0x00100000u
#define _TMFAILURE_NEST       0x00200000u
#define _TMFAILURE_DBG        0x00400000u
#define _TMFAILURE_INT        0x00800000u
#define _TMFAILURE_TRIVIAL    0x01000000u
#define _TMFAILURE_DISABLED   0x02000000u
#define _TMFAILURE_UNDO_LOG   0x04000000u
#endif //_TMFAILURE_REASON

#else
#error "abort_status.h not implemented"
#endif        

#endif

