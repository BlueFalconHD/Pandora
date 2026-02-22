#include "PatchOSVariantModule.h"

#include "../Globals.h"
#include "../Pandora.h"
#include "../Utils/PandoraLog.h"

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>

PatchOSVariantModule *PatchOSVariantModule::activeInstance_ = nullptr;

const PandoraModuleDescriptor &PatchOSVariantModule::descriptor() const {
  static const PandoraModuleDescriptor kDescriptor = {
      kModuleId,
      "patch_osvariant",
      "pandora_enable_osvariant",
      "pandora_enable_patch_osvarient",
      false,
  };
  return kDescriptor;
}

IOReturn PatchOSVariantModule::onStart(Pandora &service) {
  onStop(service);

  service_ = &service;

  KUError kuStatus = service.kernelUtilities().init();
  if (kuStatus != KUErrorSuccess) {
    PANDORA_LOG_DEFAULT("patch_osvariant: KernelUtilities init failed: %d",
                        kuStatus);
    resetState();
    return kIOReturnError;
  }

  if (!service.versionUtilities().init()) {
    PANDORA_LOG_DEFAULT("patch_osvariant: VersionUtilities init failed");
    resetState();
    return kIOReturnError;
  }

  if (!service.versionUtilities().isSupportedVersion()) {
    PANDORA_LOG_DEFAULT("patch_osvariant: unsupported macOS/device: %s / %s",
                        service.versionUtilities().getBuildVersion(),
                        service.versionUtilities().getModelIdentifier());
    resetState();
    return kIOReturnUnsupported;
  }

  uintptr_t unslid = 0;
  if (!service.versionUtilities().getOsVariantStatusBacking(&unslid) ||
      unslid == 0 || unslid == kNoHook) {
    PANDORA_LOG_DEFAULT(
        "patch_osvariant: failed to get osvariant_status_backing address");
    resetState();
    return kIOReturnNotFound;
  }

  uint64_t slid = service.kernelUtilities().kslide(unslid);

  IOWorkLoop *workLoop = service.getWorkLoop();
  if (!workLoop) {
    PANDORA_LOG_DEFAULT("patch_osvariant: no workloop available");
    resetState();
    return kIOReturnNoResources;
  }

  IOTimerEventSource *timer =
      IOTimerEventSource::timerEventSource(&service, timerHandler);
  if (!timer) {
    PANDORA_LOG_DEFAULT("patch_osvariant: failed to create timer");
    resetState();
    return kIOReturnNoResources;
  }

  IOReturn addRc = workLoop->addEventSource(timer);
  if (addRc != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT(
        "patch_osvariant: failed to add timer to workloop: 0x%x", addRc);
    timer->release();
    resetState();
    return addRc;
  }

  timer_ = timer;
  unslidAddr_ = unslid;
  slidAddr_ = slid;
  lastValue_ = 0;
  activeInstance_ = this;

  PANDORA_LOG_DEFAULT("patch_osvariant: started (unslid=0x%llx slid=0x%llx)",
                      static_cast<unsigned long long>(unslid),
                      static_cast<unsigned long long>(slid));

  timer_->setTimeoutUS(kPatchIntervalUS);
  return kIOReturnSuccess;
}

void PatchOSVariantModule::onStop(Pandora &service) {
  (void)service;

  if (timer_) {
    timer_->cancelTimeout();
    if (service_) {
      IOWorkLoop *workLoop = service_->getWorkLoop();
      if (workLoop) {
        workLoop->removeEventSource(timer_);
      }
    }
    timer_->release();
  }

  if (activeInstance_ == this) {
    activeInstance_ = nullptr;
  }

  resetState();
}

void PatchOSVariantModule::onError(Pandora &service, IOReturn error) {
  PANDORA_LOG_DEFAULT("patch_osvariant: error 0x%x", error);
  onStop(service);
}

void PatchOSVariantModule::onShutdown() {
  if (activeInstance_ == this) {
    activeInstance_ = nullptr;
  }
  resetState();
}

void PatchOSVariantModule::timerHandler(OSObject *owner,
                                        IOTimerEventSource *sender) {
  (void)owner;

  if (!sender || !activeInstance_) {
    if (sender) {
      sender->setTimeoutUS(kPatchIntervalUS);
    }
    return;
  }

  activeInstance_->runTimerTick(sender);
}

void PatchOSVariantModule::runTimerTick(IOTimerEventSource *sender) {
  if (!sender || !service_) {
    if (sender) {
      sender->setTimeoutUS(kPatchIntervalUS);
    }
    return;
  }

  if (slidAddr_ == 0) {
    sender->setTimeoutUS(kPatchIntervalUS);
    return;
  }

  uint64_t value = 0;
  KUError rc = KernelUtilities::kread(slidAddr_, &value, sizeof(value));
  if (rc != KUErrorSuccess) {
    PANDORA_LOG_DEFAULT("patch_osvariant: kread failed: %d", rc);
    sender->setTimeoutUS(kPatchIntervalUS);
    return;
  }

  if (value != lastValue_) {
    PANDORA_LOG_DEFAULT("patch_osvariant: value changed 0x%016llx -> 0x%016llx",
                        static_cast<unsigned long long>(lastValue_),
                        static_cast<unsigned long long>(value));
    lastValue_ = value;
  }

  if (value == 0) {
    uint64_t newValue = 0x70010000f38882cf;
    rc = KernelUtilities::kwrite(slidAddr_, &newValue, sizeof(newValue));
    if (rc == KUErrorSuccess) {
      pandora_runtime_state().telemetry.workloopSawZero = true;
      PANDORA_LOG_DEFAULT("patch_osvariant: kwrite succeeded");
    } else {
      PANDORA_LOG_DEFAULT("patch_osvariant: kwrite failed: %d", rc);
    }
  }

  sender->setTimeoutUS(kPatchIntervalUS);
}

void PatchOSVariantModule::resetState() {
  timer_ = nullptr;
  unslidAddr_ = 0;
  slidAddr_ = 0;
  lastValue_ = 0;
  service_ = nullptr;
}

