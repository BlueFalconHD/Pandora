#include "ModuleSystem.h"

#include "HwAccessModule.h"
#include "PatchOSVariantModule.h"
#include "../Pandora.h"
#include "../Utils/PandoraLog.h"

#include <pexpert/pexpert.h>
#include <string.h>

namespace {

static constexpr size_t kMaxModules = 16;
static constexpr size_t kMaxUserClientMethods = 128;

struct PandoraModuleRuntime {
  PandoraModule *module;
  bool enabled;
  bool started;
  IOReturn startError;
};

struct PandoraUserClientMethodCookie {
  PandoraModule *module;
  PandoraUserClientMethodHandler handler;
};

struct PandoraUserClientMethodRuntime {
  uint32_t selector;
  IOExternalMethodDispatch dispatch;
  PandoraUserClientMethodCookie cookie;
};

PandoraModuleRuntime g_modules[kMaxModules] = {};
size_t g_module_count = 0;
bool g_defaults_registered = false;

PandoraUserClientMethodRuntime g_userclient_methods[kMaxUserClientMethods] = {};
size_t g_userclient_method_count = 0;

static HwAccessModule g_hw_access_module;
static PatchOSVariantModule g_patch_osvariant_module;

PandoraModuleRuntime *find_module_runtime(uint16_t moduleId) {
  for (size_t i = 0; i < g_module_count; ++i) {
    if (g_modules[i].module &&
        g_modules[i].module->descriptor().identifier == moduleId) {
      return &g_modules[i];
    }
  }

  return nullptr;
}

bool register_module(PandoraModule *module) {
  if (!module) {
    return false;
  }

  if (g_module_count >= kMaxModules) {
    PANDORA_LOG_DEFAULT("Module registry full, dropping module: %s",
                        module->descriptor().name);
    return false;
  }

  if (find_module_runtime(module->descriptor().identifier)) {
    PANDORA_LOG_DEFAULT("Duplicate module id 0x%x for module %s",
                        module->descriptor().identifier,
                        module->descriptor().name);
    return false;
  }

  PandoraModuleRuntime runtime = {};
  runtime.module = module;
  runtime.enabled = false;
  runtime.started = false;
  runtime.startError = kIOReturnSuccess;
  g_modules[g_module_count++] = runtime;

  return true;
}

void ensure_defaults_registered() {
  if (g_defaults_registered) {
    return;
  }

  register_module(&g_hw_access_module);
  register_module(&g_patch_osvariant_module);
  g_defaults_registered = true;
}

bool module_enabled_from_boot_args(const PandoraModuleDescriptor &desc) {
  if (pandora_bootarg_enabled(desc.bootArg, desc.defaultEnabled)) {
    return true;
  }

  if (desc.altBootArg && desc.altBootArg[0] != '\0') {
    return pandora_bootarg_enabled(desc.altBootArg, false);
  }

  return false;
}

void clear_userclient_methods() {
  g_userclient_method_count = 0;
  bzero(g_userclient_methods, sizeof(g_userclient_methods));
}

IOReturn add_userclient_method(PandoraModule &module, uint16_t localSelector,
                               PandoraUserClientMethodHandler handler,
                               uint32_t checkScalarInputCount,
                               uint32_t checkStructureInputSize,
                               uint32_t checkScalarOutputCount,
                               uint32_t checkStructureOutputSize) {
  if (!handler) {
    return kIOReturnBadArgument;
  }

  if (g_userclient_method_count >= kMaxUserClientMethods) {
    PANDORA_LOG_DEFAULT("Too many userclient methods, module=%s",
                        module.descriptor().name);
    return kIOReturnNoResources;
  }

  const uint32_t selector =
      pandora_compose_selector(module.descriptor().identifier, localSelector);

  for (size_t i = 0; i < g_userclient_method_count; ++i) {
    if (g_userclient_methods[i].selector == selector) {
      PANDORA_LOG_DEFAULT("Duplicate userclient selector 0x%x from module %s",
                          selector, module.descriptor().name);
      return kIOReturnExclusiveAccess;
    }
  }

  PandoraUserClientMethodRuntime &entry =
      g_userclient_methods[g_userclient_method_count++];
  entry.selector = selector;
  entry.dispatch = {
      (IOExternalMethodAction)&pandora_modules_userclient_dispatch,
      checkScalarInputCount,
      checkStructureInputSize,
      checkScalarOutputCount,
      checkStructureOutputSize,
  };
  entry.cookie.module = &module;
  entry.cookie.handler = handler;

  PANDORA_LOG_DEFAULT("Registered userclient method module=%s id=0x%x local=%u",
                      module.descriptor().name, selector, localSelector);
  return kIOReturnSuccess;
}

IOReturn rebuild_userclient_dispatch_table() {
  clear_userclient_methods();

  for (size_t i = 0; i < g_module_count; ++i) {
    PandoraModuleRuntime &runtime = g_modules[i];
    if (!runtime.started || !runtime.module) {
      continue;
    }

    PandoraUserClientMethodRegistrar registrar(*runtime.module);
    runtime.module->registerUserClientMethods(registrar);
  }

  PANDORA_LOG_DEFAULT("Userclient dispatch table has %zu methods",
                      g_userclient_method_count);
  return kIOReturnSuccess;
}

} // namespace

void PandoraModule::onError(Pandora &service, IOReturn error) {
  (void)service;
  (void)error;
}

void PandoraModule::onShutdown() {}

bool PandoraModule::handlesUserClient() const {
  return false;
}

bool PandoraModule::authorizeUserClient(task_t owningTask, void *securityID,
                                        uint32_t type) {
  (void)owningTask;
  (void)securityID;
  (void)type;
  return false;
}

void PandoraModule::registerUserClientMethods(
    PandoraUserClientMethodRegistrar &registrar) {
  (void)registrar;
}

IOReturn PandoraUserClientMethodRegistrar::addMethod(
    uint16_t localSelector, PandoraUserClientMethodHandler handler,
    uint32_t checkScalarInputCount, uint32_t checkStructureInputSize,
    uint32_t checkScalarOutputCount, uint32_t checkStructureOutputSize) {
  return add_userclient_method(module_, localSelector, handler,
                               checkScalarInputCount,
                               checkStructureInputSize,
                               checkScalarOutputCount,
                               checkStructureOutputSize);
}

bool pandora_bootarg_enabled(const char *name, bool defaultEnabled) {
  if (!name || name[0] == '\0') {
    return defaultEnabled;
  }

  char buf[64] = {};
  if (!PE_parse_boot_argn(name, buf, sizeof(buf))) {
    return defaultEnabled;
  }

  if (buf[0] == '\0') {
    return true;
  }

  if (buf[0] == '0') {
    return false;
  }

  if (buf[0] == 'f' || buf[0] == 'F' || buf[0] == 'n' || buf[0] == 'N') {
    return false;
  }

  return true;
}

void pandora_modules_register_defaults() {
  ensure_defaults_registered();
}

void pandora_modules_configure_from_boot_args() {
  ensure_defaults_registered();

  for (size_t i = 0; i < g_module_count; ++i) {
    PandoraModuleRuntime &runtime = g_modules[i];
    const PandoraModuleDescriptor &desc = runtime.module->descriptor();

    runtime.enabled = module_enabled_from_boot_args(desc);
    runtime.started = false;
    runtime.startError = kIOReturnSuccess;

    PANDORA_LOG_DEFAULT("Module %s (id=0x%x, boot-arg=%s): %s", desc.name,
                        desc.identifier, desc.bootArg,
                        runtime.enabled ? "enabled" : "disabled");
  }

  clear_userclient_methods();
}

IOReturn pandora_modules_start(Pandora *service) {
  if (!service) {
    return kIOReturnBadArgument;
  }

  ensure_defaults_registered();

  for (size_t i = 0; i < g_module_count; ++i) {
    PandoraModuleRuntime &runtime = g_modules[i];
    if (!runtime.enabled) {
      continue;
    }

    const PandoraModuleDescriptor &desc = runtime.module->descriptor();
    PANDORA_LOG_DEFAULT("Starting module: %s", desc.name);

    IOReturn rc = runtime.module->onStart(*service);
    if (rc == kIOReturnSuccess) {
      runtime.started = true;
      continue;
    }

    runtime.startError = rc;
    PANDORA_LOG_DEFAULT("Module %s failed to start: 0x%x", desc.name, rc);
    runtime.module->onError(*service, rc);

    for (size_t j = i; j-- > 0;) {
      PandoraModuleRuntime &rollbackRuntime = g_modules[j];
      if (rollbackRuntime.started) {
        rollbackRuntime.module->onStop(*service);
        rollbackRuntime.started = false;
      }
    }

    clear_userclient_methods();
    return rc;
  }

  IOReturn methodRc = rebuild_userclient_dispatch_table();
  if (methodRc != kIOReturnSuccess) {
    pandora_modules_stop(service);
    return methodRc;
  }

  return kIOReturnSuccess;
}

void pandora_modules_stop(Pandora *service) {
  if (!service) {
    return;
  }

  for (size_t i = g_module_count; i-- > 0;) {
    PandoraModuleRuntime &runtime = g_modules[i];
    if (!runtime.started) {
      continue;
    }

    runtime.module->onStop(*service);
    runtime.started = false;
  }

  clear_userclient_methods();
}

void pandora_modules_shutdown() {
  ensure_defaults_registered();

  for (size_t i = 0; i < g_module_count; ++i) {
    PandoraModuleRuntime &runtime = g_modules[i];
    runtime.module->onShutdown();
    runtime.enabled = false;
    runtime.started = false;
    runtime.startError = kIOReturnSuccess;
  }

  clear_userclient_methods();
}

size_t pandora_modules_count() {
  ensure_defaults_registered();
  return g_module_count;
}

bool pandora_modules_get_info(size_t index, PandoraModuleInfo *out) {
  ensure_defaults_registered();
  if (!out || index >= g_module_count) {
    return false;
  }

  const PandoraModuleRuntime &runtime = g_modules[index];
  out->descriptor = runtime.module->descriptor();
  out->enabled = runtime.enabled;
  out->started = runtime.started;
  out->startError = runtime.startError;
  return true;
}

bool pandora_modules_is_started(uint16_t moduleId) {
  const PandoraModuleRuntime *runtime = find_module_runtime(moduleId);
  return runtime ? runtime->started : false;
}

bool pandora_modules_authorize_user_client(task_t owningTask, void *securityID,
                                           uint32_t type) {
  bool sawUserClientModule = false;
  bool allow = false;

  for (size_t i = 0; i < g_module_count; ++i) {
    const PandoraModuleRuntime &runtime = g_modules[i];
    if (!runtime.started || !runtime.module->handlesUserClient()) {
      continue;
    }

    sawUserClientModule = true;
    if (runtime.module->authorizeUserClient(owningTask, securityID, type)) {
      allow = true;
    }
  }

  return sawUserClientModule && allow;
}

bool pandora_modules_lookup_userclient_method(uint32_t selector,
                                              PandoraUserClientMethodLookup *out) {
  if (!out) {
    return false;
  }

  for (size_t i = 0; i < g_userclient_method_count; ++i) {
    const PandoraUserClientMethodRuntime &entry = g_userclient_methods[i];
    if (entry.selector == selector) {
      out->dispatch = &entry.dispatch;
      out->reference = const_cast<PandoraUserClientMethodCookie *>(&entry.cookie);
      return true;
    }
  }

  return false;
}

IOReturn pandora_modules_userclient_dispatch(PandoraUserClient *client,
                                             void *reference,
                                             IOExternalMethodArguments *args) {
  if (!client || !reference || !args) {
    return kIOReturnBadArgument;
  }

  auto *cookie = static_cast<PandoraUserClientMethodCookie *>(reference);
  if (!cookie->module || !cookie->handler) {
    return kIOReturnBadArgument;
  }

  return cookie->handler(client, cookie->module, args);
}
