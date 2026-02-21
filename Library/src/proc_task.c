#include "kstructs.h"
#include "memdiff.h"
#include "ptr_utils.h"
#include <stdint.h>
#include <stdio.h>

// get task based on proc
// generally unsafe, doesn't require having a reference
// has as many safety checks as possible so accessing the wrong data shouldn't crash kernel
ks_task_t proc_to_task(ks_proc_t proc) {
  if (!proc) {
    printf("proc_to_task: proc is NULL\n");
    return 0;
  }

  if (ptr_is_kernel((uint64_t)proc)) {
    printf("proc_to_task: proc pointer is kernel (expected memoryview): 0x%llx\n", (unsigned long long)proc);
    return 0;
  }

  if (!ptr_is_kernel((uint64_t)proc->p_proc_ro)) {
    printf("proc_to_task: proc_ro pointer is user (expected kernel): 0x%llx\n", (unsigned long long)proc->p_proc_ro);
    return 0;
  }

  uint64_t task = 0;

  memdiff_view *procview = memdiff_create((uintptr_t)proc->p_proc_ro, sizeof(struct ks_proc_ro));
  if (!procview) {
    printf("proc_to_task: failed to create memdiff view for proc_ro\n");
    return 0;
  }

  ks_proc_ro_t proc_ro_v = (ks_proc_ro_t)procview->original_copy;

  if (proc_ro_v->pr_proc != proc) {
    printf("proc_to_task: proc_ro->pr_proc does not point back to proc\n");
    printf("proc_to_task: proc_ro->pr_proc = 0x%llx, expected 0x%llx\n",
           (unsigned long long)proc_ro_v->pr_proc, (unsigned long long)proc);
    memdiff_destroy(procview);
    return 0;
  }

  task = (uint64_t)proc_ro_v->pr_task;

  // create the memdiff
  memdiff_view *taskview = memdiff_create(task, sizeof(struct ks_task));
  if (!taskview) {
      printf("proc_to_task: failed to create memdiff view for task\n");
      memdiff_destroy(procview);
      return 0;
  }

  // verify proc and task RO ptrs are the same
  ks_task_t task_v = (ks_task_t)taskview->original_copy;
  if (task_v->bsd_info_ro != proc->p_proc_ro) {
    printf("proc_to_task: task->bsd_info does not point back to proc_ro\n");
    printf("proc_to_task: task->bsd_info = 0x%llx, expected 0x%llx\n",
           (unsigned long long)task_v->bsd_info_ro, (unsigned long long)proc->p_proc_ro);
    memdiff_destroy(procview);
    memdiff_destroy(taskview);
    return 0;
  }

  return task_v;
}

ks_task_t proc_to_task_unchecked(ks_proc_t proc) {
  if (!proc) {
    printf("proc_to_task: proc is NULL\n");
    return 0;
  }

  if (ptr_is_kernel((uint64_t)proc)) {
    printf("proc_to_task: proc pointer is kernel (expected memoryview): 0x%llx\n", (unsigned long long)proc);
    return 0;
  }

  if (!ptr_is_kernel((uint64_t)proc->p_proc_ro)) {
    printf("proc_to_task: proc_ro pointer is user (expected kernel): 0x%llx\n", (unsigned long long)proc->p_proc_ro);
    return 0;
  }

  uint64_t task = 0;

  memdiff_view *procview = memdiff_create((uintptr_t)proc->p_proc_ro, sizeof(struct ks_proc_ro));
  if (!procview) {
    printf("proc_to_task: failed to create memdiff view for proc_ro\n");
    return 0;
  }

  ks_proc_ro_t proc_ro_v = (ks_proc_ro_t)procview->original_copy;
  task = (uint64_t)proc_ro_v->pr_task;

  // create the memdiff
  memdiff_view *taskview = memdiff_create(task, sizeof(struct ks_task));
  if (!taskview) {
      printf("proc_to_task: failed to create memdiff view for task\n");
      memdiff_destroy(procview);
      return 0;
  }

  ks_task_t task_v = (ks_task_t)taskview->original_copy;
  return task_v;
}

// get proc based on task
// generally unsafe, doesn't require having a reference
// has as many safety checks as possible so accessing the wrong data shouldn't crash kernel
ks_proc_t task_to_proc(ks_task_t task) {
  if (!task) {
    printf("task_to_proc: task is NULL\n");
    return 0;
  }

  if (ptr_is_kernel((uint64_t)task)) {
    printf("task_to_proc: task pointer is kernel (expected memoryview): 0x%llx\n", (unsigned long long)task);
    return 0;
  }

  if (!ptr_is_kernel((uint64_t)task->bsd_info_ro)) {
    printf("task_to_proc: task bsd_info pointer is user (expected kernel): 0x%llx\n", (unsigned long long)task->bsd_info_ro);
    return 0;
  }

  uint64_t proc = 0;

  memdiff_view *taskview = memdiff_create((uintptr_t)task->bsd_info_ro, sizeof(struct ks_proc_ro));
  if (!taskview) {
    printf("task_to_proc: failed to create memdiff view for task bsd_info\n");
    return 0;
  }

  ks_proc_ro_t task_bsd_info_v = (ks_proc_ro_t)taskview->original_copy;
  if (task_bsd_info_v->pr_task != task) {
    printf("task_to_proc: task bsd_info->pr_task does not point back to task\n");
    printf("task_to_proc: task bsd_info->pr_task = 0x%llx, expected 0x%llx\n",
          (unsigned long long)task_bsd_info_v->pr_task, (unsigned long long)task);
    memdiff_destroy(taskview);
    return 0;
  }

  proc = (uint64_t)task_bsd_info_v->pr_proc;

  // create the memdiff for proc
  memdiff_view *procview = memdiff_create(proc, sizeof(struct ks_proc));
  if (!procview) {
    printf("task_to_proc: failed to create memdiff view for proc\n");
    memdiff_destroy(taskview);
    return 0;
  }

  // verify proc and task RO ptrs are the same
  ks_proc_t proc_v = (ks_proc_t)procview->original_copy;
  if (proc_v->p_proc_ro != task->bsd_info_ro) {
    printf("task_to_proc: proc->p_proc_ro does not point back to task bsd_info\n");
    printf("task_to_proc: proc->p_proc_ro = 0x%llx, expected 0x%llx\n",
          (unsigned long long)proc_v->p_proc_ro, (unsigned long long)task->bsd_info_ro);
    memdiff_destroy(taskview);
    memdiff_destroy(procview);
    return 0;
  }

  memdiff_destroy(taskview);
  return proc_v;
}
