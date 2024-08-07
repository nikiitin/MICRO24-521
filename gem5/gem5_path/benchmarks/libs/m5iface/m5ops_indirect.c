#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include "m5ops_indirect.h"

#define M5OP(name, func) __typeof__(name)* M5OP_MERGE_TOKENS(name, _indirect) = M5OP_MERGE_TOKENS(name, _nop); 
M5OP_FOREACH 
#undef M5OP

void m5_op_indirect_set_mode(M5OpIndirectMode mode) {
  if (mode == M5_OP_INDIRECT_MODE_NOP) {
#   define M5OP(name, func) M5OP_MERGE_TOKENS(name, _indirect) = M5OP_MERGE_TOKENS(name, _nop); 
    M5OP_FOREACH 
#   undef M5OP
  } else if (mode == M5_OP_INDIRECT_MODE_INSTRUCTION) {
#   define M5OP(name, func) M5OP_MERGE_TOKENS(name, _indirect) = name; 
    M5OP_FOREACH 
#   undef M5OP
  } else if (mode == M5_OP_INDIRECT_MODE_ADDRESS) {
#   define M5OP(name, func) M5OP_MERGE_TOKENS(name, _indirect) = M5OP_MERGE_TOKENS(name, _addr); 
    M5OP_FOREACH 
#   undef M5OP
  } else {
    assert(false);
  }
}
