#include "Globals.h"

static PandoraRuntimeState g_runtime_state = {};

PandoraRuntimeState &pandora_runtime_state() {
  return g_runtime_state;
}

