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

template <typename R, typename... Ts>
LIBC_INLINE R syscall_impl(long __number, Ts... ts) {
  static_assert(sizeof...(Ts) <= 6, "Too many arguments for syscall");
  return cpp::bit_or_static_cast<R>(syscall_impl(__number, (long)ts...));
}

// Linux-specific function for checking
namespace linux_utils {
LIBC_INLINE_VAR constexpr unsigned long MAX_ERRNO = 4095;
// Ideally, this should be defined using PAGE_OFFSET
// However, that is a configurable parameter. We mimic kernel's behavior
// by checking against MAX_ERRNO.
template <typename PointerLike>
LIBC_INLINE constexpr bool is_valid_mmap(PointerLike ptr) {
  long addr = cpp::bit_cast<long>(ptr);
  return LIBC_LIKELY(addr > 0 || addr < -static_cast<long>(MAX_ERRNO));
}
} // namespace linux_utils

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SYSCALL_H
