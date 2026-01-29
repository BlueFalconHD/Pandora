#ifndef ROUTINES_H
#define ROUTINES_H

#include <stdint.h>

extern "C" uint64_t arm_kvtophys(uint64_t va);

extern "C" uint64_t arbitrary_call(uint64_t func, uint64_t arg0, uint64_t arg1,
                                   uint64_t arg2, uint64_t arg3, uint64_t arg4,
                                   uint64_t arg5, uint64_t arg6, uint64_t arg7,
                                   uint64_t arg8, uint64_t arg9);
#endif // ROUTINES_H
