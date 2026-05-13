//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// ErrorOr-returning syscall wrapper for chmod.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CHMOD_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CHMOD_H

#include "hdr/fcntl_macros.h"
#include "hdr/types/mode_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> chmod(const char *path, mode_t mode) {
#ifdef SYS_chmod
  int ret = syscall_impl<int>(SYS_chmod, path, mode);
#elif defined(SYS_fchmodat)
  int ret = syscall_impl<int>(SYS_fchmodat, AT_FDCWD, path, mode, 0);
#elif defined(SYS_fchmodat2)
  int ret = syscall_impl<int>(SYS_fchmodat2, AT_FDCWD, path, mode, 0,
                              AT_SYMLINK_NOFOLLOW);
#else
#error "chmod, fchmodat and fchmodat2 syscalls not available."
#endif
  if (ret < 0)
    return Error(-ret);
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_CHMOD_H
