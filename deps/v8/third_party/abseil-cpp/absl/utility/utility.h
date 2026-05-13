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
#include <utility>

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
using std::apply;
using std::exchange;
using std::forward;

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

using std::make_from_tuple;

template <size_t N>
using make_index_sequence ABSL_DEPRECATE_AND_INLINE() =
    std::make_index_sequence<N>;

template <class T, T N>
using make_integer_sequence ABSL_DEPRECATE_AND_INLINE() =
    std::make_integer_sequence<T, N>;

using std::move;

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
