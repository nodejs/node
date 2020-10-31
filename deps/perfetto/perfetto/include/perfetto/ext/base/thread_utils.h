/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_BASE_THREAD_UTILS_H_
#define INCLUDE_PERFETTO_EXT_BASE_THREAD_UTILS_H_

#include <string>

#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#include <pthread.h>
#include <string.h>
#include <algorithm>
#endif

// Internal implementation utils that aren't as widely useful/supported as
// base/thread_utils.h.

namespace perfetto {
namespace base {

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
// Sets the "comm" of the calling thread to the first 15 chars of the given
// string.
inline bool MaybeSetThreadName(const std::string& name) {
  char buf[16] = {};
  size_t sz = std::min(name.size(), static_cast<size_t>(15));
  strncpy(buf, name.c_str(), sz);

#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
  return pthread_setname_np(buf) == 0;
#else
  return pthread_setname_np(pthread_self(), buf) == 0;
#endif
}
#else
inline void MaybeSetThreadName(const std::string&) {}
#endif

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_THREAD_UTILS_H_
