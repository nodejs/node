//===--- clock_settime linux implementation ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/time/clock_settime.h"
#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/syscall.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h>

#if defined(SYS_clock_settime64)
#include <linux/time_types.h>
#endif

namespace LIBC_NAMESPACE_DECL {
namespace internal {
ErrorOr<int> clock_settime(clockid_t clockid, const timespec *ts) {
  int ret;
#if defined(SYS_clock_settime)
  ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_settime,
                                          static_cast<long>(clockid),
                                          reinterpret_cast<long>(ts));
#elif defined(SYS_clock_settime64)
  static_assert(
      sizeof(time_t) == sizeof(int64_t),
      "SYS_clock_settime64 requires struct timespec with 64-bit members.");

  __kernel_timespec ts64{};

  // Populate the 64-bit kernel structure from the user-provided timespec
  ts64.tv_sec = static_cast<decltype(ts64.tv_sec)>(ts->tv_sec);
  ts64.tv_nsec = static_cast<decltype(ts64.tv_nsec)>(ts->tv_nsec);

  ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_settime64,
                                          static_cast<long>(clockid),
                                          reinterpret_cast<long>(&ts64));
#else
#error "SYS_clock_settime and SYS_clock_settime64 syscalls not available."
#endif
  if (ret < 0)
    return Error(-ret);
  return ret;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
