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
                    Heap::HeapGrowingFactor(34, 1, 4.0));
  CheckEqualRounded(3.553, Heap::HeapGrowingFactor(45, 1, 4.0));
  CheckEqualRounded(2.830, Heap::HeapGrowingFactor(50, 1, 4.0));
  CheckEqualRounded(1.478, Heap::HeapGrowingFactor(100, 1, 4.0));
  CheckEqualRounded(1.193, Heap::HeapGrowingFactor(200, 1, 4.0));
  CheckEqualRounded(1.121, Heap::HeapGrowingFactor(300, 1, 4.0));
  CheckEqualRounded(Heap::HeapGrowingFactor(300, 1, 4.0),
                    Heap::HeapGrowingFactor(600, 2, 4.0));
  CheckEqualRounded(Heap::kMinHeapGrowingFactor,
                    Heap::HeapGrowingFactor(400, 1, 4.0));
}

TEST(Heap, MaxHeapGrowingFactor) {
  CheckEqualRounded(
      1.3, Heap::MaxHeapGrowingFactor(Heap::kMinOldGenerationSize * MB));
  CheckEqualRounded(
      1.600, Heap::MaxHeapGrowingFactor(Heap::kMaxOldGenerationSize / 2 * MB));
  CheckEqualRounded(
      1.999,
      Heap::MaxHeapGrowingFactor(
          (Heap::kMaxOldGenerationSize - Heap::kPointerMultiplier) * MB));
  CheckEqualRounded(4.0,
                    Heap::MaxHeapGrowingFactor(
                        static_cast<size_t>(Heap::kMaxOldGenerationSize) * MB));
}

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

TEST(Heap, OldGenerationSize) {
  uint64_t configurations[][2] = {
      {0, i::Heap::kMinOldGenerationSize},
      {512, i::Heap::kMinOldGenerationSize},
      {1 * i::GB, 256 * i::Heap::kPointerMultiplier},
      {2 * static_cast<uint64_t>(i::GB), 512 * i::Heap::kPointerMultiplier},
      {4 * static_cast<uint64_t>(i::GB), i::Heap::kMaxOldGenerationSize},
      {8 * static_cast<uint64_t>(i::GB), i::Heap::kMaxOldGenerationSize}};

  for (auto configuration : configurations) {
    ASSERT_EQ(configuration[1],
              static_cast<uint64_t>(
                  i::Heap::ComputeMaxOldGenerationSize(configuration[0])));
  }
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
    EXPECT_TRUE(base::OS::GetRandomMmapAddr() == nullptr);
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

}  // namespace internal
}  // namespace v8
