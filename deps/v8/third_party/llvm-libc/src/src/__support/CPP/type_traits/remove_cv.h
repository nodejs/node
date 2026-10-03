//===-- remove_cv type_traits -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_CV_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_CV_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// remove_cv
template <class T> struct remove_cv : cpp::type_identity<T> {};
template <class T> struct remove_cv<const T> : cpp::type_identity<T> {};
template <class T> struct remove_cv<volatile T> : cpp::type_identity<T> {};
template <class T>
struct remove_cv<const volatile T> : cpp::type_identity<T> {};
template <class T> using remove_cv_t = typename remove_cv<T>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_REMOVE_CV_H
