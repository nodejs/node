// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-maybe.h"

#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {
struct Movable {
  Movable() = default;

  Movable(const Movable&) = delete;
  Movable& operator=(const Movable&) = delete;

  Movable(Movable&&) V8_NOEXCEPT = default;
  Movable& operator=(Movable&&) V8_NOEXCEPT = default;
};
}  // namespace

TEST(MaybeTest, AllowMovableTypes) {
  Maybe<Movable> m1 = Just(Movable{});
  EXPECT_TRUE(m1.IsJust());

  Maybe<Movable> m2 = Just<Movable>({});
  EXPECT_TRUE(m2.IsJust());

  Maybe<Movable> m3 = Nothing<Movable>();
  EXPECT_TRUE(m3.IsNothing());

  Maybe<Movable> m4 = Just(Movable{});
  Movable mm = std::move(m4).FromJust();
  USE(mm);
}

}  // namespace internal
}  // namespace v8
