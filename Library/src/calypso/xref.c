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
  return (insn & 0xE0000000u) == 0x80000000u;
}

static inline uint32_t adrp_extract_imm21(uint32_t insn) {
  uint32_t immlo = (insn >> 29) & 0x3;
  uint32_t immhi = (insn >> 5) & 0x7FFFF;
  uint32_t imm21 = (immhi << 2) | immlo;
  return sign_extend_21_to_32(imm21);
}

// here, the instruction actually lives at view->base_address + offset where the offset is
// the distance from the start of the view to the instruction.
//
// 1) validate that view->base_address is 4 byte aligned (instructions are 4 byte aligned)
//    1a) ensure the view length is a multiple of 4 (or clamp iteration to len & ~3)
// 2) mask out target address bits which represent sub-4KB offset (target_page_4k = target_addr & ~0xFFF)
// 3) iterate through the view in 4 byte increments looking for uint32 with the adrp opcode
//    3a) for each u32 with adrp opcode, extract the immlo and immhi bits to build the imm21
//    3b) reconstruct the immediate value by sign-extending imm21 and shifting left by 12
//    3c) compute insn_addr = view->base_address + offset, then pc_page_4k = insn_addr & ~0xFFF
//    3d) computed = pc_page_4k + (imm_signed << 12)
//    3e) if the computed page matches the target page
//        3x) for 4KB: computed == target_page_4k
//        3y) if we have room in the results array, store the address of the matching adrp instruction in the results array
//        3z) increment the count of matches found
//    3f) continue iterating through the view to find more matches until the end of the view is reached
// 4) return the count of matches found
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

  printf("xref_find_adrp: searching for adrp instructions referencing target address 0x%llx (target page 0x%llx) in view with base address 0x%llx and size 0x%zx\n",
         (unsigned long long)target_addr, (unsigned long long)target_page_4k, (unsigned long long)view->base_address, view->size);

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
