#include "PandoraUserClient.h"

#include "Globals.h"
#include "Modules/ModuleSystem.h"
#include "Utils/PandoraLog.h"

#define super IOUserClient
OSDefineMetaClassAndFinalStructors(PandoraUserClient, IOUserClient);

bool PandoraUserClient::initWithTask(task_t owningTask, void *securityID,
                                     uint32_t type) {
  if (!super::initWithTask(owningTask, securityID, type)) {
    return false;
  }

  pandora_log_ensure_initialized();

  bool allow = false;
  OSObject *entitlement =
      copyClientEntitlement(owningTask, "com.bluefalconhd.pandora.krw");
  if (entitlement) {
    allow = (entitlement == kOSBooleanTrue);
    entitlement->release();
  }
  if (!allow) {
    PANDORA_LOG_DEFAULT(
        "PandoraUserClient::initWithTask: missing entitlement");
    return false;
  }

  if (!pandora_modules_authorize_user_client(owningTask, securityID, type)) {
    PANDORA_LOG_DEFAULT("PandoraUserClient::initWithTask: denied by modules");
    return false;
  }

  const PandoraRuntimeState &runtime = pandora_runtime_state();
  PANDORA_LOG_DEFAULT("PandoraUserClient::initWithTask: workloop saw zero=%d",
                      runtime.telemetry.workloopSawZero ? 1 : 0);

  return true;
}

IOReturn PandoraUserClient::externalMethod(uint32_t selector,
                                           IOExternalMethodArguments *args,
                                           IOExternalMethodDispatch *dispatch,
                                           OSObject *target,
                                           void *reference) {
  (void)dispatch;
  (void)target;
  (void)reference;

  PandoraUserClientMethodLookup lookup = {};
  if (!pandora_modules_lookup_userclient_method(selector, &lookup) ||
      !lookup.dispatch) {
    return kIOReturnUnsupported;
  }

  return super::externalMethod(
      selector, args,
      const_cast<IOExternalMethodDispatch *>(lookup.dispatch), this,
      lookup.reference);
}
