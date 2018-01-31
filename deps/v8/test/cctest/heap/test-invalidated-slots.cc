// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/invalidated-slots.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

Page* HeapTester::AllocateByteArraysOnPage(
    Heap* heap, std::vector<ByteArray*>* byte_arrays) {
  PauseAllocationObserversScope pause_observers(heap);
  const int kLength = 256 - ByteArray::kHeaderSize;
  const int kSize = ByteArray::SizeFor(kLength);
  CHECK_EQ(kSize, 256);
  Isolate* isolate = heap->isolate();
  PagedSpace* old_space = heap->old_space();
  Page* page;
  // Fill a page with byte arrays.
  {
    AlwaysAllocateScope always_allocate(isolate);
    heap::SimulateFullSpace(old_space);
    ByteArray* byte_array;
    CHECK(heap->AllocateByteArray(kLength, TENURED).To(&byte_array));
    byte_arrays->push_back(byte_array);
    page = Page::FromAddress(byte_array->address());
    CHECK_EQ(page->area_size() % kSize, 0u);
    size_t n = page->area_size() / kSize;
    for (size_t i = 1; i < n; i++) {
      CHECK(heap->AllocateByteArray(kLength, TENURED).To(&byte_array));
      byte_arrays->push_back(byte_array);
      CHECK_EQ(page, Page::FromAddress(byte_array->address()));
    }
  }
  CHECK_NULL(page->invalidated_slots());
  return page;
}

HEAP_TEST(InvalidatedSlotsNoInvalidatedRanges) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray*> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  InvalidatedSlotsFilter filter(page);
  for (auto byte_array : byte_arrays) {
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kPointerSize) {
      CHECK(filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsSomeInvalidatedRanges) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray*> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Register every second byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i += 2) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray* byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kPointerSize) {
      if (i % 2 == 0) {
        CHECK(!filter.IsValid(addr));
      } else {
        CHECK(filter.IsValid(addr));
      }
    }
  }
}

HEAP_TEST(InvalidatedSlotsAllInvalidatedRanges) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray*> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Register the all byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray* byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kPointerSize) {
      CHECK(!filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsAfterTrimming) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray*> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Register the all byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  // Trim byte arrays and check that the slots outside the byte arrays are
  // considered valid. Free space outside invalidated object can be reused
  // during evacuation for allocation of the evacuated objects. That can
  // add new valid slots to evacuation candidates.
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray* byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    heap->RightTrimFixedArray(byte_array, byte_array->length());
    for (Address addr = start; addr < end; addr += kPointerSize) {
      CHECK(filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsEvacuationCandidate) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray*> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  page->MarkEvacuationCandidate();
  // Register the all byte arrays as invalidated.
  // This should be no-op because the page is marked as evacuation
  // candidate.
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  // All slots must still be valid.
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray* byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kPointerSize) {
      CHECK(filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsResetObjectRegression) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray*> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Ensure that the first array has smaller size then the rest.
  heap->RightTrimFixedArray(byte_arrays[0], byte_arrays[0]->length() - 8);
  // Register the all byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  // All slots must still be invalid.
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray* byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kPointerSize) {
      CHECK(!filter.IsValid(addr));
    }
  }
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
