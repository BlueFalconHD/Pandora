#ifndef PANDORALOG_H
#define PANDORALOG_H

/*
 * Pandora Logging System
 *
 * This system provides structured logging using os_log with multiple categories
 * for better organization and filtering of log messages.
 *
 * Usage:
 * - Call pandora_log_init() once during kext startup
 * - Use appropriate logging macros throughout the code
 * - Call pandora_log_cleanup() during kext shutdown
 *
 * Viewing Logs:
 * To view all Pandora logs:
 *   log show --predicate 'subsystem == "com.bluefalconhd.pandora"'
 *
 * To view specific categories:
 *   log show --predicate 'subsystem == "com.bluefalconhd.pandora" && category
 * == "general"' log show --predicate 'subsystem == "com.bluefalconhd.pandora"
 * && category == "kernel"' log show --predicate 'subsystem ==
 * "com.bluefalconhd.pandora" && category == "memory"' log show --predicate
 * 'subsystem == "com.bluefalconhd.pandora" && category == "userclient"'
 *
 * To stream live logs:
 *   log stream --predicate 'subsystem == "com.bluefalconhd.pandora"'
 *
 * Available log levels: DEFAULT, INFO, DEBUG, ERROR, FAULT
 */

#include <IOKit/IOLib.h>
#include <os/log.h>

extern bool pandora_log_initialized;

// Global os_log_t instances for different categories
extern os_log_t pandora_log;
extern os_log_t pandora_kernel_log;
extern os_log_t pandora_memory_log;
extern os_log_t pandora_userclient_log;

// Initialize the global logger (call this once during kext startup)
void pandora_log_init(void);

// Cleanup the global logger (call during kext shutdown)
void pandora_log_cleanup(void);

// Convenient logging macros
#define PANDORA_LOG_ERROR(fmt, ...)                                            \
  os_log_error(pandora_log, fmt, ##__VA_ARGS__)

#define PANDORA_LOG_FAULT(fmt, ...)                                            \
  os_log_fault(pandora_log, fmt, ##__VA_ARGS__)

#define PANDORA_LOG_INFO(fmt, ...) os_log_info(pandora_log, fmt, ##__VA_ARGS__)

#define PANDORA_LOG_DEBUG(fmt, ...)                                            \
  os_log_debug(pandora_log, fmt, ##__VA_ARGS__)

#define PANDORA_LOG_DEFAULT(fmt, ...) os_log(pandora_log, fmt, ##__VA_ARGS__)

// Fallback macro that works like IOLog for compatibility
#define PANDORA_LOG(fmt, ...) PANDORA_LOG_DEFAULT(fmt, ##__VA_ARGS__)

// Category-specific logging macros
#define PANDORA_KERNEL_LOG_ERROR(fmt, ...)                                     \
  os_log_error(pandora_kernel_log, fmt, ##__VA_ARGS__)
#define PANDORA_KERNEL_LOG_INFO(fmt, ...)                                      \
  os_log_info(pandora_kernel_log, fmt, ##__VA_ARGS__)
#define PANDORA_KERNEL_LOG_DEBUG(fmt, ...)                                     \
  os_log_debug(pandora_kernel_log, fmt, ##__VA_ARGS__)

#define PANDORA_MEMORY_LOG_ERROR(fmt, ...)                                     \
  os_log_error(pandora_memory_log, fmt, ##__VA_ARGS__)
#define PANDORA_MEMORY_LOG_INFO(fmt, ...)                                      \
  os_log_info(pandora_memory_log, fmt, ##__VA_ARGS__)
#define PANDORA_MEMORY_LOG_DEBUG(fmt, ...)                                     \
  os_log_debug(pandora_memory_log, fmt, ##__VA_ARGS__)

#define PANDORA_USERCLIENT_LOG_ERROR(fmt, ...)                                 \
  os_log_error(pandora_userclient_log, fmt, ##__VA_ARGS__)
#define PANDORA_USERCLIENT_LOG_INFO(fmt, ...)                                  \
  os_log_info(pandora_userclient_log, fmt, ##__VA_ARGS__)
#define PANDORA_USERCLIENT_LOG_DEBUG(fmt, ...)                                 \
  os_log_debug(pandora_userclient_log, fmt, ##__VA_ARGS__)

/*
 * Hacky pointer split macros for logging pointers in a way that
 * bypasses the <private> restriction on os_log.
 */
#if UINTPTR_MAX == 0xffffffffu /* 32-bit */
#define nopriv_psplitf "%08"
#define nopriv_psplit(p) (uint32_t)(uintptr_t)(p)
#elif UINTPTR_MAX == 0xffffffffffffffffull /* 64-bit */
/* Two 32-bit halves: high first, then low (big-endian visual order). */
#define nopriv_psplitf                                                         \
  "%08"                                                                        \
  "%08"
#define nopriv_psplit(p)                                                       \
  (uint32_t)(((uintptr_t)(p) >> 32) & 0xffffffffu),                            \
      (uint32_t)((uintptr_t)(p) & 0xffffffffu)
#else
#error Unsupported pointer size
#endif

static inline void pandora_log_ensure_initialized(void) {
  if (!pandora_log_initialized) {
    pandora_log_init();
  }
}

#endif // PANDORALOG_H
