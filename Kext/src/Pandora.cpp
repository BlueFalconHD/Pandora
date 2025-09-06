#include "Pandora.h"
#include "Globals.h"
#include "Utils/KernelUtilities.h"
#include "Utils/PandoraLog.h"
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndFinalStructors(Pandora, IOService);

bool Pandora::start(IOService *provider) {
  if (!super::start(provider))
    return false;

  pandora_log_ensure_initialized();

  // Init KU once and compute slid target
  auto st = ku_.init();
  if (st != KUErrorSuccess) {
    PANDORA_LOG_DEFAULT("KernelUtilities init failed: %d", st);
    super::stop(provider);
    return false;
  }

  PANDORA_LOG_DEFAULT("KernelUtilities init succeeded");

  if (!vu_.init()) {
    PANDORA_LOG_DEFAULT("VersionUtilities init failed");
    super::stop(provider);
    return false;
  }

  PANDORA_LOG_DEFAULT("VersionUtilities init succeeded");

  if (!vu_.isSupportedVersion()) {
    PANDORA_LOG_DEFAULT("Unsupported macOS version for this machine: %s",
                        vu_.getBuildVersion());
    goto fail;
  }

  vu_.getOsVariantStatusBacking(&unslid_addr_);
  if (unslid_addr_ == 0 || unslid_addr_ == kNoHook) {
    PANDORA_LOG_DEFAULT(
        "Failed to get osvariant_status_backing address, cannot start Pandora");
    goto fail;
  }

  slid_addr_ = ku_.kslide(unslid_addr_);

  workloop_ = IOWorkLoop::workLoop();
  if (!workloop_) {
    PANDORA_LOG_DEFAULT("Failed to create IOWorkLoop");
    goto fail;
  }

  PANDORA_LOG_DEFAULT("IOWorkLoop created successfully");

  timer_ = IOTimerEventSource::timerEventSource(this, onTimer);
  if (!timer_) {
    PANDORA_LOG_DEFAULT("Failed to create IOTimerEventSource");
    goto fail;
  }

  PANDORA_LOG_DEFAULT("IOTimerEventSource created successfully");

  if (workloop_->addEventSource(timer_) != kIOReturnSuccess) {
    PANDORA_LOG_DEFAULT("Failed to add IOTimerEventSource to workloop");
    goto fail;
  }

  PANDORA_LOG_DEFAULT("IOTimerEventSource added to workloop successfully");

  timer_->setTimeoutUS(50'000); // 10 ms
  registerService();
  return true;

fail:
  PANDORA_LOG_DEFAULT("Failed to start Pandora service, cleaning up resources");

  if (timer_) {
    PANDORA_LOG_DEFAULT("Cleaning up timer event source");
    timer_->release();
    timer_ = nullptr;
  }
  if (workloop_) {
    PANDORA_LOG_DEFAULT("Cleaning up workloop");
    workloop_->release();
    workloop_ = nullptr;
  }
  pandora_log_cleanup();
  super::stop(provider);
  return false;
}

void Pandora::stop(IOService *provider) {
  PANDORA_LOG_DEFAULT("Stopping Pandora service");

  if (timer_) {
    timer_->cancelTimeout();
    if (workloop_)
      workloop_->removeEventSource(timer_);
    timer_->release();
    timer_ = nullptr;
  }
  if (workloop_) {
    workloop_->release();
    workloop_ = nullptr;
  }
  pandora_log_cleanup();
  super::stop(provider);
}

void Pandora::free() {
  PANDORA_LOG_DEFAULT("Freeing Pandora service");

  if (timer_) {
    timer_->cancelTimeout();
    if (workloop_)
      workloop_->removeEventSource(timer_);
    timer_->release();
    timer_ = nullptr;
  }
  if (workloop_) {
    workloop_->release();
    workloop_ = nullptr;
  }
  super::free();
}

void Pandora::onTimer(OSObject *owner, IOTimerEventSource *sender) {
  auto *self = OSDynamicCast(Pandora, owner);
  if (!self || self->slid_addr_ == 0) {
    sender->setTimeoutUS(50'000);
    return;
  }

  uint64_t val = 0;
  auto rc = KernelUtilities::kread(self->slid_addr_, &val, sizeof(val));
  if (rc == KUErrorSuccess) {
    if (val != self->last_value_) {
      PANDORA_LOG_DEFAULT("Value changed: 0x%016llx -> 0x%016llx",
                          (unsigned long long)self->last_value_,
                          (unsigned long long)val);
      self->last_value_ = val;
    }
    // PANDORA_LOG_DEFAULT("kread succeeded: 0x%016llx", (unsigned long
    // long)val);
    if (val == 0) {
      PANDORA_LOG_DEFAULT("Performing kwrite when see 0...");
      uint64_t newVal = 0x70010000f38882cf;
      rc = KernelUtilities::kwrite(self->slid_addr_, &newVal, sizeof(newVal));
      if (rc == KUErrorSuccess) {
        PANDORA_LOG_DEFAULT("kwrite succeeded");
      } else {
        PANDORA_LOG_DEFAULT("kwrite failed: %d", rc);
      }
      workloopsaw0 = true;
    }
  } else {
    PANDORA_LOG_DEFAULT("kread failed: %d", rc);
  }

  sender->setTimeoutUS(50'000); // re-arm
}
