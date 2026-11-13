//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of riscv clone.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_RISCV_CLONE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_RISCV_CLONE_H

#include "hdr/stdint_proxy.h"
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
  register long a7 asm("a7") = SYS_clone;
  register long a0 asm("a0") = flags;
  register void *a1 asm("a1") = child_stack;
  register pid_t *a2 asm("a2") = parent_tid;
  register void *a3 asm("a3") = tls;
  register pid_t *a4 asm("a4") = child_tid;

  LIBC_INLINE_ASM("ecall\n\t"
                  "bnez a0, 1f\n\t"
                  // Child execution:
                  // Zero frame pointer to terminate call stack unwinding.
                  "mv s0, zero\n\t"
#if __riscv_xlen == 64
                  "ld a1, 0(sp)\n\t"
                  "ld a0, 8(sp)\n\t"
#else
                  "lw a1, 0(sp)\n\t"
                  "lw a0, 4(sp)\n\t"
#endif
                  "addi sp, sp, %c[stack_adj]\n\t"
                  "jalr a1\n\t"
                  "li a7, %[sys_exit]\n\t"
                  // Does not return.
                  "ecall\n\t"
                  // Just in case it does..
                  "unimp\n\t"
                  "1:\n\t" : "+r"(a0) : "r"(a7),
                  "r"(a1), "r"(a2), "r"(a3), "r"(a4), [sys_exit] "i"(SYS_exit),
                  [stack_adj] "i"(CLONE_STACK_ALIGNMENT) : "memory");
  return a0;
}

} // namespace detail
} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_WRAPPERS_RISCV_CLONE_H
