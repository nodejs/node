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

#include "src/base/test/utils.h"

#include <stdlib.h>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX) ||  \
    PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)
#include <limits.h>
#include <unistd.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) && \
    !PERFETTO_BUILDFLAG(PERFETTO_COMPILER_GCC)
#include <corecrt_io.h>
#include <io.h>
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#include <mach-o/dyld.h>
#endif

namespace perfetto {
namespace base {

std::string GetCurExecutableDir() {
  std::string self_path;
#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)
  char buf[PATH_MAX];
  ssize_t size = readlink("/proc/self/exe", buf, sizeof(buf));
  PERFETTO_CHECK(size != -1);
  // readlink does not null terminate.
  self_path = std::string(buf, static_cast<size_t>(size));
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
  uint32_t size = 0;
  PERFETTO_CHECK(_NSGetExecutablePath(nullptr, &size));
  self_path.resize(size);
  PERFETTO_CHECK(_NSGetExecutablePath(&self_path[0], &size) == 0);
#else
  PERFETTO_FATAL(
      "GetCurExecutableDir() not implemented on the current platform");
#endif
  // Cut binary name.
  return self_path.substr(0, self_path.find_last_of("/"));
}

std::string GetTestDataPath(const std::string& path) {
  std::string self_path = GetCurExecutableDir();
  std::string full_path = self_path + "/../../" + path;
  if (access(full_path.c_str(), 0 /*F_OK*/) == 0)
    return full_path;
  full_path = self_path + "/" + path;
  if (access(full_path.c_str(), 0 /*F_OK*/) == 0)
    return full_path;
  // Fall back to relative to root dir.
  return path;
}

}  // namespace base
}  // namespace perfetto
