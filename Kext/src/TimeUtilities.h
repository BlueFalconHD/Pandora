#ifndef TIME_UTILITIES_H
#define TIME_UTILITIES_H

#include <kern/clock.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <sys/time.h>

// Time conversion utilities for kernel extension
class TimeUtilities {
public:
  // Initialize timebase info (call once during kext startup)
  static void init();

  // Get current Unix timestamp (seconds since epoch)
  static uint64_t getCurrentUnixTimestamp();

  // Get current Unix timestamp with microseconds
  static void getCurrentUnixTime(uint32_t *seconds, uint32_t *microseconds);

  // Convert mach_absolute_time to nanoseconds since boot
  static uint64_t machTimeToNanoseconds(uint64_t machTime);

  // Convert mach_absolute_time to milliseconds since boot
  static uint64_t machTimeToMilliseconds(uint64_t machTime);

  // Convert mach_absolute_time to seconds since boot
  static uint64_t machTimeToSeconds(uint64_t machTime);

  // Get boot time as Unix timestamp
  static uint64_t getBootTime();

  // Convert mach_absolute_time to Unix timestamp
  // (boot time + elapsed time since boot)
  static uint64_t machTimeToUnixTimestamp(uint64_t machTime);

  // Get elapsed time between two mach_absolute_time values in milliseconds
  static uint64_t getElapsedMilliseconds(uint64_t startTime, uint64_t endTime);

  // Get elapsed time between two mach_absolute_time values in seconds
  static uint64_t getElapsedSeconds(uint64_t startTime, uint64_t endTime);

private:
  static bool initialized_;
  static mach_timebase_info_data_t timebaseInfo_;
  static uint64_t bootTime_; // Unix timestamp of boot time

  // Calculate boot time (called during init)
  static void calculateBootTime();
};

// Convenience macros
#define TIME_UTILS_CURRENT_UNIX_TIME() TimeUtilities::getCurrentUnixTimestamp()
#define TIME_UTILS_MACH_TO_UNIX(machTime)                                      \
  TimeUtilities::machTimeToUnixTimestamp(machTime)
#define TIME_UTILS_MACH_TO_MS(machTime)                                        \
  TimeUtilities::machTimeToMilliseconds(machTime)
#define TIME_UTILS_ELAPSED_MS(start, end)                                      \
  TimeUtilities::getElapsedMilliseconds(start, end)

// Structure for holding both types of timestamps for debugging
struct TimestampPair {
  uint64_t machTime;       // mach_absolute_time value
  uint64_t unixTime;       // Unix timestamp equivalent
  uint64_t nanosSinceBoot; // Nanoseconds since boot
};

// Helper to create a timestamp pair from current time
// Note: unixTime will be 0 if called too early in boot before calendar time is
// available
static inline TimestampPair makeCurrentTimestampPair() {
  TimestampPair pair;
  pair.machTime = mach_absolute_time();

  // Only try to get Unix time if we've been running for a while
  // This prevents calling calendar time functions too early in boot
  if (pair.machTime > 1000000000ULL) {
    pair.unixTime = TimeUtilities::machTimeToUnixTimestamp(pair.machTime);
  } else {
    pair.unixTime = 0;
  }

  pair.nanosSinceBoot = TimeUtilities::machTimeToNanoseconds(pair.machTime);
  return pair;
}

// Helper to create a timestamp pair from existing mach time
// Note: unixTime will be 0 if called too early in boot before calendar time is
// available
static inline TimestampPair makeTimestampPair(uint64_t machTime) {
  TimestampPair pair;
  pair.machTime = machTime;

  // Only try to get Unix time if we've been running for a while
  // This prevents calling calendar time functions too early in boot
  if (machTime > 1000000000ULL) {
    pair.unixTime = TimeUtilities::machTimeToUnixTimestamp(machTime);
  } else {
    pair.unixTime = 0;
  }

  pair.nanosSinceBoot = TimeUtilities::machTimeToNanoseconds(machTime);
  return pair;
}

#endif // TIME_UTILITIES_H
