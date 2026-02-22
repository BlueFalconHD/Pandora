#include "string_search.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Search for a byte pattern in a memory region, returning the address of the first match or 0 if not found.
// The pattern is specified as a byte array alongside a mask string. In the mask string:
//   - 'x' means the corresponding byte in the pattern must match exactly.
//   - '?' means the corresponding byte in the pattern is a wildcard and can match any value
//   -     any other character in the mask is invalid and will cause the function to return 0.
// Stride specifies the offset step for searching. A stride of 0 defaults to the pattern length.
uint64_t search_pattern(const uint8_t *pattern, const char *mask, const uint8_t *base, size_t size, size_t stride) {
  if (!pattern || !mask || !base || !size)
      return 0;

  size_t pat_len = 0;
  size_t req_count = 0;
  size_t first_req = (size_t)-1;
  for (;; ++pat_len) {
      char m = mask[pat_len];
      if (m == '\0')
          break;
      if (m != 'x' && m != '?')
          return 0;
      if (m == 'x') {
          if (first_req == (size_t)-1)
              first_req = pat_len;
          ++req_count;
      }
  }

  if (pat_len == 0 || pat_len > size)
      return 0;

  if (stride == 0)
      stride = pat_len;

  if (req_count == 0) {
      return (uint64_t)(uintptr_t)base;
  }

  const uint8_t anchor_byte = pattern[first_req];
  const bool no_wildcards = (req_count == pat_len);

  size_t req_idx[64];
  size_t req_idx_count = 0;
  const bool use_req_idx = (!no_wildcards && (req_count - 1) <= (sizeof(req_idx) / sizeof(req_idx[0])));
  if (use_req_idx) {
      for (size_t i = 0; i < pat_len; ++i) {
          if (mask[i] == 'x' && i != first_req)
              req_idx[req_idx_count++] = i;
      }
  }

  const size_t max_off = size - pat_len;

  for (size_t off = 0; off <= max_off; off += stride) {
      const uint8_t *cand = base + off;

      if (cand[first_req] != anchor_byte)
          continue;

      if (no_wildcards) {
          if (memcmp(cand, pattern, pat_len) == 0)
              return (uint64_t)(uintptr_t)cand;
          continue;
      }

      bool match = true;

      if (use_req_idx) {
          for (size_t k = 0; k < req_idx_count; ++k) {
              size_t i = req_idx[k];
              if (cand[i] != pattern[i]) {
                  match = false;
                  break;
              }
          }
      } else {
          size_t i = 0;

#if defined(__clang__) || defined(__GNUC__)
#pragma clang loop unroll(full)
#endif
          for (; i + 4 <= pat_len; i += 4) {
              char m0 = mask[i + 0];
              char m1 = mask[i + 1];
              char m2 = mask[i + 2];
              char m3 = mask[i + 3];

              if ((m0 == 'x' && cand[i + 0] != pattern[i + 0]) |
                  (m1 == 'x' && cand[i + 1] != pattern[i + 1]) |
                  (m2 == 'x' && cand[i + 2] != pattern[i + 2]) |
                  (m3 == 'x' && cand[i + 3] != pattern[i + 3])) {
                  match = false;
                  break;
              }
          }

          if (match) {
              for (; i < pat_len; ++i) {
                  if (mask[i] == 'x' && cand[i] != pattern[i]) {
                      match = false;
                      break;
                  }
              }
          }
      }

      if (match)
          return (uint64_t)(uintptr_t)cand;
  }

  return 0;
}
