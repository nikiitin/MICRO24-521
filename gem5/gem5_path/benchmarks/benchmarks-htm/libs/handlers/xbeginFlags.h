#ifndef __XBEGIN_LIB_FLAGS_H__
#define __XBEGIN_LIB_FLAGS_H__
    enum xbeginFlags {
        POWER_TM_FLAG = 0x00000001,
        // Maximum 16 bits for flags, the
        // rest of the register is used for the tag
        // check htm_start
        LAST_FLAG = 0x0000FFFF
    };
#endif  // XBEGIN_LIB_FLAGS