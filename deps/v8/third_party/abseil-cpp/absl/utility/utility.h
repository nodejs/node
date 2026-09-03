// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_UTILITY_UTILITY_H_
#define ABSL_UTILITY_UTILITY_H_

#include <cstddef>
#include <cstdlib>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"

// TODO(b/290784225): Include what you use cleanup required.
#include "absl/meta/type_traits.h"

// TODO(b/509512528): Deprecate the C++14/C++17 symbols publicly, in all files.

namespace absl {
ABSL_NAMESPACE_BEGIN

// Historical note: Abseil once provided implementations of these
// abstractions for platforms that had not yet provided them. Those
// platforms are no longer supported. New code should simply use the
// the ones from std directly.
template <class F, class T>
ABSL_DEPRECATE_AND_INLINE()
constexpr decltype(auto)
    apply(F&& f, T&& t) noexcept(noexcept(std::apply(std::declval<F>(),
                                                     std::declval<T>()))) {
  return std::apply(std::forward<F>(f), std::forward<T>(t));
}

template <class T1, class T2 = T1>
ABSL_DEPRECATE_AND_INLINE()
constexpr T1 exchange(T1& obj, T2&& new_value) noexcept(
    noexcept(std::exchange(std::declval<T1&>(), std::declval<T2>()))) {
  return std::exchange(obj, std::forward<T2>(new_value));
}

template <class T>
[[deprecated("Use std::forward instead.")]] [[nodiscard]] constexpr T&& forward(
    std::remove_reference_t<T>& arg ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  // NOLINTNEXTLINE: Avoid warnings about T not being the spelled type of arg.
  return std::forward<T>(arg);
}

template <class T>
[[deprecated("Use std::forward instead.")]] [[nodiscard]] constexpr T&& forward(
    std::remove_reference_t<T>&& arg ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  // NOLINTNEXTLINE: Avoid warnings about T not being the spelled type of arg.
  return std::forward<T>(arg);
}

inline constexpr const std::in_place_t& in_place ABSL_DEPRECATE_AND_INLINE() =
    std::in_place;

template <size_t I>
inline constexpr const std::in_place_index_t<I>& in_place_index
ABSL_DEPRECATE_AND_INLINE() = std::in_place_index<I>;

template <size_t I>
using in_place_index_t ABSL_DEPRECATE_AND_INLINE() = std::in_place_index_t<I>;

using in_place_t ABSL_DEPRECATE_AND_INLINE() = std::in_place_t;

template <class T>
inline constexpr const std::in_place_type_t<T>& in_place_type
ABSL_DEPRECATE_AND_INLINE() = std::in_place_type<T>;

template <class T>
using in_place_type_t ABSL_DEPRECATE_AND_INLINE() = std::in_place_type_t<T>;

template <size_t... I>
using index_sequence ABSL_DEPRECATE_AND_INLINE() = std::index_sequence<I...>;

template <class T, T... I>
using integer_sequence ABSL_DEPRECATE_AND_INLINE() =
    std::integer_sequence<T, I...>;

template <class... T>
using index_sequence_for ABSL_DEPRECATE_AND_INLINE() =
    std::index_sequence_for<T...>;

template <class T, class Tuple>
ABSL_DEPRECATE_AND_INLINE()
[[nodiscard]] constexpr decltype(std::make_from_tuple<T>(std::declval<Tuple>()))
    make_from_tuple(Tuple&& arg) noexcept(
        noexcept(std::make_from_tuple<T>(std::declval<Tuple>()))) {
  return std::make_from_tuple<T>(std::forward<Tuple>(arg));
}

template <size_t N>
using make_index_sequence ABSL_DEPRECATE_AND_INLINE() =
    std::make_index_sequence<N>;

template <class T, T N>
using make_integer_sequence ABSL_DEPRECATE_AND_INLINE() =
    std::make_integer_sequence<T, N>;

template <class It, class OutIt>
[[deprecated("Use std::move instead.")]]
constexpr OutIt move(It&& begin, It&& end, OutIt&& output) {
  return std::move(std::forward<It>(begin), std::forward<It>(end),
                   std::forward<OutIt>(output));
}

template <class T>
[[deprecated("Use std::move instead.")]]
[[nodiscard]] constexpr std::remove_reference_t<T>&&
move(T&& arg ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return std::move(arg);  // NOLINT(bugprone-move-forwarding-reference)
}

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
// Backfill for std::nontype_t. An instance of this class can be provided as a
// disambiguation tag to `absl::function_ref` to pass the address of a known
// callable at compile time.
// Requires C++20 due to `auto` template parameter.
template <auto>
struct nontype_t {
  explicit nontype_t() = default;
};
template <auto V>
constexpr nontype_t<V> nontype{};
#endif

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_UTILITY_UTILITY_H_
