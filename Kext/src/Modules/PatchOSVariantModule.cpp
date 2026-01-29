#include "ModuleSystem.h"

#include "../Globals.h"
#include "../Pandora.h"
#include "../Utils/PandoraLog.h"

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>

static constexpr uint32_t kPatchIntervalUS = 50'000;

struct PatchOSVariantState {
  IOTimerEventSource *timer{nullptr};
  uintptr_t unslidAddr{0};
  uint64_t slidAddr{0};
  uint64_t lastValue{0};
};

static PatchOSVariantState g_patch_state = {};

static void patch_osvariant_on_timer(OSObject *owner,
                                     IOTimerEventSource *sender) {
  auto *service = OSDynamicCast(Pandora, owner);
  if (!service || !sender) {
    if (sender)
      sender->setTimeoutUS(kPatchIntervalUS);
    return;
  }

  if (g_patch_state.slidAddr == 0) {
    sender->setTimeoutUS(kPatchIntervalUS);
    return;
  }

  uint64_t val = 0;
  KUError rc = KernelUtilities::kread(g_patch_state.slidAddr, &val, sizeof(val));
  if (rc == KUErrorSuccess) {
    if (val != g_patch_state.lastValue) {
      PANDORA_LOG_DEFAULT("patch_osvariant: value changed: 0x%016llx -> 0x%016llx",
                          (unsigned long long)g_patch_state.lastValue,
                          (unsigned long long)val);
      g_patch_state.lastValue = val;
    }

    if (val == 0) {
      PANDORA_LOG_DEFAULT("patch_osvariant: performing kwrite when see 0...");
      uint64_t newVal = 0x70010000f38882cf;
      rc = KernelUtilities::kwrite(g_patch_state.slidAddr, &newVal, sizeof(newVal));
      if (rc == KUErrorSuccess) {
        PANDORA_LOG_DEFAULT("patch_osvariant: kwrite succeeded");
      } else {
        PANDORA_LOG_DEFAULT("patch_osvariant: kwrite failed: %d", rc);
      }
      workloopsaw0 = true;
    }
  } else {
    PANDORA_LOG_DEFAULT("patch_osvariant: kread failed: %d", rc);
  }

  sender->setTimeoutUS(kPatchIntervalUS);
}

void pandora_patch_osvariant_stop(Pandora *service);

IOReturn pandora_patch_osvariant_start(Pandora *service) {
  if (!service) {
    return kIOReturnBadArgument;
  }

  // Ensure clean state if we're restarted.
  pandora_patch_osvariant_stop(service);

  // Init kernel utilities and version utilities through the service.
  KUError kuStatus = service->kernelUtilities().init();
  if (kuStatus != KUErrorSuccess) {
    PANDORA_LOG_DEFAULT("patch_osvariant: KernelUtilities init failed: %d",
                        kuStatus);
    return kIOReturnError;
  }

  if (!service->versionUtilities().init()) {
    PANDORA_LOG_DEFAULT("patch_osvariant: VersionUtilities init failed");
    return kIOReturnError;
  }

  if (!service->versionUtilities().isSupportedVersion()) {
    PANDORA_LOG_DEFAULT(
        "patch_osvariant: unsupported macOS/device: %s / %s",
        service->versionUtilities().getBuildVersion(),
        service->versionUtilities().getModelIdentifier());
    return kIOReturnUnsupported;
  }

  uintptr_t unslid = 0;
  if (!service->versionUtilities().getOsVariantStatusBacking(&unslid) ||
      unslid == 0 || unslid == kNoHook) {
    PANDORA_LOG_DEFAULT(
        "patch_osvariant: failed to get osvariant_status_backing address");
    return kIOReturnNotFound;
  }

  uint64_t slid = service->kernelUtilities().kslide(unslid);

  IOWorkLoop *wl = service->getWorkLoop();
  if (!wl) {
    PANDORA_LOG_DEFAULT("patch_osvariant: no workloop available");
    return kIOReturnNoResources;
  }

  IOTimerEventSource *timer =
      IOTimerEventSource::timerEventSource(service, patch_osvariant_on_timer);
  if (!timer) {
    PANDORA_LOG_DEFAULT("patch_osvariant: failed to create timer");
    return kIOReturnNoResources;
  }

  IOReturn addRc = wl->addEventSource(timer);
  if (addRc != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT(
        "patch_osvariant: failed to add timer to workloop: 0x%x", addRc);
    timer->release();
    return addRc;
  }

  g_patch_state.timer = timer;
  g_patch_state.unslidAddr = unslid;
  g_patch_state.slidAddr = slid;
  g_patch_state.lastValue = 0;

  PANDORA_LOG_DEFAULT("patch_osvariant: started (unslid=0x%llx slid=0x%llx)",
                      (unsigned long long)unslid,
                      (unsigned long long)slid);

  timer->setTimeoutUS(kPatchIntervalUS);
  return kIOReturnSuccess;
}

void pandora_patch_osvariant_stop(Pandora *service) {
  IOTimerEventSource *timer = g_patch_state.timer;
  if (timer) {
    timer->cancelTimeout();
    if (service) {
      IOWorkLoop *wl = service->getWorkLoop();
      if (wl) {
        wl->removeEventSource(timer);
      }
    }
    timer->release();
  }

  g_patch_state = {};
  (void)service;
}

void pandora_patch_osvariant_error(Pandora *service, IOReturn error) {
  PANDORA_LOG_DEFAULT("patch_osvariant: error 0x%x", error);
  pandora_patch_osvariant_stop(service);
}

void pandora_patch_osvariant_shutdown() {
  g_patch_state = {};
}
