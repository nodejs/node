//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for sigprocmask.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_DARWIN_SYSCALL_WRAPPERS_SIGPROCMASK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_DARWIN_SYSCALL_WRAPPERS_SIGPROCMASK_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/sigset_t.h"
#include "src/__support/OSUtil/darwin/syscall.h" // syscall_impl
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

#include <sys/syscall.h> // For syscall numbers.

namespace LIBC_NAMESPACE_DECL {
namespace darwin_syscalls {

LIBC_INLINE ErrorOr<int> sigprocmask(int how, const sigset_t *set,
                                     sigset_t *oldset) {
  // TODO: Use a 32-bit sigset_t on Darwin and remove this conversion.
  uint32_t kernel_set = set ? static_cast<uint32_t>(set->__signals[0]) : 0;
  uint32_t kernel_oldset = 0;
  int ret = syscall_impl<int>(SYS_sigprocmask, how, set ? &kernel_set : nullptr,
                              oldset ? &kernel_oldset : nullptr);
  if (ret != 0)
    return Error(ret);

  if (oldset)
    oldset->__signals[0] =
        static_cast<unsigned long>(kernel_oldset & 0x7FFFFFFFU);
  return ret;
}

} // namespace darwin_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_DARWIN_SYSCALL_WRAPPERS_SIGPROCMASK_H
