#include "PandoraLog.h"
#include <os/log.h>

bool pandora_log_initialized = false;

// Global os_log_t instances, initialized to default
os_log_t pandora_log = OS_LOG_DEFAULT;
os_log_t pandora_kernel_log = OS_LOG_DEFAULT;
os_log_t pandora_memory_log = OS_LOG_DEFAULT;
os_log_t pandora_userclient_log = OS_LOG_DEFAULT;

void pandora_log_init(void) {
  // Create os_log_t instances with custom subsystem and categories
  os_log_t general_log = os_log_create("com.bluefalconhd.pandora", "general");
  os_log_t kernel_log = os_log_create("com.bluefalconhd.pandora", "kernel");
  os_log_t memory_log = os_log_create("com.bluefalconhd.pandora", "memory");
  os_log_t userclient_log =
      os_log_create("com.bluefalconhd.pandora", "userclient");

  // Assign logs, fallback to default if creation fails
  pandora_log = (general_log != OS_LOG_DEFAULT) ? general_log : OS_LOG_DEFAULT;
  pandora_kernel_log =
      (kernel_log != OS_LOG_DEFAULT) ? kernel_log : OS_LOG_DEFAULT;
  pandora_memory_log =
      (memory_log != OS_LOG_DEFAULT) ? memory_log : OS_LOG_DEFAULT;
  pandora_userclient_log =
      (userclient_log != OS_LOG_DEFAULT) ? userclient_log : OS_LOG_DEFAULT;

  if (pandora_log != OS_LOG_DEFAULT) {
    PANDORA_LOG_DEFAULT(
        "Pandora logging system initialized with multiple categories");
  } else {
    os_log_error(OS_LOG_DEFAULT,
                 "Failed to create custom os_log_t instances, using default");
  }

  pandora_log_initialized = true;
}

void pandora_log_cleanup(void) {
  if (pandora_log != OS_LOG_DEFAULT) {
    PANDORA_LOG_INFO("Pandora logging system shutting down");
  }

  // Release all log instances
  if (pandora_log != OS_LOG_DEFAULT) {
    os_release(pandora_log);
    pandora_log = OS_LOG_DEFAULT;
  }
  if (pandora_kernel_log != OS_LOG_DEFAULT) {
    os_release(pandora_kernel_log);
    pandora_kernel_log = OS_LOG_DEFAULT;
  }
  if (pandora_memory_log != OS_LOG_DEFAULT) {
    os_release(pandora_memory_log);
    pandora_memory_log = OS_LOG_DEFAULT;
  }
  if (pandora_userclient_log != OS_LOG_DEFAULT) {
    os_release(pandora_userclient_log);
    pandora_userclient_log = OS_LOG_DEFAULT;
  }

  pandora_log_initialized = false;
}
