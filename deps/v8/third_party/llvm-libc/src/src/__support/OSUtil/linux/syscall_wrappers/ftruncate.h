//===-- Implementation header for ftruncate ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_FTRUNCATE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_FTRUNCATE_H

#include "hdr/types/off_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> ftruncate(int fd, off_t len) {
#ifdef SYS_ftruncate
  int ret = syscall_impl<int>(SYS_ftruncate, fd, len);
#elif defined(SYS_ftruncate64)
  // Same as ftruncate but can handle large offsets on 32-bit systems.
  static_assert(sizeof(off_t) == 8);
  int ret = syscall_impl<int>(SYS_ftruncate64, fd, (long)len,
                              (long)(((uint64_t)(len)) >> 32));
#else
#error "ftruncate and ftruncate64 syscalls not available."
#endif
  if (ret < 0)
    return Error(-static_cast<int>(ret));
  return 0;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_FTRUNCATE_H
