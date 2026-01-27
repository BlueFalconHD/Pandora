#pragma once

#include <mach/mach.h>
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

/* Debugserver-like helpers */
int pd_set_process_debugged(pid_t pid);
