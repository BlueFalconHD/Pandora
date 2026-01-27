#ifndef KPI_H
#define KPI_H

#include <mach/vm_param.h>
#include <stddef.h>

// Unsupported.exports
extern "C" void *kalloc(vm_size_t size);
extern "C" void kfree(void *address, size_t length);

// Private KPI used for PID -> task lookups
struct proc;
typedef struct proc *proc_t;
extern "C" proc_t proc_find(int pid);
extern "C" void proc_rele(proc_t p);
extern "C" task_t proc_task(proc_t p);

// Code signing / debugging allowances (same actions debugserver triggers)
extern "C" int cs_allow_invalid(proc_t p);

#endif // KPI_H
