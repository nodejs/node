// Copyright 2025 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/casts.h"

#include <type_traits>
#include <utility>

#include "gtest/gtest.h"

namespace {

struct Base {
  explicit Base(int value) : x(value) {}
  Base(const Base& other) = delete;
  Base& operator=(const Base& other) = delete;
  int x;
};
struct Derived : Base {
  explicit Derived(int value) : Base(value) {}
};

static_assert(
    std::is_same_v<
        decltype(absl::implicit_cast<Base&>(std::declval<Derived&>())), Base&>);
static_assert(std::is_same_v<decltype(absl::implicit_cast<const Base&>(
                                 std::declval<Derived>())),
                             const Base&>);

TEST(ImplicitCastTest, LValueReference) {
  Derived derived(5);
  EXPECT_EQ(&absl::implicit_cast<Base&>(derived), &derived);
  EXPECT_EQ(&absl::implicit_cast<const Base&>(derived), &derived);
}

TEST(ImplicitCastTest, RValueReference) {
  Derived derived(5);
  Base&& base = absl::implicit_cast<Base&&>(std::move(derived));
  EXPECT_EQ(&base, &derived);

  const Derived cderived(6);
  const Base&& cbase = absl::implicit_cast<const Base&&>(std::move(cderived));
  EXPECT_EQ(&cbase, &cderived);
}

}  // namespace
