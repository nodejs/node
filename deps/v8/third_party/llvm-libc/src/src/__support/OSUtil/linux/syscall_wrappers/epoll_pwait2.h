//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for epoll_pwait2.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EPOLL_PWAIT2_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EPOLL_PWAIT2_H

#include "hdr/signal_macros.h"
#include "hdr/types/sigset_t.h"
#include "hdr/types/struct_epoll_event.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> epoll_pwait2(int epfd, struct epoll_event *events,
                                      int maxevents,
                                      const struct timespec *timeout,
                                      const sigset_t *sigmask) {
  return syscall_checked<int>(SYS_epoll_pwait2, epfd, events, maxevents,
                              timeout, sigmask, NSIG / 8);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EPOLL_PWAIT2_H
