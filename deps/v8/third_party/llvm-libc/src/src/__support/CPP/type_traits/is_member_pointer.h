//===-- is_member_pointer type_traits ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_MEMBER_POINTER_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_MEMBER_POINTER_H

#include "src/__support/CPP/type_traits/false_type.h"
#include "src/__support/CPP/type_traits/remove_cv.h"
#include "src/__support/CPP/type_traits/true_type.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// is_member_pointer
template <class T> struct is_member_pointer_helper : cpp::false_type {};
template <class T, class U>
struct is_member_pointer_helper<T U::*> : cpp::true_type {};
template <class T>
struct is_member_pointer : is_member_pointer_helper<cpp::remove_cv_t<T>> {};
template <class T>
LIBC_INLINE_VAR constexpr bool is_member_pointer_v =
    is_member_pointer<T>::value;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_IS_MEMBER_POINTER_H
