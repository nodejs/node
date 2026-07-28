//===----------------------- Linux syscalls ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_H

#include "src/__support/CPP/bit.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/architectures.h"

#ifdef LIBC_TARGET_ARCH_IS_X86_32
#include "i386/syscall.h"
#elif defined(LIBC_TARGET_ARCH_IS_X86_64)
#include "x86_64/syscall.h"
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "aarch64/syscall.h"
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
#include "arm/syscall.h"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "riscv/syscall.h"
#endif

namespace LIBC_NAMESPACE_DECL {

// This function performs no error checking. For most syscalls, it's better to
// use linux_syscalls::syscall_checked below.
template <typename R, typename... Ts>
LIBC_INLINE R syscall_impl(long __number, Ts... ts) {
  static_assert(sizeof...(Ts) <= 6, "Too many arguments for syscall");
  return cpp::bit_or_static_cast<R>(syscall_impl(__number, (long)ts...));
}

namespace linux_syscalls {
LIBC_INLINE_VAR constexpr unsigned long MAX_ERRNO = 4095;

// Helper function to perform a system call, check the result and cast the
// result to the expected type. This function is safe to use on most syscalls,
// with the exception of a handful of syscalls (getpid, getuid, ...) that never
// fail.
template <typename R, typename... Ts>
LIBC_INLINE ErrorOr<R> syscall_checked(long __number, Ts... ts) {
  static_assert(sizeof...(Ts) <= 6, "Too many arguments");
  unsigned long ret =
      static_cast<unsigned long>(syscall_impl(__number, (long)ts...));
  if (ret >= -MAX_ERRNO)
    return Error(static_cast<int>(-ret));
  return cpp::bit_or_static_cast<R>(ret);
}
} // namespace linux_syscalls

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_H
