#ifndef PANDORA_H
#define PANDORA_H

#include "KernelUtilities.h"
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
  static constexpr uint64_t kUnslid{0xfffffe000c527cf0ULL};
  uint64_t slid_addr_{0}; // computed once after ku_.init()

  uint64_t last_value_{0};
};

#endif // PANDORA_H
