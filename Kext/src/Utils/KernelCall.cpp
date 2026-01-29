#include "KernelCall.h"

#include "PandoraLog.h"

#include "../routines.h"

PandoraKCallResult pandora_kcall(uint64_t fn, const uint64_t *args,
                                 size_t argCount) {
  (void)fn;
  (void)args;
  (void)argCount;
  if (fn == 0) {
    return {
        .status = kIOReturnBadArgument,
        .ret0 = 0,
    };
  }

  if (argCount > 8) {
    return {
        .status = kIOReturnBadArgument,
        .ret0 = 0,
    };
  }

  if (argCount > 0 && args == nullptr) {
    return {
        .status = kIOReturnBadArgument,
        .ret0 = 0,
    };
  }

  uint64_t callArgs[10] = {};
  for (size_t i = 0; i < argCount; i++) {
    callArgs[i] = args[i];
  }

  PANDORA_LOG_DEFAULT("KernelCall: invoking 0x%llx with %zu arg(s)", fn,
                      argCount);

  uint64_t ret0 = arbitrary_call(
      fn, callArgs[0], callArgs[1], callArgs[2], callArgs[3], callArgs[4],
      callArgs[5], callArgs[6], callArgs[7], callArgs[8], callArgs[9]);

  return {.status = kIOReturnSuccess, .ret0 = ret0};
}
