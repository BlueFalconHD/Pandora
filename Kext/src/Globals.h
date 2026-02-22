#pragma once

#include "Utils/TimeUtilities.h"

#include <stdint.h>

struct PandoraTelemetryState {
  bool kmodRan = false;
  TimestampPair kmodStartTime = {};

  bool ioServiceStartCalled = false;
  TimestampPair ioServiceStartTime = {};

  bool userClientInitialized = false;
  TimestampPair userClientInitTime = {};

  bool pid1Exists = false;
  bool workloopSawZero = false;
};

struct PandoraDebugState {
  uint64_t extraErrorData1 = 0;
  uint64_t extraErrorData2 = 0;
  uint64_t extraErrorData3 = 0;
};

struct PandoraRuntimeState {
  PandoraTelemetryState telemetry;
  PandoraDebugState debug;
};

PandoraRuntimeState &pandora_runtime_state();

