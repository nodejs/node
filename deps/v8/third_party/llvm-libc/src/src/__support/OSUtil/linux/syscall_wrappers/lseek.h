//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for lseek.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_LSEEK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_LSEEK_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/off_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<off_t> lseek(int fd, off_t offset, int whence) {
  off_t result;
#ifdef SYS_lseek
  result = syscall_impl<off_t>(SYS_lseek, fd, offset, whence);
  if (result < 0)
    return Error(-static_cast<int>(result));
#elif defined(SYS_llseek) || defined(SYS__llseek)
#ifdef SYS_llseek
  constexpr long LLSEEK_SYSCALL_NO = SYS_llseek;
#elif defined(SYS__llseek)
  constexpr long LLSEEK_SYSCALL_NO = SYS__llseek;
#endif
  uint64_t offset_64 = static_cast<uint64_t>(offset);
  int ret = syscall_impl<int>(LLSEEK_SYSCALL_NO, fd,
                              static_cast<long>(offset_64 >> 32),
                              static_cast<long>(offset_64), &result, whence);
  if (ret < 0)
    return Error(-ret);
#else
#error "lseek, llseek and _llseek syscalls not available."
#endif
  return result;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_LSEEK_H
