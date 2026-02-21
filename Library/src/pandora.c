#include "pandora.h"
#include <IOKit/IOKitLib.h>
#include <mach-o/loader.h>
#include <mach/error.h>
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <stdio.h>

#define KADDR_OBFUSCATION_KEY 0x6869726520706C7A
#define STATIC_KERNEL_BASE 0xFFFFFE0007004000

uint64_t pd_kbase = 0;
uint64_t pd_kslide = 0;

io_connect_t gClient = MACH_PORT_NULL;

static inline io_connect_t pandora_open(void) {
  io_service_t service = IOServiceGetMatchingService(
      kIOMainPortDefault, IOServiceMatching("Pandora"));
  if (!MACH_PORT_VALID(service)) {
    printf("Failed to find Pandora service\n");
    return MACH_PORT_NULL;
  }

  io_connect_t client = MACH_PORT_NULL;
  kern_return_t ret = IOServiceOpen(service, mach_task_self(), 0, &client);
  IOObjectRelease(service);
  if (ret != KERN_SUCCESS) {
    printf("Failed to open Pandora service: %x (%s)\n", ret,
           mach_error_string(ret));
    return MACH_PORT_NULL;
  }
  return client;
}

static inline kern_return_t pandora_read(io_connect_t client, uint64_t kaddr,
                                         void *uaddr, uint64_t len) {
  uint64_t in[] = {kaddr, (uint64_t)uaddr, len};
  return IOConnectCallScalarMethod(client, 0, in, 3, NULL, NULL);
}

static inline kern_return_t pandora_write(io_connect_t client, void *uaddr,
                                          uint64_t kaddr, uint64_t len) {
  uint64_t in[] = {(uint64_t)uaddr, kaddr, len};
  return IOConnectCallScalarMethod(client, 1, in, 3, NULL, NULL);
}

static inline kern_return_t pandora_get_kbase(io_connect_t client,
                                              uint64_t *out) {
  uint32_t outCnt = 1;
  return IOConnectCallScalarMethod(client, 2, NULL, 0, out, &outCnt);
}

static inline kern_return_t pandora_get_metadata(io_connect_t client,
                                                 PandoraMetadata *metadata) {
  size_t outputSize = sizeof(PandoraMetadata);
  return IOConnectCallStructMethod(client, 3, NULL, 0, metadata, &outputSize);
}

static inline kern_return_t pandora_proc_read(io_connect_t client, pid_t pid,
                                              uint64_t paddr, void *uaddr,
                                              uint64_t len) {
  uint64_t in[] = {(uint64_t)(int64_t)pid, paddr, (uint64_t)uaddr, len};
  return IOConnectCallScalarMethod(client, 4, in, 4, NULL, NULL);
}

static inline kern_return_t pandora_proc_write(io_connect_t client, pid_t pid,
                                               void *uaddr, uint64_t paddr,
                                               uint64_t len) {
  uint64_t in[] = {(uint64_t)(int64_t)pid, (uint64_t)uaddr, paddr, len};
  return IOConnectCallScalarMethod(client, 5, in, 4, NULL, NULL);
}

static inline kern_return_t pandora_kcall(io_connect_t client,
                                          uint32_t selector,
                                          const PandoraKCallRequest *req,
                                          PandoraKCallResponse *resp) {
  if (!req || !resp) {
    return KERN_INVALID_ARGUMENT;
  }

  size_t outSize = sizeof(*resp);
  return IOConnectCallStructMethod(client, selector, req, sizeof(*req), resp,
                                   &outSize);
}

static inline kern_return_t pandora_run_arb_func_with_task_arg_pid(
    io_connect_t client, uint32_t selector, uint64_t funcAddr, pid_t pid,
    uint64_t *ret0) {
  uint64_t in[] = {funcAddr, (uint64_t)(int64_t)pid};
  uint64_t out = 0;
  uint32_t outCnt = 1;
  kern_return_t kr =
      IOConnectCallScalarMethod(client, selector, in, 2, &out, &outCnt);
  if (kr == KERN_SUCCESS && ret0 && outCnt == 1) {
    *ret0 = out;
  }
  return kr;
}

void pandora_close(io_connect_t client) { IOServiceClose(client); }

int pd_init(void) {
  gClient = pandora_open();
  return MACH_PORT_VALID(gClient) ? 0 : -1;
}

void pd_deinit(void) {
  if (MACH_PORT_VALID(gClient)) {
    pandora_close(gClient);
    gClient = MACH_PORT_NULL;
  }
}

uint8_t pd_read8(uint64_t addr) {
  uint8_t val = 0;
  pandora_read(gClient, addr, &val, sizeof(val));
  return val;
}

uint16_t pd_read16(uint64_t addr) {
  uint16_t val = 0;
  pandora_read(gClient, addr, &val, sizeof(val));
  return val;
}

uint32_t pd_read32(uint64_t addr) {
  uint32_t val = 0;
  pandora_read(gClient, addr, &val, sizeof(val));
  return val;
}

uint64_t pd_read64(uint64_t addr) {
  uint64_t val = 0;
  pandora_read(gClient, addr, &val, sizeof(val));
  return val;
}

int pd_readbuf(uint64_t addr, void *buf, size_t len) {
  return pandora_read(gClient, addr, buf, len);
}

kern_return_t pd_write8(uint64_t addr, uint8_t val) {
  return pandora_write(gClient, &val, addr, sizeof(val));
}

kern_return_t pd_write16(uint64_t addr, uint16_t val) {
  return pandora_write(gClient, &val, addr, sizeof(val));
}

kern_return_t pd_write32(uint64_t addr, uint32_t val) {
  return pandora_write(gClient, &val, addr, sizeof(val));
}

kern_return_t pd_write64(uint64_t addr, uint64_t val) {
  return pandora_write(gClient, &val, addr, sizeof(val));
}

kern_return_t pd_writebuf(uint64_t addr, const void *buf, size_t len) {
  if (!buf || len == 0) {
    return KERN_INVALID_ARGUMENT;
  }
  return pandora_write(gClient, (void *)buf, addr, len);
}

uint8_t pd_pread8(pid_t pid, uint64_t addr) {
  uint8_t val = 0;
  pandora_proc_read(gClient, pid, addr, &val, sizeof(val));
  return val;
}

uint16_t pd_pread16(pid_t pid, uint64_t addr) {
  uint16_t val = 0;
  pandora_proc_read(gClient, pid, addr, &val, sizeof(val));
  return val;
}

uint32_t pd_pread32(pid_t pid, uint64_t addr) {
  uint32_t val = 0;
  pandora_proc_read(gClient, pid, addr, &val, sizeof(val));
  return val;
}

uint64_t pd_pread64(pid_t pid, uint64_t addr) {
  uint64_t val = 0;
  pandora_proc_read(gClient, pid, addr, &val, sizeof(val));
  return val;
}

int pd_preadbuf(pid_t pid, uint64_t addr, void *buf, size_t len) {
  return pandora_proc_read(gClient, pid, addr, buf, len);
}

kern_return_t pd_pwrite8(pid_t pid, uint64_t addr, uint8_t val) {
  return pandora_proc_write(gClient, pid, &val, addr, sizeof(val));
}

kern_return_t pd_pwrite16(pid_t pid, uint64_t addr, uint16_t val) {
  return pandora_proc_write(gClient, pid, &val, addr, sizeof(val));
}

kern_return_t pd_pwrite32(pid_t pid, uint64_t addr, uint32_t val) {
  return pandora_proc_write(gClient, pid, &val, addr, sizeof(val));
}

kern_return_t pd_pwrite64(pid_t pid, uint64_t addr, uint64_t val) {
  return pandora_proc_write(gClient, pid, &val, addr, sizeof(val));
}

kern_return_t pd_pwritebuf(pid_t pid, uint64_t addr, const void *buf,
                           size_t len) {
  if (!buf || len == 0) {
    return KERN_INVALID_ARGUMENT;
  }
  return pandora_proc_write(gClient, pid, (void *)buf, addr, len);
}

uint64_t pd_get_kernel_base() {
  static uint64_t kbase = 0;

  if (kbase) {
    return pd_kbase;
  }

  kern_return_t ret = pandora_get_kbase(gClient, &kbase);
  if (ret != KERN_SUCCESS) {
    return 0;
  }
  pd_kbase = kbase ^ KADDR_OBFUSCATION_KEY; // unlock
  pd_kslide = pd_kbase - STATIC_KERNEL_BASE;

  return pd_kbase;
}

int pd_get_metadata(PandoraMetadata *metadata) {
  if (!metadata) {
    return -1;
  }

  kern_return_t ret = pandora_get_metadata(gClient, metadata);
  if (ret != KERN_SUCCESS) {
    printf("Failed to get Pandora metadata: %x\n", ret);
    return -1;
  }

  return 0;
}

kern_return_t pd_kcall(const PandoraKCallRequest *req, PandoraKCallResponse *resp) {
  if (!MACH_PORT_VALID(gClient)) {
    return KERN_INVALID_CAPABILITY;
  }

  // Prefer the updated selector numbering (6/7). Fall back to the older
  // numbering (7/8) for compatibility with older kext builds.
  kern_return_t kr =
      pandora_kcall(gClient, PANDORA_UC_SELECTOR_KCALL_PREFERRED, req, resp);
  if (kr == kIOReturnBadArgument || kr == kIOReturnUnsupported) {
    kr = pandora_kcall(gClient, 7 /* legacy KCALL */, req, resp);
  }
  return kr;
}

kern_return_t pd_kcall_simple(uint64_t fn, const uint64_t *args,
                              uint32_t argCount, uint64_t *ret0) {
  if (argCount > 8) {
    return KERN_INVALID_ARGUMENT;
  }

  PandoraKCallRequest req = {
      .fn = fn,
      .argCount = argCount,
      .reserved = 0,
      .args = {0},
  };
  for (uint32_t i = 0; i < argCount; i++) {
    req.args[i] = args ? args[i] : 0;
  }

  PandoraKCallResponse resp = {.status = KERN_FAILURE, .ret0 = 0};
  kern_return_t kr = pd_kcall(&req, &resp);
  if (ret0) {
    *ret0 = resp.ret0;
  }
  return (kr == KERN_SUCCESS) ? resp.status : kr;
}

kern_return_t pd_run_arb_func_with_task_arg_pid(uint64_t funcAddr, pid_t pid,
                                                uint64_t *ret0) {
  if (!MACH_PORT_VALID(gClient)) {
    return KERN_INVALID_CAPABILITY;
  }

  kern_return_t kr = pandora_run_arb_func_with_task_arg_pid(
      gClient, PANDORA_UC_SELECTOR_RUN_ARB_FUNC_WITH_TASK_ARG_PID_PREFERRED,
      funcAddr, pid, ret0);
  if (kr == kIOReturnBadArgument || kr == kIOReturnUnsupported) {
    kr = pandora_run_arb_func_with_task_arg_pid(gClient, 8 /* legacy RUN_ARB */,
                                                funcAddr, pid, ret0);
  }
  return kr;
}
