#include "hexdump.h"
#include "kernel/kernel_task.h"
#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <capstone.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <mach/error.h>

#include "proc_ro.h"

#include "kernel/kstructs.h"
#include "kernel/memdiff.h"
#include "kernel/proc_task.h"
#include "kernel/kernel_macho.h"

// for mach o parsing stuff
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

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

  printf("%s", kernel_proc->p_forkcopy.p_comm);

end:
  pd_deinit();
  return 0;
}
