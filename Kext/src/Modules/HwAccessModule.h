#pragma once

#include "ModuleSystem.h"

class Pandora;

class HwAccessModule final : public PandoraModule {
public:
  static constexpr uint16_t kModuleId = 0x0001;

  const PandoraModuleDescriptor &descriptor() const override;

  IOReturn onStart(Pandora &service) override;
  void onStop(Pandora &service) override;
  void onError(Pandora &service, IOReturn error) override;
  void onShutdown() override;

  bool handlesUserClient() const override;
  bool authorizeUserClient(task_t owningTask, void *securityID,
                           uint32_t type) override;
  void registerUserClientMethods(
      PandoraUserClientMethodRegistrar &registrar) override;

private:
  Pandora *service_{nullptr};
  bool active_{false};

  static HwAccessModule *fromModule(PandoraModule *module);

  static IOReturn methodKRead(PandoraUserClient *client, PandoraModule *module,
                              IOExternalMethodArguments *args);
  static IOReturn methodKWrite(PandoraUserClient *client, PandoraModule *module,
                               IOExternalMethodArguments *args);
  static IOReturn methodGetKernelBase(PandoraUserClient *client,
                                      PandoraModule *module,
                                      IOExternalMethodArguments *args);
  static IOReturn methodGetPandoraLoadMetadata(PandoraUserClient *client,
                                               PandoraModule *module,
                                               IOExternalMethodArguments *args);
  static IOReturn methodPReadPid(PandoraUserClient *client,
                                 PandoraModule *module,
                                 IOExternalMethodArguments *args);
  static IOReturn methodPWritePid(PandoraUserClient *client,
                                  PandoraModule *module,
                                  IOExternalMethodArguments *args);
  static IOReturn methodKCall(PandoraUserClient *client, PandoraModule *module,
                              IOExternalMethodArguments *args);
  static IOReturn methodRunArbFuncWithTaskArgPid(
      PandoraUserClient *client, PandoraModule *module,
      IOExternalMethodArguments *args);
};

