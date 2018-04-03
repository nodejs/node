// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

template <typename T>
class UtilsTest : public ::testing::Test {};

typedef ::testing::Types<signed char, unsigned char,
                         short,                    // NOLINT(runtime/int)
                         unsigned short,           // NOLINT(runtime/int)
                         int, unsigned int, long,  // NOLINT(runtime/int)
                         unsigned long,            // NOLINT(runtime/int)
                         long long,                // NOLINT(runtime/int)
                         unsigned long long,       // NOLINT(runtime/int)
                         int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                         int64_t, uint64_t>
    IntegerTypes;

TYPED_TEST_CASE(UtilsTest, IntegerTypes);

TYPED_TEST(UtilsTest, SaturateSub) {
  TypeParam min = std::numeric_limits<TypeParam>::min();
  TypeParam max = std::numeric_limits<TypeParam>::max();
  EXPECT_EQ(SaturateSub<TypeParam>(min, 0), min);
  EXPECT_EQ(SaturateSub<TypeParam>(max, 0), max);
  EXPECT_EQ(SaturateSub<TypeParam>(max, min), max);
  EXPECT_EQ(SaturateSub<TypeParam>(min, max), min);
  EXPECT_EQ(SaturateSub<TypeParam>(min, max / 3), min);
  EXPECT_EQ(SaturateSub<TypeParam>(min + 1, 2), min);
  if (std::numeric_limits<TypeParam>::is_signed) {
    EXPECT_EQ(SaturateSub<TypeParam>(min, min), static_cast<TypeParam>(0));
    EXPECT_EQ(SaturateSub<TypeParam>(0, min), max);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 3, min), max);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 5, min), max);
    EXPECT_EQ(SaturateSub<TypeParam>(min / 3, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(min / 9, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max, min / 3), max);
    EXPECT_EQ(SaturateSub<TypeParam>(min, max / 3), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 3 * 2, min / 2), max);
    EXPECT_EQ(SaturateSub<TypeParam>(min / 3 * 2, max / 2), min);
  } else {
    EXPECT_EQ(SaturateSub<TypeParam>(min, min), min);
    EXPECT_EQ(SaturateSub<TypeParam>(0, min), min);
    EXPECT_EQ(SaturateSub<TypeParam>(0, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 3, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max - 3, max), min);
  }
  TypeParam test_cases[] = {static_cast<TypeParam>(min / 23),
                            static_cast<TypeParam>(max / 3),
                            63,
                            static_cast<TypeParam>(min / 6),
                            static_cast<TypeParam>(max / 55),
                            static_cast<TypeParam>(min / 2),
                            static_cast<TypeParam>(max / 2),
                            0,
                            1,
                            2,
                            3,
                            4,
                            42};
  TRACED_FOREACH(TypeParam, x, test_cases) {
    TRACED_FOREACH(TypeParam, y, test_cases) {
      if (std::numeric_limits<TypeParam>::is_signed) {
        EXPECT_EQ(SaturateSub<TypeParam>(x, y), x - y);
      } else {
        EXPECT_EQ(SaturateSub<TypeParam>(x, y), y > x ? min : x - y);
      }
    }
  }
}

TYPED_TEST(UtilsTest, SaturateAdd) {
  TypeParam min = std::numeric_limits<TypeParam>::min();
  TypeParam max = std::numeric_limits<TypeParam>::max();
  EXPECT_EQ(SaturateAdd<TypeParam>(min, min), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max, max), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(min, min / 3), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max / 8 * 7, max / 3 * 2), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(min / 3 * 2, min / 8 * 7), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max / 20 * 18, max / 25 * 18), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(min / 3 * 2, min / 3 * 2), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max - 1, 2), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(max - 100, 101), max);
  TypeParam test_cases[] = {static_cast<TypeParam>(min / 23),
                            static_cast<TypeParam>(max / 3),
                            63,
                            static_cast<TypeParam>(min / 6),
                            static_cast<TypeParam>(max / 55),
                            static_cast<TypeParam>(min / 2),
                            static_cast<TypeParam>(max / 2),
                            0,
                            1,
                            2,
                            3,
                            4,
                            42};
  TRACED_FOREACH(TypeParam, x, test_cases) {
    TRACED_FOREACH(TypeParam, y, test_cases) {
      EXPECT_EQ(SaturateAdd<TypeParam>(x, y), x + y);
    }
  }
}

}  // namespace internal
}  // namespace v8
