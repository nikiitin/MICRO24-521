.macro m5op_func_nop, name, func
        .globl \name
        .func \name
\name:
        ret
        .endfunc
.endm

.text

#define M5OP(name, func) m5op_func_nop M5OP_MERGE_TOKENS(name, _nop), func;
        M5OP_FOREACH
#undef M5OP
