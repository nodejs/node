//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of aarch64 clone.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_AARCH64_CLONE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_AARCH64_CLONE_H

#include "hdr/types/pid_t.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For SYS_clone, SYS_exit

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

constexpr uintptr_t CLONE_STACK_ALIGNMENT = 16;

namespace detail {

LIBC_INLINE long clone_impl(int flags, void *child_stack, pid_t *parent_tid,
                            void *tls, pid_t *child_tid) {
  register long x8 asm("x8") = SYS_clone;
  register long x0 asm("x0") = flags;
  register void *x1 asm("x1") = child_stack;
  register pid_t *x2 asm("x2") = parent_tid;
  register void *x3 asm("x3") = tls;
  register pid_t *x4 asm("x4") = child_tid;

  LIBC_INLINE_ASM("svc #0\n\t"
                  "cbnz x0, 1f\n\t"
                  // Child execution:
                  // Zero frame pointer to terminate call stack unwinding.
                  "mov x29, #0\n\t"
                  "ldp x1, x0, [sp], #16\n\t"
                  "blr x1\n\t"
                  "mov x8, %[sys_exit]\n\t"
                  // Does not return.
                  "svc #0\n\t"
                  // Just in case it does..
                  "brk #0\n\t"
                  "1:\n\t" : "+r"(x0) : "r"(x8),
                  "r"(x1), "r"(x2), "r"(x3),
                  "r"(x4), [sys_exit] "i"(SYS_exit) : "memory", "cc");
  return x0;
}

} // namespace detail
} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_AARCH64_CLONE_H
