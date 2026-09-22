//===-- is_trivially_copyable type_traits -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_COPYABLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_COPYABLE_H

#include "src/__support/CPP/type_traits/integral_constant.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_trivially_copyable
template <class T>
struct is_trivially_copyable
    : public integral_constant<bool, __is_trivially_copyable(T)> {};

template <class T>
LIBC_INLINE_VAR constexpr bool is_trivially_copyable_v =
    is_trivially_copyable<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_COPYABLE_H
