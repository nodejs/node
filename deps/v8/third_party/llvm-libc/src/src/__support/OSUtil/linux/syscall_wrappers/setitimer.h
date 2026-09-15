//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for setitimer.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SETITIMER_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SETITIMER_H

#include "hdr/errno_macros.h"
#include "hdr/types/struct_itimerval.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<int> setitimer(int which, const struct itimerval *new_val,
                                   struct itimerval *old_val) {
  if constexpr (sizeof(time_t) == sizeof(long)) {
    return syscall_checked<int>(SYS_setitimer, which, new_val, old_val);
  } else {
    // There is no SYS_setitimer_time64 call, so we can't use time_t directly,
    // and need to convert it to long first.
    long old_val32[4];
    long *old_val32_ptr = old_val ? old_val32 : nullptr;
    long new_val32[4];
    long *new_val32_ptr = nullptr;

    if (new_val) {
      // Check for overflow before casting to 32-bit long. We'll let the kernel
      // do the final validation. We're just making sure the truncation does not
      // change the value.
      auto fits_long = [](auto val) {
        return val <= cpp::numeric_limits<long>::max() &&
               val >= cpp::numeric_limits<long>::min();
      };
      if (!fits_long(new_val->it_interval.tv_sec) ||
          !fits_long(new_val->it_value.tv_sec) ||
          !fits_long(new_val->it_interval.tv_usec) ||
          !fits_long(new_val->it_value.tv_usec)) {
        return Error(EINVAL);
      }

      new_val32[0] = static_cast<long>(new_val->it_interval.tv_sec);
      new_val32[1] = static_cast<long>(new_val->it_interval.tv_usec);
      new_val32[2] = static_cast<long>(new_val->it_value.tv_sec);
      new_val32[3] = static_cast<long>(new_val->it_value.tv_usec);
      new_val32_ptr = new_val32;
    }

    ErrorOr<int> ret = syscall_checked<int>(SYS_setitimer, which, new_val32_ptr,
                                            old_val32_ptr);

    if (!ret)
      return ret;

    if (old_val) {
      old_val->it_interval.tv_sec = old_val32[0];
      old_val->it_interval.tv_usec = old_val32[1];
      old_val->it_value.tv_sec = old_val32[2];
      old_val->it_value.tv_usec = old_val32[3];
    }
    return ret;
  }
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_SETITIMER_H
