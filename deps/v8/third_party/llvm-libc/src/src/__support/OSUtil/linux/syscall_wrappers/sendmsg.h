//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for sendmsg.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SENDMSG_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SENDMSG_H

#include "hdr/types/ssize_t.h"
#include "hdr/types/struct_msghdr.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<ssize_t> sendmsg(int sockfd, const struct msghdr *msg,
                                     int flags) {
  return syscall_checked<ssize_t>(SYS_sendmsg, sockfd, msg, flags);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SENDMSG_H
