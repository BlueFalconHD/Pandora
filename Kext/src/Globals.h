#pragma once

#include "Utils/TimeUtilities.h"
#include <stdbool.h>
#include <stdint.h>

// Global variable to track if the kmod has run
extern bool kmod_run;

// Global variable to track when (timestamp) the kmod start function was called
extern TimestampPair kmod_start_time;

// Global variable to track whether the IOService start function has been called
extern bool io_service_start_called;

// Global variable to track when (timestamp) the IOService start function was
// called
extern TimestampPair io_service_start_time;

// Global variable to track whether the user client has been initialized (only
// changes on first user client connection)
extern bool user_client_initialized;

// Global variable to track when (timestamp) the last user client initialization
// was done
extern TimestampPair user_client_init_time;

// Global variable to track if PID 1 (launchd) exists at the time of kext start
// (in ioservice)
extern bool pid1_exists;

// Extra error data for debugging purposes
extern uint64_t extraerrdata1;
extern uint64_t extraerrdata2;
extern uint64_t extraerrdata3;

extern bool workloopsaw0;
