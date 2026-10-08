//===-- make_signed type_traits ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_MAKE_SIGNED_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_MAKE_SIGNED_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/types.h" // LIBC_TYPES_HAS_INT128

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// make_signed
template <typename T> struct make_signed;
template <> struct make_signed<char> : type_identity<char> {};
template <> struct make_signed<signed char> : type_identity<char> {};
template <> struct make_signed<short> : type_identity<short> {};
template <> struct make_signed<int> : type_identity<int> {};
template <> struct make_signed<long> : type_identity<long> {};
template <> struct make_signed<long long> : type_identity<long long> {};
template <> struct make_signed<unsigned char> : type_identity<char> {};
template <> struct make_signed<unsigned short> : type_identity<short> {};
template <> struct make_signed<unsigned int> : type_identity<int> {};
template <> struct make_signed<unsigned long> : type_identity<long> {};
template <>
struct make_signed<unsigned long long> : type_identity<long long> {};
#ifdef LIBC_TYPES_HAS_INT128
template <> struct make_signed<__int128_t> : type_identity<__int128_t> {};
template <> struct make_signed<__uint128_t> : type_identity<__int128_t> {};
#endif
template <typename T> using make_signed_t = typename make_signed<T>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_MAKE_SIGNED_H
