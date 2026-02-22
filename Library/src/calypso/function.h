#ifndef CALYPSO_FUNCTION_H
#define CALYPSO_FUNCTION_H

#include "kernel/memdiff.h"
#include <stdint.h>

uint64_t find_function_start_bti(memdiff_view *view, uint64_t func_ka);

#endif
