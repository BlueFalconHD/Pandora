#ifndef CALYPSO_XREF_H
#define CALYPSO_XREF_H

#include "../kernel/memdiff.h"
#include <stddef.h>
#include <stdint.h>


int xref_find_adrp(memdiff_view *view, uint64_t target_addr, uint64_t *results, size_t max_results);
int xref_find_adrp_add(memdiff_view *view, uint64_t target_addr, uint64_t *results, size_t max_results);
int xref_adrp_add(memdiff_view *view, uint64_t target_addr, uint64_t *results, size_t max_results);

#endif
