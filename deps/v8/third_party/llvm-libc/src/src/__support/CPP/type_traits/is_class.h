//===-- is_class type_traits ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CLASS_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CLASS_H

#include "src/__support/CPP/type_traits/bool_constant.h"
#include "src/__support/CPP/type_traits/false_type.h"
#include "src/__support/CPP/type_traits/is_union.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_class
namespace detail {
template <class T> cpp::bool_constant<!cpp::is_union_v<T>> test(int T::*);
template <class> cpp::false_type test(...);
} // namespace detail
template <class T> struct is_class : decltype(detail::test<T>(nullptr)) {};
template <typename T>
LIBC_INLINE_VAR constexpr bool is_class_v = is_class<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_CLASS_H
