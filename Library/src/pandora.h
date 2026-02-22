#pragma once

#include <mach/mach.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

// Structure for holding both types of timestamps for debugging
typedef struct {
  uint64_t machTime;       // mach_absolute_time value
  uint64_t unixTime;       // Unix timestamp equivalent
  uint64_t nanosSinceBoot; // Nanoseconds since boot
} TimestampPair;

// Timing and generic metadata about the Pandora kext for debugging
typedef struct {
  TimestampPair kmod_start_time; // Timestamp when the kmod start function was
                                 // called. 0 if not called yet
  TimestampPair
      io_service_start_time; // Timestamp when the IOService start function
                             // was called. 0 if not called yet
  TimestampPair
      user_client_init_time; // Timestamp when the last user client was
                             // initialized. 0 if not initialized yet
  bool pid1_exists; // Whether PID 1 (launchd) exists at the time of kext start
} PandoraMetadata;

// ---------------------------------------------------------------------------
// UserClient method selectors
//
// Note: The Kext and Library historically drifted on selector numbering.
// The current Kext expects module-scoped selectors:
//   selector = (module_id << 16) | local_selector
// The library wrappers keep runtime fallback to legacy unscoped selectors.
// ---------------------------------------------------------------------------
typedef enum {
  PANDORA_UC_MODULE_ID_HW_ACCESS = 0x0001,
  PANDORA_UC_MODULE_ID_PATCH_OSVARIANT = 0x0002,
} PandoraUserClientModuleId;

#define PANDORA_UC_MODULE_SELECTOR_SHIFT 16u
#define PANDORA_UC_SELECTOR_COMPOSE(module_id, local_selector)                  \
  (((uint32_t)(module_id) << PANDORA_UC_MODULE_SELECTOR_SHIFT) |               \
   (uint32_t)(local_selector))

typedef enum {
  PANDORA_UC_LOCAL_SELECTOR_KREAD = 0,
  PANDORA_UC_LOCAL_SELECTOR_KWRITE = 1,
  PANDORA_UC_LOCAL_SELECTOR_GET_KERNEL_BASE = 2,
  PANDORA_UC_LOCAL_SELECTOR_GET_METADATA = 3,
  PANDORA_UC_LOCAL_SELECTOR_PREAD_PID = 4,
  PANDORA_UC_LOCAL_SELECTOR_PWRITE_PID = 5,
  PANDORA_UC_LOCAL_SELECTOR_KCALL = 7,
  PANDORA_UC_LOCAL_SELECTOR_RUN_ARB_FUNC_WITH_TASK_ARG_PID = 8,
} PandoraHwAccessLocalSelector;

typedef enum {
  PANDORA_UC_SELECTOR_KREAD =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_KREAD),
  PANDORA_UC_SELECTOR_KWRITE =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_KWRITE),
  PANDORA_UC_SELECTOR_GET_KERNEL_BASE =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_GET_KERNEL_BASE),
  PANDORA_UC_SELECTOR_GET_METADATA =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_GET_METADATA),
  PANDORA_UC_SELECTOR_PREAD_PID =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_PREAD_PID),
  PANDORA_UC_SELECTOR_PWRITE_PID =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_PWRITE_PID),
  PANDORA_UC_SELECTOR_KCALL =
      PANDORA_UC_SELECTOR_COMPOSE(PANDORA_UC_MODULE_ID_HW_ACCESS,
                                  PANDORA_UC_LOCAL_SELECTOR_KCALL),
  PANDORA_UC_SELECTOR_RUN_ARB_FUNC_WITH_TASK_ARG_PID =
      PANDORA_UC_SELECTOR_COMPOSE(
          PANDORA_UC_MODULE_ID_HW_ACCESS,
          PANDORA_UC_LOCAL_SELECTOR_RUN_ARB_FUNC_WITH_TASK_ARG_PID),
} PandoraUserClientSelector;

// Request/response for the kernel-call interface.
typedef struct {
  uint64_t fn;       // kernel VA of function to call
  uint32_t argCount; // number of args used (<= 8)
  uint32_t reserved;
  uint64_t args[8];
} PandoraKCallRequest;

typedef struct {
  kern_return_t status;
  uint64_t ret0;
} PandoraKCallResponse;

extern uint64_t pd_kbase;
extern uint64_t pd_kslide;

#define kslide(x) (x + pd_kslide - 0x8000)
#define kunslide(x) (x - pd_kslide + 0x8000)

/* Initialisation and deinitialisation */
int pd_init(void);
void pd_deinit(void);

/* Virtual read/write */
uint8_t pd_read8(uint64_t addr);
uint16_t pd_read16(uint64_t addr);
uint32_t pd_read32(uint64_t addr);
uint64_t pd_read64(uint64_t addr);
int pd_readbuf(uint64_t addr, void *buf, size_t len);
kern_return_t pd_write8(uint64_t addr, uint8_t val);
kern_return_t pd_write16(uint64_t addr, uint16_t val);
kern_return_t pd_write32(uint64_t addr, uint32_t val);
kern_return_t pd_write64(uint64_t addr, uint64_t val);
kern_return_t pd_writebuf(uint64_t addr, const void *buf, size_t len);

/* Process read/write (by PID) */
uint8_t pd_pread8(pid_t pid, uint64_t addr);
uint16_t pd_pread16(pid_t pid, uint64_t addr);
uint32_t pd_pread32(pid_t pid, uint64_t addr);
uint64_t pd_pread64(pid_t pid, uint64_t addr);
int pd_preadbuf(pid_t pid, uint64_t addr, void *buf, size_t len);
kern_return_t pd_pwrite8(pid_t pid, uint64_t addr, uint8_t val);
kern_return_t pd_pwrite16(pid_t pid, uint64_t addr, uint16_t val);
kern_return_t pd_pwrite32(pid_t pid, uint64_t addr, uint32_t val);
kern_return_t pd_pwrite64(pid_t pid, uint64_t addr, uint64_t val);
kern_return_t pd_pwritebuf(pid_t pid, uint64_t addr, const void *buf,
                           size_t len);

/* Kernel utilities */
uint64_t pd_get_kernel_base();

/* Metadata utilities */
int pd_get_metadata(PandoraMetadata *metadata);

/* Newer userclient methods */
kern_return_t pd_kcall(const PandoraKCallRequest *req, PandoraKCallResponse *resp);
kern_return_t pd_kcall_simple(uint64_t fn, const uint64_t *args, uint32_t argCount,
                              uint64_t *ret0);
kern_return_t pd_run_arb_func_with_task_arg_pid(uint64_t funcAddr, pid_t pid,
                                                uint64_t *ret0);
