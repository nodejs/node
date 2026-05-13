//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for readlink.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_READLINK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_READLINK_H

#include "hdr/fcntl_macros.h"
#include "hdr/types/ssize_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<ssize_t> readlink(const char *path, char *buf,
                                      size_t bufsiz) {
#ifdef SYS_readlink
  ssize_t ret = syscall_impl<ssize_t>(SYS_readlink, path, buf, bufsiz);
#elif defined(SYS_readlinkat)
  ssize_t ret =
      syscall_impl<ssize_t>(SYS_readlinkat, AT_FDCWD, path, buf, bufsiz);
#else
#error "readlink and readlinkat syscalls not available."
#endif
  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_READLINK_H
