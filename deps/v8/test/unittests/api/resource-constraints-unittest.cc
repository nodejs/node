// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-isolate.h"
#include "src/flags/flags.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

TEST(ResourceConstraints, ConfigureDefaultsFromHeapSizeSmall) {
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaultsFromHeapSize(1 * MB, 1 * MB);
  ASSERT_EQ(Heap::MinOldGenerationSize(),
            constraints.max_old_generation_size_in_bytes());
  ASSERT_EQ(Heap::MinYoungGenerationSize(),
            constraints.max_young_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_old_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_young_generation_size_in_bytes());
}

TEST(ResourceConstraints, ConfigureDefaultsFromHeapSizeLarge) {
  const uint64_t physical_memory = 0;
  const size_t heap_max_size = Heap::DefaultMaxHeapSize(physical_memory);
  v8::ResourceConstraints constraints;
  const size_t expected_young_gen_max_size =
      Heap::DefaultMaxSemiSpaceSize(physical_memory) *
      (internal::v8_flags.minor_ms ? 2 : 3);
  constraints.ConfigureDefaultsFromHeapSize(
      8u * MB, heap_max_size + expected_young_gen_max_size);
  // Check that for large heap sizes max semi space size is set to the maximum
  // supported capacity (i.e. 8MB with pointer compression and 16MB without;
  // MinorMS supports double capacity).
  ASSERT_EQ(expected_young_gen_max_size,
            constraints.max_young_generation_size_in_bytes());
  ASSERT_EQ(heap_max_size, constraints.max_old_generation_size_in_bytes());
  // Check that for small initial heap sizes initial semi space size is set to
  // the minimum supported capacity.
  ASSERT_EQ((internal::v8_flags.minor_ms ? 2 : 3) * 2u * MB,
            constraints.initial_young_generation_size_in_bytes());
  ASSERT_EQ(8u * MB - (internal::v8_flags.minor_ms ? 2 : 3) * 2u * MB,
            constraints.initial_old_generation_size_in_bytes());
}

TEST(ResourceConstraints, ConfigureDefaults) {
  const uint64_t physical_memory = 2u * GB;
  const size_t heap_max_size = Heap::DefaultMaxHeapSize(physical_memory);
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaults(2u * GB, 0u);
  ASSERT_EQ(heap_max_size / 2, constraints.max_old_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_old_generation_size_in_bytes());
  ASSERT_EQ(Heap::DefaultMaxSemiSpaceSize(physical_memory) / 2 *
                (internal::v8_flags.minor_ms ? (2 * 2) : 3),
            constraints.max_young_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_young_generation_size_in_bytes());
}

}  // namespace v8::internal
