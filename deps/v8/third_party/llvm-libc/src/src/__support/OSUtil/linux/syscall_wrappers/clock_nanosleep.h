//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for clock_nanosleep syscall wrapper.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CLOCK_NANOSLEEP_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CLOCK_NANOSLEEP_H

#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h>

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> clock_nanosleep(clockid_t clockid, int flags,
                                         const timespec *req, timespec *rem) {
#if defined(SYS_clock_nanosleep_time64)
  static_assert(
      sizeof(time_t) == sizeof(int64_t),
      "SYS_clock_nanosleep_time64 requires struct timespec with 64-bit "
      "members.");
  return syscall_checked<int>(SYS_clock_nanosleep_time64, clockid, flags, req,
                              rem);
#elif defined(SYS_clock_nanosleep)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  return syscall_checked<int>(SYS_clock_nanosleep, clockid, flags, req, rem);
#else
#error                                                                         \
    "SYS_clock_nanosleep and SYS_clock_nanosleep_time64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CLOCK_NANOSLEEP_H
