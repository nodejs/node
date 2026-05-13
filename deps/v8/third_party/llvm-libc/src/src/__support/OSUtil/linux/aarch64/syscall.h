//===--------- inline implementation of aarch64 syscalls ----------* C++ *-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_AARCH64_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_AARCH64_SYSCALL_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"

#define REGISTER_DECL_0                                                        \
  register long x8 __asm__("x8") = number;                                     \
  register long x0 __asm__("x0");
#define REGISTER_DECL_1                                                        \
  register long x8 __asm__("x8") = number;                                     \
  register long x0 __asm__("x0") = arg1;
#define REGISTER_DECL_2 REGISTER_DECL_1 register long x1 __asm__("x1") = arg2;
#define REGISTER_DECL_3                                                        \
  REGISTER_DECL_2                                                              \
  register long x2 __asm__("x2") = arg3;
#define REGISTER_DECL_4                                                        \
  REGISTER_DECL_3                                                              \
  register long x3 __asm__("x3") = arg4;
#define REGISTER_DECL_5                                                        \
  REGISTER_DECL_4                                                              \
  register long x4 __asm__("x4") = arg5;
#define REGISTER_DECL_6                                                        \
  REGISTER_DECL_5                                                              \
  register long x5 __asm__("x5") = arg6;

#define REGISTER_CONSTRAINT_0 "r"(x8)
#define REGISTER_CONSTRAINT_1 REGISTER_CONSTRAINT_0, "r"(x0)
#define REGISTER_CONSTRAINT_2 REGISTER_CONSTRAINT_1, "r"(x1)
#define REGISTER_CONSTRAINT_3 REGISTER_CONSTRAINT_2, "r"(x2)
#define REGISTER_CONSTRAINT_4 REGISTER_CONSTRAINT_3, "r"(x3)
#define REGISTER_CONSTRAINT_5 REGISTER_CONSTRAINT_4, "r"(x4)
#define REGISTER_CONSTRAINT_6 REGISTER_CONSTRAINT_5, "r"(x5)

#define SYSCALL_INSTR(input_constraint)                                        \
  LIBC_INLINE_ASM("svc 0" : "=r"(x0) : input_constraint : "memory", "cc")

namespace LIBC_NAMESPACE_DECL {

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number) {
  REGISTER_DECL_0;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_0);
  return x0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1) {
  REGISTER_DECL_1;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_1);
  return x0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2) {
  REGISTER_DECL_2;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_2);
  return x0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2, long arg3) {
  REGISTER_DECL_3;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_3);
  return x0;
}

[[gnu::always_inline]] LIBC_INLINE long
syscall_impl(long number, long arg1, long arg2, long arg3, long arg4) {
  REGISTER_DECL_4;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_4);
  return x0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2, long arg3,
                                                     long arg4, long arg5) {
  REGISTER_DECL_5;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_5);
  return x0;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long number, long arg1,
                                                     long arg2, long arg3,
                                                     long arg4, long arg5,
                                                     long arg6) {
  REGISTER_DECL_6;
  SYSCALL_INSTR(REGISTER_CONSTRAINT_6);
  return x0;
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

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_AARCH64_SYSCALL_H
