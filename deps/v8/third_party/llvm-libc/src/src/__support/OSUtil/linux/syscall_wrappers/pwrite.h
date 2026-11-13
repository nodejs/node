//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for pwrite.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_PWRITE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_PWRITE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/off_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/ssize_t.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers.

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<ssize_t> pwrite(int fd, const void *buf, size_t count,
                                    off_t offset) {
  if constexpr (sizeof(long) == sizeof(uint32_t) &&
                sizeof(off_t) == sizeof(uint64_t)) {
    // This is a 32-bit system with a 64-bit offset, offset must be split.
    const uint64_t bits = cpp::bit_cast<uint64_t>(offset);
    const uint32_t lo = bits & UINT32_MAX;
    const uint32_t hi = bits >> 32;
    const long offset_low = cpp::bit_cast<long>(static_cast<long>(lo));
    const long offset_high = cpp::bit_cast<long>(static_cast<long>(hi));
    return syscall_checked<ssize_t>(SYS_pwrite64, fd, buf, count, offset_low,
                                    offset_high);
  } else {
    return syscall_checked<ssize_t>(SYS_pwrite64, fd, buf, count, offset);
  }
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_PWRITE_H
