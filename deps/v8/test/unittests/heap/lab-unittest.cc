// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-internal.h"
#include "src/heap/heap.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/spaces-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace heap {

namespace {

Address AllocateLabBackingStore(Heap* heap, size_t size_in_bytes) {
  AllocationResult result = heap->old_space()->AllocateRawAligned(
      static_cast<int>(size_in_bytes), kDoubleAligned);
  Address adr = result.ToObjectChecked().address();
  return adr;
}

bool AllocateFromLab(Heap* heap, LocalAllocationBuffer* lab,
                     size_t size_in_bytes,
                     AllocationAlignment alignment = kTaggedAligned) {
  HeapObject obj;
  AllocationResult result =
      lab->AllocateRawAligned(static_cast<int>(size_in_bytes), alignment);
  if (result.To(&obj)) {
    heap->CreateFillerObjectAt(obj.address(), static_cast<int>(size_in_bytes),
                               ClearRecordedSlots::kNo);
    return true;
  }
  return false;
}

void VerifyIterable(Address base, Address limit,
                    std::vector<size_t> expected_size) {
  EXPECT_LE(base, limit);
  HeapObject object;
  size_t counter = 0;
  while (base < limit) {
    object = HeapObject::FromAddress(base);
    EXPECT_TRUE(object.IsFreeSpaceOrFiller());
    EXPECT_LT(counter, expected_size.size());
    EXPECT_EQ(expected_size[counter], static_cast<size_t>(object.Size()));
    base += object.Size();
    counter++;
  }
}

}  // namespace

using LabTest = TestWithIsolate;

TEST_F(LabTest, InvalidLab) {
  LocalAllocationBuffer lab = LocalAllocationBuffer::InvalidBuffer();
  EXPECT_FALSE(lab.IsValid());
}

TEST_F(LabTest, UnusedLabImplicitClose) {
  Heap* heap = isolate()->heap();
  const size_t kLabSize = 4 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  size_t expected_sizes_raw[1] = {kLabSize};
  std::vector<size_t> expected_sizes(expected_sizes_raw,
                                     expected_sizes_raw + 1);
  {
    AllocationResult lab_backing_store =
        AllocationResult::FromObject(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    EXPECT_TRUE(lab.IsValid());
  }
  VerifyIterable(base, limit, expected_sizes);
}

TEST_F(LabTest, SimpleAllocate) {
  Heap* heap = isolate()->heap();
  const size_t kLabSize = 4 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  size_t sizes_raw[1] = {128};
  size_t expected_sizes_raw[2] = {128, kLabSize - 128};
  std::vector<size_t> sizes(sizes_raw, sizes_raw + 1);
  std::vector<size_t> expected_sizes(expected_sizes_raw,
                                     expected_sizes_raw + 2);
  {
    AllocationResult lab_backing_store =
        AllocationResult::FromObject(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    EXPECT_TRUE(lab.IsValid());
    for (auto size : sizes) {
      AllocateFromLab(heap, &lab, size);
    }
  }
  VerifyIterable(base, limit, expected_sizes);
}

TEST_F(LabTest, AllocateUntilLabOOM) {
  Heap* heap = isolate()->heap();
  const size_t kLabSize = 2 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  // The following objects won't fit in {kLabSize}.
  size_t sizes_raw[5] = {512, 512, 128, 512, 512};
  size_t expected_sizes_raw[5] = {512, 512, 128, 512, 384 /* left over */};
  std::vector<size_t> sizes(sizes_raw, sizes_raw + 5);
  std::vector<size_t> expected_sizes(expected_sizes_raw,
                                     expected_sizes_raw + 5);
  size_t sum = 0;
  {
    AllocationResult lab_backing_store =
        AllocationResult::FromObject(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    EXPECT_TRUE(lab.IsValid());
    for (auto size : sizes) {
      if (AllocateFromLab(heap, &lab, size)) {
        sum += size;
      }
    }
    EXPECT_EQ(kLabSize - sum, 384u);
  }
  VerifyIterable(base, limit, expected_sizes);
}

TEST_F(LabTest, AllocateExactlyUntilLimit) {
  Heap* heap = isolate()->heap();
  const size_t kLabSize = 2 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  size_t sizes_raw[4] = {512, 512, 512, 512};
  size_t expected_sizes_raw[5] = {512, 512, 512, 512, 0};
  std::vector<size_t> sizes(sizes_raw, sizes_raw + 4);
  std::vector<size_t> expected_sizes(expected_sizes_raw,
                                     expected_sizes_raw + 5);
  {
    AllocationResult lab_backing_store =
        AllocationResult::FromObject(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    EXPECT_TRUE(lab.IsValid());
    size_t sum = 0;
    for (auto size : sizes) {
      if (AllocateFromLab(heap, &lab, size)) {
        sum += size;
      } else {
        break;
      }
    }
    EXPECT_EQ(kLabSize - sum, 0u);
  }
  VerifyIterable(base, limit, expected_sizes);
}

TEST_F(LabTest, MergeSuccessful) {
  Heap* heap = isolate()->heap();
  const size_t kLabSize = 2 * KB;
  Address base1 = AllocateLabBackingStore(heap, 2 * kLabSize);
  Address limit1 = base1 + kLabSize;
  Address base2 = limit1;
  Address limit2 = base2 + kLabSize;

  size_t sizes1_raw[4] = {512, 512, 512, 256};
  size_t expected_sizes1_raw[5] = {512, 512, 512, 256, 256};
  std::vector<size_t> sizes1(sizes1_raw, sizes1_raw + 4);
  std::vector<size_t> expected_sizes1(expected_sizes1_raw,
                                      expected_sizes1_raw + 5);

  size_t sizes2_raw[5] = {256, 512, 512, 512, 512};
  size_t expected_sizes2_raw[10] = {512, 512, 512, 256, 256,
                                    512, 512, 512, 512, 0};
  std::vector<size_t> sizes2(sizes2_raw, sizes2_raw + 5);
  std::vector<size_t> expected_sizes2(expected_sizes2_raw,
                                      expected_sizes2_raw + 10);

  {
    AllocationResult lab_backing_store1 =
        AllocationResult::FromObject(HeapObject::FromAddress(base1));
    LocalAllocationBuffer lab1 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store1, kLabSize);
    EXPECT_TRUE(lab1.IsValid());
    size_t sum = 0;
    for (auto size : sizes1) {
      if (AllocateFromLab(heap, &lab1, size)) {
        sum += size;
      } else {
        break;
      }
    }

    AllocationResult lab_backing_store2 =
        AllocationResult::FromObject(HeapObject::FromAddress(base2));
    LocalAllocationBuffer lab2 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store2, kLabSize);
    EXPECT_TRUE(lab2.IsValid());
    EXPECT_TRUE(lab2.TryMerge(&lab1));
    EXPECT_FALSE(lab1.IsValid());
    for (auto size : sizes2) {
      if (AllocateFromLab(heap, &lab2, size)) {
        sum += size;
      } else {
        break;
      }
    }
    EXPECT_EQ(2 * kLabSize - sum, 0u);
  }
  VerifyIterable(base1, limit1, expected_sizes1);
  VerifyIterable(base1, limit2, expected_sizes2);
}

TEST_F(LabTest, MergeFailed) {
  Heap* heap = isolate()->heap();
  const size_t kLabSize = 2 * KB;
  Address base1 = AllocateLabBackingStore(heap, 3 * kLabSize);
  Address base2 = base1 + kLabSize;
  Address base3 = base2 + kLabSize;

  {
    AllocationResult lab_backing_store1 =
        AllocationResult::FromObject(HeapObject::FromAddress(base1));
    LocalAllocationBuffer lab1 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store1, kLabSize);
    EXPECT_TRUE(lab1.IsValid());

    AllocationResult lab_backing_store2 =
        AllocationResult::FromObject(HeapObject::FromAddress(base2));
    LocalAllocationBuffer lab2 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store2, kLabSize);
    EXPECT_TRUE(lab2.IsValid());

    AllocationResult lab_backing_store3 =
        AllocationResult::FromObject(HeapObject::FromAddress(base3));
    LocalAllocationBuffer lab3 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store3, kLabSize);
    EXPECT_TRUE(lab3.IsValid());

    EXPECT_FALSE(lab3.TryMerge(&lab1));
  }
}

TEST_F(LabTest, AllocateAligned) {
  if (kTaggedSize != kUInt32Size)
    GTEST_SKIP() << "Test only works with 32-bit tagged values.";

  Heap* heap = isolate()->heap();
  const size_t kLabSize = 2 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  std::pair<size_t, AllocationAlignment> sizes_raw[2] = {
      std::make_pair(116, kTaggedAligned), std::make_pair(64, kDoubleAligned)};
  std::vector<std::pair<intptr_t, AllocationAlignment>> sizes(sizes_raw,
                                                              sizes_raw + 2);
  size_t expected_sizes_raw[4] = {116, 4, 64, 1864};
  std::vector<size_t> expected_sizes(expected_sizes_raw,
                                     expected_sizes_raw + 4);

  {
    AllocationResult lab_backing_store =
        AllocationResult::FromObject(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    EXPECT_TRUE(lab.IsValid());
    for (auto pair : sizes) {
      if (!AllocateFromLab(heap, &lab, pair.first, pair.second)) {
        break;
      }
    }
  }
  VerifyIterable(base, limit, expected_sizes);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
