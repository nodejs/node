//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for dup2.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_DUP2_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_DUP2_H

#include "hdr/fcntl_macros.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> dup2(int oldfd, int newfd) {
#ifdef SYS_dup2
  int ret = syscall_impl<int>(SYS_dup2, oldfd, newfd);
#elif defined(SYS_dup3)
  if (oldfd == newfd) {
#if defined(SYS_fcntl)
    int ret = syscall_impl<int>(SYS_fcntl, oldfd, F_GETFD);
#elif defined(SYS_fcntl64)
    int ret = syscall_impl<int>(SYS_fcntl64, oldfd, F_GETFD);
#else
#error "SYS_fcntl and SYS_fcntl64 syscalls not available."
#endif
    if (ret >= 0)
      return oldfd;
    return Error(-ret);
  }
  int ret = syscall_impl<int>(SYS_dup3, oldfd, newfd, 0);
#else
#error "dup2 and dup3 syscalls not available."
#endif
  if (ret < 0)
    return Error(-ret);
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_DUP2_H
