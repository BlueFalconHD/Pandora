#include "memdiff.h"
#include "pandora.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

memdiff_view *memdiff_create(const uintptr_t kernel_address, size_t size) {
  memdiff_view *view = malloc(sizeof(memdiff_view));
  if (!view) {
    printf("memdiff_create: failed to allocate memdiff_view\n");
    return NULL;
  }

  view->size = size;
  view->base_address = kernel_address;

  view->original_copy = malloc(size);
  view->modified_copy = malloc(size);
  if (!view->original_copy || !view->modified_copy) {
    printf("memdiff_create: failed to allocate memory for copies\n");
    free(view->original_copy);
    free(view->modified_copy);
    free(view);
    return NULL;
  }

  // Read the original memory into both copies
  int err = pd_readbuf(kernel_address, view->original_copy, size);
  if (err != 0) {
    printf("memdiff_create: failed to read kernel memory at 0x%llx: %d\n",
           (unsigned long long)kernel_address, err);
    free(view->original_copy);
    free(view->modified_copy);
    free(view);
    return NULL;
  }

  memcpy(view->modified_copy, view->original_copy, size);
  return view;
};

int diff_bytes(const uint8_t *a, const uint8_t *b, size_t size) {
  int diff_count = 0;
  for (size_t i = 0; i < size; i++) {
    if (a[i] != b[i]) {
      diff_count++;
    }
  }
  return diff_count;
}

int memdiff_commit(memdiff_view *view) {
  // read the memory at the current kernel address and compare to stored
  // original copy
  uint8_t *current_copy = malloc(view->size);
  if (!current_copy) {
    printf("memdiff_commit: failed to allocate memory for current copy\n");
    return -1;
  }
  int err = pd_readbuf(view->base_address, current_copy, view->size);
  if (err != 0) {
    printf("memdiff_commit: failed to read kernel memory at 0x%llx: %d\n",
           (unsigned long long)view->base_address, err);
    free(current_copy);
    return -1;
  };

  int diff_count = diff_bytes(view->original_copy, current_copy, view->size);

  // perform some simple TOCTOU checks to see if the memory has changed since
  // the view was created if so abort committing changes as this could lead to
  // corrupted lock states or similar if the memory is being modified
  // concurrently by the kernel
  //
  // THIS IS NOT SAFE but neither is monkeypatching kernel memory
  // it is possible and very likely kernel memory will change values in between now and actual committing but this is a simple check to at least catch some cases where the memory has changed since view creation and warn the user about potential issues with committing in that case
  if (diff_count != 0) {
    printf("memdiff_commit: aborting commit due to TOCTOU check failure - "
           "memory has changed since view creation (diff_count=%d)\n",
           diff_count);
    free(current_copy);
    return -1;
  }

  // iterate through size bytes in both original and modified copies and write
  // any different bytes to kernel memory
  int commit_count = 0;
  for (size_t i = 0; i < view->size; i++) {
    if (view->original_copy[i] != view->modified_copy[i]) {
      int write_err = pd_write8(view->base_address + i, view->modified_copy[i]);
      if (write_err != 0) {
        printf("memdiff_commit: failed to write byte at offset %zu (0x%llx) to kernel memory: %x\n", i, (unsigned long long)(view->base_address + i), write_err);
              free(current_copy);
        return -1;
      }
      commit_count++;
    }
  }

  printf("memdiff_commit: committed %d byte(s) to kernel memory at 0x%llx\n",
         commit_count, (unsigned long long)view->base_address);
  return 0;
}
