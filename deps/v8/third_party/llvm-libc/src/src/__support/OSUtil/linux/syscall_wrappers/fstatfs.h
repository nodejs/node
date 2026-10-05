//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for fstatfs.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_FSTATFS_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_FSTATFS_H

#include "hdr/types/struct_statfs.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> fstatfs(int fd, struct statfs *buf) {
#ifdef SYS_fstatfs64
  static_assert(sizeof(statfs::f_blocks) == 8,
                "Can only be used with 64-bit version of the struct");
  return syscall_checked<int>(SYS_fstatfs64, fd, sizeof(*buf), buf);
#else
  static_assert(
      sizeof(statfs::f_blocks) == sizeof(long),
      "The fallback is unsafe on 32-bit platforms with 64-bit f_blocks.");
  return syscall_checked<int>(SYS_fstatfs, fd, buf);
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_FSTATFS_H
