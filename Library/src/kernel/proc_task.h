#ifndef PROC_TASK_H
#define PROC_TASK_H

#include "kstructs.h"
#include "memdiff.h"

// these functions allocate memdiff views with no way to free them
memdiff_view *proc_to_taskv_check(ks_proc_t proc, bool check, uint64_t proc_addr);
#define proc_to_taskv(proc, addr)       proc_to_taskv_check(proc, true, addr)
#define proc_to_taskv_unchecked(proc)   proc_to_taskv_check(proc, false, 0)
#define procv_to_taskv(procv)           proc_to_taskv_check((ks_proc_t)((procv)->original_copy), true, ((procv)->base_address))
#define procv_to_taskv_unchecked(procv) proc_to_taskv_check((ks_proc_t)((procv)->original_copy), false, ((procv)->base_address))

memdiff_view *task_to_procv_check(ks_task_t task, bool check, uint64_t task_addr);
#define task_to_procv(task, addr)       task_to_procv_check(task, true, addr)
#define task_to_procv_unchecked(task)   task_to_procv_check(task, false, 0)
#define taskv_to_procv(taskv)           task_to_procv_check((ks_task_t)((taskv)->original_copy), true, ((taskv)->base_address))
#define taskv_to_procv_unchecked(taskv) task_to_procv_check((ks_task_t)((taskv)->original_copy), false, ((taskv)->base_address))

// these functions return the original part of
ks_task_t proc_to_task_check(ks_proc_t proc, bool check, uint64_t proc_addr);
#define proc_to_task(proc, addr)       proc_to_task_check(proc, true, addr)
#define proc_to_task_unchecked(proc)   proc_to_task_check(proc, false, 0)
#define procv_to_task(procv)           proc_to_task_check((ks_proc_t)((procv)->original_copy), true, ((procv)->base_address))
#define procv_to_task_unchecked(procv) proc_to_task_check((ks_proc_t)((procv)->original_copy), false, ((procv)->base_address))

ks_proc_t task_to_proc_check(ks_task_t task, bool check, uint64_t task_addr);
#define task_to_proc(task, addr)       task_to_proc_check(task, true, addr)
#define task_to_proc_unchecked(task)   task_to_proc_check(task, false, 0)
#define taskv_to_proc(taskv)           task_to_proc_check((ks_task_t)((taskv)->original_copy), true, ((taskv)->base_address))
#define taskv_to_proc_unchecked(taskv) task_to_proc_check((ks_task_t)((taskv)->original_copy), false, ((taskv)->base_address))

#endif
