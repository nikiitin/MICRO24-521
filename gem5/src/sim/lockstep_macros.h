#ifndef LOCKSTEP_H
#define LOCKSTEP_H

/***** LOCKSTEP MODE SUPPORT ***********/
#define REPLAY_RWSET_MARK_BEGIN  ('b')
#define REPLAY_RWSET_MARK_READ   ('r')
#define REPLAY_RWSET_MARK_WRITE  ('w')
#define REPLAY_RWSET_MARK_END	 ('c')
// 8 bytes = 16 hex chars
#define REPLAY_RWSET_SEQNO_LEN    (sizeof(uint64_t)*2)
#define REPLAY_RWSET_ADDR_LEN     (sizeof(uint64_t)*2)
#define REPLAY_RWSET_VALUE_LEN    (sizeof(uint64_t)*2)
#define REPLAY_RWSET_PC_LEN    (sizeof(uint64_t)*2)
#define REPLAY_RWSET_UPC_LEN    (sizeof(uint8_t)*2)
/***************************************/

#endif

