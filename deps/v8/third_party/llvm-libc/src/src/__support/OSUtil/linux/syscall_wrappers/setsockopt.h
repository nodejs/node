//===-- Implementation header for setsockopt --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SETSOCKOPT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SETSOCKOPT_H

#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include "hdr/types/socklen_t.h"
#include <linux/net.h>   // For SYS_SETSOCKOPT socketcall number.
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> setsockopt(int sockfd, int level, int optname,
                                    const void *optval, socklen_t optlen) {
#ifdef SYS_setsockopt
  int ret =
      syscall_impl<int>(SYS_setsockopt, sockfd, level, optname, optval, optlen);
#elif defined(SYS_socketcall)
  unsigned long sockcall_args[5] = {static_cast<unsigned long>(sockfd),
                                    static_cast<unsigned long>(level),
                                    static_cast<unsigned long>(optname),
                                    reinterpret_cast<unsigned long>(optval),
                                    static_cast<unsigned long>(optlen)};
  int ret = syscall_impl<int>(SYS_socketcall, SYS_SETSOCKOPT, sockcall_args);
#else
#error "setsockopt and socketcall syscalls unavailable for this platform."
#endif

  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SETSOCKOPT_H
