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

// default options: show offsets, 16 bytes per line, group by 4
static const hexdump_opts_t default_hexdump_opts = {
    .show_ascii = true,
    .show_offset = true,
    .bytes_per_line = 16,
    .group_size = 4,
};

void hexdump(const void *data, size_t size, hexdump_opts_t opts);
