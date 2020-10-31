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

#include "perfetto/base/time.h"
#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace perfetto {
namespace base {

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

TimeNanos GetWallTimeNs() {
  LARGE_INTEGER freq;
  ::QueryPerformanceFrequency(&freq);
  LARGE_INTEGER counter;
  ::QueryPerformanceCounter(&counter);
  double elapsed_nanoseconds = (1e9 * counter.QuadPart) / freq.QuadPart;
  return TimeNanos(static_cast<uint64_t>(elapsed_nanoseconds));
}

TimeNanos GetThreadCPUTimeNs() {
  FILETIME dummy, kernel_ftime, user_ftime;
  ::GetThreadTimes(GetCurrentThread(), &dummy, &dummy, &kernel_ftime,
                   &user_ftime);
  uint64_t kernel_time = kernel_ftime.dwHighDateTime * 0x100000000 +
                         kernel_ftime.dwLowDateTime;
  uint64_t user_time = user_ftime.dwHighDateTime * 0x100000000 +
                       user_ftime.dwLowDateTime;

  return TimeNanos((kernel_time + user_time) * 100);
}

void SleepMicroseconds(unsigned interval_us) {
  // The Windows Sleep function takes a millisecond count. Round up so that
  // short sleeps don't turn into a busy wait. Note that the sleep granularity
  // on Windows can dynamically vary from 1 ms to ~16 ms, so don't count on this
  // being a short sleep.
  ::Sleep(static_cast<DWORD>((interval_us + 999) / 1000));
}

#else  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

void SleepMicroseconds(unsigned interval_us) {
  ::usleep(static_cast<useconds_t>(interval_us));
}

#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

}  // namespace base
}  // namespace perfetto
