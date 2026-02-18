#ifndef MEMDIFF_H
#define MEMDIFF_H

#include <stdint.h>
#include <stddef.h>


typedef struct {
    size_t size;

    uint8_t *original_copy;
    uint8_t *modified_copy;

    uintptr_t base_address;
} memdiff_view;

memdiff_view *memdiff_create(const uintptr_t kernel_address, size_t size);
int memdiff_commit(memdiff_view *view);

#endif /* MEMDIFF_H */
