

#include "kernel_macho.h"
#include "ptr_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int kernel_macho_once = 0;

memdiff_view *kernel_macho_header_view;
memdiff_view *kernel_macho_cmds_view;
memdiff_view *kernel_macho_symbols_nlist_view;
memdiff_view *kernel_macho_symbols_strtable_view;

struct segment_command_64 **kernel_macho_segments;
size_t kernel_macho_segment_count;


int kernel_macho_init(uint64_t kbase) {
  if (kernel_macho_once) {
    printf("kernel_macho_init: already initialized\n");
    return -1;
  }

  if (kbase == 0) {
    goto fail;
  }

  if (!ptr_is_kernel(kbase)) {
    printf("kernel_macho_init: kbase is not a kernel pointer: 0x%llx\n", (unsigned long long)kbase);
    goto fail;
  }

  kernel_macho_header_view = memdiff_create(kbase, sizeof(struct mach_header_64));
  if (!kernel_macho_header_view) {
    printf("kernel_macho_init: failed to create memdiff view for kernel mach-o header\n");
    goto fail;
  }

  if (kernel_macho_header->magic != MH_MAGIC_64) {
    printf("kernel_macho_init: invalid mach-o magic: 0x%08x\n", kernel_macho_header->magic);
    goto fail;
  }

  kernel_macho_cmds_view = memdiff_create(kbase + sizeof(struct mach_header_64), kernel_macho_header->sizeofcmds);
  if (!kernel_macho_cmds_view) {
    printf("kernel_macho_init: failed to create memdiff view for kernel mach-o load commands\n");
    goto fail;
  }

  struct load_command *load_cmd = kernel_macho_cmds;

  // first pass - count segments to know how much memory to allocate for segments array
  kernel_macho_segment_count = 0;
  load_cmd = kernel_macho_cmds;
  for (uint32_t i = 0; i < kernel_macho_header->ncmds; i++) {
    if (load_cmd->cmd == LC_SEGMENT_64) {
      kernel_macho_segment_count++;
    }
    load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
  }

  kernel_macho_segments = calloc(kernel_macho_segment_count, sizeof(*kernel_macho_segments));
  if (!kernel_macho_segments) {
    printf("kernel_macho_init: failed to allocate segments array\n");
    goto fail;
  }

  // second pass - create segments array
  int seg_index = 0;
  load_cmd = kernel_macho_cmds;
  for (uint32_t i = 0; i < kernel_macho_header->ncmds; i++) {
    if (load_cmd->cmd == LC_SEGMENT_64) {
      struct segment_command_64 *seg = (struct segment_command_64 *)load_cmd;
      kernel_macho_segments[seg_index] = seg;
      seg_index++;
    }
    load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
  }

  // third pass - find symtab
  load_cmd = kernel_macho_cmds;
  for (uint32_t i = 0; i < kernel_macho_header->ncmds; i++) {
    if (load_cmd->cmd == LC_SYMTAB) {
      struct symtab_command *symtab = (struct symtab_command *)load_cmd;
      uint64_t sym_vmaddr = kernel_macho_fileoff_to_vmaddr(symtab->symoff);
      uint64_t str_vmaddr = kernel_macho_fileoff_to_vmaddr(symtab->stroff);
      if (sym_vmaddr == 0 || str_vmaddr == 0) {
        printf("kernel_macho_init: failed to translate symtab file offsets: symoff=0x%llx stroff=0x%llx\n",
               (unsigned long long)symtab->symoff, (unsigned long long)symtab->stroff);
        goto fail;
      }
      kernel_macho_symbols_nlist_view = memdiff_create(sym_vmaddr, (size_t)symtab->nsyms * sizeof(struct nlist_64));
      kernel_macho_symbols_strtable_view = memdiff_create(str_vmaddr, (size_t)symtab->strsize);
      if (!kernel_macho_symbols_nlist_view || !kernel_macho_symbols_strtable_view) {
        printf("kernel_macho_init: failed to create memdiff views for symbol table\n");
        goto fail;
      }
      break;
    }
    load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
  }

  kernel_macho_once = 1;
  return 0;

fail:

  kernel_macho_deinit();
  return -1;

}

uint64_t kernel_macho_deinit() {
  if (kernel_macho_header_view) {
    memdiff_destroy(kernel_macho_header_view);
    kernel_macho_header_view = NULL;
  }
  if (kernel_macho_cmds_view) {
    memdiff_destroy(kernel_macho_cmds_view);
    kernel_macho_cmds_view = NULL;
  }
  if (kernel_macho_symbols_nlist_view) {
    memdiff_destroy(kernel_macho_symbols_nlist_view);
    kernel_macho_symbols_nlist_view = NULL;
  }
  if (kernel_macho_symbols_strtable_view) {
    memdiff_destroy(kernel_macho_symbols_strtable_view);
    kernel_macho_symbols_strtable_view = NULL;
  }
  if (kernel_macho_segments) {
    free(kernel_macho_segments);
    kernel_macho_segments = NULL;
    kernel_macho_segment_count = 0;
  }

  kernel_macho_once = 0;
  return 0;
}

uint64_t kernel_macho_fileoff_to_vmaddr(uint64_t fileoff) {
  for (size_t i = 0; i < kernel_macho_segment_count; i++) {
    struct segment_command_64 *seg = kernel_macho_segments[i];
    if (fileoff >= seg->fileoff && fileoff < seg->fileoff + seg->filesize) {
      return seg->vmaddr + (fileoff - seg->fileoff);
    }
  }

  printf("kernel_macho_fileoff_to_vmaddr: failed to translate file offset 0x%llx\n", (unsigned long long)fileoff);
  return 0;
}

uint64_t kernel_macho_find_symbol(const char *symbol_name) {
  if (!symbol_name || !kernel_macho_symbols_nlist_view || !kernel_macho_symbols_strtable_view) {
    printf("kernel_macho_find_symbol: invalid arguments or symbol table not initialized\n");
    return -1;
  }

  struct nlist_64 *nlist = kernel_macho_symbols_nlist;
  char *strtable = kernel_macho_symbols_strtable;

  for (size_t i = 0; i < kernel_macho_symbols_nlist_view->size / sizeof(struct nlist_64); i++) {
    if (strcmp(strtable + nlist[i].n_un.n_strx, symbol_name) == 0) {
      return nlist[i].n_value;
    }
  }

  printf("kernel_macho_find_symbol: symbol not found: %s\n", symbol_name);
  return -1;
}

uint64_t kernel_macho_find_symbol_or_die(const char *symbol_name) {
  uint64_t addr = kernel_macho_find_symbol(symbol_name);
  if (addr == (uint64_t)-1) {
    printf("kernel_macho_find_symbol_or_die: symbol not found: %s\n", symbol_name);
    exit(1);
  }
  return addr;
}

uint64_t kernel_macho_find_symbol_partial(const char *needle) {
  if (!needle || !kernel_macho_symbols_nlist_view || !kernel_macho_symbols_strtable_view) {
    printf("kernel_macho_find_symbol_partial: invalid arguments or symbol table not initialized\n");
    return -1;
  }

  struct nlist_64 *nlist = kernel_macho_symbols_nlist;
  char *strtable = kernel_macho_symbols_strtable;

  for (size_t i = 0; i < kernel_macho_symbols_nlist_view->size / sizeof(struct nlist_64); i++) {
    if (strstr(strtable + nlist[i].n_un.n_strx, needle) != NULL) {
      return nlist[i].n_value;
    }
  }

  printf("kernel_macho_find_symbol_partial: symbol not found with needle: %s\n", needle);
  return -1;
}

int kernel_macho_print_segments() {
  if (!kernel_macho_segments) {
    printf("kernel_macho_print_segments: segments not initialized\n");
    return -1;
  }

  for (size_t i = 0; i < kernel_macho_segment_count; i++) {
    struct segment_command_64 *seg = kernel_macho_segments[i];
    printf("Segment %zu: name=%s vmaddr=0x%llx vmsize=0x%llx fileoff=0x%llx filesize=0x%llx\n",
           i, seg->segname, (unsigned long long)seg->vmaddr, (unsigned long long)seg->vmsize,
           (unsigned long long)seg->fileoff, (unsigned long long)seg->filesize);

    // get sections
    struct section_64 *sections = (struct section_64 *)((uint8_t *)seg + sizeof(struct segment_command_64));
    for (uint32_t j = 0; j < seg->nsects; j++) {
      struct section_64 *sect = &sections[j];
      printf("  Section %u: name=%s.%s addr=0x%llx size=0x%llx offset=0x%llx\n", j, sect->segname, sect->sectname, (uint64_t)sect->addr, (uint64_t)sect->size, (uint64_t)sect->offset);
    }
  }

  return 0;
}

int kernel_macho_find_segment_by_name(const char *segname, struct segment_command_64 **out_seg) {
  if (!segname || !out_seg || !kernel_macho_segments) {
    printf("kernel_macho_find_segment_by_name: invalid arguments or segments not initialized\n");
    return -1;
  }

  for (size_t i = 0; i < kernel_macho_segment_count; i++) {
    struct segment_command_64 *seg = kernel_macho_segments[i];
    if (strcmp(seg->segname, segname) == 0) {
      *out_seg = seg;
      return 0;
    }
  }

  printf("kernel_macho_find_segment_by_name: segment not found with name: %s\n", segname);
  return -1;
}

int kernel_macho_find_section_by_name(const char *segname, const char *sectname, struct section_64 **out_sect) {
  if (!segname || !sectname || !out_sect || !kernel_macho_segments) {
    printf("kernel_macho_find_section_by_name: invalid arguments or segments not initialized\n");
    return -1;
  }

  for (size_t i = 0; i < kernel_macho_segment_count; i++) {
    struct segment_command_64 *seg = kernel_macho_segments[i];
    if (strcmp(seg->segname, segname) == 0) {
      struct section_64 *sections = (struct section_64 *)((uint8_t *)seg + sizeof(struct segment_command_64));
      for (uint32_t j = 0; j < seg->nsects; j++) {
        struct section_64 *sect = &sections[j];
        if (strcmp(sect->sectname, sectname) == 0) {
          *out_sect = sect;
          return 0;
        }
      }
    }
  }

  printf("kernel_macho_find_section_by_name: section not found with name: %s.%s\n", segname, sectname);
  return -1;
}
