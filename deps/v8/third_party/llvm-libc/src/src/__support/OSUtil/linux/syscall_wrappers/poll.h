//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for poll.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_POLL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_POLL_H

#include "hdr/types/nfds_t.h"
#include "hdr/types/struct_pollfd.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> poll(struct pollfd *fds, nfds_t nfds, int timeout) {
#if defined(SYS_poll)
  return syscall_checked<int>(SYS_poll, fds, nfds, timeout);
#else // no SYS_poll
  timespec ts, *tsp;
  if (timeout >= 0) {
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000000;
    tsp = &ts;
  } else {
    tsp = nullptr;
  }
#if defined(SYS_ppoll_time64)
  return syscall_checked<int>(SYS_ppoll_time64, fds, nfds, tsp, nullptr, 0);
#elif defined(SYS_ppoll)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  return syscall_checked<int>(SYS_ppoll, fds, nfds, tsp, nullptr, 0);
#else
#error "poll, ppoll, ppoll_time64 syscalls not available."
#endif // defined(SYS_ppoll) || defined(SYS_ppoll_time64)
#endif // defined(SYS_poll)
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_POLL_H
