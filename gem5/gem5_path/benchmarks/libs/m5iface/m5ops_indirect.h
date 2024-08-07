/*
 * Copyright (c) 2003-2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GEM5_M5OP_INDIRECT_H__
#define __GEM5_M5OP_INDIRECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <gem5/m5ops.h>

typedef enum {
  M5_OP_INDIRECT_MODE_NOP,
  M5_OP_INDIRECT_MODE_INSTRUCTION,
  M5_OP_INDIRECT_MODE_ADDRESS,
} M5OpIndirectMode;
  
// Choose the values of m5_XXX_indirect pointers. Defaults to M5_OP_INDIRECT_MODE_NOP.
void m5_op_indirect_set_mode(M5OpIndirectMode mode);
  
/*
 * Create _indirect versions for all declarations,
 * e.g. m5_exit_inddirect
 *
 * Some of those declarations are not defined for certain ISAs, e.g. X86
 * does not have _semi, but we felt that ifdefing them out could cause more
 * trouble tham leaving them in.
 */
#define M5OP(name, func) extern __typeof__(name)* M5OP_MERGE_TOKENS(name, _indirect);
M5OP_FOREACH
#undef M5OP

#ifdef __cplusplus
}
#endif
#endif // __GEM5_M5OP_H__
