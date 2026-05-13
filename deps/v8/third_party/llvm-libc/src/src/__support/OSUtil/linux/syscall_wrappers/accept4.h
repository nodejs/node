//===-- Implementation header for accept4 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ACCEPT4_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ACCEPT4_H

#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_sockaddr.h"
#include <linux/net.h>   // For SYS_ACCEPT4 socketcall number.
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> accept4(int sockfd, struct sockaddr *addr,
                                 socklen_t *addrlen, int flags) {
#ifdef SYS_accept4
  int ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_accept4, sockfd, addr,
                                              addrlen, flags);
#elif defined(SYS_socketcall)
  unsigned long sockcall_args[4] = {static_cast<unsigned long>(sockfd),
                                    reinterpret_cast<unsigned long>(addr),
                                    reinterpret_cast<unsigned long>(addrlen),
                                    static_cast<unsigned long>(flags)};
  int ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_socketcall, SYS_ACCEPT4,
                                              sockcall_args);
#else
#error "accept4 and socketcall syscalls unavailable for this platform."
#endif

  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ACCEPT4_H
