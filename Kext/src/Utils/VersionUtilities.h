#pragma once
#include "PandoraLog.h"

#include <stdint.h>
#include <string.h>

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOService.h>

struct VersionEntry {
  const char *build;            // e.g. "25A5346a"
  const char *deviceIdentifier; // e.g. "Mac14,7"
  const uintptr_t osvariant_status_backing;
};

constexpr uintptr_t kNoHook = UINTPTR_MAX;

static constexpr VersionEntry kVersionTable[] = {
    {"25A5346a", "Mac14,7", 0xfffffe000c527cf0ULL},
    {"25A5351b", "Mac14,7", 0xFFFFFE000C52BCF0ULL},
    {"25A5351b", "Mac15,13", 0xFFFFFE000C4C7D00ULL},
};

static constexpr size_t kVersionTableCount =
    sizeof(kVersionTable) / sizeof(kVersionTable[0]);

class VersionUtilities {
public:
  bool init() {
    IORegistryEntry *root = IORegistryEntry::getRegistryRoot();

    if (!root) {
      PANDORA_LOG_ERROR("VersionUtilities::init: Failed to get registry root");
      return false;
    }

    OSObject *buildVersionObj = root->getProperty(kOSBuildVersionKey);

    if (!buildVersionObj) {
      PANDORA_LOG_ERROR("VersionUtilities::init: Failed to get build version "
                        "property from registry root");
      return false;
    }

    OSString *buildVersionStr = OSDynamicCast(OSString, buildVersionObj);
    if (buildVersionStr == nullptr) {
      PANDORA_LOG_ERROR("VersionUtilities::init: Build version property is not "
                        "of type OSString");
      return false;
    }

    const char *buildVersionCStr = buildVersionStr->getCStringNoCopy();

    // print the build version for debugging purposes
    PANDORA_LOG_DEFAULT("VersionUtilities::init: Detected build version: %s",
                        buildVersionCStr);

    strlcpy(_buildVersion, buildVersionCStr, sizeof(_buildVersion));

    // Get IOPlatformExpertDevice IOService
    OSDictionary *matching =
        IOService::serviceMatching("IOPlatformExpertDevice");
    if (!matching) {
      PANDORA_LOG_ERROR("init: Failed to create matching dictionary for "
                        "IOPlatformExpertDevice");
      return false;
    }

    IOService *platformService = IOService::waitForService(matching);
    if (!platformService) {
      PANDORA_LOG_ERROR("init: IOPlatformExpertDevice IOService not found");
      return false;
    }

    PANDORA_LOG_DEFAULT("init: Found IOPlatformExpertDevice IOService: %s",
                        platformService->getName());

    OSObject *prop = platformService->getProperty("model");
    if (!prop) {
      PANDORA_LOG_ERROR("init: Failed to get 'model' property");
      platformService->release();
      return false;
    }

    OSData *modelData = OSDynamicCast(OSData, prop);
    if (!modelData) {
      PANDORA_LOG_ERROR("init: 'model' property is not OSData");
      platformService->release();
      return false;
    }

    const uint8_t *bytes =
        static_cast<const uint8_t *>(modelData->getBytesNoCopy());
    size_t len = modelData->getLength();

    // Copy up to first NUL or buffer-1
    size_t i = 0;
    const size_t maxOut = sizeof(_modelIdentifier) - 1;
    for (; i < len && i < maxOut && bytes[i] != 0; ++i) {
    }
    if (i)
      bcopy(bytes, _modelIdentifier, i);
    _modelIdentifier[i] = '\0';

    PANDORA_LOG_DEFAULT("init: Detected model identifier: %s",
                        _modelIdentifier);

    platformService->release();
    return true;
  }

  const char *getBuildVersion() const { return _buildVersion; }
  const char *getModelIdentifier() const { return _modelIdentifier; }

  bool isSupportedVersion() const {
    const VersionEntry *entry = getVersionEntry();
    return entry != nullptr;
  }

  bool getOsVariantStatusBacking(uintptr_t *data) const {
    const VersionEntry *entry = getVersionEntry();
    if (entry) {
      if (entry->osvariant_status_backing != kNoHook) {
        *data = entry->osvariant_status_backing;
        return true;
      }
      *data = 0;
      return false;
    }
    *data = 0;
    return false;
  }

private:
  char _buildVersion[256];
  char _modelIdentifier[256];

  const VersionEntry *getVersionEntry() const {
    for (size_t i = 0; i < kVersionTableCount; ++i) {
      if (strcmp(kVersionTable[i].build, _buildVersion) == 0 &&
          strcmp(kVersionTable[i].deviceIdentifier, _modelIdentifier) == 0) {
        return &kVersionTable[i];
      }
    }
    return nullptr;
  }
};
