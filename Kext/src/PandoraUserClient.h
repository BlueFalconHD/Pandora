#pragma once

#include "Utils/TimeUtilities.h"

#include <IOKit/IOUserClient.h>
#include <stdint.h>

static constexpr uint32_t kPandoraUserClientModuleSelectorShift = 16u;
static constexpr uint16_t kPandoraUserClientModuleIdHwAccess = 0x0001;
static constexpr uint16_t kPandoraUserClientModuleIdPatchOsVariant = 0x0002;

static inline constexpr uint32_t pandoraMakeSelector(uint16_t moduleId,
                                                     uint16_t localSelector) {
  return (static_cast<uint32_t>(moduleId)
          << kPandoraUserClientModuleSelectorShift) |
         static_cast<uint32_t>(localSelector);
}

// Timing and generic metadata about the Pandora kext for debugging.
struct PandoraMetadata {
  TimestampPair kmod_start_time;
  TimestampPair io_service_start_time;
  TimestampPair user_client_init_time;
  bool pid1_exists;
};

struct PandoraKCallRequest {
  uint64_t fn;
  uint32_t argCount;
  uint32_t reserved;
  uint64_t args[8];
};

struct PandoraKCallResponse {
  IOReturn status;
  uint64_t ret0;
};

class PandoraUserClient final : public IOUserClient {
  OSDeclareFinalStructors(PandoraUserClient);

public:
  bool initWithTask(task_t owningTask, void *securityID, uint32_t type) override;
  IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                          IOExternalMethodDispatch *dispatch,
                          OSObject *target, void *reference) override;
};
