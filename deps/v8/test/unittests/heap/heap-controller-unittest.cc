// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"

#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"

#include "src/heap/heap-controller.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using MemoryControllerTest = TestWithIsolate;

double Round(double x) {
  // Round to three digits.
  return floor(x * 1000 + 0.5) / 1000;
}

void CheckEqualRounded(double expected, double actual) {
  expected = Round(expected);
  actual = Round(actual);
  EXPECT_DOUBLE_EQ(expected, actual);
}

namespace {

using V8Controller = MemoryController<V8HeapTrait>;

}  // namespace

TEST_F(MemoryControllerTest, HeapGrowingFactor) {
  CheckEqualRounded(V8HeapTrait::kMaxGrowingFactor,
                    V8Controller::DynamicGrowingFactor(34, 1, 4.0));
  CheckEqualRounded(3.553, V8Controller::DynamicGrowingFactor(45, 1, 4.0));
  CheckEqualRounded(2.830, V8Controller::DynamicGrowingFactor(50, 1, 4.0));
  CheckEqualRounded(1.478, V8Controller::DynamicGrowingFactor(100, 1, 4.0));
  CheckEqualRounded(1.193, V8Controller::DynamicGrowingFactor(200, 1, 4.0));
  CheckEqualRounded(1.121, V8Controller::DynamicGrowingFactor(300, 1, 4.0));
  CheckEqualRounded(V8Controller::DynamicGrowingFactor(300, 1, 4.0),
                    V8Controller::DynamicGrowingFactor(600, 2, 4.0));
  CheckEqualRounded(V8HeapTrait::kMinGrowingFactor,
                    V8Controller::DynamicGrowingFactor(400, 1, 4.0));
}

TEST_F(MemoryControllerTest, MaxHeapGrowingFactor) {
  CheckEqualRounded(1.3,
                    V8Controller::MaxGrowingFactor(V8HeapTrait::kMinSize * MB));
  CheckEqualRounded(
      1.600, V8Controller::MaxGrowingFactor(V8HeapTrait::kMaxSize / 2 * MB));
  CheckEqualRounded(
      1.999, V8Controller::MaxGrowingFactor(
                 (V8HeapTrait::kMaxSize - Heap::kPointerMultiplier) * MB));
  CheckEqualRounded(4.0, V8Controller::MaxGrowingFactor(
                             static_cast<size_t>(V8HeapTrait::kMaxSize) * MB));
}

TEST_F(MemoryControllerTest, OldGenerationAllocationLimit) {
  Heap* heap = i_isolate()->heap();
  size_t old_gen_size = 128 * MB;
  size_t max_old_generation_size = 512 * MB;
  double gc_speed = 100;
  double mutator_speed = 1;
  size_t new_space_capacity = 16 * MB;

  double factor = V8Controller::GrowingFactor(heap, max_old_generation_size,
                                              gc_speed, mutator_speed);

  EXPECT_EQ(static_cast<size_t>(old_gen_size * factor + new_space_capacity),
            V8Controller::CalculateAllocationLimit(
                heap, old_gen_size, max_old_generation_size, new_space_capacity,
                factor, Heap::HeapGrowingMode::kDefault));

  factor = Min(factor, V8HeapTrait::kConservativeGrowingFactor);
  EXPECT_EQ(static_cast<size_t>(old_gen_size * factor + new_space_capacity),
            V8Controller::CalculateAllocationLimit(
                heap, old_gen_size, max_old_generation_size, new_space_capacity,
                factor, Heap::HeapGrowingMode::kSlow));

  factor = Min(factor, V8HeapTrait::kConservativeGrowingFactor);
  EXPECT_EQ(static_cast<size_t>(old_gen_size * factor + new_space_capacity),
            V8Controller::CalculateAllocationLimit(
                heap, old_gen_size, max_old_generation_size, new_space_capacity,
                factor, Heap::HeapGrowingMode::kConservative));

  factor = V8HeapTrait::kMinGrowingFactor;
  EXPECT_EQ(static_cast<size_t>(old_gen_size * factor + new_space_capacity),
            V8Controller::CalculateAllocationLimit(
                heap, old_gen_size, max_old_generation_size, new_space_capacity,
                factor, Heap::HeapGrowingMode::kMinimal));
}

TEST_F(MemoryControllerTest, MaxOldGenerationSize) {
  uint64_t configurations[][2] = {
      {0, V8HeapTrait::kMinSize},
      {512, V8HeapTrait::kMinSize},
      {1 * GB, 256 * Heap::kPointerMultiplier},
      {2 * static_cast<uint64_t>(GB), 512 * Heap::kPointerMultiplier},
      {4 * static_cast<uint64_t>(GB), V8HeapTrait::kMaxSize},
      {8 * static_cast<uint64_t>(GB), V8HeapTrait::kMaxSize}};

  for (auto configuration : configurations) {
    ASSERT_EQ(configuration[1],
              static_cast<uint64_t>(
                  Heap::ComputeMaxOldGenerationSize(configuration[0])));
  }
}

}  // namespace internal
}  // namespace v8
