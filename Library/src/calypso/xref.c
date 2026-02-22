#include "xref.h"
#include "kernel/memdiff.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static inline int32_t sign_extend_21_to_32(uint32_t val21) {
  val21 &= 0x1fffff; // 21 bit mask
  return (int32_t)((int32_t)(val21 << 11) >> 11);
}

static inline bool insn_is_adrp(uint32_t insn) {
  return (insn & 0x9F000000u) == 0x90000000u;
}

static inline uint32_t adrp_extract_imm21(uint32_t insn) {
  uint32_t immlo = (insn >> 29) & 0x3;
  uint32_t immhi = (insn >> 5) & 0x7FFFF;
  uint32_t imm21 = (immhi << 2) | immlo;
  return sign_extend_21_to_32(imm21);
}

static inline uint8_t insn_extract_rd(uint32_t insn) {
  return (uint8_t)(insn & 0x1Fu);
}

static inline bool insn_is_add_imm(uint32_t insn) {
  return (insn & 0x7F000000u) == 0x11000000u;
}

// Searches for ADRP instructions in the given memory view that compute the page address of target_addr.
// Returns the number of matches found, and if results is non-NULL, fills in up to max_results addresses of matching instructions.
// The addresses returned in results are the virtual addresses of the ADRP instructions that reference the target page, not the offsets within the view.
int xref_find_adrp(memdiff_view *view, uint64_t target_addr, uint64_t *results, size_t max_results) {
  if (!view || !view->original_copy) {
    return -1;
  }

  if ((view->base_address & 0x3u) != 0) {
    return -1;
  }

  const uint8_t *buf = view->original_copy;
  size_t len = view->size & ~(size_t)0x3;
  uint64_t target_page_4k = target_addr & ~0xFFFULL;

  int count = 0;
  for (size_t offset = 0; offset + sizeof(uint32_t) <= len; offset += sizeof(uint32_t)) {
    uint32_t insn;
    memcpy(&insn, buf + offset, sizeof(insn));

    if (!insn_is_adrp(insn)) {
      continue;
    }

    int64_t imm_signed = (int64_t)(int32_t)adrp_extract_imm21(insn);
    uint64_t insn_addr = view->base_address + offset;
    uint64_t pc_page_4k = insn_addr & ~0xFFFULL;
    uint64_t computed = (uint64_t)((int64_t)pc_page_4k + (imm_signed << 12));

    if (computed == target_page_4k) {
      if (results && (size_t)count < max_results) {
        results[count] = insn_addr;
      }
      count++;
    }
  }

  return count;
}

// Searches for ADRP+add instruction pairs in the given memory view which compute
// the full address of target_addr. Returns the number of matches found, and if results is non-NULL, fills in up to max_results addresses of matching ADRP instructions.
// The addresses returned in results are the virtual addresses of the ADRP instructions that reference the target address, not the offsets within the view.
int xref_find_adrp_add(memdiff_view *view, uint64_t target_addr, uint64_t *results, size_t max_results) {
  if (!view || !view->original_copy) {
    return -1;
  }

  if ((view->base_address & 0x3u) != 0) {
    return -1;
  }

  const uint8_t *buf = view->original_copy;
  size_t len = view->size & ~(size_t)0x3;

  int count = 0;
  for (size_t offset = 0; offset + (2 * sizeof(uint32_t)) <= len; offset += sizeof(uint32_t)) {
    uint32_t adrp_insn;
    memcpy(&adrp_insn, buf + offset, sizeof(adrp_insn));

    if (!insn_is_adrp(adrp_insn)) {
      continue;
    }

    uint32_t add_insn;
    memcpy(&add_insn, buf + offset + sizeof(uint32_t), sizeof(add_insn));

    if (!insn_is_add_imm(add_insn)) {
      continue;
    }
    if (((add_insn >> 31) & 0x1u) == 0) {
      continue;
    }

    uint32_t add_shift = (add_insn >> 22) & 0x3u;
    if (add_shift > 1) {
      continue;
    }

    uint8_t add_rd = (uint8_t)(add_insn & 0x1Fu);
    uint8_t add_rn = (uint8_t)((add_insn >> 5) & 0x1Fu);
    uint64_t add_imm = (uint64_t)((add_insn >> 10) & 0xFFFu);
    if (add_shift == 1) {
      add_imm <<= 12;
    }

    uint8_t adrp_rd = insn_extract_rd(adrp_insn);
    if (add_rd != adrp_rd || add_rn != adrp_rd) {
      continue;
    }

    int64_t imm_signed = (int64_t)(int32_t)adrp_extract_imm21(adrp_insn);
    uint64_t adrp_addr = view->base_address + offset;
    uint64_t pc_page_4k = adrp_addr & ~0xFFFULL;
    uint64_t page_base = (uint64_t)((int64_t)pc_page_4k + (imm_signed << 12));
    uint64_t computed = page_base + add_imm;

    if (computed == target_addr) {
      if (results && (size_t)count < max_results) {
        results[count] = adrp_addr;
      }
      count++;
    }
  }

  return count;
}
