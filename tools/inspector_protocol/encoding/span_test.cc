// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "span.h"

#include "gtest/gtest.h"

namespace inspector_protocol {
template <typename T>
class SpanTest : public ::testing::Test {};

using TestTypes = ::testing::Types<uint8_t, uint16_t>;
TYPED_TEST_CASE(SpanTest, TestTypes);

TYPED_TEST(SpanTest, Empty) {
  span<TypeParam> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(0, empty.size());
  EXPECT_EQ(0, empty.size_bytes());
  EXPECT_EQ(empty.begin(), empty.end());
}

TYPED_TEST(SpanTest, SingleItem) {
  TypeParam single_item = 42;
  span<TypeParam> singular(&single_item, 1);
  EXPECT_FALSE(singular.empty());
  EXPECT_EQ(1, singular.size());
  EXPECT_EQ(sizeof(TypeParam), static_cast<size_t>(singular.size_bytes()));
  EXPECT_EQ(singular.begin() + 1, singular.end());
  EXPECT_EQ(42, singular[0]);
}

TYPED_TEST(SpanTest, FiveItems) {
  std::vector<TypeParam> test_input = {31, 32, 33, 34, 35};
  span<TypeParam> five_items(test_input.data(), 5);
  EXPECT_FALSE(five_items.empty());
  EXPECT_EQ(5, five_items.size());
  EXPECT_EQ(sizeof(TypeParam) * 5,
            static_cast<size_t>(five_items.size_bytes()));
  EXPECT_EQ(five_items.begin() + 5, five_items.end());
  EXPECT_EQ(31, five_items[0]);
  EXPECT_EQ(32, five_items[1]);
  EXPECT_EQ(33, five_items[2]);
  EXPECT_EQ(34, five_items[3]);
  EXPECT_EQ(35, five_items[4]);
  span<TypeParam> three_items = five_items.subspan(2);
  EXPECT_EQ(3, three_items.size());
  EXPECT_EQ(33, three_items[0]);
  EXPECT_EQ(34, three_items[1]);
  EXPECT_EQ(35, three_items[2]);
  span<TypeParam> two_items = five_items.subspan(2, 2);
  EXPECT_EQ(2, two_items.size());
  EXPECT_EQ(33, two_items[0]);
  EXPECT_EQ(34, two_items[1]);
}
}  // namespace inspector_protocol
