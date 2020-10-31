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

#ifndef INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_H_
#define INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_H_

#include <functional>

#include "perfetto/base/build_config.h"

// The POSIX watchdog is only supported on Linux and Android in non-embedder
// builds.
#if PERFETTO_BUILDFLAG(PERFETTO_WATCHDOG)
#include "perfetto/ext/base/watchdog_posix.h"
#else
#include "perfetto/ext/base/watchdog_noop.h"
#endif

namespace perfetto {
namespace base {

// Make the limits more relaxed on desktop, where multi-GB traces are likely.
// Multi-GB traces can take bursts of cpu time to write into disk at the end of
// the trace.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
constexpr uint32_t kWatchdogDefaultCpuLimit = 75;
constexpr uint32_t kWatchdogDefaultCpuWindow = 5 * 60 * 1000;  // 5 minutes.
#else
constexpr uint32_t kWatchdogDefaultCpuLimit = 90;
constexpr uint32_t kWatchdogDefaultCpuWindow = 10 * 60 * 1000;  // 10 minutes.
#endif

// The default memory margin we give to our processes. This is used as as a
// constant to put on top of the trace buffers.
constexpr uint64_t kWatchdogDefaultMemorySlack = 32 * 1024 * 1024;  // 32 MiB.
constexpr uint32_t kWatchdogDefaultMemoryWindow = 30 * 1000;  // 30 seconds.

inline void RunTaskWithWatchdogGuard(const std::function<void()>& task) {
  // Maximum time a single task can take in a TaskRunner before the
  // program suicides.
  constexpr int64_t kWatchdogMillis = 30000;  // 30s

  Watchdog::Timer handle =
      base::Watchdog::GetInstance()->CreateFatalTimer(kWatchdogMillis);
  task();

  // Suppress unused variable warnings in the client library amalgamated build.
  (void)kWatchdogDefaultCpuLimit;
  (void)kWatchdogDefaultCpuWindow;
  (void)kWatchdogDefaultMemorySlack;
  (void)kWatchdogDefaultMemoryWindow;
}

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_H_
