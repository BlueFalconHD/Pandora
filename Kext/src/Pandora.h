#ifndef PANDORA_H
#define PANDORA_H

#include "Utils/KernelUtilities.h"
#include "Utils/VersionUtilities.h"
#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>

class Pandora final : public IOService {
OSDeclareFinalStructors(Pandora)

    public : bool start(IOService *provider) override;
  void stop(IOService *provider) override;
  void free() override;
  IOWorkLoop *getWorkLoop() const override { return workloop_; }

private:
  static void onTimer(OSObject *owner, IOTimerEventSource *sender);

  IOWorkLoop *workloop_{nullptr};
  IOTimerEventSource *timer_{nullptr};

  KernelUtilities ku_; // single instance
  VersionUtilities vu_;

  uintptr_t unslid_addr_{0};
  uint64_t slid_addr_{0}; // computed once after ku_.init()

  uint64_t last_value_{0};
  bool disable_osvariant_{false};
};

#endif // PANDORA_H
