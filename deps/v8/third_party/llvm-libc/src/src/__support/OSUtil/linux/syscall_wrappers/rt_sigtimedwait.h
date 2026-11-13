//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for rt_sigtimedwait.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RT_SIGTIMEDWAIT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RT_SIGTIMEDWAIT_H

#include "hdr/types/siginfo_t.h"
#include "hdr/types/sigset_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h>

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> rt_sigtimedwait(const sigset_t *set, siginfo_t *info,
                                         const struct timespec *timeout) {
#if defined(SYS_rt_sigtimedwait_time64)
  static_assert(
      sizeof(time_t) == sizeof(int64_t),
      "SYS_rt_sigtimedwait_time64 requires struct timespec with 64-bit "
      "members.");
  return syscall_checked<int>(SYS_rt_sigtimedwait_time64, set, info, timeout,
                              sizeof(sigset_t));
#elif defined(SYS_rt_sigtimedwait)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  return syscall_checked<int>(SYS_rt_sigtimedwait, set, info, timeout,
                              sizeof(sigset_t));
#else
#error "rt_sigtimedwait and rt_sigtimedwait_time64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RT_SIGTIMEDWAIT_H
