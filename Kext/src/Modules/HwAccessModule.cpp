#include "HwAccessModule.h"

#include "ModuleSystem.h"
#include "../Utils/PandoraLog.h"

static bool g_hw_access_active = false;

bool pandora_hw_access_active() {
  return g_hw_access_active;
}

IOReturn pandora_hw_access_start(Pandora *service) {
  (void)service;
  g_hw_access_active = true;
  PANDORA_LOG_DEFAULT("hw_access: started");
  return kIOReturnSuccess;
}

void pandora_hw_access_stop(Pandora *service) {
  (void)service;
  g_hw_access_active = false;
  PANDORA_LOG_DEFAULT("hw_access: stopped");
}

void pandora_hw_access_error(Pandora *service, IOReturn error) {
  (void)service;
  g_hw_access_active = false;
  PANDORA_LOG_DEFAULT("hw_access: error 0x%x", error);
}

void pandora_hw_access_shutdown() {
  g_hw_access_active = false;
}
