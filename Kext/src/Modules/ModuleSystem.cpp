#include "ModuleSystem.h"

#include "../Utils/PandoraLog.h"

#include <pexpert/pexpert.h>
#include <string.h>

// Module hook declarations
IOReturn pandora_hw_access_start(Pandora *service);
void pandora_hw_access_stop(Pandora *service);
void pandora_hw_access_error(Pandora *service, IOReturn error);
void pandora_hw_access_shutdown();

IOReturn pandora_patch_osvariant_start(Pandora *service);
void pandora_patch_osvariant_stop(Pandora *service);
void pandora_patch_osvariant_error(Pandora *service, IOReturn error);
void pandora_patch_osvariant_shutdown();

static PandoraModule g_modules[] = {
    {
        "hw_access",
        "pandora_enable_hw_access",
        nullptr,
        false,
        false,
        false,
        kIOReturnSuccess,
        pandora_hw_access_start,
        pandora_hw_access_stop,
        pandora_hw_access_error,
        pandora_hw_access_shutdown,
    },
    {
        "patch_osvariant",
        "pandora_enable_osvariant",
        "pandora_enable_patch_osvarient", // backwards-compatible typo alias
        false,
        false,
        false,
        kIOReturnSuccess,
        pandora_patch_osvariant_start,
        pandora_patch_osvariant_stop,
        pandora_patch_osvariant_error,
        pandora_patch_osvariant_shutdown,
    },
};

bool pandora_bootarg_enabled(const char *name, bool defaultEnabled) {
  if (!name || name[0] == '\0') {
    return defaultEnabled;
  }

  char buf[64] = {};
  if (!PE_parse_boot_argn(name, buf, sizeof(buf))) {
    return defaultEnabled;
  }

  // Boot-arg present without value (e.g. -foo) often parses as empty.
  if (buf[0] == '\0') {
    return true;
  }

  if (buf[0] == '0') {
    return false;
  }

  // Common falsey strings (avoid strcasecmp to keep deps minimal)
  if (buf[0] == 'f' || buf[0] == 'F' || buf[0] == 'n' || buf[0] == 'N') {
    return false;
  }

  return true;
}

static bool module_enabled_from_boot_args(const PandoraModule &m) {
  if (pandora_bootarg_enabled(m.bootArg, m.defaultEnabled)) {
    return true;
  }
  if (m.altBootArg && m.altBootArg[0] != '\0') {
    return pandora_bootarg_enabled(m.altBootArg, false);
  }
  return false;
}

void pandora_modules_configure_from_boot_args() {
  const size_t count = sizeof(g_modules) / sizeof(g_modules[0]);
  for (size_t i = 0; i < count; ++i) {
    g_modules[i].enabled = module_enabled_from_boot_args(g_modules[i]);
    g_modules[i].started = false;
    g_modules[i].startError = kIOReturnSuccess;

    PANDORA_LOG_DEFAULT("Module %s (boot-arg: %s): %s", g_modules[i].name,
                        g_modules[i].bootArg,
                        g_modules[i].enabled ? "enabled" : "disabled");
  }
}

IOReturn pandora_modules_start(Pandora *service) {
  const size_t count = sizeof(g_modules) / sizeof(g_modules[0]);
  for (size_t i = 0; i < count; ++i) {
    if (!g_modules[i].enabled) {
      continue;
    }

    PANDORA_LOG_DEFAULT("Starting module: %s", g_modules[i].name);
    IOReturn st =
        g_modules[i].start ? g_modules[i].start(service) : kIOReturnSuccess;
    if (st == kIOReturnSuccess) {
      g_modules[i].started = true;
      continue;
    }

    g_modules[i].startError = st;
    PANDORA_LOG_DEFAULT("Module %s failed to start: 0x%x", g_modules[i].name,
                        st);
    if (g_modules[i].onError) {
      g_modules[i].onError(service, st);
    }

    // Stop any already-started modules in reverse order.
    for (size_t j = i; j-- > 0;) {
      if (g_modules[j].started && g_modules[j].stop) {
        PANDORA_LOG_DEFAULT("Stopping module due to failure: %s",
                            g_modules[j].name);
        g_modules[j].stop(service);
        g_modules[j].started = false;
      }
    }

    return st;
  }

  return kIOReturnSuccess;
}

void pandora_modules_stop(Pandora *service) {
  const size_t count = sizeof(g_modules) / sizeof(g_modules[0]);
  for (size_t i = count; i-- > 0;) {
    if (!g_modules[i].started) {
      continue;
    }

    PANDORA_LOG_DEFAULT("Stopping module: %s", g_modules[i].name);
    if (g_modules[i].stop) {
      g_modules[i].stop(service);
    }
    g_modules[i].started = false;
  }
}

void pandora_modules_shutdown() {
  const size_t count = sizeof(g_modules) / sizeof(g_modules[0]);
  for (size_t i = 0; i < count; ++i) {
    if (g_modules[i].shutdown) {
      g_modules[i].shutdown();
    }
    g_modules[i].enabled = false;
    g_modules[i].started = false;
    g_modules[i].startError = kIOReturnSuccess;
  }
}

size_t pandora_modules_count() {
  return sizeof(g_modules) / sizeof(g_modules[0]);
}

const PandoraModule *pandora_modules_get(size_t *count) {
  if (count) {
    *count = pandora_modules_count();
  }
  return g_modules;
}
