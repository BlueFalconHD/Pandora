#pragma once

#include <assert.h>
#include <stdint.h>

static inline uint32_t encode_ret_lr(void) { return 0xD65F03C0u; }

static inline uint32_t encode_ret(unsigned n) {
  assert(n < 31);
  return 0xD65F0000u | (n << 5);
}
