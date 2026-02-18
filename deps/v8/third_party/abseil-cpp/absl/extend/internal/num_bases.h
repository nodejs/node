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

#ifndef ABSL_EXTEND_INTERNAL_NUM_BASES_H_
#define ABSL_EXTEND_INTERNAL_NUM_BASES_H_

#include <stddef.h>

#include <optional>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/extend/internal/num_initializers.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace aggregate_internal {

template <typename U>
struct OnlyBases {
  template <typename T>
  using IsStrictBase =
      std::enable_if_t<std::is_base_of_v<T, U> && !std::is_same_v<T, U>, int>;

  template <typename T, IsStrictBase<T> = 0>
  operator T&&() const;  // NOLINT(google-explicit-constructor)
  template <typename T, IsStrictBase<T> = 0>
  operator T&() const;  // NOLINT(google-explicit-constructor)
};

template <typename T, size_t... I>
constexpr std::optional<int> NumBasesImpl(std::index_sequence<I...>) {
  if constexpr (BraceInitializableWith<
                    T, OnlyBases<T>, OnlyBases<T>,
                    decltype(  // Cast to void to avoid unintentionally invoking
                               // comma operator or warning on unused LHS
                        (void)I, Anything{})...>::value) {
    // The first two initializers look like bases.
    // The second might be a member or a base, so we give up.
    return std::nullopt;
  } else if constexpr (BraceInitializableWith<
                           T, OnlyBases<T>, Anything,
                           decltype(  // Cast to void to avoid unintentionally
                                      // invoking comma operator or warning on
                                      // unused LHS
                               (void)I, Anything{})...>::value) {
    // The first initializer is a base, but not the second. We have 1 base.
    return 1;
  } else {
    // The first initializer is not a base. We have 0 bases.
    return 0;
  }
}

// Returns the number of base classes of the given type, with best effort:
//
// - If the type is empty (i.e. `std::is_empty_v<T>` is true), returns nullopt.
// - If the type has no bases, returns 0.
// - If the type has exactly 1 base, returns 1.
// - Otherwise, returns nullopt.
template <typename T>
constexpr std::optional<int> NumBases() {
  if constexpr (std::is_empty_v<T>) {
    return std::nullopt;
  } else {
    constexpr int num_initializers = NumInitializers<T>();
    return NumBasesImpl<T>(std::make_index_sequence<num_initializers - 1>());
  }
}

}  // namespace aggregate_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_NUM_BASES_H_
