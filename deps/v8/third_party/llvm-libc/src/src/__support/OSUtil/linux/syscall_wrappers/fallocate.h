//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the fallocate, which is the
/// base syscall wrapper for all fallocate dependent calls.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_FALLOCATE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_FALLOCATE_H

#include "hdr/errno_macros.h"
#include "hdr/types/off_t.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> fallocate(int fd, int mode, off_t offset, off_t size) {
  if constexpr (sizeof(long) == sizeof(uint32_t) &&
                sizeof(off_t) == sizeof(uint64_t)) {
    uint64_t offset_bits = cpp::bit_cast<uint64_t>(offset);
    long offset_low = static_cast<long>(offset_bits & UINT32_MAX);
    long offset_high = static_cast<long>(offset_bits >> 32);
    uint64_t len_bits = cpp::bit_cast<uint64_t>(size);
    long len_low = static_cast<long>(len_bits & UINT32_MAX);
    long len_high = static_cast<long>(len_bits >> 32);
    return syscall_checked<int>(SYS_fallocate, fd, mode, offset_low,
                                offset_high, len_low, len_high);
  } else {
    return syscall_checked<int>(SYS_fallocate, fd, mode, offset, size);
  }
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL
#endif
