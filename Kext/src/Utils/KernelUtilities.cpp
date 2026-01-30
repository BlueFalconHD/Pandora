#include "KernelUtilities.h"
#include "../Globals.h"
#include "PandoraLog.h"
#include "../routines.h"

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

static KUError kread_iomd(uint64_t address, void *buffer, size_t size) {
  if (address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withAddressRange(
      address, size, kIODirectionIn, kernel_task);
  if (!memDesc) {
    return KUErrorMemoryAllocationFailed;
  }

  IOReturn ret = memDesc->prepare();
  if (ret != kIOReturnSuccess) {
    extraerrdata1 = (uint64_t)(uint32_t)ret;
    memDesc->release();
    return KUErrorMemoryPreperationFailed;
  }

  bzero(buffer, size);
  uint64_t bytesRead = memDesc->readBytes(0, buffer, size);
  memDesc->complete();
  memDesc->release();

  return (bytesRead == size) ? KUErrorSuccess : KUErrorNotEnoughBytesRead;
}

static KUError kread_via_physmap(uint64_t address, void *buffer, size_t size) {
  if (address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  uint8_t *out = static_cast<uint8_t *>(buffer);
  uint64_t vaddr = address;
  size_t remaining = size;

  while (remaining) {
    vm_offset_t paddr = arm_kvtophys(vaddr);
    if (paddr == 0) {
      return KUErrorNotEnoughBytesRead;
    }

    size_t page_off = static_cast<size_t>(paddr & PAGE_MASK);
    size_t chunk = PAGE_SIZE - page_off;
    if (chunk > remaining) {
      chunk = remaining;
    }

    IOPhysicalAddress paddr_aligned =
        static_cast<IOPhysicalAddress>(paddr & ~static_cast<vm_offset_t>(PAGE_MASK));
    IOByteCount map_len = static_cast<IOByteCount>(page_off + chunk);

    IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withPhysicalAddress(
        paddr_aligned, map_len, kIODirectionIn);
    if (!memDesc) {
      return KUErrorMemoryAllocationFailed;
    }

    IOReturn ret = memDesc->prepare();
    if (ret != kIOReturnSuccess) {
      extraerrdata1 = (uint64_t)(uint32_t)ret;
      memDesc->release();
      return KUErrorMemoryPreperationFailed;
    }

    uint64_t bytesRead = memDesc->readBytes(
        static_cast<IOByteCount>(page_off), out, static_cast<IOByteCount>(chunk));
    memDesc->complete();
    memDesc->release();

    if (bytesRead != chunk) {
      extraerrdata1 = chunk;
      extraerrdata2 = bytesRead;
      return KUErrorNotEnoughBytesRead;
    }

    out += chunk;
    vaddr += chunk;
    remaining -= chunk;
  }

  return KUErrorSuccess;
}

static KUError kwrite_via_physmap(uint64_t address, const void *buffer,
                                  size_t size) {
  if (address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  const uint8_t *in = static_cast<const uint8_t *>(buffer);
  uint64_t vaddr = address;
  size_t remaining = size;

  while (remaining) {
    vm_offset_t paddr = arm_kvtophys(vaddr);
    if (paddr == 0) {
      return KUErrorNotEnoughBytesRead;
    }

    size_t page_off = static_cast<size_t>(paddr & PAGE_MASK);
    size_t chunk = PAGE_SIZE - page_off;
    if (chunk > remaining) {
      chunk = remaining;
    }

    IOPhysicalAddress paddr_aligned =
        static_cast<IOPhysicalAddress>(paddr & ~static_cast<vm_offset_t>(PAGE_MASK));
    IOByteCount map_len = static_cast<IOByteCount>(page_off + chunk);

    IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withPhysicalAddress(
        paddr_aligned, map_len, kIODirectionInOut);
    if (!memDesc) {
      return KUErrorMemoryAllocationFailed;
    }

    IOReturn ret = memDesc->prepare();
    if (ret != kIOReturnSuccess) {
      extraerrdata1 = (uint64_t)(uint32_t)ret;
      memDesc->release();
      return KUErrorMemoryPreperationFailed;
    }

    uint64_t bytesWritten =
        memDesc->writeBytes(static_cast<IOByteCount>(page_off), in,
                            static_cast<IOByteCount>(chunk));
    memDesc->complete();
    memDesc->release();

    if (bytesWritten != chunk) {
      extraerrdata1 = chunk;
      extraerrdata2 = bytesWritten;
      return KUErrorNotEnoughBytesRead;
    }

    in += chunk;
    vaddr += chunk;
    remaining -= chunk;
  }

  return KUErrorSuccess;
}

KUError KernelUtilities::kread(uint64_t address, void *buffer, size_t size) {
  KUError err = kread_iomd(address, buffer, size);
  if (err == KUErrorSuccess) {
    return KUErrorSuccess;
  }

  // IOMemoryDescriptor reads can fail for certain protected mappings (e.g.
  // Z_SUBMAP_IDX_READ_ONLY / PMAP_MAPPING_TYPE_ROZONE used by proc_ro).
  bzero(buffer, size);
  return kread_via_physmap(address, buffer, size);
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
    extraerrdata1 = (uint64_t)(uint32_t)ret;
    memDesc->release();
    KUError fallbackErr = kwrite_via_physmap(address, buffer, size);
    return (fallbackErr == KUErrorSuccess) ? KUErrorSuccess
                                          : KUErrorMemoryPreperationFailed;
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
    KUError fallbackErr = kwrite_via_physmap(address, buffer, size);
    return (fallbackErr == KUErrorSuccess) ? KUErrorSuccess
                                          : KUErrorNotEnoughBytesRead;
  }
  memDesc->complete();
  memDesc->release();
  return KUErrorSuccess;
}

KUError KernelUtilities::pread(task_t task, uint64_t address, void *buffer,
                               size_t size) {
  if (task == TASK_NULL || address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withAddressRange(
      address, size, kIODirectionIn, task);
  if (!memDesc) {
    PANDORA_MEMORY_LOG_ERROR(
        "Failed to create IOMemoryDescriptor for proc read at address 0x%llx, size %zu",
        address, size);
    return KUErrorMemoryAllocationFailed;
  }

  IOReturn ret = memDesc->prepare();
  if (ret != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT(
        "Failed to prepare IOMemoryDescriptor for proc read at address 0x%llx, IOReturn: 0x%x",
        address, ret);
    memDesc->release();
    return KUErrorMemoryPreperationFailed;
  }

  bzero(buffer, size);
  uint64_t bytesRead = memDesc->readBytes(0, buffer, size);

  if (bytesRead != size) {
    PANDORA_LOG_DEFAULT(
        "Proc read incomplete at address 0x%llx: expected %zu bytes, got %llu bytes",
        address, size, bytesRead);
    memDesc->complete();
    memDesc->release();
    return KUErrorNotEnoughBytesRead;
  }

  memDesc->complete();
  memDesc->release();
  return KUErrorSuccess;
}

KUError KernelUtilities::pwrite(task_t task, uint64_t address,
                                const void *buffer, size_t size) {
  if (task == TASK_NULL || address == 0 || buffer == nullptr || size == 0) {
    return KUErrorBadArgument;
  }

  IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withAddressRange(
      address, size, kIODirectionInOut, task);
  if (!memDesc) {
    PANDORA_LOG_DEFAULT(
        "Failed to create IOMemoryDescriptor for proc write at address 0x%llx, size %zu",
        address, size);
    return KUErrorMemoryAllocationFailed;
  }

  IOReturn ret = memDesc->prepare();
  if (ret != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT(
        "Failed to prepare IOMemoryDescriptor for proc write at address 0x%llx, IOReturn: 0x%x",
        address, ret);
    memDesc->release();
    return KUErrorMemoryPreperationFailed;
  }

  uint64_t bytesWritten = memDesc->writeBytes(0, buffer, size);
  if (bytesWritten != size) {
    PANDORA_LOG_DEFAULT(
        "Proc write incomplete at address 0x%llx: expected %zu bytes, wrote %llu bytes",
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

    KUError err = kread_iomd(address, buffer, readSize);
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
