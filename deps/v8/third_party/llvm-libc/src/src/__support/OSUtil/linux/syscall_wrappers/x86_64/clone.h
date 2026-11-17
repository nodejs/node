//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of x86_64 clone.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_X86_64_CLONE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_X86_64_CLONE_H

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
  long ret;
  register pid_t *r10 __asm__("r10") = child_tid;
  register void *r8 __asm__("r8") = tls;

  LIBC_INLINE_ASM("syscall\n\t"
                  "testq %%rax, %%rax\n\t"
                  "jnz 1f\n\t"
                  // Child execution:
                  // Zero frame pointer to terminate call stack unwinding.
                  "xorl %%ebp, %%ebp\n\t"
                  "popq %%rax\n\t"
                  "popq %%rdi\n\t"
                  "call *%%rax\n\t"
                  "movl %%eax, %%edi\n\t"
                  "movl %[sys_exit], %%eax\n\t"
                  // Does not return.
                  "syscall\n\t"
                  // Just in case it does..
                  "ud2\n\t"
                  "1:\n\t" : "=a"(ret) : "0"(SYS_clone),
                  "D"(flags), "S"(child_stack), "d"(parent_tid), "r"(r10),
                  "r"(r8), [sys_exit] "i"(SYS_exit) : "rcx", "r11", "memory");
  return ret;
}

} // namespace detail
} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_X86_64_CLONE_H
