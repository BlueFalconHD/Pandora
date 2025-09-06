#include "PandoraUserClient.h"
#include "Globals.h"
#include "Utils/KernelUtilities.h"
#include "Utils/PandoraLog.h"
#include "Utils/TimeUtilities.h"
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

#define super IOUserClient
OSDefineMetaClassAndFinalStructors(PandoraUserClient, IOUserClient);

bool PandoraUserClient::initWithTask(task_t owningTask, void *securityID,
                                     uint32_t type) {
  if (!super::initWithTask(owningTask, securityID, type)) {
    return false;
  }

  pandora_log_ensure_initialized();

  PANDORA_LOG_DEFAULT("PandoraUserClient::initWithTask: Initializing Pandora "
                      "User Client with task %p, security ID %p, type %u",
                      owningTask, securityID, type);

  PANDORA_LOG_DEFAULT(
      "PandoraUserClient::initWithTask: current saw 0 state: %d", workloopsaw0);

  bool allow = false;
  OSObject *entitlement =
      copyClientEntitlement(owningTask, "com.bluefalconhd.pandora.krw");
  if (entitlement) {
    allow = (entitlement == kOSBooleanTrue);
    entitlement->release();
  };

  KUError kuError = ku.init();
  if (kuError != KUErrorSuccess) {
    PANDORA_LOG_ERROR("PandoraUserClient::initWithTask: Failed to initialize "
                      "KernelUtilities, "
                      "error code: %s (%d)",
                      get_error_name(kuError), kuError);

    return false;
  }

  uint64_t kernelBase = ku.kernelBase();
  uint64_t kernelSlide = ku.kernelSlideValue();

  if (kernelBase == 0 || kernelSlide == 0) {
    PANDORA_LOG_ERROR("PandoraUserClient::initWithTask: Either kernel base "
                      "or slide is zero, "
                      "initialization failed");
    return false;
  }

  PANDORA_LOG_DEFAULT("Kernel base address: 0x%08x%08x, slide: 0x%llx",
                      (uint32_t)(kernelBase >> 32),
                      (uint32_t)(kernelBase & 0xFFFFFFFFu), kernelSlide);

  user_client_initialized = true;
  user_client_init_time = makeCurrentTimestampPair();

  return allow;
}

IOReturn PandoraUserClient::externalMethod(uint32_t selector,
                                           IOExternalMethodArguments *args,
                                           IOExternalMethodDispatch *dispatch,
                                           OSObject *target, void *reference) {
  static const IOExternalMethodDispatch methods[] = {
      /* 0 */ {(IOExternalMethodAction)&PandoraUserClient::kread, 3, 0, 0, 0},
      /* 1 */ {(IOExternalMethodAction)&PandoraUserClient::kwrite, 3, 0, 0, 0},
      /* 2 */
      {(IOExternalMethodAction)&PandoraUserClient::getKernelBase, 0, 0, 1, 0},
      /* 3 */
      {(IOExternalMethodAction)&PandoraUserClient::getPandoraLoadMetadata, 0, 0,
       0, sizeof(PandoraMetadata)},
  };

  if (selector < sizeof(methods) / sizeof(methods[0])) {
    dispatch = const_cast<IOExternalMethodDispatch *>(&methods[selector]);
    target = this;
  }

  return super::externalMethod(selector, args, dispatch, target, reference);
}

IOReturn PandoraUserClient::kread(PandoraUserClient *client, void *reference,
                                  IOExternalMethodArguments *args) {
  uint64_t kaddr = args->scalarInput[0]; // Kernel address to read from
  user_addr_t uaddr =
      args->scalarInput[1];          // User-space buffer to copy data into
  size_t len = args->scalarInput[2]; // Length of data to read

  if (!kaddr || !uaddr || !len)
    return kIOReturnBadArgument;

  // Allocate a buffer to read the data
  void *buffer = IOMalloc(len);
  if (!buffer) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::kread: Failed to allocate buffer of size %zu @ "
        "0x%llx\n",
        len, kaddr);
    return kIOReturnNoMemory;
  }

  KUError err = KernelUtilities::kread(kaddr, buffer, len);

  if (err != KUErrorSuccess) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::kread: Failed to read %zu bytes from kernel "
        "address 0x%llx. Error code: %d\n",
        len, kaddr, err);
    IOFree(buffer, len);
    return kIOReturnVMError;
  }

  // Log the read data
  PANDORA_LOG_DEFAULT(
      "KextRWUserClient::kread: Read %zu bytes from kernel address "
      "0x%llx\n",
      len, kaddr);

  // copy to user space
  int error = copyout(buffer, uaddr, len);

  if (error != 0) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "KextRWUserClient::kread: Failed to copy data to user space. "
        "Error code: %d\n",
        error);
  }

  IOFree(buffer, len);
  return (error == 0 && err == KUErrorSuccess) ? kIOReturnSuccess
                                               : kIOReturnVMError;
}

IOReturn PandoraUserClient::kwrite(PandoraUserClient *client, void *reference,
                                   IOExternalMethodArguments *args) {
  user_addr_t uaddr =
      args->scalarInput[0];              // User-space buffer to copy data from
  uint64_t kaddr = args->scalarInput[1]; // Kernel address to write to

  size_t len = args->scalarInput[2]; // Length of data to write

  if (!kaddr || !uaddr || !len)
    return kIOReturnBadArgument;

  // Allocate a buffer to read data from user space
  void *buffer = IOMalloc(len);
  if (!buffer) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::kread: Failed to allocate buffer of size %zu @ "
        "0x%llx\n",
        len, kaddr);
    return kIOReturnNoMemory;
  }

  // copy from user space
  int error = copyin(uaddr, buffer, len);
  if (error != 0) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::kwrite: Failed to copy data from user space. "
        "Error code: %d\n",
        error);
    IOFree(buffer, len);
    return kIOReturnVMError;
  }

  // write to kernel space
  KUError err = KernelUtilities::kwrite(kaddr, buffer, len);
  if (err != KUErrorSuccess) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::kwrite: Failed to write %zu bytes to kernel "
        "address 0x%llx. Error code: %s (%d). extra data [%llu, %llu, %llu]\n",
        len, kaddr, get_error_name(err), err, extraerrdata1, extraerrdata2,
        extraerrdata3);
    IOFree(buffer, len);
    return kIOReturnVMError;
  }

  return kIOReturnSuccess;
}

IOReturn PandoraUserClient::getKernelBase(PandoraUserClient *client,
                                          void *reference,
                                          IOExternalMethodArguments *args) {
  uint64_t kbase = client->ku.kernelBase();

  if (kbase == 0) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::getKernelBase: Failed to retrieve kernel base "
        "address, it is zero");
    return kIOReturnError;
  }

  PANDORA_LOG_DEFAULT("PandoraUserClient::getKernelBase: Kernel base address: "
                      "0x%08x%08x",
                      (uint32_t)(kbase >> 32), (uint32_t)(kbase & 0xFFFFFFFFu));

  args->scalarOutput[0] =
      kbase ^ KADDR_OBFUSCATION_KEY; // XOR with key to obfuscate
  return kIOReturnSuccess;
}

IOReturn
PandoraUserClient::getPandoraLoadMetadata(PandoraUserClient *client,
                                          void *reference,
                                          IOExternalMethodArguments *args) {
  if (args->structureOutputSize < sizeof(PandoraMetadata)) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "PandoraUserClient::getPandoraLoadMetadata: Output buffer too small. "
        "Expected %zu, got %u",
        sizeof(PandoraMetadata), args->structureOutputSize);
    return kIOReturnBadArgument;
  }

  PandoraMetadata metadata = {};
  metadata.kmod_start_time =
      kmod_run ? kmod_start_time : TimestampPair{0, 0, 0};
  metadata.io_service_start_time =
      io_service_start_called ? io_service_start_time : TimestampPair{0, 0, 0};
  metadata.user_client_init_time =
      user_client_initialized ? user_client_init_time : TimestampPair{0, 0, 0};
  metadata.pid1_exists = pid1_exists;

  memcpy(args->structureOutput, &metadata, sizeof(PandoraMetadata));
  args->structureOutputSize = sizeof(PandoraMetadata);

  PANDORA_LOG_DEFAULT(
      "PandoraUserClient::getPandoraLoadMetadata: Returned metadata - "
      "kmod_start: %llu (%llu), io_service_start: %llu (%llu), "
      "user_client_init: %llu (%llu), "
      "pid1_exists: %s",
      metadata.kmod_start_time.unixTime, metadata.kmod_start_time.machTime,
      metadata.io_service_start_time.unixTime,
      metadata.io_service_start_time.machTime,
      metadata.user_client_init_time.unixTime,
      metadata.user_client_init_time.machTime,
      metadata.pid1_exists ? "true" : "false");

  return kIOReturnSuccess;
}
