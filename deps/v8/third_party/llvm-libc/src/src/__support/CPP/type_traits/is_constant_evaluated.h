//===-- is_constant_evaluated type_traits -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONSTANT_EVALUATED_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONSTANT_EVALUATED_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

LIBC_INLINE constexpr bool is_constant_evaluated() {
#if LIBC_HAS_BUILTIN_IS_CONSTANT_EVALUATED
  return __builtin_is_constant_evaluated();
#else
  return false;
#endif
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CONSTANT_EVALUATED_H
