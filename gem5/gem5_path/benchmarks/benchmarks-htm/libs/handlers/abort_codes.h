#ifndef ABORT_CODES_H
#define ABORT_CODES_H

// Set by abort handler if htm_cancel does not provide any code (sanity checks)
#define CANCEL_TRANSACTION_DEFAULT_CODE 0xfe

#if defined AARCH64

/************************************************************************
 * RESERVED|INT|DBG|NEST|SIZE|ERR|IMP|MEM|CNCL|RTRY|    REASON (14-0)   *
 *63-24*****23**22**21****20**19**18**17**16***15***14*****************0*/
#define TME_CODE_FALLBACK_LOCK_LOCKED    0x7fff

// Masks to decode ret into fields
#define TME_FAILURE_REASON_DECODE(ret) (ret & _TMFAILURE_REASON)

#elif defined X86

// Bit-shifts to encode value into bit-field code
#define _XABORT_CODE_ENCODE(code)         ((((uint64_t)code) & 0xFF) << 24)

// Masks to decode ret into fields
#define _XABORT_CODE_DECODE(ret)         (uint64_t)(((ret) >> 24) & 0xFF)

// Set by abort handler if abort due to fallback lock locked after xbegin
#define XABORT_CODE_FALLBACK_LOCK_LOCKED 0xff

#endif

#define M5_XBEGIN_TAG_ENCODE(tag) (tag)

#define M5_XBEGIN_TAG_DECODE(ret) (ret & 0xFF)

#endif

