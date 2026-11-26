//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for timerfd_settime.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMERFD_SETTIME_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMERFD_SETTIME_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/struct_itimerspec.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> timerfd_settime(int fd, int flags,
                                         const struct itimerspec *new_value,
                                         struct itimerspec *old_value) {
#if defined(SYS_timerfd_settime64)
  static_assert(
      sizeof(timespec::tv_sec) == sizeof(int64_t),
      "SYS_timerfd_settime64 requires struct timespec with 64-bit members.");
  return syscall_checked<int>(SYS_timerfd_settime64, fd, flags, new_value,
                              old_value);
#elif defined(SYS_timerfd_settime)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "SYS_timerfd_settime requires struct timespec::tv_nsec to match long; "
      "use SYS_timerfd_settime64 on 32-bit platforms with 64-bit time_t.");
  return syscall_checked<int>(SYS_timerfd_settime, fd, flags, new_value,
                              old_value);
#else
#error "SYS_timerfd_settime and SYS_timerfd_settime64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMERFD_SETTIME_H
