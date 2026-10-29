//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for getcwd.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_GETCWD_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_GETCWD_H

#include "hdr/errno_macros.h"
#include "hdr/types/ssize_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<ssize_t> getcwd(char *buf, size_t size) {
  ssize_t ret = syscall_impl<ssize_t>(SYS_getcwd, buf, size);
  if (ret < 0) {
    return Error(static_cast<int>(-ret));
  }

  // Return ENOENT for unreachable paths. This can occur, for example,
  // when getcwd is called in a directory after `chroot` has switched
  // the filesystem root.
  if (ret == 0 || buf[0] != '/') {
    return Error(ENOENT);
  }

  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_GETCWD_H
