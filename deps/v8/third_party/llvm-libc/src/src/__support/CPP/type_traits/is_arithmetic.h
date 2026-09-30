//===-- is_arithmetic type_traits -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ARITHMETIC_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ARITHMETIC_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/is_floating_point.h"
#include "src/__support/CPP/type_traits/is_integral.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_arithmetic
template <typename T>
struct is_arithmetic : cpp::bool_constant<(cpp::is_integral_v<T> ||
                                           cpp::is_floating_point_v<T>)> {};
template <typename T>
LIBC_INLINE_VAR constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_ARITHMETIC_H
