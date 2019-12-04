// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/heap/heap.h"

namespace v8 {

TEST(ResourceConstraints, ConfigureDefaultsFromHeapSizeSmall) {
  const size_t KB = static_cast<size_t>(i::KB);
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t pm = i::Heap::kPointerMultiplier;
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaultsFromHeapSize(1 * MB, 1 * MB);
  ASSERT_EQ(i::Heap::MinOldGenerationSize(),
            constraints.max_old_generation_size_in_bytes());
  ASSERT_EQ(3 * 512 * pm * KB,
            constraints.max_young_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_old_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_young_generation_size_in_bytes());
}

TEST(ResourceConstraints, ConfigureDefaultsFromHeapSizeLarge) {
  const size_t KB = static_cast<size_t>(i::KB);
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t pm = i::Heap::kPointerMultiplier;
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaultsFromHeapSize(100u * MB, 3000u * MB);
  ASSERT_EQ(3000u * MB - 3 * 8192 * pm * KB,
            constraints.max_old_generation_size_in_bytes());
  ASSERT_EQ(3 * 8192 * pm * KB,
            constraints.max_young_generation_size_in_bytes());
  ASSERT_EQ(100u * MB - 3 * 512 * pm * KB,
            constraints.initial_old_generation_size_in_bytes());
  ASSERT_EQ(3 * 512 * pm * KB,
            constraints.initial_young_generation_size_in_bytes());
}

TEST(ResourceConstraints, ConfigureDefaults) {
  const size_t KB = static_cast<size_t>(i::KB);
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t pm = i::Heap::kPointerMultiplier;
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaults(2048u * MB, 0u);
  ASSERT_EQ(512u * pm * MB, constraints.max_old_generation_size_in_bytes());
  ASSERT_EQ(3 * 4096 * pm * KB,
            constraints.max_young_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_old_generation_size_in_bytes());
  ASSERT_EQ(0u, constraints.initial_young_generation_size_in_bytes());
}

}  // namespace v8
