// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/aligned-slot-allocator.h"

#include "src/base/bits.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

class AlignedSlotAllocatorUnitTest : public ::testing::Test {
 public:
  AlignedSlotAllocatorUnitTest() = default;
  ~AlignedSlotAllocatorUnitTest() override = default;

  // Helper method to test AlignedSlotAllocator::Allocate.
  void Allocate(int size, int expected) {
    int next = allocator_.NextSlot(size);
    int result = allocator_.Allocate(size);
    EXPECT_EQ(next, result);  // NextSlot/Allocate are consistent.
    EXPECT_EQ(expected, result);
    EXPECT_EQ(0, result & (size - 1));  // result is aligned to size.
    int slot_end = result + static_cast<int>(base::bits::RoundUpToPowerOfTwo32(
                                static_cast<uint32_t>(size)));
    EXPECT_LE(slot_end, allocator_.Size());  // allocator Size is beyond slot.
  }

  // Helper method to test AlignedSlotAllocator::AllocateUnaligned.
  void AllocateUnaligned(int size, int expected, int expected1, int expected2,
                         int expected4) {
    int size_before = allocator_.Size();
    int result = allocator_.AllocateUnaligned(size);
    EXPECT_EQ(size_before, result);  // AllocateUnaligned/Size are consistent.
    EXPECT_EQ(expected, result);
    EXPECT_EQ(result + size, allocator_.Size());
    EXPECT_EQ(expected1, allocator_.NextSlot(1));
    EXPECT_EQ(expected2, allocator_.NextSlot(2));
    EXPECT_EQ(expected4, allocator_.NextSlot(4));
  }

  AlignedSlotAllocator allocator_;
};

TEST_F(AlignedSlotAllocatorUnitTest, NumSlotsForWidth) {
  constexpr int kSlotBytes = AlignedSlotAllocator::kSlotSize;
  for (int slot_size = 1; slot_size <= 4 * kSlotBytes; ++slot_size) {
    EXPECT_EQ(AlignedSlotAllocator::NumSlotsForWidth(slot_size),
              (slot_size + kSlotBytes - 1) / kSlotBytes);
  }
}

TEST_F(AlignedSlotAllocatorUnitTest, Allocate1) {
  Allocate(1, 0);
  EXPECT_EQ(2, allocator_.NextSlot(2));
  EXPECT_EQ(4, allocator_.NextSlot(4));

  Allocate(1, 1);
  EXPECT_EQ(2, allocator_.NextSlot(2));
  EXPECT_EQ(4, allocator_.NextSlot(4));

  Allocate(1, 2);
  EXPECT_EQ(4, allocator_.NextSlot(2));
  EXPECT_EQ(4, allocator_.NextSlot(4));

  Allocate(1, 3);
  EXPECT_EQ(4, allocator_.NextSlot(2));
  EXPECT_EQ(4, allocator_.NextSlot(4));

  // Make sure we use 1-fragments.
  Allocate(1, 4);
  Allocate(2, 6);
  Allocate(1, 5);

  // Make sure we use 2-fragments.
  Allocate(2, 8);
  Allocate(1, 10);
  Allocate(1, 11);
}

TEST_F(AlignedSlotAllocatorUnitTest, Allocate2) {
  Allocate(2, 0);
  EXPECT_EQ(2, allocator_.NextSlot(1));
  EXPECT_EQ(4, allocator_.NextSlot(4));

  Allocate(2, 2);
  EXPECT_EQ(4, allocator_.NextSlot(1));
  EXPECT_EQ(4, allocator_.NextSlot(4));

  // Make sure we use 2-fragments.
  Allocate(1, 4);
  Allocate(2, 6);
  Allocate(2, 8);
}

TEST_F(AlignedSlotAllocatorUnitTest, Allocate4) {
  Allocate(4, 0);
  EXPECT_EQ(4, allocator_.NextSlot(1));
  EXPECT_EQ(4, allocator_.NextSlot(2));

  Allocate(1, 4);
  Allocate(4, 8);

  Allocate(2, 6);
  Allocate(4, 12);
}

TEST_F(AlignedSlotAllocatorUnitTest, AllocateUnaligned) {
  AllocateUnaligned(1, 0, 1, 2, 4);
  AllocateUnaligned(1, 1, 2, 2, 4);

  Allocate(1, 2);

  AllocateUnaligned(2, 3, 5, 6, 8);

  // Advance to leave 1- and 2- fragments below Size.
  Allocate(4, 8);

  // AllocateUnaligned should allocate at the end, and clear fragments.
  AllocateUnaligned(0, 12, 12, 12, 12);
}

TEST_F(AlignedSlotAllocatorUnitTest, LargeAllocateUnaligned) {
  AllocateUnaligned(11, 0, 11, 12, 12);
  AllocateUnaligned(11, 11, 22, 22, 24);
  AllocateUnaligned(13, 22, 35, 36, 36);
}

TEST_F(AlignedSlotAllocatorUnitTest, Size) {
  allocator_.Allocate(1);
  EXPECT_EQ(1, allocator_.Size());
  // Allocate 2, leaving a fragment at 1. Size should be at 4.
  allocator_.Allocate(2);
  EXPECT_EQ(4, allocator_.Size());
  // Allocate should consume fragment.
  EXPECT_EQ(1, allocator_.Allocate(1));
  // Size should still be 4.
  EXPECT_EQ(4, allocator_.Size());
}

TEST_F(AlignedSlotAllocatorUnitTest, Align) {
  EXPECT_EQ(0, allocator_.Align(1));
  EXPECT_EQ(0, allocator_.Size());

  // Allocate 1 to become misaligned.
  Allocate(1, 0);

  // 4-align.
  EXPECT_EQ(3, allocator_.Align(4));
  EXPECT_EQ(4, allocator_.NextSlot(1));
  EXPECT_EQ(4, allocator_.NextSlot(2));
  EXPECT_EQ(4, allocator_.NextSlot(4));
  EXPECT_EQ(4, allocator_.Size());

  // Allocate 2 to become misaligned.
  Allocate(2, 4);

  // 4-align.
  EXPECT_EQ(2, allocator_.Align(4));
  EXPECT_EQ(8, allocator_.NextSlot(1));
  EXPECT_EQ(8, allocator_.NextSlot(2));
  EXPECT_EQ(8, allocator_.NextSlot(4));
  EXPECT_EQ(8, allocator_.Size());

  // No change when we're already aligned.
  EXPECT_EQ(0, allocator_.Align(2));
  EXPECT_EQ(8, allocator_.NextSlot(1));
  EXPECT_EQ(8, allocator_.NextSlot(2));
  EXPECT_EQ(8, allocator_.NextSlot(4));
  EXPECT_EQ(8, allocator_.Size());
}

}  // namespace internal
}  // namespace v8
