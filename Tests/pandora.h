#pragma once

#include <mach/mach.h>
#include <stddef.h>
#include <stdint.h>

// Timing and generic metadata about the Pandora kext for debugging
typedef struct {
  uint64_t kmod_start_time;       // Timestamp when the kmod start function was
                                  // called. 0 if not called yet
  uint64_t io_service_start_time; // Timestamp when the IOService start function
                                  // was called. 0 if not called yet
  uint64_t user_client_init_time; // Timestamp when the last user client was
                                  // initialized. 0 if not initialized yet
  bool pid1_exists; // Whether PID 1 (launchd) exists at the time of kext start
} PandoraMetadata;

/* Initialisation and deinitialisation */
int pd_init(void);
void pd_deinit(void);

/* Virtual read/write */
uint8_t pd_read8(uint64_t addr);
uint16_t pd_read16(uint64_t addr);
uint32_t pd_read32(uint64_t addr);
uint64_t pd_read64(uint64_t addr);
int pd_readbuf(uint64_t addr, void *buf, size_t len);

/* Kernel utilities */
uint64_t pd_get_kernel_base();

/* Metadata utilities */
int pd_get_metadata(PandoraMetadata *metadata);
