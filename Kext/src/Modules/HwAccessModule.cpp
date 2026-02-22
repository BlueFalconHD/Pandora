#include "HwAccessModule.h"

#include "../Globals.h"
#include "../Pandora.h"
#include "../PandoraUserClient.h"
#include "../Utils/KernelCall.h"
#include "../Utils/KernelUtilities.h"
#include "../Utils/PandoraLog.h"
#include "../Utils/TimeUtilities.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOReturn.h>
#include <kern/task.h>
#include <libkern/copyio.h>
#include <sys/proc.h>
#include <string.h>

extern "C" task_t proc_task(proc_t p);

namespace {

static constexpr uint64_t kKaddrObfuscationKey = 0x6869726520706C7AULL;

enum HwAccessMethod : uint16_t {
  kMethodKRead = 0,
  kMethodKWrite = 1,
  kMethodGetKernelBase = 2,
  kMethodGetPandoraLoadMetadata = 3,
  kMethodPReadPid = 4,
  kMethodPWritePid = 5,
  kMethodKCall = 7,
  kMethodRunArbFuncWithTaskArgPid = 8,
};

} // namespace

const PandoraModuleDescriptor &HwAccessModule::descriptor() const {
  static const PandoraModuleDescriptor kDescriptor = {
      kModuleId,
      "hw_access",
      "pandora_enable_hw_access",
      nullptr,
      false,
  };
  return kDescriptor;
}

IOReturn HwAccessModule::onStart(Pandora &service) {
  service_ = &service;

  KUError kuStatus = service.kernelUtilities().init();
  if (kuStatus != KUErrorSuccess) {
    PANDORA_LOG_DEFAULT("hw_access: KernelUtilities init failed: %d", kuStatus);
    service_ = nullptr;
    return kIOReturnError;
  }

  if (service.kernelUtilities().kernelBase() == 0 ||
      service.kernelUtilities().kernelSlideValue() == 0) {
    PANDORA_LOG_DEFAULT("hw_access: kernel base/slide not available");
    service_ = nullptr;
    return kIOReturnError;
  }

  active_ = true;
  PANDORA_LOG_DEFAULT("hw_access: started");
  return kIOReturnSuccess;
}

void HwAccessModule::onStop(Pandora &service) {
  (void)service;
  active_ = false;
  service_ = nullptr;
  PANDORA_LOG_DEFAULT("hw_access: stopped");
}

void HwAccessModule::onError(Pandora &service, IOReturn error) {
  PANDORA_LOG_DEFAULT("hw_access: error 0x%x", error);
  onStop(service);
}

void HwAccessModule::onShutdown() {
  active_ = false;
  service_ = nullptr;
}

bool HwAccessModule::handlesUserClient() const {
  return true;
}

bool HwAccessModule::authorizeUserClient(task_t owningTask, void *securityID,
                                         uint32_t type) {
  (void)owningTask;
  (void)securityID;
  (void)type;

  if (!active_ || !service_) {
    PANDORA_LOG_DEFAULT("hw_access: deny userclient (module inactive)");
    return false;
  }

  if (!service_->kernelUtilities().isInitialized()) {
    KUError kuStatus = service_->kernelUtilities().init();
    if (kuStatus != KUErrorSuccess) {
      PANDORA_LOG_DEFAULT("hw_access: KernelUtilities init failed in authorize: %d",
                          kuStatus);
      return false;
    }
  }

  if (service_->kernelUtilities().kernelBase() == 0 ||
      service_->kernelUtilities().kernelSlideValue() == 0) {
    PANDORA_LOG_DEFAULT("hw_access: deny userclient (kernel base/slide unavailable)");
    return false;
  }

  PandoraRuntimeState &runtime = pandora_runtime_state();
  runtime.telemetry.userClientInitialized = true;
  runtime.telemetry.userClientInitTime = makeCurrentTimestampPair();

  PANDORA_LOG_DEFAULT("hw_access: allow userclient");
  return true;
}

void HwAccessModule::registerUserClientMethods(
    PandoraUserClientMethodRegistrar &registrar) {
  (void)registrar.addMethod(kMethodKRead, &HwAccessModule::methodKRead, 3, 0, 0,
                            0);
  (void)registrar.addMethod(kMethodKWrite, &HwAccessModule::methodKWrite, 3, 0,
                            0, 0);
  (void)registrar.addMethod(kMethodGetKernelBase,
                            &HwAccessModule::methodGetKernelBase, 0, 0, 1, 0);
  (void)registrar.addMethod(kMethodGetPandoraLoadMetadata,
                            &HwAccessModule::methodGetPandoraLoadMetadata, 0, 0,
                            0, sizeof(PandoraMetadata));
  (void)registrar.addMethod(kMethodPReadPid, &HwAccessModule::methodPReadPid, 4,
                            0, 0, 0);
  (void)registrar.addMethod(kMethodPWritePid, &HwAccessModule::methodPWritePid,
                            4, 0, 0, 0);
  (void)registrar.addMethod(kMethodKCall, &HwAccessModule::methodKCall, 0,
                            sizeof(PandoraKCallRequest), 0,
                            sizeof(PandoraKCallResponse));
  (void)registrar.addMethod(kMethodRunArbFuncWithTaskArgPid,
                            &HwAccessModule::methodRunArbFuncWithTaskArgPid, 2,
                            0, 1, 0);
}

HwAccessModule *HwAccessModule::fromModule(PandoraModule *module) {
  if (!module) {
    return nullptr;
  }

  if (module->descriptor().identifier != kModuleId) {
    return nullptr;
  }

  return static_cast<HwAccessModule *>(module);
}

IOReturn HwAccessModule::methodKCall(PandoraUserClient *client,
                                     PandoraModule *module,
                                     IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args || !args->structureInput ||
      args->structureInputSize < sizeof(PandoraKCallRequest) ||
      !args->structureOutput ||
      args->structureOutputSize < sizeof(PandoraKCallResponse)) {
    return kIOReturnBadArgument;
  }

  const auto *req = static_cast<const PandoraKCallRequest *>(args->structureInput);
  if (req->argCount > 8) {
    return kIOReturnBadArgument;
  }

  PandoraKCallResult res = pandora_kcall(req->fn, req->args, req->argCount);

  PandoraKCallResponse out = {};
  out.status = res.status;
  out.ret0 = res.ret0;

  memcpy(args->structureOutput, &out, sizeof(out));
  args->structureOutputSize = sizeof(out);
  return kIOReturnSuccess;
}

IOReturn HwAccessModule::methodKRead(PandoraUserClient *client,
                                     PandoraModule *module,
                                     IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args) {
    return kIOReturnBadArgument;
  }

  uint64_t kaddr = args->scalarInput[0];
  user_addr_t uaddr = args->scalarInput[1];
  size_t len = args->scalarInput[2];

  if (!kaddr || !uaddr || !len) {
    return kIOReturnBadArgument;
  }

  void *buffer = IOMalloc(len);
  if (!buffer) {
    PANDORA_USERCLIENT_LOG_ERROR(
        "HwAccessModule::kread: allocation failed size=%zu addr=0x%llx", len,
        kaddr);
    return kIOReturnNoMemory;
  }

  KUError err = KernelUtilities::kread(kaddr, buffer, len);
  if (err != KUErrorSuccess) {
    const PandoraRuntimeState &runtime = pandora_runtime_state();
    PANDORA_USERCLIENT_LOG_ERROR(
        "HwAccessModule::kread failed size=%zu addr=0x%llx err=%s(%d) extra=[%llu,%llu,%llu]",
        len, kaddr, get_error_name(err), err,
        runtime.debug.extraErrorData1,
        runtime.debug.extraErrorData2,
        runtime.debug.extraErrorData3);
    IOFree(buffer, len);
    return kIOReturnVMError;
  }

  int copyErr = copyout(buffer, uaddr, len);
  IOFree(buffer, len);
  return (copyErr == 0) ? kIOReturnSuccess : kIOReturnVMError;
}

IOReturn HwAccessModule::methodKWrite(PandoraUserClient *client,
                                      PandoraModule *module,
                                      IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args) {
    return kIOReturnBadArgument;
  }

  user_addr_t uaddr = args->scalarInput[0];
  uint64_t kaddr = args->scalarInput[1];
  size_t len = args->scalarInput[2];

  if (!kaddr || !uaddr || !len) {
    return kIOReturnBadArgument;
  }

  void *buffer = IOMalloc(len);
  if (!buffer) {
    return kIOReturnNoMemory;
  }

  int copyErr = copyin(uaddr, buffer, len);
  if (copyErr != 0) {
    IOFree(buffer, len);
    return kIOReturnVMError;
  }

  KUError err = KernelUtilities::kwrite(kaddr, buffer, len);
  IOFree(buffer, len);

  if (err != KUErrorSuccess) {
    const PandoraRuntimeState &runtime = pandora_runtime_state();
    PANDORA_USERCLIENT_LOG_ERROR(
        "HwAccessModule::kwrite failed size=%zu addr=0x%llx err=%s(%d) extra=[%llu,%llu,%llu]",
        len, kaddr, get_error_name(err), err,
        runtime.debug.extraErrorData1,
        runtime.debug.extraErrorData2,
        runtime.debug.extraErrorData3);
    return kIOReturnVMError;
  }

  return kIOReturnSuccess;
}

IOReturn HwAccessModule::methodPReadPid(PandoraUserClient *client,
                                        PandoraModule *module,
                                        IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args) {
    return kIOReturnBadArgument;
  }

  pid_t pid = static_cast<pid_t>(args->scalarInput[0]);
  uint64_t paddr = args->scalarInput[1];
  user_addr_t uaddr = args->scalarInput[2];
  size_t len = args->scalarInput[3];

  if (!pid || !paddr || !uaddr || !len) {
    return kIOReturnBadArgument;
  }

  proc_t p = proc_find(pid);
  if (!p) {
    return kIOReturnNotFound;
  }

  task_t t = proc_task(p);
  if (t == TASK_NULL) {
    proc_rele(p);
    return kIOReturnNotFound;
  }

  void *buffer = IOMalloc(len);
  if (!buffer) {
    proc_rele(p);
    return kIOReturnNoMemory;
  }

  KUError err = KernelUtilities::pread(t, paddr, buffer, len);
  if (err != KUErrorSuccess) {
    IOFree(buffer, len);
    proc_rele(p);
    return kIOReturnVMError;
  }

  int copyErr = copyout(buffer, uaddr, len);
  IOFree(buffer, len);
  proc_rele(p);

  return (copyErr == 0) ? kIOReturnSuccess : kIOReturnVMError;
}

IOReturn HwAccessModule::methodPWritePid(PandoraUserClient *client,
                                         PandoraModule *module,
                                         IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args) {
    return kIOReturnBadArgument;
  }

  pid_t pid = static_cast<pid_t>(args->scalarInput[0]);
  user_addr_t uaddr = args->scalarInput[1];
  uint64_t paddr = args->scalarInput[2];
  size_t len = args->scalarInput[3];

  if (!pid || !paddr || !uaddr || !len) {
    return kIOReturnBadArgument;
  }

  proc_t p = proc_find(pid);
  if (!p) {
    return kIOReturnNotFound;
  }

  task_t t = proc_task(p);
  if (t == TASK_NULL) {
    proc_rele(p);
    return kIOReturnNotFound;
  }

  void *buffer = IOMalloc(len);
  if (!buffer) {
    proc_rele(p);
    return kIOReturnNoMemory;
  }

  int copyErr = copyin(uaddr, buffer, len);
  if (copyErr != 0) {
    IOFree(buffer, len);
    proc_rele(p);
    return kIOReturnVMError;
  }

  KUError err = KernelUtilities::pwrite(t, paddr, buffer, len);
  IOFree(buffer, len);
  proc_rele(p);

  return (err == KUErrorSuccess) ? kIOReturnSuccess : kIOReturnVMError;
}

IOReturn HwAccessModule::methodGetKernelBase(PandoraUserClient *client,
                                             PandoraModule *module,
                                             IOExternalMethodArguments *args) {
  (void)client;
  if (!args) {
    return kIOReturnBadArgument;
  }

  HwAccessModule *self = fromModule(module);
  if (!self || !self->service_) {
    return kIOReturnNotReady;
  }

  uint64_t kbase = self->service_->kernelUtilities().kernelBase();
  if (kbase == 0) {
    return kIOReturnError;
  }

  args->scalarOutput[0] = (kbase ^ kKaddrObfuscationKey);
  return kIOReturnSuccess;
}

IOReturn HwAccessModule::methodGetPandoraLoadMetadata(
    PandoraUserClient *client, PandoraModule *module,
    IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args || !args->structureOutput ||
      args->structureOutputSize < sizeof(PandoraMetadata)) {
    return kIOReturnBadArgument;
  }

  const PandoraRuntimeState &runtime = pandora_runtime_state();

  PandoraMetadata metadata = {};
  metadata.kmod_start_time = runtime.telemetry.kmodRan
                                 ? runtime.telemetry.kmodStartTime
                                 : TimestampPair{0, 0, 0};
  metadata.io_service_start_time =
      runtime.telemetry.ioServiceStartCalled
          ? runtime.telemetry.ioServiceStartTime
          : TimestampPair{0, 0, 0};
  metadata.user_client_init_time =
      runtime.telemetry.userClientInitialized
          ? runtime.telemetry.userClientInitTime
          : TimestampPair{0, 0, 0};
  metadata.pid1_exists = runtime.telemetry.pid1Exists;

  memcpy(args->structureOutput, &metadata, sizeof(metadata));
  args->structureOutputSize = sizeof(metadata);
  return kIOReturnSuccess;
}

IOReturn HwAccessModule::methodRunArbFuncWithTaskArgPid(
    PandoraUserClient *client, PandoraModule *module,
    IOExternalMethodArguments *args) {
  (void)client;
  (void)module;

  if (!args) {
    return kIOReturnBadArgument;
  }

  uint64_t funcAddr = args->scalarInput[0];
  pid_t pid = static_cast<pid_t>(args->scalarInput[1]);

  if (!funcAddr || !pid) {
    return kIOReturnBadArgument;
  }

  proc_t p = proc_find(pid);
  if (!p) {
    return kIOReturnNotFound;
  }

  task_t t = proc_task(p);
  if (t == TASK_NULL) {
    proc_rele(p);
    return kIOReturnNotFound;
  }

  uint64_t taskArg = reinterpret_cast<uint64_t>(t);
  PandoraKCallResult res = pandora_kcall(funcAddr, &taskArg, 1);
  proc_rele(p);

  args->scalarOutput[0] = res.ret0;
  return res.status;
}
