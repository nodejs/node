//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for copy_file_range.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_COPY_FILE_RANGE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_COPY_FILE_RANGE_H

#include "hdr/errno_macros.h"
#include "hdr/types/off_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/ssize_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<ssize_t> copy_file_range(int fd_in, off_t *off_in,
                                             int fd_out, off_t *off_out,
                                             size_t len, unsigned int flags) {
#ifdef SYS_copy_file_range
  return syscall_checked<ssize_t>(SYS_copy_file_range, fd_in, off_in, fd_out,
                                  off_out, len, flags);
#elif defined(__NR_copy_file_range)
  return syscall_checked<ssize_t>(__NR_copy_file_range, fd_in, off_in, fd_out,
                                  off_out, len, flags);
#else
  (void)fd_in;
  (void)off_in;
  (void)fd_out;
  (void)off_out;
  (void)len;
  (void)flags;
  return ErrorOr<ssize_t>::from_error(ENOSYS);
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_COPY_FILE_RANGE_H
