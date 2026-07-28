//===-- Syscall wrapper for mmap --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MMAP_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MMAP_H

#include "hdr/errno_macros.h"
#include "hdr/types/off_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<void *> mmap(void *addr, size_t size, int prot, int flags,
                                 int fd, off_t offset) {
  if (offset < 0)
    return Error(EINVAL);
#ifdef SYS_mmap2
  // The mmap2 syscall uses 4k units, regardless of the actual page, size on
  // almost every architecture. If porting to a new architecture (Openrisc,
  // hexagon?), please confirm this code is correct.
  constexpr off_t MMAP2_FACTOR = 4096;
  if (offset % MMAP2_FACTOR != 0)
    return Error(EINVAL);
  offset /= MMAP2_FACTOR;
  long syscall_number = SYS_mmap2;
#elif defined(SYS_mmap)
  long syscall_number = SYS_mmap;
#else
#error "mmap or mmap2 syscalls not available."
#endif

  long offset_for_syscall = static_cast<long>(offset);
  if (offset_for_syscall != offset)
    return Error(EINVAL); // This can happen if long is smaller than off_t

  // TODO: Reject sizes that are (after page alignment) larger than PTRDIFF_MAX.
  // This is mainly relevant for 32-bit architectures.

  return syscall_checked<void *>(syscall_number, addr, size, prot, flags, fd,
                                 offset_for_syscall);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MMAP_H
