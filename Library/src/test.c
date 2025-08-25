#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <capstone.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "osvariant_status/patch.h"

// for converting kern_return_t to human-readable error message
#include <mach/error.h>

int main() {
  if (pd_init() == -1) {
    printf("Failed to initialize Pandora. Is the kernel extension loaded?\n");
    return 1;
  }

  uint64_t kernelBase = pd_get_kernel_base();

  if (kernelBase == 0) {
    printf("Failed to get kernel base address. Make sure SIP is disabled.\n");
    pd_deinit();
    return 1;
  }

  uint64_t slideAddress = kslide(0xFFFFFE000C527CF0);
  uint64_t slideValue = pd_read64(slideAddress);
  printf("Value at slide address (0x%llx): 0x%llx\n", slideAddress, slideValue);

  // write value to same address
  const uint64_t newVal = 0x70010002f388828f;

  if (slideValue != newVal) {
    kern_return_t res = pd_write64(slideAddress, newVal);

    if (res != KERN_SUCCESS) {
      printf("Failed to write to slide address (0x%llx)\n", slideAddress);
      printf("Error: %x\n", res);

      // print human-readable error message
      printf("Error message: %s\n", mach_error_string(res));

      pd_deinit();
      return 1;
    }
  }

  char *errorMessage = NULL;
  bool res = patch_osvariant_status(&errorMessage);
  if (!res) {
    printf("Failed to patch osvariant status: %s\n", errorMessage);
    free(errorMessage);
    pd_deinit();
    return 1;
  }

  printf("Successfully patched osvariant status!\n");

  pd_deinit();
  return 0;
}
