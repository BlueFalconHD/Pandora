#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
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

  printf("Kernel base found: 0x%llx\n", kernelBase);

  // test reading a value from kernel memory
  uint64_t testAddress = kernelBase;
  uint64_t testValue = pd_read64(testAddress);
  printf("Value at kernel base address (0x%llx): 0x%llx\n", testAddress,
         testValue);

  PandoraMetadata metadata;
  if (pd_get_metadata(&metadata) == -1) {
    printf("Failed to get Pandora metadata.\n");
    pd_deinit();
    return 1;
  }
  printf("Pandora Metadata:\n");
  printf("  kmod_start_time: %llu\n", metadata.kmod_start_time);
  printf("  io_service_start_time: %llu\n", metadata.io_service_start_time);
  printf("  user_client_init_time: %llu\n", metadata.user_client_init_time);
  printf("  pid1_exists: %s\n", metadata.pid1_exists ? "true" : "false");

  pd_deinit();
  return 0;
}
