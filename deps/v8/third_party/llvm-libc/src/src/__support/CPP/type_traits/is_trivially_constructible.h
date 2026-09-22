//===-- is_trivially_constructible type_traits ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_CONSTRUCTIBLE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_CONSTRUCTIBLE_H

#include "src/__support/CPP/type_traits/integral_constant.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_trivially_constructible
template <class T, class... Args>
struct is_trivially_constructible
    : integral_constant<bool, __is_trivially_constructible(T, Args...)> {};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_TRIVIALLY_CONSTRUCTIBLE_H
