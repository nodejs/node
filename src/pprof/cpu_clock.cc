// Copyright 2023 Datadog, Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified for inclusion in Node.js core: ported from
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/thread-cpu-clock.cc
//
// SPDX-License-Identifier: Apache-2.0

#include "pprof/cpu_clock.h"

#ifdef __APPLE__
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <pthread.h>
#include <time.h>
#endif

#include <atomic>

namespace node {
namespace pprof {

namespace {

constexpr int64_t kNanosPerSecond = 1000 * 1000 * 1000;
constexpr int64_t kNanosPerMicro = 1000;

#ifdef _WIN32
int64_t FileTimeToNanos(FILETIME t) {
  return static_cast<int64_t>((static_cast<uint64_t>(t.dwHighDateTime) << 32) |
                              static_cast<uint64_t>(t.dwLowDateTime)) *
         100;
}

int64_t ThreadTimesNanos(HANDLE thread) {
  FILETIME creation_time, exit_time, kernel_time, user_time;
  if (!GetThreadTimes(
          thread, &creation_time, &exit_time, &kernel_time, &user_time)) {
    return 0;
  }
  return FileTimeToNanos(kernel_time) + FileTimeToNanos(user_time);
}
#else
int64_t ClockNanos(clockid_t clock_id) {
  timespec ts;
  if (clock_gettime(clock_id, &ts) != 0) return 0;
  return static_cast<int64_t>(ts.tv_sec) * kNanosPerSecond + ts.tv_nsec;
}
#endif  // _WIN32

#ifdef __APPLE__
// Written once by InitializeMonotonicClock() before any signal handler reads
// it, then read-only.
std::atomic<uint32_t> timebase_numer{0};
std::atomic<uint32_t> timebase_denom{0};
#endif  // __APPLE__

}  // namespace

int64_t CurrentThreadCpuNanos() {
#ifdef _WIN32
  return ThreadTimesNanos(GetCurrentThread());
#else
  return ClockNanos(CLOCK_THREAD_CPUTIME_ID);
#endif
}

int64_t ProcessCpuNanos() {
#ifdef _WIN32
  FILETIME creation_time, exit_time, kernel_time, user_time;
  if (!GetProcessTimes(GetCurrentProcess(),
                       &creation_time,
                       &exit_time,
                       &kernel_time,
                       &user_time)) {
    return 0;
  }
  return FileTimeToNanos(kernel_time) + FileTimeToNanos(user_time);
#else
  return ClockNanos(CLOCK_PROCESS_CPUTIME_ID);
#endif
}

#ifndef _WIN32
void InitializeMonotonicClock() {
#ifdef __APPLE__
  if (timebase_denom.load(std::memory_order_acquire) != 0) return;
  mach_timebase_info_data_t info;
  if (mach_timebase_info(&info) != KERN_SUCCESS || info.denom == 0) {
    info.numer = 1;
    info.denom = 1;
  }
  timebase_numer.store(info.numer, std::memory_order_relaxed);
  timebase_denom.store(info.denom, std::memory_order_release);
#endif  // __APPLE__
}

int64_t MonotonicNowMicros() {
  int64_t ticks;
#ifdef __APPLE__
  uint64_t numer = timebase_numer.load(std::memory_order_relaxed);
  uint64_t denom = timebase_denom.load(std::memory_order_acquire);
  if (denom == 0) {
    numer = 1;
    denom = 1;
  }
  ticks = static_cast<int64_t>(mach_absolute_time() * numer /
                               (kNanosPerMicro * denom));
#else
  ticks = ClockNanos(CLOCK_MONOTONIC) / kNanosPerMicro;
#endif  // __APPLE__
  // V8 makes sure TimeTicks::Now() never returns 0.
  return ticks + 1;
}
#endif  // _WIN32

ThreadCpuClock::ThreadCpuClock() {
#ifdef _WIN32
  // GetCurrentThread() returns a pseudo handle that always refers to the
  // calling thread, so it must be duplicated to read this thread's times from
  // another thread.
  if (!DuplicateHandle(GetCurrentProcess(),
                       GetCurrentThread(),
                       GetCurrentProcess(),
                       &thread_,
                       THREAD_QUERY_LIMITED_INFORMATION,
                       FALSE,
                       0)) {
    thread_ = nullptr;
  }
#elif defined(__APPLE__)
  // Unlike mach_thread_self(), this does not add a reference to the port.
  thread_ = pthread_mach_thread_np(pthread_self());
#else
  valid_ = pthread_getcpuclockid(pthread_self(), &clockid_) == 0;
#endif
}

ThreadCpuClock::~ThreadCpuClock() {
#ifdef _WIN32
  if (thread_ != nullptr) CloseHandle(thread_);
#endif
}

int64_t ThreadCpuClock::NowNanos() const {
#ifdef _WIN32
  if (thread_ == nullptr) return 0;
  return ThreadTimesNanos(thread_);
#elif defined(__APPLE__)
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  thread_basic_info_data_t info;
  kern_return_t kr = thread_info(thread_,
                                 THREAD_BASIC_INFO,
                                 reinterpret_cast<thread_info_t>(&info),
                                 &count);
  if (kr != KERN_SUCCESS) return 0;
  return (static_cast<int64_t>(info.user_time.seconds) +
          info.system_time.seconds) *
             kNanosPerSecond +
         (static_cast<int64_t>(info.user_time.microseconds) +
          info.system_time.microseconds) *
             kNanosPerMicro;
#else
  if (!valid_) return 0;
  return ClockNanos(clockid_);
#endif
}

}  // namespace pprof
}  // namespace node
