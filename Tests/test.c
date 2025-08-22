#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void format_timestamp(uint64_t nanosSinceBoot, char *buffer, size_t bufsize) {
  if (nanosSinceBoot == 0) {
    snprintf(buffer, bufsize, "Not set");
    return;
  }

  // Get current time info
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  // Get current mach time and convert to nanos
  uint64_t currentMachTime = mach_absolute_time();
  mach_timebase_info_data_t timebase;
  mach_timebase_info(&timebase);
  uint64_t currentNanosSinceBoot =
      currentMachTime * timebase.numer / timebase.denom;

  // Calculate the actual timestamp
  uint64_t nanosAgo = currentNanosSinceBoot - nanosSinceBoot;
  uint64_t secondsAgo = nanosAgo / 1000000000;

  time_t eventTime = now.tv_sec - secondsAgo;
  struct tm *tm_info = localtime(&eventTime);

  strftime(buffer, bufsize, "%Y-%m-%d %H:%M:%S", tm_info);

  // Add milliseconds
  uint64_t millis = (nanosSinceBoot % 1000000000) / 1000000;
  char temp[256];
  snprintf(temp, sizeof(temp), "%s.%03llu", buffer, millis);
  strncpy(buffer, temp, bufsize - 1);
}

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

  char timeBuffer[256];

  printf("Pandora Metadata:\n");
  printf("  kmod_start_time:\n");
  format_timestamp(metadata.kmod_start_time.nanosSinceBoot, timeBuffer,
                   sizeof(timeBuffer));
  printf("    Time: %s\n", timeBuffer);
  printf("    Mach time: %llu\n", metadata.kmod_start_time.machTime);
  printf("    Nanos since boot: %llu (%.2f seconds)\n",
         metadata.kmod_start_time.nanosSinceBoot,
         metadata.kmod_start_time.nanosSinceBoot / 1000000000.0);

  printf("  io_service_start_time:\n");
  format_timestamp(metadata.io_service_start_time.nanosSinceBoot, timeBuffer,
                   sizeof(timeBuffer));
  printf("    Time: %s\n", timeBuffer);
  printf("    Mach time: %llu\n", metadata.io_service_start_time.machTime);
  printf("    Nanos since boot: %llu (%.2f seconds)\n",
         metadata.io_service_start_time.nanosSinceBoot,
         metadata.io_service_start_time.nanosSinceBoot / 1000000000.0);

  printf("  user_client_init_time:\n");
  format_timestamp(metadata.user_client_init_time.nanosSinceBoot, timeBuffer,
                   sizeof(timeBuffer));
  printf("    Time: %s\n", timeBuffer);
  printf("    Mach time: %llu\n", metadata.user_client_init_time.machTime);
  printf("    Nanos since boot: %llu (%.2f seconds)\n",
         metadata.user_client_init_time.nanosSinceBoot,
         metadata.user_client_init_time.nanosSinceBoot / 1000000000.0);

  printf("  pid1_exists: %s\n", metadata.pid1_exists ? "true" : "false");

  pd_deinit();
  return 0;
}
