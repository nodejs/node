// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>
#include <unordered_map>

#include "span.h"
#include "test_platform.h"

namespace v8_crdtp {
// =============================================================================
// span - sequence of bytes
// =============================================================================

template <typename T>
class SpanTest : public ::testing::Test {};

using TestTypes = ::testing::Types<uint8_t, uint16_t>;
TYPED_TEST_SUITE(SpanTest, TestTypes);

TYPED_TEST(SpanTest, Empty) {
  span<TypeParam> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(0u, empty.size());
  EXPECT_EQ(0u, empty.size_bytes());
  EXPECT_EQ(empty.begin(), empty.end());
}

TYPED_TEST(SpanTest, SingleItem) {
  TypeParam single_item = 42;
  span<TypeParam> singular(&single_item, 1);
  EXPECT_FALSE(singular.empty());
  EXPECT_EQ(1u, singular.size());
  EXPECT_EQ(sizeof(TypeParam), singular.size_bytes());
  EXPECT_EQ(singular.begin() + 1, singular.end());
  EXPECT_EQ(42, singular[0]);
}

TYPED_TEST(SpanTest, FiveItems) {
  std::vector<TypeParam> test_input = {31, 32, 33, 34, 35};
  span<TypeParam> five_items(test_input.data(), 5);
  EXPECT_FALSE(five_items.empty());
  EXPECT_EQ(5u, five_items.size());
  EXPECT_EQ(sizeof(TypeParam) * 5, five_items.size_bytes());
  EXPECT_EQ(five_items.begin() + 5, five_items.end());
  EXPECT_EQ(31, five_items[0]);
  EXPECT_EQ(32, five_items[1]);
  EXPECT_EQ(33, five_items[2]);
  EXPECT_EQ(34, five_items[3]);
  EXPECT_EQ(35, five_items[4]);
  span<TypeParam> three_items = five_items.subspan(2);
  EXPECT_EQ(3u, three_items.size());
  EXPECT_EQ(33, three_items[0]);
  EXPECT_EQ(34, three_items[1]);
  EXPECT_EQ(35, three_items[2]);
  span<TypeParam> two_items = five_items.subspan(2, 2);
  EXPECT_EQ(2u, two_items.size());
  EXPECT_EQ(33, two_items[0]);
  EXPECT_EQ(34, two_items[1]);
}

TEST(SpanFromTest, FromConstCharAndLiteral) {
  // Testing this is useful because strlen(nullptr) is undefined.
  EXPECT_EQ(nullptr, SpanFrom(nullptr).data());
  EXPECT_EQ(0u, SpanFrom(nullptr).size());

  const char* kEmpty = "";
  EXPECT_EQ(kEmpty, reinterpret_cast<const char*>(SpanFrom(kEmpty).data()));
  EXPECT_EQ(0u, SpanFrom(kEmpty).size());

  const char* kFoo = "foo";
  EXPECT_EQ(kFoo, reinterpret_cast<const char*>(SpanFrom(kFoo).data()));
  EXPECT_EQ(3u, SpanFrom(kFoo).size());

  EXPECT_EQ(3u, SpanFrom("foo").size());
}

TEST(SpanFromTest, FromVectorUint8AndUint16) {
  std::vector<uint8_t> foo = {'f', 'o', 'o'};
  span<uint8_t> foo_span = SpanFrom(foo);
  EXPECT_EQ(foo.size(), foo_span.size());

  std::vector<uint16_t> bar = {0xff, 0xef, 0xeb};
  span<uint16_t> bar_span = SpanFrom(bar);
  EXPECT_EQ(bar.size(), bar_span.size());
}

TEST(SpanComparisons, ByteWiseLexicographicalOrder) {
  // Compare the empty span.
  EXPECT_FALSE(SpanLessThan(span<uint8_t>(), span<uint8_t>()));
  EXPECT_TRUE(SpanEquals(span<uint8_t>(), span<uint8_t>()));

  // Compare message with itself.
  std::string msg = "Hello, world";
  EXPECT_FALSE(SpanLessThan(SpanFrom(msg), SpanFrom(msg)));
  EXPECT_TRUE(SpanEquals(SpanFrom(msg), SpanFrom(msg)));

  // Compare message and copy.
  EXPECT_FALSE(SpanLessThan(SpanFrom(msg), SpanFrom(std::string(msg))));
  EXPECT_TRUE(SpanEquals(SpanFrom(msg), SpanFrom(std::string(msg))));

  // Compare two messages. |lesser_msg| < |msg| because of the first
  // byte ('A' < 'H').
  std::string lesser_msg = "A lesser message.";
  EXPECT_TRUE(SpanLessThan(SpanFrom(lesser_msg), SpanFrom(msg)));
  EXPECT_FALSE(SpanLessThan(SpanFrom(msg), SpanFrom(lesser_msg)));
  EXPECT_FALSE(SpanEquals(SpanFrom(msg), SpanFrom(lesser_msg)));
}

// TODO(johannes): The following shows how the span can be used in an
// std::unordered_map as a key. Once we have a production usage, we'll move
// SpanHash, SpanEq, SpanHasher into the header.

// A simple hash code, inspired by http://stackoverflow.com/q/1646807.
constexpr inline size_t SpanHash(span<uint8_t> s) noexcept {
  size_t hash = 17;
  for (uint8_t c : s)
    hash = 31 * hash + c;
  return hash;
}

// Structs for making std::unordered_map with std::span<uint8_t> keys.
struct SpanEq {
  constexpr inline bool operator()(span<uint8_t> l, span<uint8_t> r) const {
    return SpanEquals(l, r);
  }
};

struct SpanHasher {
  constexpr inline size_t operator()(span<uint8_t> s) const {
    return SpanHash(s);
  }
};

TEST(SpanHasherAndSpanEq, SpanAsKeyInUnorderedMap) {
  // A very simple smoke test for unordered_map, storing three key/value pairs.
  std::unordered_map<span<uint8_t>, int32_t, SpanHasher, SpanEq> a_map;
  a_map[SpanFrom("foo")] = 1;
  a_map[SpanFrom("bar")] = 2;
  a_map[SpanFrom("baz")] = 3;
  EXPECT_EQ(3u, a_map.size());
  EXPECT_EQ(1, a_map[SpanFrom("foo")]);
  EXPECT_EQ(2, a_map[SpanFrom("bar")]);
  EXPECT_EQ(3, a_map[SpanFrom("baz")]);
}
}  // namespace v8_crdtp
