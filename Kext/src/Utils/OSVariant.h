#ifndef OS_VARIANT_H
#define OS_VARIANT_H

#include <stdint.h>

// 0-1   internal_content (0=absent,1=present,2=unknown,3=forced-on)
// 2-3   can_has_debugger
// 6-7   internal_diags_profile
// 8-9   factory_content
// 14-15 is_ephemeral
// 18-19 base_system_content
// 22-23 darwinos_content
// 24-25 has_full_logging
// 28-31 constant 0xF (sentinel marking “userland-generated”)

// 32    disabled_status.content   (set if “content” is disabled)
// 33    disabled_status.diags     (... “diagnostics”)
// 34    disabled_status.ui        (... “ui”)
// 35    disabled_status.security  (... “security”)

// 48-51 os_boot_mode
//         1 = normal (no string),
//         2 = “fvunlock”, 3 = “kcgen”, 4 = “diagnostics”,
//         5 = “migration”, 6 = “eacs”, 7 = “post-upgrade”

// 60-62 constant 0x7 (another magic tag)

// don't add reserved bits here, only the sentinel bits and magic tags
struct OSVariant {
  uint64_t internal_content : 2;       // 0-1
  uint64_t can_has_debugger : 2;       // 2-3
  uint64_t internal_diags_profile : 2; // 6-7
  uint64_t factory_content : 2;        // 8-9
  uint64_t is_ephemeral : 2;           // 14-15
  uint64_t base_system_content : 2;    // 18-19
  uint64_t darwinos_content : 2;       // 22-23
  uint64_t has_full_logging : 2;       // 24-25
  uint64_t user_sentinel : 4;          // 28-31 (constant sentinel)

  uint64_t disabled_status_content : 1;  // 32
  uint64_t disabled_status_diags : 1;    // 33
  uint64_t disabled_status_ui : 1;       // 34
  uint64_t disabled_status_security : 1; // 35

  uint64_t os_boot_mode : 4; // 48-51
  uint64_t magic_tag : 3;    // 60-62 (constant sentinel)
};

/*
 * Decode an OSVariant from a 64-bit integer.
 * The input is expected to be in the same format as the OSVariant structure.
 */
static inline struct OSVariant decode_os_variant(uint64_t value) {
  struct OSVariant variant;
  variant.internal_content = (value >> 0) & 0x3;
  variant.can_has_debugger = (value >> 2) & 0x3;
  variant.internal_diags_profile = (value >> 6) & 0x3;
  variant.factory_content = (value >> 8) & 0x3;
  variant.is_ephemeral = (value >> 14) & 0x3;
  variant.base_system_content = (value >> 18) & 0x3;
  variant.darwinos_content = (value >> 22) & 0x3;
  variant.has_full_logging = (value >> 24) & 0x3;

  variant.disabled_status_content = (value >> 32) & 0x1;
  variant.disabled_status_diags = (value >> 33) & 0x1;
  variant.disabled_status_ui = (value >> 34) & 0x1;
  variant.disabled_status_security = (value >> 35) & 0x1;

  variant.os_boot_mode = (value >> 48) & 0xF; // Boot mode
  variant.magic_tag = (value >> 60) & 0x7;    // Magic tag

  return variant;
}

enum OSVariantError {
  OSVariantErrorOK = 0,
  OSVariantErrorInvalidMagicTag,
  OSVariantErrorInvalidUserSentinel,
  OSVariantErrorInvalidBootMode,
  OSVariantErrorInvalidInternalContent,
  OSVariantErrorInvalidCanHasDebugger,
  OSVariantErrorInvalidInternalDiagsProfile,
  OSVariantErrorInvalidFactoryContent,
  OSVariantErrorInvalidIsEphemeral,
  OSVariantErrorInvalidBaseSystemContent,
  OSVariantErrorInvalidDarwinosContent,
  OSVariantErrorInvalidHasFullLogging
};

/*
 * Convert an OSVariantError to a human-readable string.
 */
static inline const char *os_variant_error_to_string(OSVariantError error) {
  switch (error) {
  case OSVariantErrorOK:
    return "OK";
  case OSVariantErrorInvalidMagicTag:
    return "Invalid magic tag";
  case OSVariantErrorInvalidUserSentinel:
    return "Invalid user sentinel";
  case OSVariantErrorInvalidBootMode:
    return "Invalid boot mode";
  case OSVariantErrorInvalidInternalContent:
    return "Invalid internal content";
  case OSVariantErrorInvalidCanHasDebugger:
    return "Invalid can has debugger";
  case OSVariantErrorInvalidInternalDiagsProfile:
    return "Invalid internal diagnostics profile";
  case OSVariantErrorInvalidFactoryContent:
    return "Invalid factory content";
  case OSVariantErrorInvalidIsEphemeral:
    return "Invalid is ephemeral";
  case OSVariantErrorInvalidBaseSystemContent:
    return "Invalid base system content";
  case OSVariantErrorInvalidDarwinosContent:
    return "Invalid darwinos content";
  case OSVariantErrorInvalidHasFullLogging:
    return "Invalid has full logging";
  default:
    return "Unknown error";
  }
}

/*
 * Verify if the OSVariant is valid.
 * Checks if the sentinels are set correctly and if the values are within
 * expected ranges.
 */
static inline OSVariantError
is_valid_os_variant(const struct OSVariant *variant) {
  if (variant->magic_tag != 0x7) {
    return OSVariantErrorInvalidMagicTag;
  }

  // Check special bits
  if (variant->user_sentinel != 0xF) {
    return OSVariantErrorInvalidUserSentinel;
  }

  if (variant->internal_content > 3 || variant->internal_content < 0 ||
      variant->internal_content == 1) {
    return OSVariantErrorInvalidInternalContent;
  }

  if (variant->can_has_debugger > 3 || variant->can_has_debugger < 0 ||
      variant->can_has_debugger == 1) {
    return OSVariantErrorInvalidCanHasDebugger;
  }

  if (variant->internal_diags_profile > 3 ||
      variant->internal_diags_profile < 0 ||
      variant->internal_diags_profile == 1) {
    return OSVariantErrorInvalidInternalDiagsProfile;
  }

  if (variant->factory_content > 3 || variant->factory_content < 0 ||
      variant->factory_content == 1) {
    return OSVariantErrorInvalidFactoryContent;
  }

  if (variant->is_ephemeral > 3 || variant->is_ephemeral < 0 ||
      variant->is_ephemeral == 1) {
    return OSVariantErrorInvalidIsEphemeral;
  }

  if (variant->base_system_content > 3 || variant->base_system_content < 0 ||
      variant->base_system_content == 1) {
    return OSVariantErrorInvalidBaseSystemContent;
  }

  if (variant->darwinos_content > 3 || variant->darwinos_content < 0 ||
      variant->darwinos_content == 1) {
    return OSVariantErrorInvalidDarwinosContent;
  }

  if (variant->has_full_logging > 3 || variant->has_full_logging < 0 ||
      variant->has_full_logging == 1) {
    return OSVariantErrorInvalidHasFullLogging;
  }

  if (variant->os_boot_mode < 1 || variant->os_boot_mode > 7) {
    return OSVariantErrorInvalidBootMode;
  }

  return OSVariantErrorOK;
}

#endif // OS_VARIANT_H
