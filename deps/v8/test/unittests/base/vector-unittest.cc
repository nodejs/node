// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/vector.h"

#include <algorithm>

#include "testing/gmock-support.h"

namespace v8 {
namespace base {

TEST(VectorTest, Factories) {
  auto vec = base::CStrVector("foo");
  EXPECT_EQ(3u, vec.size());
  EXPECT_EQ(0, memcmp(vec.begin(), "foo", 3));

  vec = base::ArrayVector("foo");
  EXPECT_EQ(4u, vec.size());
  EXPECT_EQ(0, memcmp(vec.begin(), "foo\0", 4));

  vec = base::CStrVector("foo\0\0");
  EXPECT_EQ(3u, vec.size());
  EXPECT_EQ(0, memcmp(vec.begin(), "foo", 3));

  vec = base::CStrVector("");
  EXPECT_EQ(0u, vec.size());

  vec = base::CStrVector("\0");
  EXPECT_EQ(0u, vec.size());
}

// Test operator== and operator!= on different Vector types.
TEST(VectorTest, Equals) {
  auto foo1 = base::CStrVector("foo");
  auto foo2 = base::ArrayVector("ffoo") + 1;
  EXPECT_EQ(4u, foo2.size());  // Includes trailing '\0'.
  foo2.Truncate(foo2.size() - 1);
  // This is a requirement for the test.
  EXPECT_NE(foo1.begin(), foo2.begin());
  EXPECT_EQ(foo1, foo2);

  // Compare base::Vector<char> against base::Vector<const char>.
  char arr1[] = {'a', 'b', 'c'};
  char arr2[] = {'a', 'b', 'c'};
  char arr3[] = {'a', 'b', 'd'};
  base::Vector<char> vec1_char = base::ArrayVector(arr1);
  base::Vector<const char> vec1_const_char = vec1_char;
  base::Vector<char> vec2_char = base::ArrayVector(arr2);
  base::Vector<char> vec3_char = base::ArrayVector(arr3);
  EXPECT_NE(vec1_char.begin(), vec2_char.begin());
  // Note: We directly call operator== and operator!= here (without EXPECT_EQ or
  // EXPECT_NE) to have full control over the arguments.
  EXPECT_TRUE(vec1_char == vec1_const_char);
  EXPECT_TRUE(vec1_char == vec2_char);
  EXPECT_TRUE(vec1_const_char == vec2_char);
  EXPECT_TRUE(vec1_const_char != vec3_char);
  EXPECT_TRUE(vec3_char != vec2_char);
  EXPECT_TRUE(vec3_char != vec1_const_char);
}

// Test that the constexpr factory methods work.
TEST(VectorTest, ConstexprFactories) {
  static constexpr int kInit1[] = {4, 11, 3};
  static constexpr auto kVec1 = base::ArrayVector(kInit1);
  static_assert(kVec1.size() == 3);
  EXPECT_THAT(kVec1, testing::ElementsAreArray(kInit1));

  static constexpr auto kVec2 = base::VectorOf(kInit1, 2);
  static_assert(kVec2.size() == 2);
  EXPECT_THAT(kVec2, testing::ElementsAre(4, 11));

  static constexpr const char kInit3[] = "foobar";
  static constexpr auto kVec3 = base::StaticCharVector(kInit3);
  static_assert(kVec3.size() == 6);
  EXPECT_THAT(kVec3, testing::ElementsAreArray(kInit3, kInit3 + 6));
}

TEST(VectorTest, SpanConversion) {
  int arr[] = {1, 2, 3};
  std::span<int, 3> static_span(arr);
  std::span<int> dynamic_span(arr);

  base::Vector<int> vec_from_static = static_span;
  base::Vector<int> vec_from_dynamic = dynamic_span;
  base::Vector<const int> const_vec_from_mutable_span = dynamic_span;
  EXPECT_EQ(vec_from_static, vec_from_dynamic);
  EXPECT_EQ(vec_from_dynamic, const_vec_from_mutable_span);

  std::span<int> span_from_vec = vec_from_dynamic;
  std::span<const int> const_span_from_vec = vec_from_dynamic;
  std::span<int> explicit_span_from_vec =
      static_cast<std::span<int>>(vec_from_dynamic);
  std::span<const int> explicit_const_span_from_vec =
      static_cast<std::span<const int>>(vec_from_dynamic);
  std::span s(vec_from_dynamic);
  static_assert(std::is_same_v<decltype(s), std::span<int>>);
  const base::Vector<int> const_vec_of_mutable = dynamic_span;
  std::span<int> span_from_const_vec = const_vec_of_mutable;
  EXPECT_EQ(span_from_vec.data(), arr);
  EXPECT_EQ(span_from_vec.size(), 3u);
  EXPECT_EQ(const_span_from_vec.data(), arr);
  EXPECT_EQ(const_span_from_vec.size(), 3u);
  EXPECT_EQ(explicit_span_from_vec.data(), arr);
  EXPECT_EQ(explicit_span_from_vec.size(), 3u);
  EXPECT_EQ(explicit_const_span_from_vec.data(), arr);
  EXPECT_EQ(explicit_const_span_from_vec.size(), 3u);
  EXPECT_EQ(s.data(), arr);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(span_from_const_vec.data(), arr);
  EXPECT_EQ(span_from_const_vec.size(), 3u);
}

TEST(VectorTest, SpanCompatibleMethods) {
  static constexpr int kArr[] = {10, 20, 30, 40, 50};
  static constexpr auto kVec = base::ArrayVector(kArr);

  static_assert(kVec.front() == 10);
  static_assert(kVec.back() == 50);

  static constexpr auto kFirst2 = kVec.first(2);
  static_assert(kFirst2.size() == 2);
  static_assert(kFirst2.front() == 10);
  static_assert(kFirst2.back() == 20);
  EXPECT_THAT(kFirst2, testing::ElementsAre(10, 20));
  EXPECT_EQ(kVec.first(5), kVec);
  EXPECT_TRUE(kVec.first(0).empty());

  static constexpr auto kLast2 = kVec.last(2);
  static_assert(kLast2.size() == 2);
  static_assert(kLast2.front() == 40);
  static_assert(kLast2.back() == 50);
  EXPECT_THAT(kLast2, testing::ElementsAre(40, 50));
  EXPECT_EQ(kVec.last(5), kVec);
  EXPECT_TRUE(kVec.last(0).empty());

  static constexpr auto kSubFrom2 = kVec.subspan(2);
  static_assert(kSubFrom2.size() == 3);
  static_assert(kSubFrom2.front() == 30);
  static_assert(kSubFrom2.back() == 50);
  EXPECT_THAT(kSubFrom2, testing::ElementsAre(30, 40, 50));
  EXPECT_EQ(kVec.subspan(0), kVec);
  EXPECT_TRUE(kVec.subspan(5).empty());

  static constexpr auto kSubMiddle = kVec.subspan(1, 3);
  static_assert(kSubMiddle.size() == 3);
  static_assert(kSubMiddle.front() == 20);
  static_assert(kSubMiddle.back() == 40);
  EXPECT_THAT(kSubMiddle, testing::ElementsAre(20, 30, 40));
  EXPECT_EQ(kVec.subspan(0, 5), kVec);
  EXPECT_TRUE(kVec.subspan(2, 0).empty());
  EXPECT_TRUE(kVec.subspan(5, 0).empty());
  EXPECT_TRUE(base::Vector<int>{}.subspan(0, 0).empty());

  int mutable_arr[] = {1, 2, 3};
  base::Vector<int> mutable_vec = base::ArrayVector(mutable_arr);
  mutable_vec.front() = 100;
  mutable_vec.back() = 300;
  EXPECT_THAT(mutable_vec, testing::ElementsAre(100, 2, 300));

  const base::Vector<int> const_mutable_vec = mutable_vec;
  const_mutable_vec.front() = 10;
  const_mutable_vec.back() = 30;
  EXPECT_THAT(mutable_vec, testing::ElementsAre(10, 2, 30));
}

TEST(VectorTest, ArrayConversion) {
  std::array<int, 3> arr = {1, 2, 3};
  const std::array<int, 3> const_arr = {1, 2, 3};

  base::Vector<int> vec_from_arr = arr;
  base::Vector<const int> const_vec_from_arr = arr;
  base::Vector<const int> const_vec_from_const_arr = const_arr;

  EXPECT_EQ(vec_from_arr.data(), arr.data());
  EXPECT_EQ(vec_from_arr.size(), 3u);
  EXPECT_EQ(const_vec_from_arr.data(), arr.data());
  EXPECT_EQ(const_vec_from_arr.size(), 3u);
  EXPECT_EQ(const_vec_from_const_arr.data(), const_arr.data());
  EXPECT_EQ(const_vec_from_const_arr.size(), 3u);
  EXPECT_EQ(vec_from_arr, const_vec_from_arr);
  EXPECT_EQ(const_vec_from_arr, const_vec_from_const_arr);
}

}  // namespace base
}  // namespace v8
