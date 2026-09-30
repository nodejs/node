//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for read.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_SYSCALL_WRAPPERS_READ_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_SYSCALL_WRAPPERS_READ_H

#include "hdr/types/ssize_t.h"
#include "src/__support/OSUtil/freebsd/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers.

namespace LIBC_NAMESPACE_DECL {
namespace freebsd_syscalls {

LIBC_INLINE ErrorOr<ssize_t> read(int fd, void *buf, size_t count) {
  SyscallReturn ret = syscall_impl(SYS_read, fd, buf, count);
  if (ret.is_error)
    return Error(static_cast<int>(ret.value));
  return static_cast<ssize_t>(ret.value);
}

} // namespace freebsd_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_SYSCALL_WRAPPERS_READ_H
