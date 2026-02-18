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

#include "absl/extend/internal/num_bases.h"

#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace aggregate_internal {
namespace {

using ::testing::Eq;
using ::testing::Optional;

TEST(NumBases, NoBases) {
  struct S {
    int i;
  };
  EXPECT_THAT(NumBases<S>(), Optional(0));

  struct Empty {};
  // We can't deal with empty classes right now.
  EXPECT_THAT(NumBases<Empty>(), Eq(std::nullopt));
}

TEST(NumBases, OneBase) {
  struct EmptyBase {};
  struct NonEmptyBase {
    int i;
  };

  // We can't deal with empty classes right now.
  struct EmptyEmpty : EmptyBase {};
  EXPECT_THAT(NumBases<EmptyEmpty>(), Eq(std::nullopt));

  struct EmptyNonEmpty : NonEmptyBase {};
  EXPECT_THAT(NumBases<EmptyNonEmpty>(), Optional(1));

  struct NonEmptyEmpty : EmptyBase {
    int i;
  };
  EXPECT_THAT(NumBases<NonEmptyEmpty>(), Optional(1));

  struct NonEmptyNonEmpty : NonEmptyBase {
    int j;
  };
  EXPECT_THAT(NumBases<NonEmptyNonEmpty>(), Optional(1));
}

TEST(NumBases, TwoBases) {
  struct Base1 {};
  struct Base2 {};
  struct S : Base1, Base2 {
    int i;
  };

  EXPECT_THAT(NumBases<S>(), Eq(std::nullopt));
}

TEST(NumBases, OneBaseAndField) {
  // Special case for 1 base where we fail to detect it.
  // The first member also happens to be a base.

  struct Base {};
  struct S : Base {
    Base member;
  };

  EXPECT_THAT(NumBases<S>(), Eq(std::nullopt));
}

}  // namespace
}  // namespace aggregate_internal
ABSL_NAMESPACE_END
}  // namespace absl
