// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_TRAITS_H_
#define V8_TYPE_TRAITS_H_

#include <type_traits>

namespace v8 {
namespace internal {

// Conjunction metafunction.
template <class... Args>
struct conjunction;

template <>
struct conjunction<> : std::true_type {};

template <class T>
struct conjunction<T> : T {};

template <class T, class... Args>
struct conjunction<T, Args...>
    : std::conditional<T::value, conjunction<Args...>, T>::type {};

// Disjunction metafunction.
template <class... Args>
struct disjunction;

template <>
struct disjunction<> : std::true_type {};

template <class T>
struct disjunction<T> : T {};

template <class T, class... Args>
struct disjunction<T, Args...>
    : std::conditional<T::value, T, disjunction<Args...>>::type {};

// Negation metafunction.
template <class T>
struct negation : std::integral_constant<bool, !T::value> {};

}  // namespace internal
}  // namespace v8

#endif  // V8_TYPE_TRAITS_H_
