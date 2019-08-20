// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/iterator.h"

#include <deque>

#include "test/unittests/test-utils.h"

namespace v8 {
namespace base {

TEST(IteratorTest, IteratorRangeEmpty) {
  base::iterator_range<char*> r;
  EXPECT_EQ(r.begin(), r.end());
  EXPECT_EQ(r.end(), r.cend());
  EXPECT_EQ(r.begin(), r.cbegin());
  EXPECT_TRUE(r.empty());
  EXPECT_EQ(0, r.size());
}


TEST(IteratorTest, IteratorRangeArray) {
  int array[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  base::iterator_range<int*> r1(&array[0], &array[10]);
  for (auto i : r1) {
    EXPECT_EQ(array[i], i);
  }
  EXPECT_EQ(10, r1.size());
  EXPECT_FALSE(r1.empty());
  for (size_t i = 0; i < arraysize(array); ++i) {
    EXPECT_EQ(r1[i], array[i]);
  }
  base::iterator_range<int*> r2(&array[0], &array[0]);
  EXPECT_EQ(0, r2.size());
  EXPECT_TRUE(r2.empty());
  for (auto i : array) {
    EXPECT_EQ(r2.end(), std::find(r2.begin(), r2.end(), i));
  }
}


TEST(IteratorTest, IteratorRangeDeque) {
  using C = std::deque<int>;
  C c;
  c.push_back(1);
  c.push_back(2);
  c.push_back(2);
  base::iterator_range<typename C::iterator> r(c.begin(), c.end());
  EXPECT_EQ(3, r.size());
  EXPECT_FALSE(r.empty());
  EXPECT_TRUE(c.begin() == r.begin());
  EXPECT_TRUE(c.end() == r.end());
  EXPECT_EQ(0, std::count(r.begin(), r.end(), 0));
  EXPECT_EQ(1, std::count(r.begin(), r.end(), 1));
  EXPECT_EQ(2, std::count(r.begin(), r.end(), 2));
}

}  // namespace base
}  // namespace v8
