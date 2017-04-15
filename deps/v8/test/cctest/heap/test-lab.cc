// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"
#include "src/heap/spaces-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/heap/incremental-marking.h -> src/objects-inl.h
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

static Address AllocateLabBackingStore(Heap* heap, intptr_t size_in_bytes) {
  AllocationResult result = heap->old_space()->AllocateRaw(
      static_cast<int>(size_in_bytes), kDoubleAligned);
  Address adr = result.ToObjectChecked()->address();
  return adr;
}


static void VerifyIterable(v8::internal::Address base,
                           v8::internal::Address limit,
                           std::vector<intptr_t> expected_size) {
  CHECK_LE(reinterpret_cast<intptr_t>(base), reinterpret_cast<intptr_t>(limit));
  HeapObject* object = nullptr;
  size_t counter = 0;
  while (base < limit) {
    object = HeapObject::FromAddress(base);
    CHECK(object->IsFiller());
    CHECK_LT(counter, expected_size.size());
    CHECK_EQ(expected_size[counter], object->Size());
    base += object->Size();
    counter++;
  }
}


static bool AllocateFromLab(Heap* heap, LocalAllocationBuffer* lab,
                            intptr_t size_in_bytes,
                            AllocationAlignment alignment = kWordAligned) {
  HeapObject* obj;
  AllocationResult result =
      lab->AllocateRawAligned(static_cast<int>(size_in_bytes), alignment);
  if (result.To(&obj)) {
    heap->CreateFillerObjectAt(obj->address(), static_cast<int>(size_in_bytes),
                               ClearRecordedSlots::kNo);
    return true;
  }
  return false;
}


TEST(InvalidLab) {
  LocalAllocationBuffer lab = LocalAllocationBuffer::InvalidBuffer();
  CHECK(!lab.IsValid());
}


TEST(UnusedLabImplicitClose) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  heap->root(Heap::kOnePointerFillerMapRootIndex);
  const int kLabSize = 4 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  intptr_t expected_sizes_raw[1] = {kLabSize};
  std::vector<intptr_t> expected_sizes(expected_sizes_raw,
                                       expected_sizes_raw + 1);
  {
    AllocationResult lab_backing_store(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    CHECK(lab.IsValid());
  }
  VerifyIterable(base, limit, expected_sizes);
}


TEST(SimpleAllocate) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  const int kLabSize = 4 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  intptr_t sizes_raw[1] = {128};
  intptr_t expected_sizes_raw[2] = {128, kLabSize - 128};
  std::vector<intptr_t> sizes(sizes_raw, sizes_raw + 1);
  std::vector<intptr_t> expected_sizes(expected_sizes_raw,
                                       expected_sizes_raw + 2);
  {
    AllocationResult lab_backing_store(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    CHECK(lab.IsValid());
    intptr_t sum = 0;
    for (auto size : sizes) {
      if (AllocateFromLab(heap, &lab, size)) {
        sum += size;
      }
    }
  }
  VerifyIterable(base, limit, expected_sizes);
}


TEST(AllocateUntilLabOOM) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  const int kLabSize = 2 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  // The following objects won't fit in {kLabSize}.
  intptr_t sizes_raw[5] = {512, 512, 128, 512, 512};
  intptr_t expected_sizes_raw[5] = {512, 512, 128, 512, 384 /* left over */};
  std::vector<intptr_t> sizes(sizes_raw, sizes_raw + 5);
  std::vector<intptr_t> expected_sizes(expected_sizes_raw,
                                       expected_sizes_raw + 5);
  intptr_t sum = 0;
  {
    AllocationResult lab_backing_store(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    CHECK(lab.IsValid());
    for (auto size : sizes) {
      if (AllocateFromLab(heap, &lab, size)) {
        sum += size;
      }
    }
    CHECK_EQ(kLabSize - sum, 384);
  }
  VerifyIterable(base, limit, expected_sizes);
}


TEST(AllocateExactlyUntilLimit) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  const int kLabSize = 2 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  intptr_t sizes_raw[4] = {512, 512, 512, 512};
  intptr_t expected_sizes_raw[5] = {512, 512, 512, 512, 0};
  std::vector<intptr_t> sizes(sizes_raw, sizes_raw + 4);
  std::vector<intptr_t> expected_sizes(expected_sizes_raw,
                                       expected_sizes_raw + 5);
  {
    AllocationResult lab_backing_store(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    CHECK(lab.IsValid());
    intptr_t sum = 0;
    for (auto size : sizes) {
      if (AllocateFromLab(heap, &lab, size)) {
        sum += size;
      } else {
        break;
      }
    }
    CHECK_EQ(kLabSize - sum, 0);
  }
  VerifyIterable(base, limit, expected_sizes);
}


TEST(MergeSuccessful) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  const int kLabSize = 2 * KB;
  Address base1 = AllocateLabBackingStore(heap, 2 * kLabSize);
  Address limit1 = base1 + kLabSize;
  Address base2 = limit1;
  Address limit2 = base2 + kLabSize;

  intptr_t sizes1_raw[4] = {512, 512, 512, 256};
  intptr_t expected_sizes1_raw[5] = {512, 512, 512, 256, 256};
  std::vector<intptr_t> sizes1(sizes1_raw, sizes1_raw + 4);
  std::vector<intptr_t> expected_sizes1(expected_sizes1_raw,
                                        expected_sizes1_raw + 5);

  intptr_t sizes2_raw[5] = {256, 512, 512, 512, 512};
  intptr_t expected_sizes2_raw[10] = {512, 512, 512, 256, 256,
                                      512, 512, 512, 512, 0};
  std::vector<intptr_t> sizes2(sizes2_raw, sizes2_raw + 5);
  std::vector<intptr_t> expected_sizes2(expected_sizes2_raw,
                                        expected_sizes2_raw + 10);

  {
    AllocationResult lab_backing_store1(HeapObject::FromAddress(base1));
    LocalAllocationBuffer lab1 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store1, kLabSize);
    CHECK(lab1.IsValid());
    intptr_t sum = 0;
    for (auto size : sizes1) {
      if (AllocateFromLab(heap, &lab1, size)) {
        sum += size;
      } else {
        break;
      }
    }

    AllocationResult lab_backing_store2(HeapObject::FromAddress(base2));
    LocalAllocationBuffer lab2 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store2, kLabSize);
    CHECK(lab2.IsValid());
    CHECK(lab2.TryMerge(&lab1));
    CHECK(!lab1.IsValid());
    for (auto size : sizes2) {
      if (AllocateFromLab(heap, &lab2, size)) {
        sum += size;
      } else {
        break;
      }
    }
    CHECK_EQ(2 * kLabSize - sum, 0);
  }
  VerifyIterable(base1, limit1, expected_sizes1);
  VerifyIterable(base1, limit2, expected_sizes2);
}


TEST(MergeFailed) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  const int kLabSize = 2 * KB;
  Address base1 = AllocateLabBackingStore(heap, 3 * kLabSize);
  Address base2 = base1 + kLabSize;
  Address base3 = base2 + kLabSize;

  {
    AllocationResult lab_backing_store1(HeapObject::FromAddress(base1));
    LocalAllocationBuffer lab1 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store1, kLabSize);
    CHECK(lab1.IsValid());

    AllocationResult lab_backing_store2(HeapObject::FromAddress(base2));
    LocalAllocationBuffer lab2 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store2, kLabSize);
    CHECK(lab2.IsValid());

    AllocationResult lab_backing_store3(HeapObject::FromAddress(base3));
    LocalAllocationBuffer lab3 =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store3, kLabSize);
    CHECK(lab3.IsValid());

    CHECK(!lab3.TryMerge(&lab1));
  }
}


#ifdef V8_HOST_ARCH_32_BIT
TEST(AllocateAligned) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  const int kLabSize = 2 * KB;
  Address base = AllocateLabBackingStore(heap, kLabSize);
  Address limit = base + kLabSize;
  std::pair<intptr_t, AllocationAlignment> sizes_raw[2] = {
      std::make_pair(116, kWordAligned), std::make_pair(64, kDoubleAligned)};
  std::vector<std::pair<intptr_t, AllocationAlignment>> sizes(sizes_raw,
                                                              sizes_raw + 2);
  intptr_t expected_sizes_raw[4] = {116, 4, 64, 1864};
  std::vector<intptr_t> expected_sizes(expected_sizes_raw,
                                       expected_sizes_raw + 4);

  {
    AllocationResult lab_backing_store(HeapObject::FromAddress(base));
    LocalAllocationBuffer lab =
        LocalAllocationBuffer::FromResult(heap, lab_backing_store, kLabSize);
    CHECK(lab.IsValid());
    for (auto pair : sizes) {
      if (!AllocateFromLab(heap, &lab, pair.first, pair.second)) {
        break;
      }
    }
  }
  VerifyIterable(base, limit, expected_sizes);
}
#endif  // V8_HOST_ARCH_32_BIT

}  // namespace internal
}  // namespace v8
