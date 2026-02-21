#include "memdiff.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

// kernel memory sections

extern memdiff_view *kernel_macho_header_view;
extern memdiff_view *kernel_macho_cmds_view;
extern memdiff_view *kernel_macho_symbols_nlist_view;
extern memdiff_view *kernel_macho_symbols_strtable_view;

#define kernel_macho_header ((struct mach_header_64 *)kernel_macho_header_view->original_copy)
#define kernel_macho_cmds ((struct load_command *)kernel_macho_cmds_view->original_copy)
#define kernel_macho_symbols_nlist ((struct nlist_64 *)kernel_macho_symbols_nlist_view->original_copy)
#define kernel_macho_symbols_strtable ((char *)kernel_macho_symbols_strtable_view->original_copy)

extern int kernel_macho_once;

// user managed data

extern struct segment_command_64 *kernel_macho_segments;
extern size_t kernel_macho_segment_count;

// functions

int kernel_macho_init(uint64_t kbase);
uint64_t kernel_macho_deinit();
uint64_t kernel_macho_fileoff_to_vmaddr(uint64_t fileoff);
uint64_t kernel_macho_find_symbol(const char *symbol_name);
