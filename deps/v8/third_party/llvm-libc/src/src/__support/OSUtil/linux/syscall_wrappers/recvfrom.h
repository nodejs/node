//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for recvfrom.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RECVFROM_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RECVFROM_H

#include "hdr/types/socklen_t.h"
#include "hdr/types/ssize_t.h"
#include "hdr/types/struct_sockaddr.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<ssize_t> recvfrom(int sockfd, void *buf, size_t len,
                                      int flags, sockaddr *__restrict src_addr,
                                      socklen_t *__restrict addrlen) {
  return syscall_checked<ssize_t>(SYS_recvfrom, sockfd, buf, len, flags,
                                  src_addr, addrlen);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RECVFROM_H
