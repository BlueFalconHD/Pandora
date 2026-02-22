#include "hexdump.h"
#include "kernel/kernel_task.h"
#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <capstone.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <mach/error.h>

#include "kernel/kstructs.h"
#include "kernel/memdiff.h"
#include "kernel/proc_task.h"
#include "kernel/kernel_macho.h"

// for mach o parsing stuff
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "calypso/string_search.h"
#include "calypso/page_size.h"
#include "calypso/xref.h"

#define FUNC_PROC_FIND kslide(0xFFFFFE0008DFEE7CULL)
#define FUNC_PROC_RELE kslide(0xFFFFFE0008DFF518ULL)
#define FUNC_PROC_TASK kslide(0xFFFFFE0008E02274ULL)
#define FUNC_RO_FOR_PROC kslide(0xFFFFFE0008E010C0ULL)

#define PROC_STRUCT_SIZE_PTR kslide(0xFFFFFE000C551C30ULL)
#define PROC_STRUCT_SIZE_DEFAULT 0x7A0ULL
#define PROC_STRUCT_SIZE_MAX 0x10000ULL

#define PROC_OFF_RO_PTR 0x18ULL
#define PROC_OFF_FLAGS 0x468ULL
#define TASK_OFF_RO_PTR 0x3E8ULL

int main(int argc, char *argv[]) {
  if (pd_init() == -1) {
    printf("Failed to initialize Pandora. Is the kernel extension loaded?\n");
    return 1;
  }

  if (!pd_get_kernel_base()) {
    printf("Failed to get kernel base address. Make sure SIP is disabled.\n");
    pd_deinit();
    return 1;
  }

  kernel_macho_init(pd_kbase);
  kernel_proc_task_init();

  kernel_macho_print_segments();

  struct section_64 *sect = NULL;
  int findres = kernel_macho_find_section_by_name("__TEXT", "__cstring", &sect);
  if (findres != 0 || !sect) {
    printf("Failed to find __TEXT.__cstring section\n");
    goto end;
  }

  memdiff_view *cstring_view = memdiff_create(sect->addr, sect->size);
  if (!cstring_view) {
    printf("Failed to create memdiff view for __TEXT.__cstring section\n");
    goto end;
  }

  uint64_t tss_str = search_pattern((const uint8_t *)"com.apple.private.thread-set-state", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", cstring_view->original_copy, cstring_view->size, 1);
  if (!tss_str) {
    printf("Failed to find thread-set-state string in __TEXT.__cstring\n");
    memdiff_destroy(cstring_view);
    goto end;
  }
  printf("Found thread-set-state string at 0x%llx\n", (unsigned long long)tss_str + cstring_view->base_address);

  hexdump((const uint8_t *)(tss_str - 0x100), 0x200 + 34, default_hexdump_opts);

  // make a memdiff view of the text segment of kernel
  struct section_64 *text_sect = NULL;
  findres = kernel_macho_find_section_by_name("__TEXT_EXEC", "__text", &text_sect);
  if (findres != 0 || !text_sect) {
    printf("Failed to find __TEXT_EXEC.__text section\n");
    memdiff_destroy(cstring_view);
    goto end;
  }

  memdiff_view *text_view = memdiff_create(text_sect->addr, text_sect->size);
  if (!text_view) {
    printf("Failed to create memdiff view for __TEXT_EXEC.__text section\n");
    memdiff_destroy(cstring_view);
    goto end;
  }


  uint64_t *results = calloc(16, sizeof(uint64_t));
  int found = xref_find_adrp(text_view, tss_str + cstring_view->base_address, results, 16);


  printf("Found %u adrp instructions referencing thread-set-state string:\n", found);
  for (int i = 0; i < found; i++) {
    printf("  0x%llx\n", results[i]);
  }

  memset(results, 0, 16 * sizeof(uint64_t));
  found = xref_find_adrp(text_view, kunslide(tss_str + cstring_view->base_address), results, 16);

  printf("Found %u adrp instructions referencing unkslid thread-set-state:\n", found);
  for (int i = 0; i < found; i++) {
    printf("  0x%llx\n", results[i]);
  }

  printf("\noffsets from IDA (kslid with 0x%llx):\n", pd_kslide);
  printf("actual xref to target: 0x%llx\n", kslide(0xFFFFFE00088DCDFC));
  printf("actual target address: 0x%llx\n", kslide(0xFFFFFE000705047A));


end:
  pd_deinit();
  return 0;
}
