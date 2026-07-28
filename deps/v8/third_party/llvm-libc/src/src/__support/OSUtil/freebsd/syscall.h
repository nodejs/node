//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// FreeBSD syscalls.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_SYSCALL_H

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

namespace LIBC_NAMESPACE_DECL {

struct SyscallReturn {
  long value;
  bool is_error;
};

} // namespace LIBC_NAMESPACE_DECL

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

template <typename... Ts>
LIBC_INLINE SyscallReturn syscall_impl(long __number, Ts... ts) {
  static_assert(sizeof...(Ts) <= 6, "Too many arguments for syscall");
  return syscall_impl(__number, (long)ts...);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_SYSCALL_H
