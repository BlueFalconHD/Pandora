#include "kpi.h"

#include "Utils/KernelCall.h"
#include "Utils/KernelUtilities.h"
#include "Utils/PandoraLog.h"
#include "Utils/VersionUtilities.h"

#include <IOKit/IOReturn.h>
#include <kern/task.h>

#include <stdint.h>

static uint64_t g_proc_task_fn = 0;

static bool resolve_proc_task_fn(uint64_t *out) {
  if (!out) {
    return false;
  }

  // Fast path if already cached
  if (g_proc_task_fn != 0) {
    *out = g_proc_task_fn;
    return true;
  }

  VersionUtilities vu;
  if (!vu.init()) {
    PANDORA_LOG_DEFAULT("proc_task shim: VersionUtilities init failed");
    return false;
  }

  uintptr_t unslid = 0;
  if (!vu.getProcTask(&unslid) || unslid == 0 || unslid == kNoHook) {
    PANDORA_LOG_DEFAULT(
        "proc_task shim: no proc_task address for %s / %s (unslid=0x%llx)",
        vu.getBuildVersion(), vu.getModelIdentifier(),
        (unsigned long long)unslid);
    return false;
  }

  KernelUtilities ku;
  KUError kuInit = ku.init();
  if (kuInit != KUErrorSuccess) {
    PANDORA_LOG_DEFAULT("proc_task shim: KernelUtilities init failed: %d",
                        kuInit);
    return false;
  }

  uint64_t slid = ku.kslide(unslid);
  if (slid == 0) {
    PANDORA_LOG_DEFAULT("proc_task shim: kslide returned 0 for unslid=0x%llx",
                        (unsigned long long)unslid);
    return false;
  }

  g_proc_task_fn = slid;
  *out = slid;
  PANDORA_LOG_DEFAULT("proc_task shim: resolved fn=0x%llx (unslid=0x%llx)",
                      (unsigned long long)slid, (unsigned long long)unslid);
  return true;
}

extern "C" task_t proc_task(proc_t p) {
  if (!p) {
    return TASK_NULL;
  }

  uint64_t fn = 0;
  if (!resolve_proc_task_fn(&fn) || fn == 0) {
    return TASK_NULL;
  }

  uint64_t args[1] = {reinterpret_cast<uint64_t>(p)};
  PandoraKCallResult res = pandora_kcall(fn, args, 1);
  if (res.status != kIOReturnSuccess || res.ret0 == 0) {
    PANDORA_LOG_DEFAULT("proc_task shim: kcall failed: status=0x%x ret0=0x%llx",
                        res.status, (unsigned long long)res.ret0);
    return TASK_NULL;
  }

  return reinterpret_cast<task_t>(res.ret0);
}
