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
void memdiff_destroy(memdiff_view *view);

static inline uint64_t MEMDIFF_UVA_TO_KVA(memdiff_view *view, uintptr_t uva) {
    if (!view || !view->original_copy) {
        return 0;
    }
    if (uva < view->base_address || uva >= view->base_address + view->size) {
        return 0;
    }
    size_t offset = (size_t)(uva - view->base_address);
    return (uint64_t)(uintptr_t)(view->original_copy + offset);
}

static inline uint64_t MEMDIFF_KVA_TO_UVA(memdiff_view *view, uintptr_t kva) {
    if (!view || !view->original_copy) {
        return 0;
    }
    if (kva < (uintptr_t)view->original_copy || kva >= (uintptr_t)(view->original_copy + view->size)) {
        return 0;
    }
    size_t offset = (size_t)(kva - (uintptr_t)view->original_copy);
    return view->base_address + offset;
}

#define MEMDIFF_CREATE(ptr, type) memdiff_create((uintptr_t)(ptr), sizeof(type))

#endif /* MEMDIFF_H */
