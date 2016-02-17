// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "src/objects.h"
#include "src/objects-inl.h"

#include "src/handles.h"
#include "src/handles-inl.h"

#include "src/heap/heap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

double Round(double x) {
  // Round to three digits.
  return floor(x * 1000 + 0.5) / 1000;
}


void CheckEqualRounded(double expected, double actual) {
  expected = Round(expected);
  actual = Round(actual);
  EXPECT_DOUBLE_EQ(expected, actual);
}


TEST(Heap, HeapGrowingFactor) {
  CheckEqualRounded(Heap::kMaxHeapGrowingFactor,
                    Heap::HeapGrowingFactor(34, 1));
  CheckEqualRounded(3.553, Heap::HeapGrowingFactor(45, 1));
  CheckEqualRounded(2.830, Heap::HeapGrowingFactor(50, 1));
  CheckEqualRounded(1.478, Heap::HeapGrowingFactor(100, 1));
  CheckEqualRounded(1.193, Heap::HeapGrowingFactor(200, 1));
  CheckEqualRounded(1.121, Heap::HeapGrowingFactor(300, 1));
  CheckEqualRounded(Heap::HeapGrowingFactor(300, 1),
                    Heap::HeapGrowingFactor(600, 2));
  CheckEqualRounded(Heap::kMinHeapGrowingFactor,
                    Heap::HeapGrowingFactor(400, 1));
}

}  // namespace internal
}  // namespace v8
