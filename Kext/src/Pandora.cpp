#include "Pandora.h"
#include "Globals.h"
#include "Modules/ModuleSystem.h"
#include "Utils/KernelUtilities.h"
#include "Utils/PandoraLog.h"
#include "Utils/TimeUtilities.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <sys/proc.h>

#define super IOService
OSDefineMetaClassAndFinalStructors(Pandora, IOService);

bool Pandora::start(IOService *provider) {
  if (!super::start(provider))
    return false;

  pandora_log_ensure_initialized();

  io_service_start_called = true;
  io_service_start_time = makeCurrentTimestampPair();

  // Track whether launchd (pid 1) exists at the time IOService starts.
  pid1_exists = false;
  proc_t p1 = proc_find(1);
  if (p1) {
    pid1_exists = true;
    proc_rele(p1);
  }

  workloop_ = IOWorkLoop::workLoop();
  if (!workloop_) {
    PANDORA_LOG_DEFAULT("Failed to create IOWorkLoop");
    goto fail;
  }

  PANDORA_LOG_DEFAULT("IOWorkLoop created successfully");

  pandora_modules_configure_from_boot_args();
  {
    IOReturn rc = pandora_modules_start(this);
    if (rc != kIOReturnSuccess) {
      PANDORA_LOG_DEFAULT("Module start failed: 0x%x", rc);
      goto fail;
    }
  }

  registerService();
  return true;

fail:
  PANDORA_LOG_DEFAULT("Failed to start Pandora service, cleaning up resources");

  pandora_modules_stop(this);
  if (workloop_) {
    PANDORA_LOG_DEFAULT("Cleaning up workloop");
    workloop_->release();
    workloop_ = nullptr;
  }
  pandora_log_cleanup();
  super::stop(provider);
  return false;
}

void Pandora::stop(IOService *provider) {
  PANDORA_LOG_DEFAULT("Stopping Pandora service");

  pandora_modules_stop(this);
  if (workloop_) {
    workloop_->release();
    workloop_ = nullptr;
  }
  pandora_log_cleanup();
  super::stop(provider);
}

void Pandora::free() {
  super::free();
}
