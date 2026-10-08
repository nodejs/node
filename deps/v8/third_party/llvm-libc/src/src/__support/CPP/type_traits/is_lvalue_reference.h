//===-- is_lvalue_reference type_traits -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_LVALUE_REFERENCE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_LVALUE_REFERENCE_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/false_type.h"
#include "src/__support/CPP/type_traits/true_type.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_lvalue_reference
#if __has_builtin(__is_lvalue_reference)
template <typename T>
struct is_lvalue_reference : bool_constant<__is_lvalue_reference(T)> {};
#else
template <typename T> struct is_lvalue_reference : public false_type {};
template <typename T> struct is_lvalue_reference<T &> : public true_type {};
#endif
template <class T>
LIBC_INLINE_VAR constexpr bool is_lvalue_reference_v =
    is_lvalue_reference<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_LVALUE_REFERENCE_H
