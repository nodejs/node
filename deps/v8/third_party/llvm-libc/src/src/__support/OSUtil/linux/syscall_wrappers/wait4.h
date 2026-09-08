//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for wait4.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_WAIT4_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_WAIT4_H

#include "hdr/signal_macros.h"
#include "hdr/sys_wait_macros.h"
#include "hdr/types/pid_t.h"
#include "hdr/types/siginfo_t.h"
#include "hdr/types/struct_rusage.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

LIBC_INLINE ErrorOr<pid_t> wait4(pid_t pid, int *wstatus, int options,
                                 struct rusage *rusage) {
#ifdef SYS_wait4
  return syscall_checked<pid_t>(SYS_wait4, pid, wstatus, options, rusage);
#elif defined(SYS_waitid)
  // Architectures without wait4 (e.g. riscv32) provide waitid instead.
  int idtype = P_PID;
  if (pid == -1) {
    idtype = P_ALL;
  } else if (pid < -1) {
    idtype = P_PGID;
    pid *= -1;
  } else if (pid == 0) {
    idtype = P_PGID;
  }

  options |= WEXITED;

  // Linux always writes si_pid and si_signo (see SYSCALL_DEFINE5(waitid) in
  // kernel/exit.c), zeroing them when WNOHANG found nothing, so si_pid is a
  // reliable "was anything reaped" flag. Initialize it anyway: POSIX only
  // requires this since POSIX.1-2008 TC1.
  siginfo_t info;
  info.si_pid = 0;
  auto result =
      syscall_checked<pid_t>(SYS_waitid, idtype, pid, &info, options, rusage);
  if (!result.has_value())
    return result;

  // WNOHANG with nothing to reap. Return 0 without touching wstatus, which is
  // what wait4 does; falling through would clobber it via the default case.
  if (info.si_pid == 0)
    return 0;

  if (wstatus) {
    switch (info.si_code) {
    case CLD_EXITED:
      *wstatus = W_EXITCODE(info.si_status, 0);
      break;
    case CLD_DUMPED:
      *wstatus = info.si_status | WCOREFLAG;
      break;
    case CLD_KILLED:
      *wstatus = info.si_status;
      break;
    case CLD_TRAPPED:
    case CLD_STOPPED:
      *wstatus = W_STOPCODE(info.si_status);
      break;
    case CLD_CONTINUED:
      // Set wstatus to a value that the caller can check via WIFCONTINUED.
      // glibc has a non-POSIX macro definition __W_CONTINUED for this value.
      *wstatus = 0xffff;
      break;
    default:
      *wstatus = 0;
      break;
    }
  }
  return info.si_pid;
#else
#error "wait4 and waitid syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_WAIT4_H
