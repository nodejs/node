//===-- Implementation header for utimensat ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_UTIMENSAT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_UTIMENSAT_H

#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include "hdr/types/struct_timespec.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> utimensat(int dirfd, const char *path,
                                   const struct timespec times[2], int flags) {
#if defined(SYS_utimensat_time64)
  int ret = syscall_impl<int>(SYS_utimensat_time64, dirfd, path, times, flags);
#elif defined(SYS_utimensat)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  int ret = syscall_impl<int>(SYS_utimensat, dirfd, path, times, flags);
#else
#error "utimensat or utimensat_time64 syscalls not available."
#endif

  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return ret;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_UTIMENSAT_H
