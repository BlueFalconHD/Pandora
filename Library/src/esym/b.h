#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <assert.h>

static inline uint32_t encode_b(int64_t byte_off) {
  assert((byte_off & 3) == 0);
  int64_t imm26 = byte_off / 4; // signed
  assert(imm26 >= -(1LL << 25) && imm26 <= ((1LL << 25) - 1));
  return 0x14000000u | ((uint32_t)imm26 & 0x03FFFFFFu);
}

static inline uint32_t encode_bl(int64_t byte_off) {
  assert((byte_off & 3) == 0);
  int64_t imm26 = byte_off / 4; // signed
  assert(imm26 >= -(1LL << 25) && imm26 <= ((1LL << 25) - 1));
  return 0x94000000u | ((uint32_t)imm26 & 0x03FFFFFFu);
}

static inline uint32_t encode_br(unsigned reg) {
  assert(reg < 31);
  return 0xD61F0000u | (reg << 5);
}

static inline uint32_t encode_blr(unsigned reg) {
  assert(reg < 31);
  return 0xD63F0000u | (reg << 5);
}

static inline uint32_t encode_bti_none(void) { return 0xD503241Fu; } // bti
static inline uint32_t encode_bti_c(void) { return 0xD503245Fu; }    // bti c
static inline uint32_t encode_bti_j(void) { return 0xD503249Fu; }    // bti j
static inline uint32_t encode_bti_jc(void) { return 0xD50324DFu; }   // bti jc

static inline uint32_t encode_bti(bool j, bool c) { // j,c ∈ {0,1}
  return 0xD503241Fu | (j ? 0x80u : 0) | (c ? 0x40u : 0);
}

static inline uint32_t encode_b_to(uint64_t pc, uint64_t target) {
  return encode_b((int64_t)target - (int64_t)pc);
}
static inline uint32_t encode_bl_to(uint64_t pc, uint64_t target) {
  return encode_bl((int64_t)target - (int64_t)pc);
}
