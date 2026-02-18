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

#ifndef ABSL_EXTEND_INTERNAL_NUM_INITIALIZERS_H_
#define ABSL_EXTEND_INTERNAL_NUM_INITIALIZERS_H_

#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/base/optimization.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace aggregate_internal {

template <typename U>
struct AnythingExcept {
  template <typename T,
            std::enable_if_t<!std::is_same_v<U, std::decay_t<T>>, int> = 0>
  operator T() const {  // NOLINT(google-explicit-constructor)
    ABSL_UNREACHABLE();
  }
};

struct Anything {
  template <typename T>
  operator T() const {  // NOLINT(google-explicit-constructor)
    ABSL_UNREACHABLE();
  }
};

struct NotDefaultConstructible {
  NotDefaultConstructible() = delete;
  explicit NotDefaultConstructible(int);
};

template <typename T>
struct Tester {
  T t;
  NotDefaultConstructible n;
};

template <class Void, typename T, typename... Args>
struct BraceInitializableWithImpl : std::false_type {
  static_assert(std::is_void_v<Void>, "First template parameter must be void");
};

template <typename T, typename... Args>
struct BraceInitializableWithImpl<
    std::enable_if_t<
        !std::is_null_pointer_v<decltype(Tester<T>{std::declval<Args>()...})>>,
    T, Args...> : std::true_type {};

#ifdef __clang__
#pragma clang diagnostic push
// -Wmissing-braces triggers here because we are expanding arguments into the
// first member of `Tester`. Not having the braces is necessary because we don't
// know how many arguments are going to be used to initialize the `T` and we
// need them to be tried greedily.
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

template <typename T, typename... Args>
using BraceInitializableWith = BraceInitializableWithImpl<void, T, Args...>;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// Count the number of initializers required for a struct of type `T`. Note that
// we do not support C-style array members. Prefer using a std::array.
//
// Repeatedly tests if `T` is brace-constructible with a given number of
// arguments. Each time it is not, one more argument is added and
// `NumInitializers` is called recursively.
template <typename T, typename... Args>
constexpr int NumInitializers(Args... args) {
  static_assert(!std::is_empty_v<T>);
  // By attempting to brace-initialize a Tester<T> where the first member is
  // `AnythingExcept<T>`, we force the aggregate to be expanded, meaning the
  // first some-number of arguments are passed to `T`'s aggregate
  // initialization. This will fail (and therefore recurse into
  // `NumInitializers` with one more argument) if we have not found enough
  // arguments to initialize `T`. This is because the second member of the
  // `Tester<T>` has type `NotDefaultConstructible` which cannot be default
  // constructed.
  if constexpr (BraceInitializableWith<T, AnythingExcept<T>, Args...>::value) {
    return sizeof...(Args);
  } else {
    static_assert(sizeof...(Args) <= sizeof(T),
                  "Automatic field count does not support fields like l-value "
                  "references or bit fields. Try specifying the field count.");
    return NumInitializers<T>(args..., Anything());
  }
}

}  // namespace aggregate_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_NUM_INITIALIZERS_H_
