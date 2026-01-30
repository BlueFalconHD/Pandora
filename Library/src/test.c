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

    proc_ro_0_t *proc_ro = (proc_ro_0_t *)&buf;
    printf("proc_ro ro_flags: 0x%x\n", proc_ro->task_data.t_flags_ro);
    printf("proc_ro cs_flags: 0x%x\n", proc_ro->proc_data.p_csflags);

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

  pid_t targetPid = getpid();
  if (argc < 2) {
    printf("using self pid %d\n", targetPid);
  } else {
    targetPid = (pid_t)atoi(argv[1]);
    printf("using pid %d from cmdline\n", targetPid);
  }

  uint64_t kcallArgs[1] = {0};

  uint64_t kproc = 0;
  kcallArgs[0] = (uint64_t)(int64_t)targetPid;
  (void)pd_kcall_simple(FUNC_PROC_FIND, kcallArgs, 1, &kproc);
  if (!kproc) {
    printf("proc_find returned NULL for pid %d\n", targetPid);
    pd_deinit();
    return 1;
  }

  uint64_t procStructSize = pd_read64(PROC_STRUCT_SIZE_PTR);
  if (!procStructSize) {
    procStructSize = PROC_STRUCT_SIZE_DEFAULT;
  }
  if (procStructSize > PROC_STRUCT_SIZE_MAX) {
    printf("unexpected proc struct size: 0x%llx\n",
           (unsigned long long)procStructSize);
    procStructSize = PROC_STRUCT_SIZE_DEFAULT;
  }

  uint8_t procFlags = pd_read8(kproc + PROC_OFF_FLAGS);
  uint64_t computedKtask =
      (procFlags & 2) ? (kproc + procStructSize) : 0;

  uint64_t kernelKtask = 0;
  kcallArgs[0] = kproc;
  (void)pd_kcall_simple(FUNC_PROC_TASK, kcallArgs, 1, &kernelKtask);
  if (kernelKtask != computedKtask) {
    printf("proc_task mismatch:\n\tcomputed: 0x%llx\n\tkernel:   0x%llx\n",
           (unsigned long long)computedKtask,
           (unsigned long long)kernelKtask);
  }

  uint64_t procRoPtr = pd_read64(kproc + PROC_OFF_RO_PTR);

  if (computedKtask) {
    uint64_t taskRoPtr = pd_read64(computedKtask + TASK_OFF_RO_PTR);
    printf("kproc @ 0x%llx:\n\tro_ptr: 0x%llx\nktask @ 0x%llx:\n\tro_ptr: 0x%llx\n",
           (unsigned long long)kproc,
           (unsigned long long)procRoPtr,
           (unsigned long long)computedKtask,
           (unsigned long long)taskRoPtr);
  } else {
    printf("kproc @ 0x%llx:\n\tro_ptr: 0x%llx\nktask: <none> (proc+0x%x flags=0x%02x)\n",
           (unsigned long long)kproc,
           (unsigned long long)procRoPtr,
           (unsigned int)PROC_OFF_FLAGS,
           (unsigned int)procFlags);
  }

  uint64_t roForProc = 0;
  kcallArgs[0] = kproc;
  (void)pd_kcall_simple(FUNC_RO_FOR_PROC, kcallArgs, 1, &roForProc);
  printf("ro for proc 0x%llx: 0x%llx\n", (unsigned long long)computedKtask,
         (unsigned long long)roForProc);

  bool proc_ro_valid = true;
  bool task_ro_valid = true;

  printf("Validating proc_ro region...\n");
  if (!procRoPtr) {
    printf("\tproc_ro_ptr is NULL\n");
  } else {
    uint64_t procRoProc = pd_read64(procRoPtr);
    uint64_t procRoTask = pd_read64(procRoPtr + 8);
    printf("\tproc_ro raw: proc=0x%llx task=0x%llx\n",
           (unsigned long long)procRoProc,
           (unsigned long long)procRoTask);
    if (procRoProc != kproc) {
      printf("\tInvalid proc ro region: proc_ro_proc != kproc\n\tInvalid proc ro region: 0x%llx != 0x%llx\n",
             (unsigned long long)procRoProc,
             (unsigned long long)kproc);
      proc_ro_valid = false;
    }
    if (procRoTask != computedKtask) {
      printf("\tInvalid proc ro region: proc_ro_task != ktask\n\tInvalid proc ro region: 0x%llx != 0x%llx\n",
             (unsigned long long)procRoTask,
             (unsigned long long)computedKtask);
      proc_ro_valid = false;
    }
  }

  if (computedKtask) {
    printf("Validating task_ro region...\n");
    uint64_t taskRoPtr = pd_read64(computedKtask + TASK_OFF_RO_PTR);
    if (taskRoPtr) {
      uint64_t taskRoProc = pd_read64(taskRoPtr);
      uint64_t taskRoTask = pd_read64(taskRoPtr + 8);
      printf("\ttask_ro raw: proc=0x%llx task=0x%llx\n",
             (unsigned long long)taskRoProc,
             (unsigned long long)taskRoTask);
      if (taskRoProc != kproc) {
        printf("\tInvalid task ro region: task_ro_proc != kproc\n\tInvalid task ro region: 0x%llx != 0x%llx\n",
               (unsigned long long)taskRoProc,
               (unsigned long long)kproc);
        proc_ro_valid = false;
      }
      if (taskRoTask != computedKtask) {
        printf("\tInvalid task ro region: task_ro_task != ktask\n\tInvalid task ro region: 0x%llx != 0x%llx\n",
               (unsigned long long)taskRoTask,
               (unsigned long long)computedKtask);
        proc_ro_valid = false;
      }
    } else {
      printf("\ttask_ro_ptr is NULL\n");
    }
  }

  if (proc_ro_valid || task_ro_valid) {
      dump_ro_info(procRoPtr); // prefer proc ro ptr for convenience
  } else {
      printf("Neither proc_ro nor task_ro regions are valid!\n");
  }

  kcallArgs[0] = kproc;
  (void)pd_kcall_simple(FUNC_PROC_RELE, kcallArgs, 1, NULL);
  printf("released process structure\n");

  pd_deinit();

  printf("deiniting pandora\n");
  return 0;
}
