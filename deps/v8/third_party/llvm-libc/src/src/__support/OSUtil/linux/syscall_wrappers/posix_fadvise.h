//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// ErrorOr-returning syscall wrapper for posix_fadvise.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_POSIX_FADVISE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_POSIX_FADVISE_H

#include "hdr/errno_macros.h"
#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> posix_fadvise(int fd, int64_t offset, int64_t len,
                                       int advice) {
  if constexpr (sizeof(long) == sizeof(uint32_t)) {
    uint64_t offset_bits = cpp::bit_cast<uint64_t>(offset);
    long offset_low = static_cast<long>(offset_bits & UINT32_MAX);
    long offset_high = static_cast<long>(offset_bits >> 32);

#if defined(SYS_arm_fadvise64_64) || defined(SYS_fadvise64_64)
    uint64_t len_bits = cpp::bit_cast<uint64_t>(len);
    long len_low = static_cast<long>(len_bits & UINT32_MAX);
    long len_high = static_cast<long>(len_bits >> 32);
#endif

#if defined(SYS_arm_fadvise64_64)
    return syscall_checked<int>(SYS_arm_fadvise64_64, fd, advice, offset_low,
                                offset_high, len_low, len_high);
#elif defined(SYS_fadvise64_64)
    return syscall_checked<int>(SYS_fadvise64_64, fd, offset_low, offset_high,
                                len_low, len_high, advice);
#elif defined(SYS_fadvise64)
    if (len < 0 ||
        static_cast<uint64_t>(len) > cpp::numeric_limits<size_t>::max())
      return Error(EINVAL);
    return syscall_checked<int>(SYS_fadvise64, fd, offset_low, offset_high,
                                static_cast<size_t>(len), advice);
#else
#error "fadvise64 syscall not available."
#endif
  } else {
#if defined(SYS_fadvise64)
    return syscall_checked<int>(SYS_fadvise64, fd, offset, len, advice);
#elif defined(SYS_fadvise64_64)
    return syscall_checked<int>(SYS_fadvise64_64, fd, offset, len, advice);
#else
#error "fadvise64 syscall not available."
#endif
  }
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_POSIX_FADVISE_H
