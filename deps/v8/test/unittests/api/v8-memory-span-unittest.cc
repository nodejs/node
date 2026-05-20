// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-memory-span.h"

#include <vector>

#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(MemorySpanTest, Simple) {
  std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  v8::MemorySpan<int> span(v.begin(), v.end());
  int count = 0;
  int sum = 0;
  for (int i : span) {
    ++count;
    sum += i;
  }
  EXPECT_EQ(10, count);
  EXPECT_EQ(55, sum);
}

TEST(MemorySpanTest, Empty) {
  std::vector<int> empty;

  v8::MemorySpan<int> span(empty.begin(), empty.end());
  int count = 0;
  for (int i : span) {
    ++count;
    USE(i);
  }
  EXPECT_EQ(0, count);
}

TEST(MemorySpanTest, Reserved) {
  std::vector<int> ready;
  ready.reserve(10);

  v8::MemorySpan<int> span(ready.begin(), ready.end());
  int count = 0;
  for (int i : span) {
    ++count;
    USE(i);
  }
  EXPECT_EQ(0, count);
}

}  // namespace internal
}  // namespace v8
