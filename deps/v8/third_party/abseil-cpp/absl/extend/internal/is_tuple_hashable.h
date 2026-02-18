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

#ifndef ABSL_EXTEND_INTERNAL_IS_TUPLE_HASHABLE_H_
#define ABSL_EXTEND_INTERNAL_IS_TUPLE_HASHABLE_H_

#include <tuple>
#include <type_traits>

#include "absl/base/config.h"
#include "absl/hash/hash.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace extend_internal {

// Trait to check if all members of a tuple are absl::Hash-able.
//
template <typename T>
struct IsTupleHashable {};

// TODO(b/185498964): We wouldn't need absl::Hash if the hasher object provided
// an "is_hashable" trait.
template <typename... Ts>
struct IsTupleHashable<std::tuple<Ts...>>
    : std::integral_constant<bool,
                             (std::is_constructible_v<absl::Hash<Ts>> && ...)> {
};

}  // namespace extend_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_IS_TUPLE_HASHABLE_H_
