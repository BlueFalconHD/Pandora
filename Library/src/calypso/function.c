#include "function.h"
#include "esym/b.h"
#include <stdbool.h>
#include <string.h>

static inline bool is_bti(uint32_t instr) {
  return (instr == encode_bti_none() || instr == encode_bti_c() || instr == encode_bti_j() || instr == encode_bti_jc());
}

uint64_t find_function_start_bti(memdiff_view *view, uint64_t func_ka) {
  if (!view || !view->original_copy) {
    return 0;
  }

  if ((view->base_address & 0x3u) != 0) {
    return 0;
  }

  if (func_ka < view->base_address || func_ka >= view->base_address + view->size) {
    return 0;
  }

  size_t len = view->size & ~(size_t)0x3;
  if (len < sizeof(uint32_t)) {
    return 0;
  }

  uint64_t addr = func_ka & ~0x3ULL;
  uint64_t max_addr = view->base_address + len;
  if (addr >= max_addr) {
    addr = max_addr - sizeof(uint32_t);
  }

  for (;;) {
    size_t offset = (size_t)(addr - view->base_address);
    uint32_t instr;
    memcpy(&instr, view->original_copy + offset, sizeof(instr));

    if (is_bti(instr)) {
      return addr;
    }

    if (addr < view->base_address + sizeof(uint32_t)) {
      break;
    }
    addr -= sizeof(uint32_t);
  }

  return 0;
}
