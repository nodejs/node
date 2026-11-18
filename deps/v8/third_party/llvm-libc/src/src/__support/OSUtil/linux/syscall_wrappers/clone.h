//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for clone.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_CLONE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_CLONE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/pid_t.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#if defined(LIBC_TARGET_ARCH_IS_X86_64)
#include "src/__support/OSUtil/linux/syscall_wrappers/x86_64/clone.h"
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "src/__support/OSUtil/linux/syscall_wrappers/aarch64/clone.h"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "src/__support/OSUtil/linux/syscall_wrappers/riscv/clone.h"
#else
#error "clone is not supported for this architecture"
#endif

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

struct ChildStackFrame {
  int (*func)(void *);
  void *arg;
};

LIBC_INLINE ErrorOr<pid_t> clone(int (*func)(void *), void *stack, int flags,
                                 void *arg, pid_t *parent_tid = nullptr,
                                 void *tls = nullptr,
                                 pid_t *child_tid = nullptr) {
  uintptr_t sp = reinterpret_cast<uintptr_t>(stack);
  LIBC_ASSERT(sp % CLONE_STACK_ALIGNMENT == 0);

  // Set up func and arg at the top of the child stack in C++ so the assembly
  // only needs to pass the standard system call arguments.
  static_assert(sizeof(ChildStackFrame) <= CLONE_STACK_ALIGNMENT);
  sp -= CLONE_STACK_ALIGNMENT;
  auto *child_stack = reinterpret_cast<ChildStackFrame *>(sp);
  child_stack->func = func;
  child_stack->arg = arg;

  // We don't use the syscall function because the child thread will "return" to
  // a different stack. Any attempt to access local variables or spilled values
  // (which can be implicitly added by the compiler) will crash or cause other
  // sorts of problems.
  long ret = detail::clone_impl(flags, child_stack, parent_tid, tls, child_tid);

  if (ret < 0)
    return Error(static_cast<int>(-ret));
  return static_cast<pid_t>(ret);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_CLONE_H
