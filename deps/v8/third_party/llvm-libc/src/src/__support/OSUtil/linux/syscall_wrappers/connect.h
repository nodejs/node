//===-- Implementation header for connect -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CONNECT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CONNECT_H

#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_sockaddr.h"
#include <linux/net.h>   // For SYS_SOCKET socketcall number.
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> connect(int sockfd, const struct sockaddr *addr,
                                 socklen_t addrlen) {
#ifdef SYS_connect
  int ret =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_connect, sockfd, addr, addrlen);
#elif defined(SYS_socketcall)
  unsigned long sockcall_args[3] = {static_cast<unsigned long>(sockfd),
                                    reinterpret_cast<unsigned long>(addr),
                                    static_cast<unsigned long>(addrlen)};
  int ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_socketcall, SYS_CONNECT,
                                              sockcall_args);
#else
#error "socket and socketcall syscalls unavailable for this platform."
#endif

  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CONNECT_H
