#include "page_size.h"

int page_size_once;
uint64_t page_size;

int get_page_size() {
  if (page_size_once) {
    return page_size;
  }

  int mib[2] = {CTL_HW, HW_PAGESIZE};
  size_t size = sizeof(page_size);
  if (sysctl(mib, 2, &page_size, &size, NULL, 0) != 0) {
    return -1;
  }

  page_size_once = 1;
  return page_size;
}
