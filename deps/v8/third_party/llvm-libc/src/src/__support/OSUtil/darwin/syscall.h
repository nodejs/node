//===---------------------- Darwin syscalls ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_DARWIN_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_DARWIN_SYSCALL_H

#include "src/__support/CPP/bit.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

#ifdef LIBC_TARGET_ARCH_IS_ANY_ARM
#include "aarch64/syscall.h"
#else
#error "Unsupported architecture"
#endif

namespace LIBC_NAMESPACE_DECL {

template <typename R, typename... Ts>
LIBC_INLINE R syscall_impl(long __number, Ts... ts) {
  static_assert(sizeof...(Ts) <= 6, "Too many arguments for syscall");
  return cpp::bit_or_static_cast<R>(syscall_impl(__number, (long)ts...));
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_DARWIN_SYSCALL_H
