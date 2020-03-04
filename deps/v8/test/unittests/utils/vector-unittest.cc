// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/utils.h"
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
  CHECK_EQ(4, foo2.size());  // Includes trailing '\0'.
  foo2.Truncate(foo2.size() - 1);
  // This is a requirement for the test.
  CHECK_NE(foo1.begin(), foo2.begin());
  CHECK_EQ(foo1, foo2);

  // Compare Vector<char> against Vector<const char>.
  char arr1[] = {'a', 'b', 'c'};
  char arr2[] = {'a', 'b', 'c'};
  char arr3[] = {'a', 'b', 'd'};
  Vector<char> vec1_char = ArrayVector(arr1);
  Vector<const char> vec1_const_char = vec1_char;
  Vector<char> vec2_char = ArrayVector(arr2);
  Vector<char> vec3_char = ArrayVector(arr3);
  CHECK_NE(vec1_char.begin(), vec2_char.begin());
  // Note: We directly call operator== and operator!= here (without CHECK_EQ or
  // CHECK_NE) to have full control over the arguments.
  CHECK(vec1_char == vec1_const_char);
  CHECK(vec1_char == vec2_char);
  CHECK(vec1_const_char == vec2_char);
  CHECK(vec1_const_char != vec3_char);
  CHECK(vec3_char != vec2_char);
  CHECK(vec3_char != vec1_const_char);
}

}  // namespace internal
}  // namespace v8
