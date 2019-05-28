// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/objects-inl.h"
#include "src/objects.h"

#include "src/handles-inl.h"
#include "src/handles.h"

#include "src/heap/heap-controller.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using HeapControllerTest = TestWithIsolate;

double Round(double x) {
  // Round to three digits.
  return floor(x * 1000 + 0.5) / 1000;
}

void CheckEqualRounded(double expected, double actual) {
  expected = Round(expected);
  actual = Round(actual);
  EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(HeapControllerTest, HeapGrowingFactor) {
  HeapController heap_controller(i_isolate()->heap());
  double min_factor = heap_controller.min_growing_factor_;
  double max_factor = heap_controller.max_growing_factor_;

  CheckEqualRounded(max_factor, heap_controller.GrowingFactor(34, 1, 4.0));
  CheckEqualRounded(3.553, heap_controller.GrowingFactor(45, 1, 4.0));
  CheckEqualRounded(2.830, heap_controller.GrowingFactor(50, 1, 4.0));
  CheckEqualRounded(1.478, heap_controller.GrowingFactor(100, 1, 4.0));
  CheckEqualRounded(1.193, heap_controller.GrowingFactor(200, 1, 4.0));
  CheckEqualRounded(1.121, heap_controller.GrowingFactor(300, 1, 4.0));
  CheckEqualRounded(heap_controller.GrowingFactor(300, 1, 4.0),
                    heap_controller.GrowingFactor(600, 2, 4.0));
  CheckEqualRounded(min_factor, heap_controller.GrowingFactor(400, 1, 4.0));
}

TEST_F(HeapControllerTest, MaxHeapGrowingFactor) {
  HeapController heap_controller(i_isolate()->heap());
  CheckEqualRounded(
      1.3, heap_controller.MaxGrowingFactor(HeapController::kMinSize * MB));
  CheckEqualRounded(1.600, heap_controller.MaxGrowingFactor(
                               HeapController::kMaxSize / 2 * MB));
  CheckEqualRounded(
      1.999, heap_controller.MaxGrowingFactor(
                 (HeapController::kMaxSize - Heap::kPointerMultiplier) * MB));
  CheckEqualRounded(4.0,
                    heap_controller.MaxGrowingFactor(
                        static_cast<size_t>(HeapController::kMaxSize) * MB));
}

TEST_F(HeapControllerTest, OldGenerationAllocationLimit) {
  Heap* heap = i_isolate()->heap();
  HeapController heap_controller(heap);
  size_t old_gen_size = 128 * MB;
  size_t max_old_generation_size = 512 * MB;
  double gc_speed = 100;
  double mutator_speed = 1;
  size_t new_space_capacity = 16 * MB;

  double max_factor = heap_controller.MaxGrowingFactor(max_old_generation_size);
  double factor =
      heap_controller.GrowingFactor(gc_speed, mutator_speed, max_factor);

  EXPECT_EQ(
      static_cast<size_t>(old_gen_size * factor + new_space_capacity),
      heap->heap_controller()->CalculateAllocationLimit(
          old_gen_size, max_old_generation_size, max_factor, gc_speed,
          mutator_speed, new_space_capacity, Heap::HeapGrowingMode::kDefault));

  factor = Min(factor, heap_controller.conservative_growing_factor_);
  EXPECT_EQ(
      static_cast<size_t>(old_gen_size * factor + new_space_capacity),
      heap->heap_controller()->CalculateAllocationLimit(
          old_gen_size, max_old_generation_size, max_factor, gc_speed,
          mutator_speed, new_space_capacity, Heap::HeapGrowingMode::kSlow));

  factor = Min(factor, heap_controller.conservative_growing_factor_);
  EXPECT_EQ(static_cast<size_t>(old_gen_size * factor + new_space_capacity),
            heap->heap_controller()->CalculateAllocationLimit(
                old_gen_size, max_old_generation_size, max_factor, gc_speed,
                mutator_speed, new_space_capacity,
                Heap::HeapGrowingMode::kConservative));

  factor = heap_controller.min_growing_factor_;
  EXPECT_EQ(
      static_cast<size_t>(old_gen_size * factor + new_space_capacity),
      heap->heap_controller()->CalculateAllocationLimit(
          old_gen_size, max_old_generation_size, max_factor, gc_speed,
          mutator_speed, new_space_capacity, Heap::HeapGrowingMode::kMinimal));
}

TEST_F(HeapControllerTest, MaxOldGenerationSize) {
  HeapController heap_controller(i_isolate()->heap());
  uint64_t configurations[][2] = {
      {0, HeapController::kMinSize},
      {512, HeapController::kMinSize},
      {1 * GB, 256 * Heap::kPointerMultiplier},
      {2 * static_cast<uint64_t>(GB), 512 * Heap::kPointerMultiplier},
      {4 * static_cast<uint64_t>(GB), HeapController::kMaxSize},
      {8 * static_cast<uint64_t>(GB), HeapController::kMaxSize}};

  for (auto configuration : configurations) {
    ASSERT_EQ(configuration[1],
              static_cast<uint64_t>(
                  Heap::ComputeMaxOldGenerationSize(configuration[0])));
  }
}

}  // namespace internal
}  // namespace v8
