//===--- clock_gettime windows implementation -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "hdr/time_macros.h"

#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/time/clock_gettime.h"
#include "src/__support/time/units.h"
#include "src/__support/time/windows/performance_counter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace LIBC_NAMESPACE_DECL {
namespace internal {
ErrorOr<int> clock_gettime(clockid_t clockid, timespec *ts) {
  using namespace time_units;
  constexpr unsigned long long HNS_PER_SEC = 1_s_ns / 100ULL;
  constexpr long long SEC_LIMIT =
      cpp::numeric_limits<decltype(ts->tv_sec)>::max();
  ErrorOr<int> ret = 0;
  switch (clockid) {
  default:
    ret = cpp::unexpected(EINVAL);
    break;

  case CLOCK_MONOTONIC: {
    // see
    // https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
    // Is the performance counter monotonic (non-decreasing)?
    // Yes. performance_counter does not go backward.
    [[clang::uninitialized]] LARGE_INTEGER buffer;
    // On systems that run Windows XP or later, the function will always
    // succeed and will thus never return zero.
    ::QueryPerformanceCounter(&buffer);
    long long freq = performance_counter::get_ticks_per_second();
    long long ticks = buffer.QuadPart;
    long long tv_sec = ticks / freq;
    long long tv_nsec = (ticks % freq) * 1_s_ns / freq;
    if (LIBC_UNLIKELY(tv_sec > SEC_LIMIT)) {
      ret = cpp::unexpected(EOVERFLOW);
      break;
    }
    ts->tv_sec = static_cast<decltype(ts->tv_sec)>(tv_sec);
    ts->tv_nsec = static_cast<decltype(ts->tv_nsec)>(tv_nsec);
    break;
  }
  case CLOCK_REALTIME: {
    // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemtimepreciseasfiletime
    // GetSystemTimePreciseAsFileTime
    // This function is best suited for high-resolution time-of-day
    // measurements, or time stamps that are synchronized to UTC
    [[clang::uninitialized]] FILETIME file_time;
    [[clang::uninitialized]] ULARGE_INTEGER time;
    ::GetSystemTimePreciseAsFileTime(&file_time);
    time.LowPart = file_time.dwLowDateTime;
    time.HighPart = file_time.dwHighDateTime;

    // adjust to POSIX epoch (from Jan 1, 1601 to Jan 1, 1970)
    constexpr unsigned long long POSIX_TIME_SHIFT =
        (11644473600ULL * HNS_PER_SEC);
    if (LIBC_UNLIKELY(POSIX_TIME_SHIFT > time.QuadPart)) {
      ret = cpp::unexpected(EOVERFLOW);
      break;
    }
    time.QuadPart -= (11644473600ULL * HNS_PER_SEC);
    unsigned long long tv_sec = time.QuadPart / HNS_PER_SEC;
    unsigned long long tv_nsec = (time.QuadPart % HNS_PER_SEC) * 100ULL;
    if (LIBC_UNLIKELY(tv_sec > SEC_LIMIT)) {
      ret = cpp::unexpected(EOVERFLOW);
      break;
    }
    ts->tv_sec = static_cast<decltype(ts->tv_sec)>(tv_sec);
    ts->tv_nsec = static_cast<decltype(ts->tv_nsec)>(tv_nsec);
    break;
  }
  case CLOCK_PROCESS_CPUTIME_ID:
  case CLOCK_THREAD_CPUTIME_ID: {
    [[clang::uninitialized]] FILETIME creation_time;
    [[clang::uninitialized]] FILETIME exit_time;
    [[clang::uninitialized]] FILETIME kernel_time;
    [[clang::uninitialized]] FILETIME user_time;
    bool success;
    if (clockid == CLOCK_PROCESS_CPUTIME_ID) {
      // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes
      success = ::GetProcessTimes(::GetCurrentProcess(), &creation_time,
                                  &exit_time, &kernel_time, &user_time);
    } else {
      // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes
      success = ::GetThreadTimes(::GetCurrentThread(), &creation_time,
                                 &exit_time, &kernel_time, &user_time);
    }
    if (!success) {
      ret = cpp::unexpected(EINVAL);
      break;
    }
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
    // It is not recommended that you add and subtract values from the FILETIME
    // structure to obtain relative times. Instead, you should copy the low- and
    // high-order parts of the file time to a ULARGE_INTEGER structure, perform
    // 64-bit arithmetic on the QuadPart member, and copy the LowPart and
    // HighPart members into the FILETIME structure.
    auto kernel_time_hns = cpp::bit_cast<ULARGE_INTEGER>(kernel_time);
    auto user_time_hns = cpp::bit_cast<ULARGE_INTEGER>(user_time);
    unsigned long long total_time_hns =
        kernel_time_hns.QuadPart + user_time_hns.QuadPart;

    unsigned long long tv_sec = total_time_hns / HNS_PER_SEC;
    unsigned long long tv_nsec = (total_time_hns % HNS_PER_SEC) * 100ULL;

    if (LIBC_UNLIKELY(tv_sec > SEC_LIMIT)) {
      ret = cpp::unexpected(EOVERFLOW);
      break;
    }

    ts->tv_sec = static_cast<decltype(ts->tv_sec)>(tv_sec);
    ts->tv_nsec = static_cast<decltype(ts->tv_nsec)>(tv_nsec);

    break;
  }
  }
  return ret;
}
} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
