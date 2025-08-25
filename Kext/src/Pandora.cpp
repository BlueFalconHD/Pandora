#include "Pandora.h"
#include "Globals.h"
#include "KernelUtilities.h"
#include "PandoraLog.h"
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndFinalStructors(Pandora, IOService)

    bool Pandora::start(IOService *provider) {
  if (!super::start(provider))
    return false;

  pandora_log_ensure_initialized();

  // Init KU once and compute slid target
  auto st = ku_.init();
  if (st != KUErrorSuccess) {
    PANDORA_KERNEL_LOG_ERROR("KernelUtilities init failed: %d", st);
    super::stop(provider);
    return false;
  }
  slid_addr_ = ku_.kslide(kUnslid);
  PANDORA_KERNEL_LOG_INFO("Polling unslid=0x%llx slid=0x%llx",
                          (unsigned long long)kUnslid,
                          (unsigned long long)slid_addr_);

  workloop_ = IOWorkLoop::workLoop();
  if (!workloop_)
    goto fail;

  timer_ = IOTimerEventSource::timerEventSource(this, onTimer);
  if (!timer_)
    goto fail;

  if (workloop_->addEventSource(timer_) != kIOReturnSuccess)
    goto fail;

  timer_->setTimeoutUS(50'000); // 10 ms
  registerService();
  return true;

fail:
  if (timer_) {
    timer_->release();
    timer_ = nullptr;
  }
  if (workloop_) {
    workloop_->release();
    workloop_ = nullptr;
  }
  pandora_log_cleanup();
  super::stop(provider);
  return false;
}

void Pandora::stop(IOService *provider) {
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
    sender->setTimeoutUS(10'000);
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
      uint64_t newVal = 0x70010002f388828f;
      rc = KernelUtilities::kwrite(self->slid_addr_, &newVal, sizeof(newVal));
      if (rc == KUErrorSuccess) {
        PANDORA_LOG_DEFAULT("kwrite succeeded");
      } else {
        PANDORA_LOG_DEFAULT("kwrite failed: %d", rc);
      }
      workloopsaw0 = true;
    }
  } else {
    // PANDORA_LOG_DEFAULT("kread failed: %d", rc);
  }

  sender->setTimeoutUS(50'000); // re-arm
}
