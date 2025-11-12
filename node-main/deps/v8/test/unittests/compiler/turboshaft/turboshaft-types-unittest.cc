// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/types.h"
#include "src/handles/handles.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal::compiler::turboshaft {

class TurboshaftTypesTest : public TestWithNativeContextAndZone {
 public:
  using Kind = Type::Kind;

  TurboshaftTypesTest() : TestWithNativeContextAndZone() {}
};

TEST_F(TurboshaftTypesTest, Word32) {
  const auto max_value = std::numeric_limits<Word32Type::word_t>::max();

  // Complete range
  {
    Word32Type t = Word32Type::Any();
    EXPECT_TRUE(Word32Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(800).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(max_value).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({0, 1}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({0, max_value}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({3, 9, max_value - 1}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(0, 10, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(800, 1200, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(1, max_value - 1, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(0, max_value, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(max_value - 20, 20, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(1000, 999, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (non-wrapping)
  {
    Word32Type t = Word32Type::Range(100, 300, zone());
    EXPECT_TRUE(!Word32Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(99).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(100).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(250).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(300).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(301).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({0, 150}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({99, 100}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({100, 105}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({150, 200, 250}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({150, 300}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({300, 301}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(50, 150, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(99, 150, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(100, 150, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(150, 250, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(250, 300, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(250, 301, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(99, 301, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(800, 9000, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word32Type::Range(max_value - 100, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(250, 200, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (wrapping)
  {
    const auto large_value = max_value - 1000;
    Word32Type t = Word32Type::Range(large_value, 800, zone());
    EXPECT_TRUE(Word32Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(800).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(801).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(5000).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(large_value - 1).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(large_value).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(large_value + 5).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(max_value).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({0, 800}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({0, 801}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({0, 600, 900}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({0, max_value}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({100, max_value - 100}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({large_value - 1, large_value + 5}, zone())
                     .IsSubtypeOf(t));
    EXPECT_TRUE(
        Word32Type::Set({large_value, large_value + 5, max_value - 5}, zone())
            .IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(0, 800, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(100, 300, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(0, 801, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word32Type::Range(200, max_value - 200, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word32Type::Range(large_value - 1, max_value, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        Word32Type::Range(large_value, max_value, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(large_value + 100, max_value - 100, zone())
                    .IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Range(large_value, 800, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        Word32Type::Range(large_value + 100, 700, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word32Type::Range(large_value - 1, 799, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word32Type::Range(large_value + 1, 801, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(5000, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Set
  {
    CHECK_GT(Word32Type::kMaxSetSize, 2);
    Word32Type t = Word32Type::Set({4, 890}, zone());
    EXPECT_TRUE(!Word32Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(3).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(4).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(5).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Constant(889).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Constant(890).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({0, 4}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({4, 90}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word32Type::Set({4, 890}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({0, 4, 890}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({4, 890, 1000}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Set({890, max_value}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(0, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(4, 890, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(800, 900, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(800, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(890, 4, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Range(max_value - 5, 4, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word32Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }
}

TEST_F(TurboshaftTypesTest, Word64) {
  const auto max_value = std::numeric_limits<Word64Type::word_t>::max();

  // Complete range
  {
    Word64Type t = Word64Type::Any();
    EXPECT_TRUE(Word64Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(800).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(max_value).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({0, 1}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({0, max_value}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({3, 9, max_value - 1}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(0, 10, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(800, 1200, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(1, max_value - 1, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(0, max_value, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(max_value - 20, 20, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(1000, 999, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (non-wrapping)
  {
    Word64Type t = Word64Type::Range(100, 300, zone());
    EXPECT_TRUE(!Word64Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(99).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(100).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(250).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(300).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(301).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({0, 150}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({99, 100}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({100, 105}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({150, 200, 250}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({150, 300}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({300, 301}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(50, 150, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(99, 150, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(100, 150, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(150, 250, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(250, 300, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(250, 301, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(99, 301, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(800, 9000, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word64Type::Range(max_value - 100, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(250, 200, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (wrapping)
  {
    const auto large_value = max_value - 1000;
    Word64Type t = Word64Type::Range(large_value, 800, zone());
    EXPECT_TRUE(Word64Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(800).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(801).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(5000).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(large_value - 1).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(large_value).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(large_value + 5).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(max_value).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({0, 800}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({0, 801}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({0, 600, 900}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({0, max_value}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({100, max_value - 100}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({large_value - 1, large_value + 5}, zone())
                     .IsSubtypeOf(t));
    EXPECT_TRUE(
        Word64Type::Set({large_value, large_value + 5, max_value - 5}, zone())
            .IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(0, 800, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(100, 300, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(0, 801, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word64Type::Range(200, max_value - 200, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word64Type::Range(large_value - 1, max_value, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        Word64Type::Range(large_value, max_value, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(large_value + 100, max_value - 100, zone())
                    .IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Range(large_value, 800, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        Word64Type::Range(large_value + 100, 700, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word64Type::Range(large_value - 1, 799, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Word64Type::Range(large_value + 1, 801, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(5000, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Set
  {
    CHECK_GT(Word64Type::kMaxSetSize, 2);
    Word64Type t = Word64Type::Set({4, 890}, zone());
    EXPECT_TRUE(!Word64Type::Constant(0).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(3).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(4).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(5).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Constant(889).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Constant(890).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({0, 4}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({4, 90}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Word64Type::Set({4, 890}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({0, 4, 890}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({4, 890, 1000}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Set({890, max_value}, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(0, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(4, 890, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(800, 900, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(800, 100, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(890, 4, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Range(max_value - 5, 4, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Word64Type::Any().IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }
}

TEST_F(TurboshaftTypesTest, Float32) {
  const auto large_value =
      std::numeric_limits<Float32Type::float_t>::max() * 0.99f;
  const auto inf = std::numeric_limits<Float32Type::float_t>::infinity();
  const auto kNaN = Float32Type::kNaN;
  const auto kMinusZero = Float32Type::kMinusZero;
  const auto kNoSpecialValues = Float32Type::kNoSpecialValues;

  // Complete range (with NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float32Type t = Float32Type::Any(kNaN | kMinusZero);
    EXPECT_TRUE(Float32Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(0.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(391.113f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Set({0.13f, 91.0f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        Float32Type::Set({-100.4f, large_value}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Set({-inf, inf}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Range(0.0f, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Range(-inf, 12.3f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Range(-inf, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Complete range (without NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float32Type t = Float32Type::Any(kMinusZero);
    EXPECT_TRUE(!Float32Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(0.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(391.113f).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Set({0.13f, 91.0f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(
        !with_nan,
        Float32Type::Set({-100.4f, large_value}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Set({-inf, inf}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Range(0.0f, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Range(-inf, 12.3f, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Range(-inf, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan, Float32Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (with NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float32Type t =
        Float32Type::Range(-1.0f, 3.14159f, kNaN | kMinusZero, zone());
    EXPECT_TRUE(Float32Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-100.0f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-1.01f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(-1.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(-0.99f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(0.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(3.14159f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(3.15f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Set({-0.5f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-1.1f, 1.5f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Set({-0.9f, 1.88f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({0.0f, 3.142f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-inf, 0.3f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-inf, 0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-1.01f, 0.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Range(-1.0f, 1.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Range(0.0f, 3.14159f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(0.0f, 3.142f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(3.0f, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (without NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float32Type t = Float32Type::Range(-1.0f, 3.14159f, kMinusZero, zone());
    EXPECT_TRUE(!Float32Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-100.0f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-1.01f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(-1.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(-0.99f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(0.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(3.14159f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(3.15f).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan, Float32Type::Set({-0.5f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-1.1f, 1.5f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Set({-0.9f, 1.88f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({0.0f, 3.142f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-inf, 0.3f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-inf, 0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-1.01f, 0.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Range(-1.0f, 1.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Range(0.0f, 3.14159f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(0.0f, 3.142f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(3.0f, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Set (with NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = with_nan ? kNaN : kNoSpecialValues;
    Float32Type t = Float32Type::Set({-1.0f, 3.14159f}, kNaN, zone());
    EXPECT_TRUE(Float32Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-100.0f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-1.01f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(-1.0f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(1.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(3.14159f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(3.1415f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(inf).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-inf, 0.0f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-1.0f, 0.0f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Set({-1.0f, 3.14159f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Float32Type::Set({3.14159f, 3.1416f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-inf, -1.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-1.01f, -1.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Float32Type::Range(-1.0f, 3.14159f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(3.14159f, 4.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Set (without NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = with_nan ? kNaN : kNoSpecialValues;
    Float32Type t =
        Float32Type::Set({-1.0f, 3.14159f}, kNoSpecialValues, zone());
    EXPECT_TRUE(!Float32Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-100.0f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(-1.01f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(-1.0f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(1.0f).IsSubtypeOf(t));
    EXPECT_TRUE(Float32Type::Constant(3.14159f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(3.1415f).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Constant(inf).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-inf, 0.0f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Set({-1.0f, 0.0f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float32Type::Set({-1.0f, 3.14159f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Float32Type::Set({3.14159f, 3.1416f}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-inf, -1.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(-1.01f, -1.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Float32Type::Range(-1.0f, 3.14159f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Range(3.14159f, 4.0f, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float32Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // -0.0f corner cases
  {
    EXPECT_TRUE(!Float32Type::MinusZero().IsSubtypeOf(
        Float32Type::Set({0.0f, 1.0f}, zone())));
    EXPECT_TRUE(
        !Float32Type::Constant(0.0f).IsSubtypeOf(Float32Type::MinusZero()));
    EXPECT_TRUE(
        Float32Type::Set({3.2f}, kMinusZero, zone())
            .IsSubtypeOf(Float32Type::Range(0.0f, 4.0f, kMinusZero, zone())));
    EXPECT_TRUE(!Float32Type::Set({-1.0f, 0.0f}, kMinusZero, zone())
                     .IsSubtypeOf(Float32Type::Range(-inf, 0.0f, zone())));
  }
}

TEST_F(TurboshaftTypesTest, Float64) {
  const auto large_value =
      std::numeric_limits<Float64Type::float_t>::max() * 0.99;
  const auto inf = std::numeric_limits<Float64Type::float_t>::infinity();
  const auto kNaN = Float64Type::kNaN;
  const auto kMinusZero = Float64Type::kMinusZero;
  const auto kNoSpecialValues = Float64Type::kNoSpecialValues;

  // Complete range (with NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float64Type t = Float64Type::Any(kNaN | kMinusZero);
    EXPECT_TRUE(Float64Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(0.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(391.113).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Set({0.13, 91.0}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        Float64Type::Set({-100.4, large_value}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Set({-inf, inf}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Range(0.0, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Range(-inf, 12.3, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Range(-inf, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Complete range (without NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float64Type t = Float64Type::Any(kMinusZero);
    EXPECT_TRUE(!Float64Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(0.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(391.113).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Set({0.13, 91.0}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(
        !with_nan,
        Float64Type::Set({-100.4, large_value}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Set({-inf, inf}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Range(0.0, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Range(-inf, 12.3, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Range(-inf, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan, Float64Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (with NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float64Type t =
        Float64Type::Range(-1.0, 3.14159, kNaN | kMinusZero, zone());
    EXPECT_TRUE(Float64Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-100.0).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-1.01).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(-1.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(-0.99).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(0.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(3.14159).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(3.15).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Set({-0.5}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-1.1, 1.5}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Set({-0.9, 1.88}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({0.0, 3.142}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-inf, 0.3}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-inf, 0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-1.01, 0.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Range(-1.0, 1.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Range(0.0, 3.14159, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(0.0, 3.142, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(3.0, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Range (without NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = kMinusZero | (with_nan ? kNaN : kNoSpecialValues);
    Float64Type t = Float64Type::Range(-1.0, 3.14159, kMinusZero, zone());
    EXPECT_TRUE(!Float64Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-100.0).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-1.01).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(-1.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(-0.99).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::MinusZero().IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(0.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(3.14159).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(3.15).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan, Float64Type::Set({-0.5}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-1.1, 1.5}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Set({-0.9, 1.88}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({0.0, 3.142}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-inf, 0.3}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-inf, 0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-1.01, 0.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Range(-1.0, 1.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Range(0.0, 3.14159, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(0.0, 3.142, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(3.0, inf, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Set (with NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = with_nan ? kNaN : kNoSpecialValues;
    Float64Type t = Float64Type::Set({-1.0, 3.14159}, kNaN, zone());
    EXPECT_TRUE(Float64Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-100.0).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-1.01).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(-1.0).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(1.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(3.14159).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(3.1415).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(inf).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-inf, 0.0}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-1.0, 0.0}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Set({-1.0, 3.14159}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Float64Type::Set({3.14159, 3.1416}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-inf, -1.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-1.01, -1.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-1.0, 3.14159, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(3.14159, 4.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // Set (without NaN)
  for (bool with_nan : {false, true}) {
    uint32_t sv = with_nan ? kNaN : kNoSpecialValues;
    Float64Type t = Float64Type::Set({-1.0, 3.14159}, kNoSpecialValues, zone());
    EXPECT_TRUE(!Float64Type::NaN().IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-100.0).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(-1.01).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(-1.0).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(1.0).IsSubtypeOf(t));
    EXPECT_TRUE(Float64Type::Constant(3.14159).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(3.1415).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Constant(inf).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-inf, 0.0}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Set({-1.0, 0.0}, sv, zone()).IsSubtypeOf(t));
    EXPECT_EQ(!with_nan,
              Float64Type::Set({-1.0, 3.14159}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(
        !Float64Type::Set({3.14159, 3.1416}, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-inf, -1.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-1.01, -1.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(-1.0, 3.14159, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Range(3.14159, 4.0, sv, zone()).IsSubtypeOf(t));
    EXPECT_TRUE(!Float64Type::Any(sv).IsSubtypeOf(t));
    EXPECT_TRUE(t.IsSubtypeOf(t));
  }

  // -0.0 corner cases
  {
    EXPECT_TRUE(!Float64Type::MinusZero().IsSubtypeOf(
        Float64Type::Set({0.0, 1.0}, zone())));
    EXPECT_TRUE(
        !Float64Type::Constant(0.0).IsSubtypeOf(Float64Type::MinusZero()));
    EXPECT_TRUE(
        Float64Type::Set({3.2}, kMinusZero, zone())
            .IsSubtypeOf(Float64Type::Range(0.0, 4.0, kMinusZero, zone())));
    EXPECT_TRUE(
        Float64Type::Set({0.0}, kMinusZero, zone())
            .IsSubtypeOf(Float64Type::Range(-inf, 0.0, kMinusZero, zone())));
  }
}

TEST_F(TurboshaftTypesTest, Word32LeastUpperBound) {
  auto CheckLubIs = [&](const Word32Type& lhs, const Word32Type& rhs,
                        const Word32Type& expected) {
    EXPECT_TRUE(
        expected.IsSubtypeOf(Word32Type::LeastUpperBound(lhs, rhs, zone())));
  };

  {
    const auto lhs = Word32Type::Range(100, 400, zone());
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Word32Type::Range(50, 350, zone()),
               Word32Type::Range(50, 400, zone()));
    CheckLubIs(lhs, Word32Type::Range(150, 600, zone()),
               Word32Type::Range(100, 600, zone()));
    CheckLubIs(lhs, Word32Type::Range(150, 350, zone()), lhs);
    CheckLubIs(lhs, Word32Type::Range(350, 0, zone()),
               Word32Type::Range(100, 0, zone()));
    CheckLubIs(lhs, Word32Type::Range(400, 100, zone()), Word32Type::Any());
    CheckLubIs(lhs, Word32Type::Range(600, 0, zone()),
               Word32Type::Range(600, 400, zone()));
    CheckLubIs(lhs, Word32Type::Range(300, 150, zone()), Word32Type::Any());
  }

  {
    const auto lhs = Word32Type::Constant(18);
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Word32Type::Constant(1119),
               Word32Type::Set({18, 1119}, zone()));
    CheckLubIs(lhs, Word32Type::Constant(0), Word32Type::Set({0, 18}, zone()));
    CheckLubIs(lhs, Word32Type::Range(40, 100, zone()),
               Word32Type::Range(18, 100, zone()));
    CheckLubIs(lhs, Word32Type::Range(4, 90, zone()),
               Word32Type::Range(4, 90, zone()));
    CheckLubIs(lhs, Word32Type::Set({0, 1, 2, 3}, zone()),
               Word32Type::Set({0, 1, 2, 3, 18}, zone()));
    CheckLubIs(
        lhs, Word32Type::Constant(std::numeric_limits<uint32_t>::max()),
        Word32Type::Set({18, std::numeric_limits<uint32_t>::max()}, zone()));
  }
}

TEST_F(TurboshaftTypesTest, Word64LeastUpperBound) {
  auto CheckLubIs = [&](const Word64Type& lhs, const Word64Type& rhs,
                        const Word64Type& expected) {
    EXPECT_TRUE(
        expected.IsSubtypeOf(Word64Type::LeastUpperBound(lhs, rhs, zone())));
  };

  {
    const auto lhs = Word64Type::Range(100, 400, zone());
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Word64Type::Range(50, 350, zone()),
               Word64Type::Range(50, 400, zone()));
    CheckLubIs(lhs, Word64Type::Range(150, 600, zone()),
               Word64Type::Range(100, 600, zone()));
    CheckLubIs(lhs, Word64Type::Range(150, 350, zone()), lhs);
    CheckLubIs(lhs, Word64Type::Range(350, 0, zone()),
               Word64Type::Range(100, 0, zone()));
    CheckLubIs(lhs, Word64Type::Range(400, 100, zone()), Word64Type::Any());
    CheckLubIs(lhs, Word64Type::Range(600, 0, zone()),
               Word64Type::Range(600, 400, zone()));
    CheckLubIs(lhs, Word64Type::Range(300, 150, zone()), Word64Type::Any());
  }

  {
    const auto lhs = Word64Type::Constant(18);
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Word64Type::Constant(1119),
               Word64Type::Set({18, 1119}, zone()));
    CheckLubIs(lhs, Word64Type::Constant(0), Word64Type::Set({0, 18}, zone()));
    CheckLubIs(lhs, Word64Type::Range(40, 100, zone()),
               Word64Type::Range(18, 100, zone()));
    CheckLubIs(lhs, Word64Type::Range(4, 90, zone()),
               Word64Type::Range(4, 90, zone()));
    CheckLubIs(lhs, Word64Type::Range(0, 3, zone()),
               Word64Type::Set({0, 1, 2, 3, 18}, zone()));
    CheckLubIs(
        lhs, Word64Type::Constant(std::numeric_limits<uint64_t>::max()),
        Word64Type::Set({18, std::numeric_limits<uint64_t>::max()}, zone()));
  }
}

TEST_F(TurboshaftTypesTest, Float32LeastUpperBound) {
  auto CheckLubIs = [&](const Float32Type& lhs, const Float32Type& rhs,
                        const Float32Type& expected) {
    EXPECT_TRUE(
        expected.IsSubtypeOf(Float32Type::LeastUpperBound(lhs, rhs, zone())));
  };
  const auto kNaN = Float32Type::kNaN;

  {
    const auto lhs = Float32Type::Range(-32.19f, 94.07f, zone());
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Float32Type::Range(-32.19f, 94.07f, kNaN, zone()),
               Float32Type::Range(-32.19f, 94.07f, kNaN, zone()));
    CheckLubIs(lhs, Float32Type::NaN(),
               Float32Type::Range(-32.19f, 94.07f, kNaN, zone()));
    CheckLubIs(lhs, Float32Type::Constant(0.0f), lhs);
    CheckLubIs(lhs, Float32Type::Range(-19.9f, 31.29f, zone()), lhs);
    CheckLubIs(lhs, Float32Type::Range(-91.22f, -40.0f, zone()),
               Float32Type::Range(-91.22f, 94.07f, zone()));
    CheckLubIs(lhs, Float32Type::Range(0.0f, 1993.0f, zone()),
               Float32Type::Range(-32.19f, 1993.0f, zone()));
    CheckLubIs(lhs, Float32Type::Range(-100.0f, 100.0f, kNaN, zone()),
               Float32Type::Range(-100.0f, 100.0f, kNaN, zone()));
  }

  {
    const auto lhs = Float32Type::Constant(-0.04f);
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Float32Type::NaN(),
               Float32Type::Set({-0.04f}, kNaN, zone()));
    CheckLubIs(lhs, Float32Type::Constant(17.14f),
               Float32Type::Set({-0.04f, 17.14f}, zone()));
    CheckLubIs(lhs, Float32Type::Range(-75.4f, -12.7f, zone()),
               Float32Type::Range(-75.4f, -0.04f, zone()));
    CheckLubIs(lhs, Float32Type::Set({0.04f}, kNaN, zone()),
               Float32Type::Set({-0.04f, 0.04f}, kNaN, zone()));
  }
}

TEST_F(TurboshaftTypesTest, Float64LeastUpperBound) {
  auto CheckLubIs = [&](const Float64Type& lhs, const Float64Type& rhs,
                        const Float64Type& expected) {
    EXPECT_TRUE(
        expected.IsSubtypeOf(Float64Type::LeastUpperBound(lhs, rhs, zone())));
  };
  const auto kNaN = Float64Type::kNaN;

  {
    const auto lhs = Float64Type::Range(-32.19, 94.07, zone());
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Float64Type::Range(-32.19, 94.07, kNaN, zone()),
               Float64Type::Range(-32.19, 94.07, kNaN, zone()));
    CheckLubIs(lhs, Float64Type::NaN(),
               Float64Type::Range(-32.19, 94.07, kNaN, zone()));
    CheckLubIs(lhs, Float64Type::Constant(0.0), lhs);
    CheckLubIs(lhs, Float64Type::Range(-19.9, 31.29, zone()), lhs);
    CheckLubIs(lhs, Float64Type::Range(-91.22, -40.0, zone()),
               Float64Type::Range(-91.22, 94.07, zone()));
    CheckLubIs(lhs, Float64Type::Range(0.0, 1993.0, zone()),
               Float64Type::Range(-32.19, 1993.0, zone()));
    CheckLubIs(lhs, Float64Type::Range(-100.0, 100.0, kNaN, zone()),
               Float64Type::Range(-100.0, 100.0, kNaN, zone()));
  }

  {
    const auto lhs = Float64Type::Constant(-0.04);
    CheckLubIs(lhs, lhs, lhs);
    CheckLubIs(lhs, Float64Type::NaN(),
               Float64Type::Set({-0.04}, kNaN, zone()));
    CheckLubIs(lhs, Float64Type::Constant(17.14),
               Float64Type::Set({-0.04, 17.14}, zone()));
    CheckLubIs(lhs, Float64Type::Range(-75.4, -12.7, zone()),
               Float64Type::Range(-75.4, -0.04, zone()));
    CheckLubIs(lhs, Float64Type::Set({0.04}, kNaN, zone()),
               Float64Type::Set({-0.04, 0.04}, kNaN, zone()));
  }
}

}  // namespace v8::internal::compiler::turboshaft
