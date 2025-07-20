#ifndef ROUTINES_H
#define ROUTINES_H

#include <stdint.h>

extern "C" uint64_t arm_kvtophys(uint64_t va);

extern "C" uint64_t arbitrary_call(uint64_t func, ...);
#endif // ROUTINES_H
