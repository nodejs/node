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

#include "absl/extend/internal/dependencies.h"

#include <type_traits>

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace dependencies_internal {
namespace {

struct X {};
struct Y {};
struct A {};
struct B {
  using deps = void(A, X);
};
struct C {
  using deps = void(A, Y);
};
struct D {
  using deps = void(B, C);
};

// We don't actually want to check equality because Dependencies doesn't
// guarantee any ordering or repetition. Instead, we just want to check
// containment.
template <typename, typename>
struct Contains : std::false_type {};

template <typename Needle, typename... Haystack>
struct Contains<void(Haystack...), Needle>
    : std::integral_constant<bool, (std::is_same_v<Needle, Haystack> || ...)> {
};

template <typename T>
struct Inspect;

template <typename... Args>
struct Inspect<void (*)(Args...)> {
  template <typename T>
  static constexpr int Count() {
    return (std::is_same_v<T, Args> + ...);
  }

  static constexpr int Total() { return sizeof...(Args); }
};

TEST(Dependencies, Works) {
  testing::StaticAssertTypeEq<decltype(Dependencies<X>()), void (*)(X)>();

  EXPECT_EQ(Inspect<Dependencies<B>>::Count<A>(), 1);
  EXPECT_EQ(Inspect<Dependencies<B>>::Count<B>(), 1);
  EXPECT_EQ(Inspect<Dependencies<B>>::Count<C>(), 0);
  EXPECT_EQ(Inspect<Dependencies<B>>::Count<D>(), 0);
  EXPECT_EQ(Inspect<Dependencies<B>>::Count<X>(), 1);
  EXPECT_EQ(Inspect<Dependencies<B>>::Count<Y>(), 0);
  EXPECT_EQ(Inspect<Dependencies<B>>::Total(), 3);

  EXPECT_EQ(Inspect<Dependencies<D>>::Count<A>(), 1);
  EXPECT_EQ(Inspect<Dependencies<D>>::Count<B>(), 1);
  EXPECT_EQ(Inspect<Dependencies<D>>::Count<C>(), 1);
  EXPECT_EQ(Inspect<Dependencies<D>>::Count<D>(), 1);
  EXPECT_EQ(Inspect<Dependencies<D>>::Count<X>(), 1);
  EXPECT_EQ(Inspect<Dependencies<D>>::Count<Y>(), 1);
  EXPECT_EQ(Inspect<Dependencies<D>>::Total(), 6);
}

struct Recursion {
  using deps = void(Recursion);
};

template <bool B>
struct MutualRecursion {
  using deps = void(MutualRecursion<!B>);
};

TEST(Dependencies, Cycles) {
  EXPECT_EQ(Inspect<Dependencies<Recursion>>::Count<Recursion>(), 1);
  EXPECT_EQ(Inspect<Dependencies<Recursion>>::Total(), 1);

  EXPECT_EQ(Inspect<Dependencies<MutualRecursion<true>>>::Count<
                MutualRecursion<true>>(),
            1);
  EXPECT_EQ(Inspect<Dependencies<MutualRecursion<true>>>::Count<
                MutualRecursion<false>>(),
            1);
  EXPECT_EQ(Inspect<Dependencies<MutualRecursion<false>>>::Total(), 2);

  EXPECT_EQ(Inspect<Dependencies<MutualRecursion<false>>>::Count<
                MutualRecursion<true>>(),
            1);
  EXPECT_EQ(Inspect<Dependencies<MutualRecursion<false>>>::Count<
                MutualRecursion<false>>(),
            1);
  EXPECT_EQ(Inspect<Dependencies<MutualRecursion<true>>>::Total(), 2);
}

}  // namespace
}  // namespace dependencies_internal
ABSL_NAMESPACE_END
}  // namespace absl
