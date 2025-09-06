#include "TimeUtilities.h"
#include "PandoraLog.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <mach/mach_time.h>
#include <sys/time.h>

// Static member definitions
bool TimeUtilities::initialized_ = false;
mach_timebase_info_data_t TimeUtilities::timebaseInfo_ = {0, 0};
uint64_t TimeUtilities::bootTime_ = 0;

void TimeUtilities::init() {
  if (initialized_) {
    return;
  }

  // Get timebase info for converting mach_absolute_time
  clock_timebase_info(&timebaseInfo_);

  PANDORA_LOG_DEBUG("TimeUtilities::init: Timebase numer=%u, denom=%u",
                    timebaseInfo_.numer, timebaseInfo_.denom);

  calculateBootTime();
  initialized_ = true;

  PANDORA_LOG_INFO("TimeUtilities initialized - boot time: %llu", bootTime_);
}

uint64_t TimeUtilities::getCurrentUnixTimestamp() {
  if (!initialized_) {
    init();
  }

  clock_sec_t seconds;
  clock_usec_t microseconds;
  clock_get_calendar_microtime(&seconds, &microseconds);

  return (uint64_t)seconds;
}

void TimeUtilities::getCurrentUnixTime(uint32_t *seconds,
                                       uint32_t *microseconds) {
  if (!seconds || !microseconds) {
    return;
  }

  if (!initialized_) {
    init();
  }

  clock_sec_t sec = 0;
  clock_usec_t usec = 0;

  // Only call calendar time if we've been running for a bit
  uint64_t machTime = mach_absolute_time();
  if (machTime > 1000000000ULL || bootTime_ > 0) {
    clock_get_calendar_microtime(&sec, &usec);
  }

  *seconds = (uint32_t)sec;
  *microseconds = (uint32_t)usec;
}

uint64_t TimeUtilities::machTimeToNanoseconds(uint64_t machTime) {
  if (!initialized_) {
    init();
  }

  // Convert mach time to nanoseconds
  // Formula: (machTime * numer) / denom
  uint64_t nanos = (machTime * timebaseInfo_.numer) / timebaseInfo_.denom;
  return nanos;
}

uint64_t TimeUtilities::machTimeToMilliseconds(uint64_t machTime) {
  return machTimeToNanoseconds(machTime) / 1000000ULL;
}

uint64_t TimeUtilities::machTimeToSeconds(uint64_t machTime) {
  return machTimeToNanoseconds(machTime) / 1000000000ULL;
}

uint64_t TimeUtilities::getBootTime() {
  if (!initialized_) {
    init();
  }
  return bootTime_;
}

uint64_t TimeUtilities::machTimeToUnixTimestamp(uint64_t machTime) {
  if (!initialized_) {
    init();
  }

  // Convert mach time to seconds since boot
  uint64_t secondsSinceBoot = machTimeToSeconds(machTime);

  // Add to boot time to get Unix timestamp
  return bootTime_ + secondsSinceBoot;
}

uint64_t TimeUtilities::getElapsedMilliseconds(uint64_t startTime,
                                               uint64_t endTime) {
  if (endTime < startTime) {
    return 0;
  }

  uint64_t elapsed = endTime - startTime;
  return machTimeToMilliseconds(elapsed);
}

uint64_t TimeUtilities::getElapsedSeconds(uint64_t startTime,
                                          uint64_t endTime) {
  if (endTime < startTime) {
    return 0;
  }

  uint64_t elapsed = endTime - startTime;
  return machTimeToSeconds(elapsed);
}

void TimeUtilities::calculateBootTime() {
  // Get current time
  uint64_t currentMachTime = mach_absolute_time();

  // Safety check: don't calculate boot time too early in boot process
  if (currentMachTime < 1000000000ULL) {
    PANDORA_LOG_DEBUG("TimeUtilities::calculateBootTime: Too early in boot "
                      "process, deferring");
    return;
  }

  uint64_t currentUnixTime = getCurrentUnixTimestamp();

  // Calculate seconds since boot
  uint64_t secondsSinceBoot = machTimeToSeconds(currentMachTime);

  // Boot time = current time - time since boot
  bootTime_ = currentUnixTime - secondsSinceBoot;

  PANDORA_LOG_DEBUG("TimeUtilities::calculateBootTime: Current Unix: %llu, "
                    "Seconds since boot: %llu, Boot time: %llu",
                    currentUnixTime, secondsSinceBoot, bootTime_);
}
