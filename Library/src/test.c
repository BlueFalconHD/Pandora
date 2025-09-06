#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <capstone.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "osvariant_status/patch.h"

// for converting kern_return_t to human-readable error message
#include "hexdump.h"
#include <mach/error.h>

#define POTENTIAL_CAVE_UNSLID 0xFFFFFE00087C1880

#define ENOSYS_START_UNSLID 0xFFFFFE0008DDC77C
#define ENOSYS_PATCH_OFF 0x4

#define INSTR_ALIGN(addr) ((addr) & ~0x3ULL)
#define INSTR_ALIGN_UP(addr) (((addr) + 3) & ~0x3ULL)

#include "esym/b.h"
#include "esym/mov.h"
#include "esym/ret.h"

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

  // read instruction at ENOSYS_START_UNSLID + ENOSYS_PATCH_OFF
  uint32_t instruction = 0;
  pd_readbuf(kslide(ENOSYS_START_UNSLID + ENOSYS_PATCH_OFF), &instruction,
             sizeof(instruction));

  uint32_t jpatch = encode_b_to(kslide(ENOSYS_START_UNSLID + ENOSYS_PATCH_OFF),
                                INSTR_ALIGN_UP(kslide(POTENTIAL_CAVE_UNSLID)));
  uint32_t movcc = encode_movz_w(0, 0xdead, 0);
  uint32_t retc = encode_ret_lr();

  printf("Patching ENOSYS handler:\n");

  // write jpatch to kslide(ENOSYS_START_UNSLID + ENOSYS_PATCH_OFF)
  pd_writebuf(kslide(ENOSYS_START_UNSLID + ENOSYS_PATCH_OFF), &jpatch,
              sizeof(jpatch));
  printf("  Wrote jpatch 0x%08x to 0x%llx\n", jpatch,
         kslide(ENOSYS_START_UNSLID + ENOSYS_PATCH_OFF));

  printf("Patching cave at 0x%llx:\n", kslide(POTENTIAL_CAVE_UNSLID));

  // write movcc to kslide(POTENTIAL_CAVE_UNSLID)
  pd_writebuf(kslide(POTENTIAL_CAVE_UNSLID), &movcc, sizeof(movcc));
  printf("  Wrote movcc 0x%08x to 0x%llx\n", movcc,
         kslide(POTENTIAL_CAVE_UNSLID));

  // write retc to kslide(POTENTIAL_CAVE_UNSLID + 4)
  pd_writebuf(kslide(POTENTIAL_CAVE_UNSLID + 4), &retc, sizeof(retc));
  printf("  Wrote retc 0x%08x to 0x%llx\n", retc,
         kslide(POTENTIAL_CAVE_UNSLID + 4));

  // svc instruction which triggers ENOSYS
  long r = syscall(8);

  printf("syscall(8) => r=%ld errno=0x%lx\n", r, errno);

  pd_deinit();
  return 0;
}
