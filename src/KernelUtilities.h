#ifndef KERNELUTILITIES_H
#define KERNELUTILITIES_H

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

#define _KU_DEFAULT_KB 0xFFFFFE0007004000

// KUError enumeration for error handling
enum KUError {
  KUErrorSuccess = 0,
  KUErrorMemoryAllocationFailed = -1,
  KUErrorInvalidAddress = -2,
  KUErrorReadFailed = -3,
  KUErrorWriteFailed = -4,
  KUErrorKernelBaseNotFound = -5,
  KUErrorKernelSlideNotFound = -6,
  KUErrorInvalidResetVector = -7,
  KUErrorBadArgument = -8,
  KUErrorMemoryPreperationFailed = -9,
  KUErrorNotEnoughBytesRead = -10,
};

class KernelUtilities {
public:
  KernelUtilities()
      : kernelBaseAddress(0), kernelSlide(0),
        initStatus_(KUErrorKernelBaseNotFound) {}

  KUError init() {
    if (initialized_)
      return initStatus_;
    initStatus_ = locateKernelBase(&kernelBaseAddress, &kernelSlide);
    initialized_ = (initStatus_ == KUErrorSuccess);
    return initStatus_;
  }

  bool isInitialized() const { return initialized_; }
  KUError status() const { return initStatus_; }

  static KUError kread(uint64_t address, void *buffer, size_t size);
  static KUError kwrite(uint64_t address, const void *buffer, size_t size);

  uint64_t kernelBase() const { return kernelBaseAddress; }
  uint64_t kernelSlideValue() const { return kernelSlide; }

  // kslide(x) = (x + slide - 0x8000)
  uint64_t kslide(uint64_t address) const {
    return (address + kernelSlide - 0x8000);
  }

  // kunslide(x) = (x - slide + 0x8000)
  uint64_t kunslide(uint64_t address) const {
    return (address - kernelSlide + 0x8000);
  }

private:
  uint64_t kernelBaseAddress;
  uint64_t kernelSlide;
  bool initialized_ = false;
  KUError initStatus_;
  static KUError locateKernelBase(uint64_t *kernelBaseAddress,
                                  uint64_t *kernelSlide);
};

#endif // KERNELUTILITIES_H
