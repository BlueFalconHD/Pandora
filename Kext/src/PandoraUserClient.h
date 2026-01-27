#pragma once

#include "Utils/KernelUtilities.h"
#include "Utils/TimeUtilities.h"
#include <IOKit/IOUserClient.h>
#include <stdint.h>

// xor key so kernel doesn't censor us
#define KADDR_OBFUSCATION_KEY 0x6869726520706C7A

// Timing and generic metadata about the Pandora kext for debugging (because it
// starts too early for logging)
struct PandoraMetadata {
  TimestampPair kmod_start_time; // Timestamp when the kmod start function was
                                 // called. 0 if not called yet
  TimestampPair
      io_service_start_time; // Timestamp when the IOService start function
                             // was called. 0 if not called yet
  TimestampPair
      user_client_init_time; // Timestamp when the last user client was
                             // initialized. 0 if not initialized yet
  bool pid1_exists; // Whether PID 1 (launchd) exists at the time of kext start
};

class PandoraUserClient : public IOUserClient {
  OSDeclareFinalStructors(PandoraUserClient);

public:
  virtual bool initWithTask(task_t owningTask, void *securityID,
                            uint32_t type) override;
  virtual IOReturn externalMethod(uint32_t selector,
                                  IOExternalMethodArguments *args,
                                  IOExternalMethodDispatch *dispatch,
                                  OSObject *target, void *reference) override;

private:
  KernelUtilities ku;

  static IOReturn kread(PandoraUserClient *client, void *reference,
                        IOExternalMethodArguments *args);
  static IOReturn kwrite(PandoraUserClient *client, void *reference,
                         IOExternalMethodArguments *args);
  static IOReturn pread_pid(PandoraUserClient *client, void *reference,
                            IOExternalMethodArguments *args);
  static IOReturn pwrite_pid(PandoraUserClient *client, void *reference,
                             IOExternalMethodArguments *args);
  static IOReturn getKernelBase(PandoraUserClient *client, void *reference,
                                IOExternalMethodArguments *args);
  static IOReturn getPandoraLoadMetadata(PandoraUserClient *client,
                                         void *reference,
                                         IOExternalMethodArguments *args);

  // Debug/CS helpers
  static IOReturn setProcessDebugged(PandoraUserClient *client, void *reference,
                                     IOExternalMethodArguments *args);
};
