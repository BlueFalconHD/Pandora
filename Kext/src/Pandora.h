#ifndef PANDORA_H
#define PANDORA_H

#include "Utils/KernelUtilities.h"
#include "Utils/VersionUtilities.h"
#include <IOKit/IOService.h>

class Pandora final : public IOService {
OSDeclareFinalStructors(Pandora)

    public : bool start(IOService *provider) override;
  void stop(IOService *provider) override;
  void free() override;
  IOWorkLoop *getWorkLoop() const override { return workloop_; }

  KernelUtilities &kernelUtilities() { return ku_; }
  const KernelUtilities &kernelUtilities() const { return ku_; }
  VersionUtilities &versionUtilities() { return vu_; }
  const VersionUtilities &versionUtilities() const { return vu_; }

private:
  IOWorkLoop *workloop_{nullptr};

  KernelUtilities ku_; // single instance
  VersionUtilities vu_;
};

#endif // PANDORA_H
