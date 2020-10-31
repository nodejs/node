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

#include "src/tracing/ipc/memfd.h"

#include <errno.h>

#define PERFETTO_MEMFD_ENABLED()             \
  PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
      PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX)

#if PERFETTO_MEMFD_ENABLED()

#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

// Some android build bots use a sysroot that doesn't support memfd when
// compiling for the host, so we redefine it if necessary.
#if !defined(__NR_memfd_create)
#if defined(__x86_64__)
#define __NR_memfd_create 319
#elif defined(__i386__)
#define __NR_memfd_create 356
#elif defined(__aarch64__)
#define __NR_memfd_create 279
#elif defined(__arm__)
#define __NR_memfd_create 385
#else
#error "unsupported sysroot without memfd support"
#endif
#endif  // !defined(__NR_memfd_create)

namespace perfetto {
bool HasMemfdSupport() {
  static bool kSupportsMemfd = [] {
    // Check kernel version supports memfd_create(). Some older kernels segfault
    // executing memfd_create() rather than returning ENOSYS (b/116769556).
    static constexpr int kRequiredMajor = 3;
    static constexpr int kRequiredMinor = 17;
    struct utsname uts;
    int major, minor;
    if (uname(&uts) == 0 && strcmp(uts.sysname, "Linux") == 0 &&
        sscanf(uts.release, "%d.%d", &major, &minor) == 2 &&
        ((major < kRequiredMajor ||
          (major == kRequiredMajor && minor < kRequiredMinor)))) {
      return false;
    }

    base::ScopedFile fd;
    fd.reset(static_cast<int>(syscall(__NR_memfd_create, "perfetto_shmem",
                                      MFD_CLOEXEC | MFD_ALLOW_SEALING)));
    return !!fd;
  }();
  return kSupportsMemfd;
}

base::ScopedFile CreateMemfd(const char* name, unsigned int flags) {
  if (!HasMemfdSupport()) {
    errno = ENOSYS;
    return base::ScopedFile();
  }
  return base::ScopedFile(
      static_cast<int>(syscall(__NR_memfd_create, name, flags)));
}
}  // namespace perfetto

#else  // PERFETTO_MEMFD_ENABLED()

namespace perfetto {
bool HasMemfdSupport() {
  return false;
}
base::ScopedFile CreateMemfd(const char*, unsigned int) {
  errno = ENOSYS;
  return base::ScopedFile();
}
}  // namespace perfetto

#endif  // PERFETTO_MEMFD_ENABLED()
