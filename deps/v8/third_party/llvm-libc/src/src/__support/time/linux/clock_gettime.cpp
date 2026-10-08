//===--- clock_gettime linux implementation ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/time/clock_gettime.h"
#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/vdso.h"
#include "src/__support/OSUtil/syscall.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h>

#if defined(SYS_clock_gettime64)
#include <linux/time_types.h>
#endif

namespace LIBC_NAMESPACE_DECL {
namespace internal {
ErrorOr<int> clock_gettime(clockid_t clockid, timespec *ts) {
  using namespace vdso;
  int ret;
#if defined(SYS_clock_gettime64)
  TypedSymbol<VDSOSym::ClockGetTime64> clock_gettime64;
  if constexpr (sizeof(timespec) == sizeof(__kernel_timespec)) {
    if (LIBC_LIKELY(clock_gettime64 != nullptr))
      ret = clock_gettime64(clockid, reinterpret_cast<__kernel_timespec *>(ts));
    else
      ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_gettime64,
                                              static_cast<long>(clockid),
                                              reinterpret_cast<long>(ts));
  } else {
    __kernel_timespec ts64{};
    __kernel_timespec *ts64_ptr = nullptr;
    if (ts != nullptr)
      ts64_ptr = &ts64;
    if (ts64_ptr != nullptr && LIBC_LIKELY(clock_gettime64 != nullptr)) {
      ret = clock_gettime64(clockid, ts64_ptr);
    } else {
      ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_gettime64,
                                              static_cast<long>(clockid),
                                              reinterpret_cast<long>(ts64_ptr));
    }
    if (ret == 0 && ts != nullptr) {
      ts->tv_sec = static_cast<decltype(ts->tv_sec)>(ts64.tv_sec);
      ts->tv_nsec = static_cast<decltype(ts->tv_nsec)>(ts64.tv_nsec);
    }
  }
#elif defined(SYS_clock_gettime)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  TypedSymbol<VDSOSym::ClockGetTime> clock_gettime;
  if (LIBC_LIKELY(clock_gettime != nullptr))
    ret = clock_gettime(clockid, ts);
  else
    ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_gettime,
                                            static_cast<long>(clockid),
                                            reinterpret_cast<long>(ts));
#else
#error "SYS_clock_gettime and SYS_clock_gettime64 syscalls not available."
#endif
  if (ret < 0)
    return Error(-ret);
  return ret;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
