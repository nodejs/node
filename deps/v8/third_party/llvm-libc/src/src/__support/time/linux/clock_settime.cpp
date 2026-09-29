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
#if defined(SYS_clock_settime64)
  ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_settime64,
                                          static_cast<long>(clockid),
                                          reinterpret_cast<long>(ts));
#elif defined(SYS_clock_settime)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_clock_settime,
                                          static_cast<long>(clockid),
                                          reinterpret_cast<long>(ts));
#else
#error "SYS_clock_settime and SYS_clock_settime64 syscalls not available."
#endif
  if (ret < 0)
    return Error(-ret);
  return ret;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
