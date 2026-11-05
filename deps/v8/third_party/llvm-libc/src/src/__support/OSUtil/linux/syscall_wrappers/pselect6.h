//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for pselect6.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_PSELECT6_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_PSELECT6_H

#include "hdr/signal_macros.h"
#include "hdr/types/fd_set.h"
#include "hdr/types/sigset_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h>

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

// Note: On Linux, the raw pselect6 syscall modifies its timeout argument to
// return the remaining time if interrupted. Therefore, this wrapper accepts
// a mutable timespec pointer. The POSIX pselect entrypoint is responsible for
// making a copy to prevent mutating the user's const timeout argument.
LIBC_INLINE ErrorOr<int> pselect6(int nfds, fd_set *__restrict readfds,
                                  fd_set *__restrict writefds,
                                  fd_set *__restrict exceptfds,
                                  struct timespec *__restrict timeout,
                                  const sigset_t *__restrict sigmask) {
  // The kernel expects the signal mask size in bytes, not the number of
  // signals. NSIG is the signal count, so NSIG / 8 gives the byte size.
  const size_t SIGSETSIZE = NSIG / 8;
  struct pselect6_sigset_t {
    const sigset_t *ss;
    size_t ss_len;
  };
  pselect6_sigset_t pss{sigmask, SIGSETSIZE};

#if defined(SYS_pselect6_time64)
  static_assert(
      sizeof(time_t) == sizeof(int64_t),
      "SYS_pselect6_time64 requires struct timespec with 64-bit members.");
  return syscall_checked<int>(SYS_pselect6_time64, nfds, readfds, writefds,
                              exceptfds, timeout, &pss);
#elif defined(SYS_pselect6)
  static_assert(
      sizeof(timespec::tv_nsec) == sizeof(long),
      "This legacy syscall fallback is only safe on platforms where tv_nsec "
      "matches the register size (long). It is unsafe on 32-bit platforms "
      "with 64-bit tv_nsec.");
  return syscall_checked<int>(SYS_pselect6, nfds, readfds, writefds, exceptfds,
                              timeout, &pss);
#else
#error "pselect6 and pselect6_time64 syscalls not available."
#endif
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_PSELECT6_H
