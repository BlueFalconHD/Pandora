#include "kernel_task.h"
#include "kernel_macho.h"
#include "pandora.h"
#include "memdiff.h"
#include "proc_task.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

memdiff_view *kernel_proc_view;
memdiff_view *kernel_task_view;

int kernel_proc_task_init() {
  if (!kernel_macho_once) {
    printf("kernel_proc_task_init: kernel macho not initialized\nrun kernel_macho_init() first");
    goto fail;
  }

  uint64_t kernproc_addr = kernel_macho_find_symbol("_kernproc");
  if (kernproc_addr == 0) {
    printf("kernel_proc_task_init: failed to find _kernproc symbol in kernel macho\nMake sure kernel_macho_init() succeeded and the symbol table is correct");
    goto fail;
  }

  // read the actual address from the variable qword
  uint64_t kernproc = pd_read64(kernproc_addr);

  kernel_proc_view = memdiff_create(kernproc, sizeof(struct ks_proc));
  if (!kernel_proc_view) {
    printf("kernel_proc_task_init: failed to create memdiff view for kernproc\n");
    goto fail;
  }

  // we use an unchecked version here because the ro_data doesn't point back to the proc struct it is pointed to by
  //
  kernel_task_view = procv_to_taskv(kernel_proc_view);
  if (!kernel_task_view) {
    printf("kernel_proc_task_init: failed to create memdiff view for kernproc task\n");
    goto fail;
  }

  return 0;

fail:
  kernel_proc_task_deinit();
  return -1;
}

int kernel_proc_task_deinit() {
  if (kernel_proc_view) {
    memdiff_destroy(kernel_proc_view);
    kernel_proc_view = NULL;
  }
  if (kernel_task_view) {
    memdiff_destroy(kernel_task_view);
    kernel_task_view = NULL;
  }
  return 0;
}
