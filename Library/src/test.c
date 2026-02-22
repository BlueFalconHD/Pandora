#include "calypso/function.h"
#include "hexdump.h"
#include "kernel/kernel_task.h"
#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <capstone.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <stddef.h>
#include <stdint.h>
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

#include "patches/thread_set_state/tss.h"

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

  uint8_t *tss_ptr = (uint8_t *)(uintptr_t)search_pattern(
      (const uint8_t *)"com.apple.private.thread-set-state",
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
      cstring_view->original_copy,
      cstring_view->size,
      1);
  if (!tss_ptr) {
    printf("Failed to find thread-set-state string in __TEXT.__cstring\n");
    memdiff_destroy(cstring_view);
    goto end;
  }

  const uint8_t *cstring_base = (const uint8_t *)cstring_view->original_copy;
  const uint8_t *cstring_end = cstring_base + cstring_view->size;
  if (tss_ptr < cstring_base || tss_ptr >= cstring_end) {
    printf("Found thread-set-state pointer is out of view range: tss_ptr=0x%llx view=[0x%llx..0x%llx)\n",
           (unsigned long long)(uintptr_t)tss_ptr,
           (unsigned long long)(uintptr_t)cstring_base,
           (unsigned long long)(uintptr_t)cstring_end);
    memdiff_destroy(cstring_view);
    goto end;
  }

  size_t tss_off = (size_t)(tss_ptr - cstring_base);
  uint64_t tss_kva = cstring_view->base_address + (uint64_t)tss_off;

  printf("'com.apple.private.thread-set-state' @ 0x%llx\n", (unsigned long long)tss_kva);

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
  int found = xref_find_adrp_add(text_view, tss_kva, results, 16);

  printf("%d xrefs to 'com.apple.private.thread-set-state'\n", found);

  if (found <= 0) {
    printf("Failed to find any references to thread-set-state string\n");
    free(results);
    memdiff_destroy(text_view);
    memdiff_destroy(cstring_view);
    goto end;
  }

  uint64_t fstart = find_function_start_bti(text_view, results[0]);

  for (int i = 0; i < found; i++) {
    if (find_function_start_bti(text_view, results[i]) != fstart) {
      printf("Reference at 0x%llx is in a different function than the first reference at 0x%llx\n", results[i], results[0]);
      goto end;
    }
  }

  printf("thread_set_state_internal @ 0x%llx\n", (unsigned long long)fstart);

end:
  pd_deinit();
  return 0;
}
