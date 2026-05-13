//===-- Implementation header for unlink ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_UNLINK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_UNLINK_H

#include "hdr/fcntl_macros.h"                   // AT_FDCWD
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> unlink(const char *path) {
#ifdef SYS_unlink
  int ret = syscall_impl<int>(SYS_unlink, path);
#elif defined(SYS_unlinkat)
  int ret = syscall_impl<int>(SYS_unlinkat, AT_FDCWD, path, 0);
#else
#error "unlink and unlinkat syscalls not available."
#endif
  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return 0;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_UNLINK_H
