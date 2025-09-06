#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  bool show_ascii;
  bool show_offset;
  int bytes_per_line;
  int group_size;
} hexdump_opts_t;

void hexdump(const void *data, size_t size, hexdump_opts_t opts);
