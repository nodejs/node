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
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/thread-cpu-clock.hh
// and
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/thread-cpu-clock.cc
//
// SPDX-License-Identifier: Apache-2.0

#ifndef SRC_PPROF_CPU_CLOCK_H_
#define SRC_PPROF_CPU_CLOCK_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <pthread.h>
#include <time.h>
#endif

namespace node {
namespace pprof {

// CPU time consumed by the calling thread, in nanoseconds. Async-signal-safe
// on POSIX.
int64_t CurrentThreadCpuNanos();

// CPU time consumed by the whole process, in nanoseconds.
int64_t ProcessCpuNanos();

#ifndef _WIN32
// Must be called at least once, outside of a signal handler, before
// MonotonicNowMicros() is used from one.
void InitializeMonotonicClock();

// The current value of the clock that V8's base::TimeTicks::Now() reads, in
// microseconds, so that it can be compared with CpuProfile sample timestamps.
// V8's base library is not available to Node.js sources, so this replicates
// TimeTicks::Now() from deps/v8/src/base/platform/time.cc. Async-signal-safe.
int64_t MonotonicNowMicros();
#endif  // _WIN32

// Reads the CPU time of the thread that created it, from any thread.
class ThreadCpuClock {
 public:
  ThreadCpuClock();
  ~ThreadCpuClock();
  ThreadCpuClock(const ThreadCpuClock&) = delete;
  ThreadCpuClock& operator=(const ThreadCpuClock&) = delete;

  int64_t NowNanos() const;

 private:
#ifdef _WIN32
  HANDLE thread_ = nullptr;
#elif defined(__APPLE__)
  mach_port_t thread_ = MACH_PORT_NULL;
#else
  clockid_t clockid_{};
  bool valid_ = false;
#endif
};

// Measures the CPU time consumed by the thread that created it since the
// previous call to GetAndReset().
class ThreadCpuStopWatch {
 public:
  ThreadCpuStopWatch() : last_(clock_.NowNanos()) {}

  int64_t GetAndReset() {
    int64_t now = clock_.NowNanos();
    int64_t elapsed = now - last_;
    last_ = now;
    return elapsed;
  }

 private:
  ThreadCpuClock clock_;
  int64_t last_;
};

}  // namespace pprof
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PPROF_CPU_CLOCK_H_
