#include "hexdump.h"

#include <stdio.h>

void hexdump(const void *data, size_t size, hexdump_opts_t opts) {
  const uint8_t *byte_data = (const uint8_t *)data;
  for (size_t i = 0; i < size; i += opts.bytes_per_line) {
    if (opts.show_offset) {
      printf("%08zx  ", i);
    }
    for (size_t j = 0; j < opts.bytes_per_line; j++) {
      if (j % opts.group_size == 0 && j != 0) {
        printf(" ");
      }
      if (i + j < size) {
        printf("%02x ", byte_data[i + j]);
      } else {
        printf("   ");
      }
    }
    if (opts.show_ascii) {
      printf(" |");
      for (size_t j = 0; j < opts.bytes_per_line; j++) {
        if (i + j < size) {
          uint8_t byte = byte_data[i + j];
          printf("%c", (byte >= 32 && byte <= 126) ? byte : '.');
        } else {
          printf(" ");
        }
      }
      printf("|");
    }
    printf("\n");
  }
}
