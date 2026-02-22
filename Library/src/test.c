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

  printf("%s\n", kernel_proc->p_forkcopy.p_comm);

  memdiff_view *proc_view = kernel_proc_view;
  ks_proc_t proc_v = (ks_proc_t)proc_view->original_copy;
  uintptr_t proc_addr = proc_view->base_address;

  for (;;) {
    uintptr_t prev_link = (uintptr_t)proc_v->p_list.le_prev;
    if (prev_link == 0) {
      break;
    }

    uintptr_t prev_addr = prev_link - offsetof(struct ks_proc, p_list.le_next);
    if (prev_addr == 0 || prev_addr == proc_addr) {
      break;
    }

    memdiff_view *prev_view = memdiff_create(prev_addr, sizeof(struct ks_proc));
    if (!prev_view) {
      printf("Failed to create memdiff view for proc at 0x%llx\n",
             (unsigned long long)prev_addr);
      break;
    }

    ks_proc_t prev_v = (ks_proc_t)prev_view->original_copy;
    if ((uintptr_t)prev_v->p_list.le_next != proc_addr) {
      memdiff_destroy(prev_view);
      break;
    }

    // printf("prev: %s (%i)\n", prev_v->p_forkcopy.p_comm, prev_v->p_pid);

    if (proc_view != kernel_proc_view) {
      memdiff_destroy(proc_view);
    }
    proc_view = prev_view;
    proc_v = prev_v;
    proc_addr = prev_addr;
  }

  if (proc_view != kernel_proc_view) {
    memdiff_destroy(proc_view);
  }

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

  get_page_size();
  printf("pagesize: %llx\n", page_size);


end:
  pd_deinit();
  return 0;
}
