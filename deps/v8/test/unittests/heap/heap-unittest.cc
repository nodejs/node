// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/objects.h"
#include "src/objects-inl.h"

#include "src/handles.h"
#include "src/handles-inl.h"

#include "src/heap/heap.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

typedef TestWithIsolate HeapTest;

TEST(Heap, SemiSpaceSize) {
  const size_t KB = static_cast<size_t>(i::KB);
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t pm = i::Heap::kPointerMultiplier;
  ASSERT_EQ(1u * pm * MB / 2, i::Heap::ComputeMaxSemiSpaceSize(0u) * KB);
  ASSERT_EQ(1u * pm * MB / 2, i::Heap::ComputeMaxSemiSpaceSize(512u * MB) * KB);
  ASSERT_EQ(2u * pm * MB, i::Heap::ComputeMaxSemiSpaceSize(1024u * MB) * KB);
  ASSERT_EQ(5u * pm * MB, i::Heap::ComputeMaxSemiSpaceSize(2024u * MB) * KB);
  ASSERT_EQ(8u * pm * MB, i::Heap::ComputeMaxSemiSpaceSize(4095u * MB) * KB);
}

TEST_F(HeapTest, ASLR) {
#if V8_TARGET_ARCH_X64
#if V8_OS_MACOSX
  Heap* heap = i_isolate()->heap();
  std::set<void*> hints;
  for (int i = 0; i < 1000; i++) {
    hints.insert(heap->GetRandomMmapAddr());
  }
  if (hints.size() == 1) {
    EXPECT_TRUE((*hints.begin()) == nullptr);
    EXPECT_TRUE(i::GetRandomMmapAddr() == nullptr);
  } else {
    // It is unlikely that 1000 random samples will collide to less then 500
    // values.
    EXPECT_GT(hints.size(), 500u);
    const uintptr_t kRegionMask = 0xFFFFFFFFu;
    void* first = *hints.begin();
    for (void* hint : hints) {
      uintptr_t diff = reinterpret_cast<uintptr_t>(first) ^
                       reinterpret_cast<uintptr_t>(hint);
      EXPECT_LE(diff, kRegionMask);
    }
  }
#endif  // V8_OS_MACOSX
#endif  // V8_TARGET_ARCH_X64
}

TEST_F(HeapTest, ExternalLimitDefault) {
  Heap* heap = i_isolate()->heap();
  EXPECT_EQ(kExternalAllocationSoftLimit, heap->external_memory_limit_);
}

TEST_F(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling) {
  v8_isolate()->AdjustAmountOfExternalAllocatedMemory(+10 * MB);
  v8_isolate()->AdjustAmountOfExternalAllocatedMemory(-10 * MB);
  Heap* heap = i_isolate()->heap();
  EXPECT_GE(heap->external_memory_limit_, kExternalAllocationSoftLimit);
}

}  // namespace internal
}  // namespace v8
