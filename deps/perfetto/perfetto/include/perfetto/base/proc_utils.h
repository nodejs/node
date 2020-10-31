/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_BASE_PROC_UTILS_H_
#define INCLUDE_PERFETTO_BASE_PROC_UTILS_H_

#include <stdint.h>

#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <Windows.h>
#include <processthreadsapi.h>
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)
#include <zircon/process.h>
#include <zircon/types.h>
#else
#include <unistd.h>
#endif

namespace perfetto {
namespace base {

#if PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)
using PlatformProcessId = zx_handle_t;
inline PlatformProcessId GetProcessId() {
  return zx_process_self();
}
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
using PlatformProcessId = uint64_t;
inline PlatformProcessId GetProcessId() {
  return static_cast<uint64_t>(GetCurrentProcessId());
}
#else
using PlatformProcessId = pid_t;
inline PlatformProcessId GetProcessId() {
  return getpid();
}
#endif

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_BASE_PROC_UTILS_H_
