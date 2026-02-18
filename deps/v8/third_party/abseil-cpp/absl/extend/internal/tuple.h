// Copyright 2026 The Abseil Authors.
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

#ifndef ABSL_EXTEND_INTERNAL_TUPLE_H_
#define ABSL_EXTEND_INTERNAL_TUPLE_H_

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace extend_internal {

template <std::size_t... N, typename F, typename... T>
void ZippedForEachImpl(std::index_sequence<N...>, F f, T&&... t) {
  const auto for_i = [&](auto i) { f(std::get<i>(std::forward<T>(t))...); };
  (for_i(std::integral_constant<size_t, N>{}), ...);
}

template <typename F, typename... T>
void ZippedForEach(F f, T&... t) {
  using tuple_t = std::decay_t<decltype((std::declval<T>(), ...))>;
  using seq_t = std::make_index_sequence<std::tuple_size_v<tuple_t>>;
  (ZippedForEachImpl)(seq_t{}, f, t...);
}

template <typename F, size_t... Ns>
auto MakeTupleFromCallable(std::index_sequence<Ns...>, F f) {
  return std::make_tuple(f(std::integral_constant<size_t, Ns>{})...);
}

}  // namespace extend_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_TUPLE_H_
