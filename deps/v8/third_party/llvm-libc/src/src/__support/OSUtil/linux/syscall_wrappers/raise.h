//===-- Implementation header for raise -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RAISE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RAISE_H

#include "hdr/signal_macros.h"
#include "hdr/types/sigset_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {
LIBC_INLINE ErrorOr<int> raise(int sig) {
  class SigMaskGuard {
    [[maybe_unused]] sigset_t old_set;
    [[maybe_unused]] ErrorOr<int> &status;

  public:
    LIBC_INLINE SigMaskGuard(ErrorOr<int> &status) : old_set{}, status(status) {
      sigset_t full_set = sigset_t{{-1UL}};
      status = syscall_impl<int>(SYS_rt_sigprocmask, SIG_BLOCK, &full_set,
                                 &old_set, sizeof(sigset_t));
    }
    LIBC_INLINE ~SigMaskGuard() {
      if (status.has_value()) {
        int restore_result =
            syscall_impl<int>(SYS_rt_sigprocmask, SIG_SETMASK, &old_set,
                              nullptr, sizeof(sigset_t));
        if (restore_result < 0)
          status = Error(-restore_result);
      }
    }
  };
  ErrorOr<int> status = 0;
  {
    SigMaskGuard sig_mask(status);

    if (!status.has_value())
      return status;

    long pid = syscall_impl<long>(SYS_getpid);
    if (pid < 0)
      return Error(-static_cast<int>(pid));

    long tid = syscall_impl<long>(SYS_gettid);
    if (tid < 0)
      return Error(-static_cast<int>(tid));

    int result = syscall_impl<int>(SYS_tgkill, pid, tid, sig);
    if (result < 0)
      return Error(-result);
  }
  return status;
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_RAISE_H
