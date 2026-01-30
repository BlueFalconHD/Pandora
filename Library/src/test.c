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

typedef struct proc_t__ *kproc_t;
typedef struct task_t__ *ktask_t;

int main(int argc, char *argv[]) {
  if (pd_init() == -1) {
    printf("Failed to initialize Pandora. Is the kernel extension loaded?\n");
    return 1;
  }

  uint64_t kernelBase = pd_get_kernel_base();

  if (kernelBase == 0) {
    printf("Failed to get kernel base address. Make sure SIP is disabled.\n");
    pd_deinit();
    return 1;
  }

  uint64_t ret0;

  pid_t tpid = getpid();

  if (argc > 1) {
    tpid = (pid_t)atoi(argv[1]);
    printf("using pid %d from cmdline\n", tpid);
  } else {
      printf("using self pid %d\n", tpid);
  }

#define FUNC_FIND_PROC kslide(0xFFFFFE0008DFEE7CULL)
#define FUNC_RELE_PROC kslide(0xFFFFFE0008DFF518ULL)

#define FUNC_GET_ROFLAGS kslide(0xFFFFFE00088C2C10ULL)
#define FUNC_SET_ROFLAGS_HARDEN_BIT kslide(0xFFFFFE00088C41C8ULL)

#define PROC_OFF_TO_RO_POINTER 0x18
#define TASK_OFF_TO_RO_POINTER 0x3e8
#define PROC_STRUCT_SIZE 0x7A0

  kproc_t target_kproc_obj;

  uint64_t args[1];
  args[0] = (uint64_t)(int64_t)tpid;

  pd_kcall_simple(FUNC_FIND_PROC, args, 1, (uint64_t *)&target_kproc_obj);
  printf("found process structure at 0x%llx\n", (unsigned long long)target_kproc_obj);

  uint64_t proc_struct_ro_ptr =
      pd_read64((uint64_t)target_kproc_obj + PROC_OFF_TO_RO_POINTER);
  printf("found proc ro struct ptr: 0x%llx\n",
         (unsigned long long)proc_struct_ro_ptr);

  ktask_t ttask = (ktask_t)((uint64_t)target_kproc_obj + PROC_STRUCT_SIZE);
  printf("found task structure at 0x%llx\n", (unsigned long long)ttask);

  // call proc_rele
  args[0] = (uint64_t)(int64_t)target_kproc_obj;
  pd_kcall_simple(FUNC_RELE_PROC, args, 1, NULL);
  printf("released process structure\n");

  pd_deinit();

  printf("deiniting pandora\n");
  return 0;
}
