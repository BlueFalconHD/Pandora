#include "Pandora.h"
#include "Globals.h"
#include "PandoraLog.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <sys/proc.h>

#define super IOService
OSDefineMetaClassAndFinalStructors(Pandora, IOService);

bool Pandora::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }

  pandora_log_ensure_initialized();

  PANDORA_LOG_DEFAULT("Pandora.cpp:(Pandora(IOService)::start): I've been run");
  PANDORA_LOG_DEFAULT("kmod run? %s", kmod_run ? "yes" : "no");
  io_service_start_called = true;
  io_service_start_time = mach_absolute_time();

  registerService();

  // Check if PID 1 (launchd) exists
  proc_t pid1_proc = proc_find(1);
  if (pid1_proc != nullptr) {
    PANDORA_LOG_DEFAULT("PID 1 exists, launchd is running.");
    proc_rele(pid1_proc);
    pid1_exists = true;
  } else {
    PANDORA_LOG_DEFAULT("PID 1 does not exist, we are early enough.");
    pid1_exists = false;
  }

  /*
    // // Initialize KernelUtilities and log the kernel base address
    // KernelUtilities ku;
    // if (ku.init() == KUErrorSuccess) {
    //   // do some hackery to trick os_log into printing the kernel base
    address
    //   // without <private>ing it. split it into two parts and log them as
    //   seperate
    //   // formats while still making it look the same as %llx
    //   uint64_t base = ku.kernelBase();
    //   uint64_t slide = ku.kernelSlideValue();

    //   PANDORA_LOG_DEFAULT("Kernel base address: 0x%08x%08x, slide: 0x%llx",
    //                       (uint32_t)(base >> 32), (uint32_t)(base &
    //                       0xFFFFFFFFu), slide);

    //   setProperty("KernelBase", base, 64);
    //   setProperty("KernelSlide", slide, 64);
    // } else {
    //   PANDORA_LOG_ERROR("Failed to locate kernel base, error: %d",
    //   ku.status()); return false; // prevent further execution if
    //   initialization fails
    // }

    // setProperty("Status", (uint64_t)ku.status(), 32);

    // // Check 64-bit value at unslid<0xFFFFFE000C4F7BF0>
    // uint64_t slidAddress = ku.kslide(0xFFFFFE000C4F7BF0);
    // uint64_t value = 0;
    // KUError readStatus =
    //     KernelUtilities::kread(slidAddress, &value, sizeof(value));
    // if (readStatus != KUErrorSuccess) {
    //   PANDORA_MEMORY_LOG_ERROR("Failed to read value at 0x%llx, error: %d",
    //                            slidAddress, readStatus);
    //   return false;
    // }

    // // Read the boot mode from the kernel flags
    // uint32_t bootMode = get_os_boot_mode(value);

    // // Save to ioreg
    // setProperty("BootMode", bootMode, 32);

    // PANDORA_LOG_DEFAULT("Boot mode: %u", bootMode);

    // // Attempt to decode OSVariant
    // OSVariant v = decode_os_variant(value);

    // // Validate the OSVariant
    // if (is_valid_os_variant(&v) != OSVariantErrorOK) {
    //   PANDORA_LOG_ERROR(
    //       "Invalid OSVariant detected, raw value: 0x%llx, error: %s", value,
    //       os_variant_error_to_string(is_valid_os_variant(&v)));
    // }

    // // Print out all info
    // PANDORA_LOG_DEFAULT("OSVariant Info:");
    // PANDORA_LOG_DEFAULT("  internal_content: %d", v.internal_content);
    // PANDORA_LOG_DEFAULT("  can_has_debugger: %d", v.can_has_debugger);
    // PANDORA_LOG_DEFAULT("  internal_diags_profile: %d",
    // v.internal_diags_profile); PANDORA_LOG_DEFAULT("  factory_content: %d",
    // v.factory_content); PANDORA_LOG_DEFAULT("  is_ephemeral: %d",
    // v.is_ephemeral); PANDORA_LOG_DEFAULT("  base_system_content: %d",
    // v.base_system_content); PANDORA_LOG_DEFAULT("  darwinos_content: %d",
    // v.darwinos_content); PANDORA_LOG_DEFAULT("  has_full_logging: %d",
    // v.has_full_logging); PANDORA_LOG_DEFAULT("  disabled_status_content: %d",
    //                     v.disabled_status_content);
    // PANDORA_LOG_DEFAULT("  disabled_status_diags: %d",
    // v.disabled_status_diags); PANDORA_LOG_DEFAULT("  disabled_status_ui: %d",
    // v.disabled_status_ui); PANDORA_LOG_DEFAULT("  disabled_status_security:
    // %d",
    //                     v.disabled_status_security);
    // PANDORA_LOG_DEFAULT("  os_boot_mode: %d", v.os_boot_mode);
    // PANDORA_LOG_DEFAULT("  magic_tag: %d", v.magic_tag);

    // if (ProcUtils::pidExists(1)) {
    //   PANDORA_LOG_DEFAULT("PID 1 exists, launchd is running.");
    // } else {
    //   PANDORA_LOG_DEFAULT("PID 1 does not exist, we are early enough.");
    // }
  */

  return true;
}

void Pandora::stop(IOService *provider) {
  // Log the stop event
  PANDORA_KERNEL_LOG_INFO("Pandora kext is stopping...");

  // Cleanup the global logging system
  pandora_log_cleanup();

  super::stop(provider);
}
