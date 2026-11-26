//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for timerfd_gettime.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMERFD_GETTIME_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMERFD_GETTIME_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/struct_itimerspec.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> timerfd_gettime(int fd,
                                         struct itimerspec *curr_value) {
#if defined(SYS_timerfd_gettime64)
  static_assert(
      sizeof(timespec::tv_sec) == sizeof(int64_t),
      "SYS_timerfd_gettime64 requires struct timespec with 64-bit members.");
  return syscall_checked<int>(SYS_timerfd_gettime64, fd, curr_value);
#elif defined(SYS_timerfd_gettime)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "SYS_timerfd_gettime requires struct timespec::tv_nsec to match long; "
      "use SYS_timerfd_gettime64 on 32-bit platforms with 64-bit time_t.");
  return syscall_checked<int>(SYS_timerfd_gettime, fd, curr_value);
#else
#error "SYS_timerfd_gettime and SYS_timerfd_gettime64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMERFD_GETTIME_H
