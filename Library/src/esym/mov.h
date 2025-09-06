#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline uint32_t encode_movx(unsigned d, unsigned n) {
  assert(d < 31 && n < 31);
  return 0xAA0003E0u | (n << 16) | d;
}

static inline uint32_t encode_movw(unsigned d, unsigned n) {
  assert(d < 31 && n < 31);
  return 0x2A0003E0u | (n << 16) | d;
}

static inline uint32_t encode_movz_x(unsigned d, unsigned imm16,
                                     unsigned shift) {
  assert(d < 31);
  assert(imm16 <= 0xFFFF);
  assert(shift == 0 || shift == 16 || shift == 32 || shift == 48);
  unsigned hw = shift / 16; // 0..3
  return 0xD2800000u | (hw << 21) | (imm16 << 5) | d;
}

static inline uint32_t encode_movz_w(unsigned d, unsigned imm16,
                                     unsigned shift) {
  assert(d < 31);
  assert(imm16 <= 0xFFFF);
  assert(shift == 0 || shift == 16);
  unsigned hw = shift / 16; // 0..1
  return 0x52800000u | (hw << 21) | (imm16 << 5) | d;
}
