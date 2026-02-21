#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H


#include "memdiff.h"
#include "kstructs.h"

// kernel proc/task
extern memdiff_view *kernel_proc_view;
extern memdiff_view *kernel_task_view;
#define kernel_proc (ks_proc_t)kernel_proc_view->original_copy
#define kernel_task (ks_task_t)kernel_task_view->original_copy

#endif
