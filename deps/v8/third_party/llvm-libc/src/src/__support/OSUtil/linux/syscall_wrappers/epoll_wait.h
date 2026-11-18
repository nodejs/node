//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for epoll_wait.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EPOLL_WAIT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EPOLL_WAIT_H

#include "hdr/signal_macros.h"
#include "hdr/types/struct_epoll_event.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> epoll_wait(int epfd, struct epoll_event *events,
                                    int maxevents, int timeout) {
#ifdef SYS_epoll_wait
  return syscall_checked<int>(SYS_epoll_wait, epfd, events, maxevents, timeout);
#elif defined(SYS_epoll_pwait)
  return syscall_checked<int>(SYS_epoll_pwait, epfd, events, maxevents, timeout,
                              nullptr, NSIG / 8);
#else
#error "epoll_wait and epoll_pwait are unavailable. Unable to build epoll_wait."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EPOLL_WAIT_H
