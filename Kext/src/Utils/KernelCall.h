#pragma once

#include <IOKit/IOReturn.h>
#include <stddef.h>
#include <stdint.h>

// Stub interface for "calling" an arbitrary kernel function by address.
// This is intentionally unimplemented in this repository.
//
// If you later implement a safe/legitimate call mechanism (e.g. to a *publicly
// exported* KPI or to your own exported wrapper), plumb it in here so modules
// don't need to know the details.

struct PandoraKCallResult {
  IOReturn status;
  uint64_t ret0;
};

// Call a function at `fn` with up to 8 integer args. This stub always returns
// `kIOReturnUnsupported`.
PandoraKCallResult pandora_kcall(uint64_t fn, const uint64_t *args,
                                 size_t argCount);
