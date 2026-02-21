#ifndef PROC_TASK_H
#define PROC_TASK_H

#include "kstructs.h"

ks_task_t proc_to_task(ks_proc_t proc);
ks_task_t proc_to_task_unchecked(ks_proc_t proc);
ks_proc_t task_to_proc(ks_task_t task);

#endif
