//===---------- inline implementation of i386 syscalls ------------* C++ *-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_I386_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_I386_SYSCALL_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long num) {
  long ret;
  LIBC_INLINE_ASM("int $128" : "=a"(ret) : "a"(num) : "memory");
  return ret;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long num, long arg1) {
  long ret;
  LIBC_INLINE_ASM("int $128" : "=a"(ret) : "a"(num), "b"(arg1) : "memory");
  return ret;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long num, long arg1,
                                                     long arg2) {
  long ret;
  LIBC_INLINE_ASM("int $128"
                  : "=a"(ret)
                  : "a"(num), "b"(arg1), "c"(arg2)
                  : "memory");
  return ret;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long num, long arg1,
                                                     long arg2, long arg3) {
  long ret;
  LIBC_INLINE_ASM("int $128"
                  : "=a"(ret)
                  : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                  : "memory");
  return ret;
}

[[gnu::always_inline]] LIBC_INLINE long
syscall_impl(long num, long arg1, long arg2, long arg3, long arg4) {
  long ret;
  LIBC_INLINE_ASM("int $128"
                  : "=a"(ret)
                  : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
                  : "memory");
  return ret;
}

[[gnu::always_inline]] LIBC_INLINE long
syscall_impl(long num, long arg1, long arg2, long arg3, long arg4, long arg5) {
  long ret;
  LIBC_INLINE_ASM("int $128"
                  : "=a"(ret)
                  : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4),
                    "D"(arg5)
                  : "memory");
  return ret;
}

[[gnu::always_inline]] LIBC_INLINE long syscall_impl(long num, long arg1,
                                                     long arg2, long arg3,
                                                     long arg4, long arg5,
                                                     long arg6) {
  long ret;
  LIBC_INLINE_ASM(R"(
    push %[arg6]
    push %%ebp
    mov 4(%%esp), %%ebp
    int $128
    pop %%ebp
    add $4, %%esp
  )"
                  : "=a"(ret)
                  : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4),
                    "D"(arg5), [arg6] "m"(arg6)
                  : "memory");
  return ret;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_I386_SYSCALL_H
