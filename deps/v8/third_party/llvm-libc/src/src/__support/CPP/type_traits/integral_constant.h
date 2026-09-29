//===-- integral_constant type_traits ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INTEGRAL_CONSTANT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INTEGRAL_CONSTANT_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE_VAR
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// integral_constant
template <typename T, T v> struct integral_constant {
  using value_type = T;
  LIBC_INLINE_VAR static constexpr T value = v;
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_INTEGRAL_CONSTANT_H
