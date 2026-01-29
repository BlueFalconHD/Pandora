#pragma once

#include <IOKit/IOReturn.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

class Pandora;

struct PandoraModule {
  const char *name;
  const char *bootArg;
  const char *altBootArg; // optional
  bool defaultEnabled;

  bool enabled;
  bool started;
  IOReturn startError;

  IOReturn (*start)(Pandora *service);
  void (*stop)(Pandora *service);
  void (*onError)(Pandora *service, IOReturn error);
  void (*shutdown)();
};

// Parse a boolean-ish boot-arg.
// - If missing: returns defaultEnabled.
// - If present without value: true.
// - If value is "0", "false", "no": false, otherwise true.
bool pandora_bootarg_enabled(const char *name, bool defaultEnabled);

// Module system lifecycle.
void pandora_modules_configure_from_boot_args();
IOReturn pandora_modules_start(Pandora *service);
void pandora_modules_stop(Pandora *service);
void pandora_modules_shutdown();

// Introspection helpers.
size_t pandora_modules_count();
const PandoraModule *pandora_modules_get(size_t *count);
