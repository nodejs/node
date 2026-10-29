//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for accept.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ACCEPT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ACCEPT_H

#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_sockaddr.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> accept(int sockfd, struct sockaddr *addr,
                                socklen_t *addrlen) {
#if defined(SYS_accept)
  return syscall_checked<int>(SYS_accept, sockfd, addr, addrlen);
#elif defined(SYS_accept4)
  return syscall_checked<int>(SYS_accept4, sockfd, addr, addrlen, 0);
#else
#error "accept and accept4 syscalls unavailable for this platform."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ACCEPT_H
