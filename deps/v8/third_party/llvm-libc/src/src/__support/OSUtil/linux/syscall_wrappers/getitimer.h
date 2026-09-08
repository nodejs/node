//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for getitimer.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_GETITIMER_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_GETITIMER_H

#include "hdr/types/struct_itimerval.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> getitimer(int which, struct itimerval *curr_val) {
  if constexpr (sizeof(time_t) == sizeof(long)) {
    return syscall_checked<int>(SYS_getitimer, which, curr_val);
  } else {
    // There is no SYS_getitimer_time64 call, so we can't use time_t directly,
    // and need to convert from long first.
    long curr_val32[4];
    long *curr_val32_ptr = curr_val ? curr_val32 : nullptr;

    ErrorOr<int> ret =
        syscall_checked<int>(SYS_getitimer, which, curr_val32_ptr);

    if (!ret)
      return ret;

    if (curr_val) {
      curr_val->it_interval.tv_sec = curr_val32[0];
      curr_val->it_interval.tv_usec = curr_val32[1];
      curr_val->it_value.tv_sec = curr_val32[2];
      curr_val->it_value.tv_usec = curr_val32[3];
    }
    return ret;
  }
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_GETITIMER_H
