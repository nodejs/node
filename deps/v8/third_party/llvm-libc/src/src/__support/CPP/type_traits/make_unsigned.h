//===-- make_unsigned type_traits -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_MAKE_UNSIGNED_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_MAKE_UNSIGNED_H

#include "src/__support/CPP/type_traits/type_identity.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/types.h" // LIBC_TYPES_HAS_INT128

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// make_unsigned

template <typename T> struct make_unsigned;
template <> struct make_unsigned<char> : type_identity<unsigned char> {};
template <> struct make_unsigned<signed char> : type_identity<unsigned char> {};
template <> struct make_unsigned<short> : type_identity<unsigned short> {};
template <> struct make_unsigned<int> : type_identity<unsigned int> {};
template <> struct make_unsigned<long> : type_identity<unsigned long> {};
template <>
struct make_unsigned<long long> : type_identity<unsigned long long> {};
template <>
struct make_unsigned<unsigned char> : type_identity<unsigned char> {};
template <>
struct make_unsigned<unsigned short> : type_identity<unsigned short> {};
template <> struct make_unsigned<unsigned int> : type_identity<unsigned int> {};
template <>
struct make_unsigned<unsigned long> : type_identity<unsigned long> {};
template <>
struct make_unsigned<unsigned long long> : type_identity<unsigned long long> {};
#ifdef LIBC_TYPES_HAS_INT128
template <> struct make_unsigned<__int128_t> : type_identity<__uint128_t> {};
template <> struct make_unsigned<__uint128_t> : type_identity<__uint128_t> {};
#endif
template <typename T> using make_unsigned_t = typename make_unsigned<T>::type;

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_TYPE_TRAITS_MAKE_UNSIGNED_H
