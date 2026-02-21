#include <stdint.h>
#include <stdbool.h>

bool ptr_is_kernel(uint64_t ptr) {
  // On 64-bit ARM, kernel pointers typically have the top byte set to 0xFF.
  // This is not a guarantee but is a common convention in many iOS versions.
  return (ptr & 0xFF00000000000000ULL) == 0xFF00000000000000ULL;
}
