#pragma once

#include "ModuleSystem.h"

class IOTimerEventSource;
class Pandora;

class PatchOSVariantModule final : public PandoraModule {
public:
  static constexpr uint16_t kModuleId = 0x0002;

  const PandoraModuleDescriptor &descriptor() const override;

  IOReturn onStart(Pandora &service) override;
  void onStop(Pandora &service) override;
  void onError(Pandora &service, IOReturn error) override;
  void onShutdown() override;

private:
  static constexpr uint32_t kPatchIntervalUS = 50000;

  IOTimerEventSource *timer_{nullptr};
  uintptr_t unslidAddr_{0};
  uint64_t slidAddr_{0};
  uint64_t lastValue_{0};
  Pandora *service_{nullptr};

  static PatchOSVariantModule *activeInstance_;

  static void timerHandler(OSObject *owner, IOTimerEventSource *sender);
  void runTimerTick(IOTimerEventSource *sender);
  void resetState();
};

