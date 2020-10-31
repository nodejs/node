/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_BASE_TIME_H_
#define INCLUDE_PERFETTO_BASE_TIME_H_

#include <time.h>

#include <chrono>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WASM)
#include <emscripten/emscripten.h>
#endif

namespace perfetto {
namespace base {

using TimeSeconds = std::chrono::seconds;
using TimeMillis = std::chrono::milliseconds;
using TimeNanos = std::chrono::nanoseconds;

inline TimeNanos FromPosixTimespec(const struct timespec& ts) {
  return TimeNanos(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

void SleepMicroseconds(unsigned interval_us);

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

TimeNanos GetWallTimeNs();
TimeNanos GetThreadCPUTimeNs();

// TODO: Clock that counts time during suspend is not implemented on Windows.
inline TimeNanos GetBootTimeNs() {
  return GetWallTimeNs();
}

#elif PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)

inline TimeNanos GetWallTimeNs() {
  auto init_time_factor = []() -> uint64_t {
    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    return timebase_info.numer / timebase_info.denom;
  };

  static uint64_t monotonic_timebase_factor = init_time_factor();
  return TimeNanos(mach_absolute_time() * monotonic_timebase_factor);
}

// TODO: Clock that counts time during suspend is not implemented on Mac.
inline TimeNanos GetBootTimeNs() {
  return GetWallTimeNs();
}

inline TimeNanos GetThreadCPUTimeNs() {
  mach_port_t this_thread = mach_thread_self();
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  thread_basic_info_data_t info{};
  kern_return_t kr =
      thread_info(this_thread, THREAD_BASIC_INFO,
                  reinterpret_cast<thread_info_t>(&info), &count);
  mach_port_deallocate(mach_task_self(), this_thread);

  if (kr != KERN_SUCCESS) {
    PERFETTO_DFATAL("Failed to get CPU time.");
    return TimeNanos(0);
  }
  return TimeNanos(info.user_time.seconds * 1000000000LL +
                   info.user_time.microseconds * 1000LL +
                   info.system_time.seconds * 1000000000LL +
                   info.system_time.microseconds * 1000LL);
}

#elif PERFETTO_BUILDFLAG(PERFETTO_OS_WASM)

inline TimeNanos GetWallTimeNs() {
  return TimeNanos(static_cast<uint64_t>(emscripten_get_now()) * 1000000);
}

inline TimeNanos GetThreadCPUTimeNs() {
  return TimeNanos(0);
}

// TODO: Clock that counts time during suspend is not implemented on WASM.
inline TimeNanos GetBootTimeNs() {
  return GetWallTimeNs();
}

#elif PERFETTO_BUILDFLAG(PERFETTO_OS_NACL)

// Tracing time doesn't need to work on NaCl since its going away shortly. We
// just need to compile on it. The only function NaCl could support is
// GetWallTimeNs(), but to prevent false hope we leave it unimplemented.

inline TimeNanos GetWallTimeNs() {
  return TimeNanos(0);
}

inline TimeNanos GetThreadCPUTimeNs() {
  return TimeNanos(0);
}

inline TimeNanos GetBootTimeNs() {
  return TimeNanos(0);
}

#else  // posix

constexpr clockid_t kWallTimeClockSource = CLOCK_MONOTONIC;

inline TimeNanos GetTimeInternalNs(clockid_t clk_id) {
  struct timespec ts = {};
  PERFETTO_CHECK(clock_gettime(clk_id, &ts) == 0);
  return FromPosixTimespec(ts);
}

// Return ns from boot. Conversely to GetWallTimeNs, this clock counts also time
// during suspend (when supported).
inline TimeNanos GetBootTimeNs() {
  // Determine if CLOCK_BOOTTIME is available on the first call.
  static const clockid_t kBootTimeClockSource = [] {
    struct timespec ts = {};
    int res = clock_gettime(CLOCK_BOOTTIME, &ts);
    return res == 0 ? CLOCK_BOOTTIME : kWallTimeClockSource;
  }();
  return GetTimeInternalNs(kBootTimeClockSource);
}

inline TimeNanos GetWallTimeNs() {
  return GetTimeInternalNs(kWallTimeClockSource);
}

inline TimeNanos GetThreadCPUTimeNs() {
  return GetTimeInternalNs(CLOCK_THREAD_CPUTIME_ID);
}
#endif

inline TimeSeconds GetBootTimeS() {
  return std::chrono::duration_cast<TimeSeconds>(GetBootTimeNs());
}

inline TimeMillis GetWallTimeMs() {
  return std::chrono::duration_cast<TimeMillis>(GetWallTimeNs());
}

inline TimeSeconds GetWallTimeS() {
  return std::chrono::duration_cast<TimeSeconds>(GetWallTimeNs());
}

inline struct timespec ToPosixTimespec(TimeMillis time) {
  struct timespec ts {};
  const long time_s = static_cast<long>(time.count() / 1000);
  ts.tv_sec = time_s;
  ts.tv_nsec = (static_cast<long>(time.count()) - time_s * 1000L) * 1000000L;
  return ts;
}

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_BASE_TIME_H_
