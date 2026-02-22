#pragma once

#include <IOKit/IOReturn.h>
#include <IOKit/IOUserClient.h>
#include <kern/task.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

class Pandora;
class PandoraModule;
class PandoraUserClient;

static constexpr uint32_t kPandoraModuleSelectorShift = 16u;

static inline constexpr uint32_t pandora_compose_selector(uint16_t moduleId,
                                                           uint16_t local) {
  return (static_cast<uint32_t>(moduleId) << kPandoraModuleSelectorShift) |
         static_cast<uint32_t>(local);
}

struct PandoraModuleDescriptor {
  uint16_t identifier;
  const char *name;
  const char *bootArg;
  const char *altBootArg;
  bool defaultEnabled;
};

using PandoraUserClientMethodHandler =
    IOReturn (*)(PandoraUserClient *client, PandoraModule *module,
                 IOExternalMethodArguments *args);

class PandoraUserClientMethodRegistrar {
public:
  explicit PandoraUserClientMethodRegistrar(PandoraModule &module)
      : module_(module) {}

  IOReturn addMethod(uint16_t localSelector,
                     PandoraUserClientMethodHandler handler,
                     uint32_t checkScalarInputCount,
                     uint32_t checkStructureInputSize,
                     uint32_t checkScalarOutputCount,
                     uint32_t checkStructureOutputSize);

private:
  PandoraModule &module_;
};

class PandoraModule {
public:
  virtual ~PandoraModule() = default;

  virtual const PandoraModuleDescriptor &descriptor() const = 0;

  virtual IOReturn onStart(Pandora &service) = 0;
  virtual void onStop(Pandora &service) = 0;

  virtual void onError(Pandora &service, IOReturn error);
  virtual void onShutdown();

  virtual bool handlesUserClient() const;
  virtual bool authorizeUserClient(task_t owningTask, void *securityID,
                                   uint32_t type);
  virtual void
  registerUserClientMethods(PandoraUserClientMethodRegistrar &registrar);
};

struct PandoraModuleInfo {
  PandoraModuleDescriptor descriptor;
  bool enabled;
  bool started;
  IOReturn startError;
};

struct PandoraUserClientMethodLookup {
  const IOExternalMethodDispatch *dispatch;
  void *reference;
};

bool pandora_bootarg_enabled(const char *name, bool defaultEnabled);

void pandora_modules_register_defaults();
void pandora_modules_configure_from_boot_args();
IOReturn pandora_modules_start(Pandora *service);
void pandora_modules_stop(Pandora *service);
void pandora_modules_shutdown();

size_t pandora_modules_count();
bool pandora_modules_get_info(size_t index, PandoraModuleInfo *out);
bool pandora_modules_is_started(uint16_t moduleId);

bool pandora_modules_authorize_user_client(task_t owningTask, void *securityID,
                                           uint32_t type);
bool pandora_modules_lookup_userclient_method(uint32_t selector,
                                              PandoraUserClientMethodLookup *out);
IOReturn pandora_modules_userclient_dispatch(PandoraUserClient *client,
                                             void *reference,
                                             IOExternalMethodArguments *args);

