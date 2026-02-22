#ifndef CALYPSO_STRING_SEARCH_H
#define CALYPSO_STRING_SEARCH_H

#include <stddef.h>
#include <stdint.h>

// Search for a byte pattern in a memory region, returning the address of the first match or 0 if not found.
// The pattern is specified as a byte array alongside a mask string. In the mask string:
//   - 'x' means the corresponding byte in the pattern must match exactly.
//   - '?' means the corresponding byte in the pattern is a wildcard and can match any value
//   -     any other character in the mask is invalid and will cause the function to return 0.
// Stride specifies the offset step for searching. A stride of 0 defaults to the pattern length.
uint64_t search_pattern(const uint8_t *pattern, const char *mask, const uint8_t *base, size_t size, size_t stride);

#endif
