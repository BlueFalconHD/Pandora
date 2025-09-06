#include "KernelUtilities.h"
#include "../Globals.h"
#include "PandoraLog.h"

#include <stddef.h>
#include <stdint.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSharedDataQueue.h>
#include <IOKit/IOUserClient.h>
#include <kern/task.h>
#include <libkern/OSDebug.h>
#include <libkern/copyio.h>
#include <mach-o/loader.h>
#include <mach/vm_param.h>
#include <string.h>

static inline uint64_t getResetVector() {
  uint64_t reset_vector = 0;
  asm volatile("mrs %0, vbar_el1" : "=r"(reset_vector));
  return reset_vector;
}

KUError KernelUtilities::kread(uint64_t address, void *buffer, size_t size) {
  if (address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withAddressRange(
      address, size, kIODirectionIn, kernel_task);
  if (!memDesc) {
    PANDORA_MEMORY_LOG_ERROR("Failed to create IOMemoryDescriptor for read at "
                             "address 0x%llx, size %zu",
                             address, size);
    return KUErrorMemoryAllocationFailed;
  }

  IOReturn ret = memDesc->prepare();
  if (ret != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT("Failed to prepare IOMemoryDescriptor for read at "
                        "address 0x%llx, IOReturn: 0x%x",
                        address, ret);
    memDesc->release();
    return KUErrorMemoryPreperationFailed;
  }

  bzero(buffer, size);
  uint64_t bytesRead = memDesc->readBytes(0, buffer, size);

  if (bytesRead != size) {
    PANDORA_LOG_DEFAULT("Read operation incomplete at address 0x%llx: "
                        "expected %zu bytes, got %llu bytes",
                        address, size, bytesRead);
    memDesc->complete();
    memDesc->release();
    return KUErrorNotEnoughBytesRead;
  }

  memDesc->complete();
  memDesc->release();
  return KUErrorSuccess;
};

KUError KernelUtilities::kwrite(uint64_t address, const void *buffer,
                                size_t size) {
  if (address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withAddressRange(
      address, size, kIODirectionInOut, kernel_task);
  if (!memDesc) {
    PANDORA_LOG_DEFAULT("Failed to create IOMemoryDescriptor for write at "
                        "address 0x%llx, size %zu",
                        address, size);
    PANDORA_LOG_DEFAULT("Failed to create IOMemoryDescriptor for write at "
                        "address 0x%llx, size %zu",
                        address, size);
    PANDORA_LOG_DEFAULT("Failed to create IOMemoryDescriptor for write at "
                        "address 0x%llx, size %zu",
                        address, size);
    PANDORA_LOG_DEFAULT("Failed to create IOMemoryDescriptor for write at "
                        "address 0x%llx, size %zu",
                        address, size);
    PANDORA_LOG_DEFAULT("Failed to create IOMemoryDescriptor for write at "
                        "address 0x%llx, size %zu",
                        address, size);
    return KUErrorMemoryAllocationFailed;
  }
  IOReturn ret = memDesc->prepare();
  if (ret != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT("Failed to prepare IOMemoryDescriptor for write "
                        "at address 0x%llx, IOReturn: 0x%x",
                        address, ret);
    PANDORA_LOG_DEFAULT("Failed to prepare IOMemoryDescriptor for write "
                        "at address 0x%llx, IOReturn: 0x%x",
                        address, ret);
    PANDORA_LOG_DEFAULT("Failed to prepare IOMemoryDescriptor for write "
                        "at address 0x%llx, IOReturn: 0x%x",
                        address, ret);
    PANDORA_LOG_DEFAULT("Failed to prepare IOMemoryDescriptor for write "
                        "at address 0x%llx, IOReturn: 0x%x",
                        address, ret);
    PANDORA_LOG_DEFAULT("Failed to prepare IOMemoryDescriptor for write "
                        "at address 0x%llx, IOReturn: 0x%x",
                        address, ret);
    extraerrdata1 = ret;
    memDesc->release();
    return KUErrorMemoryPreperationFailed;
  }

  uint64_t bytesWritten = memDesc->writeBytes(0, buffer, size);
  if (bytesWritten != size) {
    PANDORA_LOG_DEFAULT("Write operation incomplete at address 0x%llx: "
                        "expected %zu bytes, wrote %llu bytes",
                        address, size, bytesWritten);
    PANDORA_LOG_DEFAULT("Write operation incomplete at address 0x%llx: "
                        "expected %zu bytes, wrote %llu bytes",
                        address, size, bytesWritten);
    PANDORA_LOG_DEFAULT("Write operation incomplete at address 0x%llx: "
                        "expected %zu bytes, wrote %llu bytes",
                        address, size, bytesWritten);
    PANDORA_LOG_DEFAULT("Write operation incomplete at address 0x%llx: "
                        "expected %zu bytes, wrote %llu bytes",
                        address, size, bytesWritten);
    PANDORA_LOG_DEFAULT("Write operation incomplete at address 0x%llx: "
                        "expected %zu bytes, wrote %llu bytes",
                        address, size, bytesWritten);
    extraerrdata1 = size;
    extraerrdata2 = bytesWritten;
    memDesc->complete();
    memDesc->release();
    return KUErrorNotEnoughBytesRead;
  }
  memDesc->complete();
  memDesc->release();
  return KUErrorSuccess;
}

KUError KernelUtilities::locateKernelBase(uint64_t *kernelBaseAddress,
                                          uint64_t *kernelSlide) {

  if (!kernelBaseAddress || !kernelSlide) {
    return KUErrorBadArgument;
  }

  uint64_t kernelPage = getResetVector();
  if (kernelPage == 0) {
    PANDORA_LOG_DEFAULT("Failed to get reset vector from vbar_el1 register");
    return KUErrorInvalidResetVector;
  }

  uint64_t address = kernelPage & ~(uint64_t)0xFFF;
  const uint64_t minAddress = 0xfffffe0000000000;
  const size_t chunkSize = 0x1000;

  uint8_t *buffer = (uint8_t *)IOMalloc(chunkSize);
  if (!buffer) {
    PANDORA_LOG_DEFAULT(
        "Failed to allocate %zu bytes for kernel base search buffer",
        chunkSize);
    return KUErrorMemoryAllocationFailed;
  }

  while (address >= minAddress) {
    size_t readSize = chunkSize;
    if (address < minAddress + chunkSize) {
      readSize = address - minAddress;
      address = minAddress;
    }

    KUError err = kread(address, buffer, readSize);
    if (err != KUErrorSuccess) {
      if (address >= chunkSize) {
        address -= chunkSize;
      } else {
        break;
      }
      continue;
    }

    for (size_t offset = 0; offset < readSize - sizeof(struct mach_header_64);
         offset += 4) {
      uint32_t magic = *(uint32_t *)(buffer + offset);
      if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
        uint64_t potentialBase = address + offset;
        // cast to mach_header_64
        struct mach_header_64 *header =
            (struct mach_header_64 *)(buffer + offset);
        if (header->filetype == MH_EXECUTE) {
          *kernelBaseAddress = potentialBase;
          *kernelSlide = potentialBase - _KU_DEFAULT_KB;

          // free
          IOFree(buffer, chunkSize);

          return KUErrorSuccess;
        }
      }
    }

    if (address >= chunkSize) {
      address -= chunkSize;
    } else {
      break;
    }
  }

  PANDORA_LOG_DEFAULT("Kernel base address not found after exhaustive search");
  IOFree(buffer, chunkSize);
  return KUErrorKernelBaseNotFound;
}
