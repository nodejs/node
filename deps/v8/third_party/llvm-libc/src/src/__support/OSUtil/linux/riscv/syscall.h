//===--------- inline implementation of riscv syscalls ------------* C++ *-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_RISCV_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_RISCV_SYSCALL_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"

#define REGISTER_DECL_0                                                        \
  register long a7 __asm__("a7") = number;                                     \
  register long a0 __asm__("a0");
#define REGISTER_DECL_1                                                        \
  register long a7 __asm__("a7") = number;                                     \
  register long a0 __asm__("a0") = arg1;
#define REGISTER_DECL_2 REGISTER_DECL_1 register long a1 __asm__("a1") = arg2;
#define REGISTER_DECL_3                                                        \
  REGISTER_DECL_2                                                              \
  register long a2 __asm__("a2") = arg3;
#define REGISTER_DECL_4                                                        \
  REGISTER_DECL_3                                                              \
  register long a3 __asm__("a3") = arg4;
#define REGISTER_DECL_5                                                        \
  REGISTER_DECL_4                                                              \
  register long a4 __asm__("a4") = arg5;
#define REGISTER_DECL_6                                                        \
  REGISTER_DECL_5                                                              \
  register long a5 __asm__("a5") = arg6;

#define REGISTER_CONSTRAINT_0 "r"(a7)
#define REGISTER_CONSTRAINT_1 REGISTER_CONSTRAINT_0, "r"(a0)
#define REGISTER_CONSTRAINT_2 REGISTER_CONSTRAINT_1, "r"(a1)
#define REGISTER_CONSTRAINT_3 REGISTER_CONSTRAINT_2, "r"(a2)
#define REGISTER_CONSTRAINT_4 REGISTER_CONSTRAINT_3, "r"(a3)
#define REGISTER_CONSTRAINT_5 REGISTER_CONSTRAINT_4, "r"(a4)
#define REGISTER_CONSTRAINT_6 REGISTER_CONSTRAINT_5, "r"(a5)

#define SYSCALL_INSTR(input_constraint)                                        \
  LIBC_INLINE_ASM("ecall\n\t" : "=r"(a0) : input_constraint : "memory")

namespace LIBC_NAMESPACE_DECL {

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number) {
  REGISTER_DECL_0;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_0);
  return a0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1) {
  REGISTER_DECL_1;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_1);
  return a0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2) {
  REGISTER_DECL_2;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_2);
  return a0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2, long arg3) {
  REGISTER_DECL_3;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_3);
  return a0;
}

[[gnu::always_inline]] LIBC_INLINE long
syscall_impl(long number, long arg1, long arg2, long arg3, long arg4) {
  REGISTER_DECL_4;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_4);
  return a0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2, long arg3,
                                                     long arg4, long arg5) {
  REGISTER_DECL_5;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_5);
  return a0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2, long arg3,
                                                     long arg4, long arg5,
                                                     long arg6) {
  REGISTER_DECL_6;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_6);
  return a0;
}

} // namespace LIBC_NAMESPACE_DECL

#undef REGISTER_DECL_0
#undef REGISTER_DECL_1
#undef REGISTER_DECL_2
#undef REGISTER_DECL_3
#undef REGISTER_DECL_4
#undef REGISTER_DECL_5
#undef REGISTER_DECL_6

#undef REGISTER_CONSTRAINT_0
#undef REGISTER_CONSTRAINT_1
#undef REGISTER_CONSTRAINT_2
#undef REGISTER_CONSTRAINT_3
#undef REGISTER_CONSTRAINT_4
#undef REGISTER_CONSTRAINT_5
#undef REGISTER_CONSTRAINT_6

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_RISCV_SYSCALL_H
