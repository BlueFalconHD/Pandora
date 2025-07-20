#ifndef KPI_H
#define KPI_H

#include <mach/vm_param.h>
#include <stddef.h>

// Unsupported.exports
extern "C" void *kalloc(vm_size_t size);
extern "C" void kfree(void *address, size_t length);

#endif // KPI_H
