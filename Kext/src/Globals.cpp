#include "Globals.h"

// Global variable to track if the kmod has run
bool kmod_run = false;

// Global variable to track when (timestamp) the kmod start function was called
TimestampPair kmod_start_time = {};

// Global variable to track whether the IOService start function has been called
bool io_service_start_called = false;

// Global variable to track when (timestamp) the IOService start function was
// called
TimestampPair io_service_start_time = {};

// Global variable to track whether the user client has been initialized (only
// changes on first user client connection)
bool user_client_initialized = false;

// Global variable to track when (timestamp) the last user client initialization
// was done
TimestampPair user_client_init_time = {};

// Global variable to track if PID 1 (launchd) exists at the time of kext start
// (in ioservice)
bool pid1_exists = false;
