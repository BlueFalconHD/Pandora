#include "Globals.h"
#include "Modules/ModuleSystem.h"
#include "Utils/PandoraLog.h"
#include "Utils/TimeUtilities.h"

#include <mach/kmod.h>
#include <mach/mach_time.h>

#include "Version.h"

static kern_return_t initialize_cpp() {
  pandora_log_ensure_initialized();

  // Initialize time utilities first
  TimeUtilities::init();

  PANDORA_LOG_DEFAULT("kmod.cpp:initialize_cpp: I've been run!");
  kmod_run = true;
  kmod_start_time = makeCurrentTimestampPair();

  return KERN_SUCCESS;
}

kern_return_t finalize_cpp() {
  PANDORA_LOG_DEFAULT(
      "kmod.cpp:finalize_cpp: Shutting down Pandora kernel extension");

  pandora_modules_shutdown();
  pandora_log_cleanup();
  return KERN_SUCCESS;
}

extern "C" {

#include <mach/kmod.h>
static kern_return_t start(kmod_info_t *ki, void *data) {
  return initialize_cpp();
}

static kern_return_t stop(kmod_info_t *ki, void *data) {
  return finalize_cpp();
}

__attribute__((visibility("default")))
KMOD_EXPLICIT_DECL(com.bluefalconhd.Pandora, PANDORA_VERSION_STRING, start,
                   stop);
}
