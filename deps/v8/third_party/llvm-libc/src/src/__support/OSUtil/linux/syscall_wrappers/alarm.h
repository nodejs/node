//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for alarm.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ALARM_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ALARM_H

#include "hdr/sys_time_macros.h"
#include "hdr/types/struct_itimerval.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/OSUtil/linux/syscall_wrappers/setitimer.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<unsigned int> alarm(unsigned int seconds) {
#ifdef SYS_alarm
  return syscall_checked<unsigned int>(SYS_alarm, seconds);
#elif defined(SYS_setitimer)
  struct itimerval old_itv;
  struct itimerval itv = {};
  itv.it_value.tv_sec = seconds;
  ErrorOr<int> ret = linux_syscalls::setitimer(ITIMER_REAL, &itv, &old_itv);
  if (!ret)
    return Error(ret.error());
  return static_cast<unsigned int>(old_itv.it_value.tv_sec +
                                   (old_itv.it_value.tv_usec > 0 ? 1 : 0));
#else
#error "alarm implementation not available for this architecture"
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_ALARM_H
