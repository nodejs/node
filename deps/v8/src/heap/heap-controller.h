// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_CONTROLLER_H_
#define V8_HEAP_HEAP_CONTROLLER_H_

#include <cstddef>
#include "src/allocation.h"
#include "src/heap/heap.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE MemoryController {
 public:
  MemoryController(Heap* heap, double min_growing_factor,
                   double max_growing_factor,
                   double conservative_growing_factor,
                   double target_mutator_utilization, size_t min_size,
                   size_t max_size)
      : heap_(heap),
        kMinGrowingFactor(min_growing_factor),
        kMaxGrowingFactor(max_growing_factor),
        kConservativeGrowingFactor(conservative_growing_factor),
        kTargetMutatorUtilization(target_mutator_utilization),
        kMinSize(min_size),
        kMaxSize(max_size) {}
  virtual ~MemoryController() {}

  // Computes the allocation limit to trigger the next garbage collection.
  size_t CalculateAllocationLimit(size_t curr_size, size_t max_size,
                                  double gc_speed, double mutator_speed,
                                  size_t new_space_capacity,
                                  Heap::HeapGrowingMode growing_mode);

  // Computes the growing step when the limit increases.
  size_t MinimumAllocationLimitGrowingStep(Heap::HeapGrowingMode growing_mode);

 protected:
  double GrowingFactor(double gc_speed, double mutator_speed,
                       double max_factor);
  double MaxGrowingFactor(size_t curr_max_size);
  virtual const char* ControllerName() = 0;

  Heap* const heap_;

  const double kMinGrowingFactor;
  const double kMaxGrowingFactor;
  const double kConservativeGrowingFactor;
  const double kTargetMutatorUtilization;
  // Sizes are in MB.
  const size_t kMinSize;
  const size_t kMaxSize;

  FRIEND_TEST(HeapControllerTest, HeapGrowingFactor);
  FRIEND_TEST(HeapControllerTest, MaxHeapGrowingFactor);
  FRIEND_TEST(HeapControllerTest, MaxOldGenerationSize);
  FRIEND_TEST(HeapControllerTest, OldGenerationAllocationLimit);
};

class HeapController : public MemoryController {
 public:
  explicit HeapController(Heap* heap)
      : MemoryController(heap, 1.1, 4.0, 1.3, 0.97, kMinHeapSize,
                         kMaxHeapSize) {}

  // Sizes are in MB.
  static const size_t kMinHeapSize = 128 * Heap::kPointerMultiplier;
  static const size_t kMaxHeapSize = 1024 * Heap::kPointerMultiplier;

 protected:
  const char* ControllerName() { return "HeapController"; }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_CONTROLLER_H_
