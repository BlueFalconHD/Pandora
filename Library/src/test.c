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

void dump_ro_info(uint64_t addr) {
    uint8_t buf[sizeof(proc_ro_0_t)];
    int err = pd_readbuf(addr, buf, sizeof(buf));
    if (err != 0) {
      printf("pd_readbuf(0x%llx) failed: %d\n", (unsigned long long)addr, err);
      return;
    }

    proc_ro_0_t *proc_ro = (proc_ro_0_t *)&buf;
    char t_flags_desc[512];
    char cs_flags_desc[1024];
    printf("proc_ro ro_flags: 0x%x (%s)\n",
           proc_ro->task_data.t_flags_ro,
           proc_ro_t_flags_ro_description(proc_ro->task_data.t_flags_ro,
                                          t_flags_desc,
                                          sizeof(t_flags_desc)));
    printf("proc_ro cs_flags: 0x%x (%s)\n",
           proc_ro->proc_data.p_csflags,
           proc_ro_p_csflags_description(proc_ro->proc_data.p_csflags,
                                         cs_flags_desc,
                                         sizeof(cs_flags_desc)));

}

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

  pid_t target_pid = getpid();
  if (argc < 2) {
    printf("using self pid %d\n", target_pid);
  } else {
    target_pid = (pid_t)atoi(argv[1]);
    printf("using pid %d from cmdline\n", target_pid);
  }

  uint64_t kcall_args[1] = {0};

  uint64_t kproc = 0;
  kcall_args[0] = (uint64_t)(int64_t)target_pid;
  (void)pd_kcall_simple(FUNC_PROC_FIND, kcall_args, 1, &kproc);
  if (!kproc) {
    printf("proc_find returned NULL for pid %d\n", target_pid);
    pd_deinit();
    return 1;
  }

  uint64_t proc_struct_size = pd_read64(PROC_STRUCT_SIZE_PTR);
  if (!proc_struct_size) {
    proc_struct_size = PROC_STRUCT_SIZE_DEFAULT;
  }
  if (proc_struct_size > PROC_STRUCT_SIZE_MAX) {
    printf("unexpected proc struct size: 0x%llx\n",
           (unsigned long long)proc_struct_size);
    proc_struct_size = PROC_STRUCT_SIZE_DEFAULT;
  }

  uint8_t proc_flags = pd_read8(kproc + PROC_OFF_FLAGS);
  uint64_t computed_ktask =
      (proc_flags & 2) ? (kproc + proc_struct_size) : 0;

  uint64_t kernel_ktask = 0;
  kcall_args[0] = kproc;
  (void)pd_kcall_simple(FUNC_PROC_TASK, kcall_args, 1, &kernel_ktask);
  if (kernel_ktask != computed_ktask) {
    printf("proc_task mismatch:\n\tcomputed: 0x%llx\n\tkernel:   0x%llx\n",
           (unsigned long long)computed_ktask,
           (unsigned long long)kernel_ktask);
  }

  uint64_t proc_ro_ptr = pd_read64(kproc + PROC_OFF_RO_PTR);

  if (computed_ktask) {
    uint64_t task_ro_ptr = pd_read64(computed_ktask + TASK_OFF_RO_PTR);
    printf("kproc @ 0x%llx:\n\tro_ptr: 0x%llx\nktask @ 0x%llx:\n\tro_ptr: 0x%llx\n",
           (unsigned long long)kproc,
           (unsigned long long)proc_ro_ptr,
           (unsigned long long)computed_ktask,
           (unsigned long long)task_ro_ptr);
  } else {
    printf("kproc @ 0x%llx:\n\tro_ptr: 0x%llx\nktask: <none> (proc+0x%x flags=0x%02x)\n",
           (unsigned long long)kproc,
           (unsigned long long)proc_ro_ptr,
           (unsigned int)PROC_OFF_FLAGS,
           (unsigned int)proc_flags);
  }

  uint64_t ro_for_proc = 0;
  kcall_args[0] = kproc;
  (void)pd_kcall_simple(FUNC_RO_FOR_PROC, kcall_args, 1, &ro_for_proc);
  printf("ro for proc kproc=0x%llx: 0x%llx\n",
         (unsigned long long)kproc,
         (unsigned long long)ro_for_proc);

  bool proc_ro_valid = true;
  bool task_ro_valid = true;

  printf("Validating proc_ro region...\n");
  if (!proc_ro_ptr) {
    printf("\tproc_ro_ptr is NULL\n");
  } else {
    uint64_t proc_ro_proc = pd_read64(proc_ro_ptr);
    uint64_t proc_ro_task = pd_read64(proc_ro_ptr + 8);
    printf("\tproc_ro raw: proc=0x%llx task=0x%llx\n",
           (unsigned long long)proc_ro_proc,
           (unsigned long long)proc_ro_task);
    if (proc_ro_proc != kproc) {
      printf("\tInvalid proc ro region: proc_ro_proc != kproc\n\tInvalid proc ro region: 0x%llx != 0x%llx\n",
             (unsigned long long)proc_ro_proc,
             (unsigned long long)kproc);
      proc_ro_valid = false;
    }
    if (proc_ro_task != computed_ktask) {
      printf("\tInvalid proc ro region: proc_ro_task != ktask\n\tInvalid proc ro region: 0x%llx != 0x%llx\n",
             (unsigned long long)proc_ro_task,
             (unsigned long long)computed_ktask);
      proc_ro_valid = false;
    }
  }

  if (computed_ktask) {
    printf("Validating task_ro region...\n");
    uint64_t task_ro_ptr = pd_read64(computed_ktask + TASK_OFF_RO_PTR);
    if (task_ro_ptr) {
      uint64_t task_ro_proc = pd_read64(task_ro_ptr);
      uint64_t task_ro_task = pd_read64(task_ro_ptr + 8);
      printf("\ttask_ro raw: proc=0x%llx task=0x%llx\n",
             (unsigned long long)task_ro_proc,
             (unsigned long long)task_ro_task);
      if (task_ro_proc != kproc) {
        printf("\tInvalid task ro region: task_ro_proc != kproc\n\tInvalid task ro region: 0x%llx != 0x%llx\n",
               (unsigned long long)task_ro_proc,
               (unsigned long long)kproc);
        proc_ro_valid = false;
      }
      if (task_ro_task != computed_ktask) {
        printf("\tInvalid task ro region: task_ro_task != ktask\n\tInvalid task ro region: 0x%llx != 0x%llx\n",
               (unsigned long long)task_ro_task,
               (unsigned long long)computed_ktask);
        proc_ro_valid = false;
      }
    } else {
      printf("\ttask_ro_ptr is NULL\n");
    }
  }

  if (proc_ro_valid || task_ro_valid) {
    dump_ro_info(proc_ro_ptr); // prefer proc ro ptr for convenience
  } else {
    printf("Neither proc_ro nor task_ro regions are valid!\n");
  }

  kcall_args[0] = kproc;
  (void)pd_kcall_simple(FUNC_PROC_RELE, kcall_args, 1, NULL);
  printf("released process structure\n");

  pd_deinit();

  printf("deiniting pandora\n");
  return 0;
}
