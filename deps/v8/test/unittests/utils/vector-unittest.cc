// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/utils/utils.h"
#include "testing/gmock-support.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

TEST(VectorTest, Factories) {
  auto vec = CStrVector("foo");
  EXPECT_EQ(3u, vec.size());
  EXPECT_EQ(0, memcmp(vec.begin(), "foo", 3));

  vec = ArrayVector("foo");
  EXPECT_EQ(4u, vec.size());
  EXPECT_EQ(0, memcmp(vec.begin(), "foo\0", 4));

  vec = CStrVector("foo\0\0");
  EXPECT_EQ(3u, vec.size());
  EXPECT_EQ(0, memcmp(vec.begin(), "foo", 3));

  vec = CStrVector("");
  EXPECT_EQ(0u, vec.size());

  vec = CStrVector("\0");
  EXPECT_EQ(0u, vec.size());
}

// Test operator== and operator!= on different Vector types.
TEST(VectorTest, Equals) {
  auto foo1 = CStrVector("foo");
  auto foo2 = ArrayVector("ffoo") + 1;
  EXPECT_EQ(4u, foo2.size());  // Includes trailing '\0'.
  foo2.Truncate(foo2.size() - 1);
  // This is a requirement for the test.
  EXPECT_NE(foo1.begin(), foo2.begin());
  EXPECT_EQ(foo1, foo2);

  // Compare Vector<char> against Vector<const char>.
  char arr1[] = {'a', 'b', 'c'};
  char arr2[] = {'a', 'b', 'c'};
  char arr3[] = {'a', 'b', 'd'};
  Vector<char> vec1_char = ArrayVector(arr1);
  Vector<const char> vec1_const_char = vec1_char;
  Vector<char> vec2_char = ArrayVector(arr2);
  Vector<char> vec3_char = ArrayVector(arr3);
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

TEST(OwnedVectorConstruction, Equals) {
  auto int_vec = OwnedVector<int>::New(4);
  EXPECT_EQ(4u, int_vec.size());
  auto find_non_zero = [](int i) { return i != 0; };
  EXPECT_EQ(int_vec.end(),
            std::find_if(int_vec.begin(), int_vec.end(), find_non_zero));

  constexpr int kInit[] = {4, 11, 3};
  auto init_vec1 = OwnedVector<int>::Of(kInit);
  // Note: {const int} should also work: We initialize the owned vector, but
  // afterwards it's non-modifyable.
  auto init_vec2 = OwnedVector<const int>::Of(ArrayVector(kInit));
  EXPECT_EQ(init_vec1.as_vector(), ArrayVector(kInit));
  EXPECT_EQ(init_vec1.as_vector(), init_vec2.as_vector());
}

// Test that the constexpr factory methods work.
TEST(VectorTest, ConstexprFactories) {
  static constexpr int kInit1[] = {4, 11, 3};
  static constexpr auto kVec1 = ArrayVector(kInit1);
  STATIC_ASSERT(kVec1.size() == 3);
  EXPECT_THAT(kVec1, testing::ElementsAreArray(kInit1));

  static constexpr auto kVec2 = VectorOf(kInit1, 2);
  STATIC_ASSERT(kVec2.size() == 2);
  EXPECT_THAT(kVec2, testing::ElementsAre(4, 11));

  static constexpr const char kInit3[] = "foobar";
  static constexpr auto kVec3 = StaticCharVector(kInit3);
  STATIC_ASSERT(kVec3.size() == 6);
  EXPECT_THAT(kVec3, testing::ElementsAreArray(kInit3, kInit3 + 6));
}

}  // namespace internal
}  // namespace v8
