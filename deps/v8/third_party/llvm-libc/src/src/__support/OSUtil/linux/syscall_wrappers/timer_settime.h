//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for timer_settime syscall wrapper.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMER_SETTIME_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMER_SETTIME_H

#include "hdr/types/struct_itimerspec.h"
#include "hdr/types/timer_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> timer_settime(timer_t timerid, int flags,
                                       const itimerspec *val, itimerspec *old) {
#if defined(SYS_timer_settime64)
  static_assert(
      sizeof(time_t) == sizeof(int64_t),
      "SYS_timer_settime64 requires struct timespec with 64-bit members.");
  return syscall_checked<int>(SYS_timer_settime64, timerid, flags, val, old);
#elif defined(SYS_timer_settime)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  return syscall_checked<int>(SYS_timer_settime, timerid, flags, val, old);
#else
#error "SYS_timer_settime and SYS_timer_settime64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_TIMER_SETTIME_H
