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

#include "absl/meta/internal/requires.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace {

TEST(RequiresTest, SimpleLambdasWork) {
  static_assert(absl::meta_internal::Requires([] {}));
  static_assert(absl::meta_internal::Requires<int>([](auto&&) {}));
  static_assert(
      absl::meta_internal::Requires<int, char>([](auto&&, auto&&) {}));
}

template <typename T>
inline constexpr bool has_cstr =
    absl::meta_internal::Requires<T>([](auto&& x) -> decltype(x.c_str()) {});

template <typename T, typename U>
inline constexpr bool have_plus = absl::meta_internal::Requires<T, U>(
    [](auto&& x, auto&& y) -> decltype(x + y) {});

TEST(RequiresTest, CanTestProperties) {
  static_assert(has_cstr<std::string>);
  static_assert(!has_cstr<std::vector<int>>);

  static_assert(have_plus<int, double>);
  static_assert(have_plus<std::string, std::string>);
  static_assert(!have_plus<std::string, double>);
}

TEST(RequiresTest, WorksWithUnmovableTypes) {
  struct S {
    S(const S&) = delete;
    int foo() { return 0; }
  };
  static_assert(
      absl::meta_internal::Requires<S>([](auto&& x) -> decltype(x.foo()) {}));
  static_assert(
      !absl::meta_internal::Requires<S>([](auto&& x) -> decltype(x.bar()) {}));
}

TEST(RequiresTest, WorksWithArrays) {
  static_assert(
      absl::meta_internal::Requires<int[2]>([](auto&& x) -> decltype(x[1]) {}));
  static_assert(
      !absl::meta_internal::Requires<int[2]>([](auto&& x) -> decltype(-x) {}));
}

}  // namespace
