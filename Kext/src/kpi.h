#ifndef KPI_H
#define KPI_H

#include <stdint.h>
#include <stddef.h>

// Unsupported.exports
extern "C" void *kalloc(uintptr_t size);
extern "C" void kfree(void *address, size_t length);

// Private KPI used for PID -> task lookups
struct proc;
typedef struct proc *proc_t;
extern "C" proc_t proc_find(int pid);
extern "C" void proc_rele(proc_t p);

// Forward-declare task_t and declare proc_task shim
struct task;
typedef struct task *task_t;
extern "C" task_t proc_task(proc_t p);

#endif // KPI_H
