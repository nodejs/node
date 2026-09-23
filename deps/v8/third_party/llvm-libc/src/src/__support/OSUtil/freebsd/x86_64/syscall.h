//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Inline implementation of x86_64 syscalls.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_X86_64_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_X86_64_SYSCALL_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"

#define SYSCALL_CLOBBER_LIST "rcx", "r11", "memory", "cc"

namespace LIBC_NAMESPACE_DECL {

LIBC_INLINE SyscallReturn syscall_impl(long __number) {
  long retcode;
  bool is_error;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

LIBC_INLINE SyscallReturn syscall_impl(long __number, long __arg1) {
  long retcode;
  bool is_error;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number), "D"(__arg1)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

LIBC_INLINE SyscallReturn syscall_impl(long __number, long __arg1,
                                       long __arg2) {
  long retcode;
  bool is_error;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number), "D"(__arg1), "S"(__arg2)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

LIBC_INLINE SyscallReturn syscall_impl(long __number, long __arg1, long __arg2,
                                       long __arg3) {
  long retcode;
  bool is_error;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number), "D"(__arg1), "S"(__arg2), "d"(__arg3)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

LIBC_INLINE SyscallReturn syscall_impl(long __number, long __arg1, long __arg2,
                                       long __arg3, long __arg4) {
  long retcode;
  bool is_error;
  register long r10 __asm__("r10") = __arg4;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number), "D"(__arg1), "S"(__arg2), "d"(__arg3), "r"(r10)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

LIBC_INLINE SyscallReturn syscall_impl(long __number, long __arg1, long __arg2,
                                       long __arg3, long __arg4, long __arg5) {
  long retcode;
  bool is_error;
  register long r10 __asm__("r10") = __arg4;
  register long r8 __asm__("r8") = __arg5;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number), "D"(__arg1), "S"(__arg2), "d"(__arg3), "r"(r10),
                 "r"(r8)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

LIBC_INLINE SyscallReturn syscall_impl(long __number, long __arg1, long __arg2,
                                       long __arg3, long __arg4, long __arg5,
                                       long __arg6) {
  long retcode;
  bool is_error;
  register long r10 __asm__("r10") = __arg4;
  register long r8 __asm__("r8") = __arg5;
  register long r9 __asm__("r9") = __arg6;
  asm volatile("syscall\n\t"
               "setc %1"
               : "=a"(retcode), "=qm"(is_error)
               : "a"(__number), "D"(__arg1), "S"(__arg2), "d"(__arg3), "r"(r10),
                 "r"(r8), "r"(r9)
               : SYSCALL_CLOBBER_LIST);
  return {retcode, is_error};
}

#undef SYSCALL_CLOBBER_LIST
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_X86_64_SYSCALL_H
