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

#ifndef ABSL_META_INTERNAL_REQUIRES_H_
#define ABSL_META_INTERNAL_REQUIRES_H_

#include <type_traits>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace meta_internal {

// C++17 port of the C++20 `requires` expressions.
// It allows easy inline test of properties of types in template code.
// https://en.cppreference.com/w/cpp/language/constraints#Requires_expressions
//
// Example usage:
//
// if constexpr (Requires<T>([](auto&& x) -> decltype(x.foo()) {})) {
//   // T has foo()
//   return t.foo();
// } else if constexpr (Requires<T>([](auto&& x) -> decltype(Bar(x)) {})) {
//   // Can call Bar with T
//   return Bar(t);
// } else if constexpr (Requires<T, U>(
//     // Can test expression with multiple inputs
//     [](auto&& x, auto&& y) -> decltype(x + y) {})) {
//   return t + t2;
// }
//
// The `Requires` function takes a list of types and a generic lambda where all
// arguments are of type `auto&&`. The lambda is never actually invoked and the
// body must be empty.
// When used this way, `Requires` returns whether the expression inside
// `decltype` is well-formed, when the lambda parameters have the types that
// are specified by the corresponding template arguments.
//
// NOTE: C++17 does not allow lambdas in template parameters, which means that
// code like the following is _not_ valid in C++17:
//
//  template <typename T,
//            typename = std::enable_if_t<gtl::Requires<T>(
//              [] (auto&& v) -> decltype(<expr>) {})>>
//
template <typename... T, typename F>
constexpr bool Requires(F) {
  return std::is_invocable_v<F, T...>;
}

}  // namespace meta_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_META_INTERNAL_REQUIRES_H_
