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
    Heap* heap, std::vector<ByteArray>* byte_arrays) {
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
    ByteArray byte_array;
    CHECK(AllocateByteArrayForTest(heap, kLength, TENURED).To(&byte_array));
    byte_arrays->push_back(byte_array);
    page = Page::FromHeapObject(byte_array);
    size_t n = page->area_size() / kSize;
    for (size_t i = 1; i < n; i++) {
      CHECK(AllocateByteArrayForTest(heap, kLength, TENURED).To(&byte_array));
      byte_arrays->push_back(byte_array);
      CHECK_EQ(page, Page::FromHeapObject(byte_array));
    }
  }
  CHECK_NULL(page->invalidated_slots());
  return page;
}

HEAP_TEST(InvalidatedSlotsNoInvalidatedRanges) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  InvalidatedSlotsFilter filter(page);
  for (ByteArray byte_array : byte_arrays) {
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kTaggedSize) {
      CHECK(filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsSomeInvalidatedRanges) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Register every second byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i += 2) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kTaggedSize) {
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
  std::vector<ByteArray> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Register the all byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kTaggedSize) {
      CHECK(!filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsAfterTrimming) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray> byte_arrays;
  Page* page = AllocateByteArraysOnPage(heap, &byte_arrays);
  // Register the all byte arrays as invalidated.
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    page->RegisterObjectWithInvalidatedSlots(byte_arrays[i],
                                             byte_arrays[i]->Size());
  }
  // Trim byte arrays and check that the slots outside the byte arrays are
  // considered invalid if the old space page was swept.
  InvalidatedSlotsFilter filter(page);
  for (size_t i = 0; i < byte_arrays.size(); i++) {
    ByteArray byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    heap->RightTrimFixedArray(byte_array, byte_array->length());
    for (Address addr = start; addr < end; addr += kTaggedSize) {
      CHECK_EQ(filter.IsValid(addr), page->SweepingDone());
    }
  }
}

HEAP_TEST(InvalidatedSlotsEvacuationCandidate) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray> byte_arrays;
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
    ByteArray byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kTaggedSize) {
      CHECK(filter.IsValid(addr));
    }
  }
}

HEAP_TEST(InvalidatedSlotsResetObjectRegression) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  std::vector<ByteArray> byte_arrays;
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
    ByteArray byte_array = byte_arrays[i];
    Address start = byte_array->address() + ByteArray::kHeaderSize;
    Address end = byte_array->address() + byte_array->Size();
    for (Address addr = start; addr < end; addr += kTaggedSize) {
      CHECK(!filter.IsValid(addr));
    }
  }
}

Handle<FixedArray> AllocateArrayOnFreshPage(Isolate* isolate,
                                            PagedSpace* old_space, int length) {
  AlwaysAllocateScope always_allocate(isolate);
  heap::SimulateFullSpace(old_space);
  return isolate->factory()->NewFixedArray(length, TENURED);
}

Handle<FixedArray> AllocateArrayOnEvacuationCandidate(Isolate* isolate,
                                                      PagedSpace* old_space,
                                                      int length) {
  Handle<FixedArray> object =
      AllocateArrayOnFreshPage(isolate, old_space, length);
  heap::ForceEvacuationCandidate(Page::FromHeapObject(*object));
  return object;
}

HEAP_TEST(InvalidatedSlotsRightTrimFixedArray) {
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_parallel_compaction = false;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  HandleScope scope(isolate);
  PagedSpace* old_space = heap->old_space();
  // Allocate a dummy page to be swept be the sweeper during evacuation.
  AllocateArrayOnFreshPage(isolate, old_space, 1);
  Handle<FixedArray> evacuated =
      AllocateArrayOnEvacuationCandidate(isolate, old_space, 1);
  Handle<FixedArray> trimmed = AllocateArrayOnFreshPage(isolate, old_space, 10);
  heap::SimulateIncrementalMarking(heap);
  for (int i = 1; i < trimmed->length(); i++) {
    trimmed->set(i, *evacuated);
  }
  {
    HandleScope scope(isolate);
    Handle<HeapObject> dead = factory->NewFixedArray(1);
    for (int i = 1; i < trimmed->length(); i++) {
      trimmed->set(i, *dead);
    }
    heap->RightTrimFixedArray(*trimmed, trimmed->length() - 1);
  }
  CcTest::CollectGarbage(i::NEW_SPACE);
  CcTest::CollectGarbage(i::OLD_SPACE);
}

HEAP_TEST(InvalidatedSlotsRightTrimLargeFixedArray) {
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_parallel_compaction = false;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  HandleScope scope(isolate);
  PagedSpace* old_space = heap->old_space();
  // Allocate a dummy page to be swept be the sweeper during evacuation.
  AllocateArrayOnFreshPage(isolate, old_space, 1);
  Handle<FixedArray> evacuated =
      AllocateArrayOnEvacuationCandidate(isolate, old_space, 1);
  Handle<FixedArray> trimmed;
  {
    AlwaysAllocateScope always_allocate(isolate);
    trimmed = factory->NewFixedArray(
        kMaxRegularHeapObjectSize / kTaggedSize + 100, TENURED);
    DCHECK(MemoryChunk::FromHeapObject(*trimmed)->InLargeObjectSpace());
  }
  heap::SimulateIncrementalMarking(heap);
  for (int i = 1; i < trimmed->length(); i++) {
    trimmed->set(i, *evacuated);
  }
  {
    HandleScope scope(isolate);
    Handle<HeapObject> dead = factory->NewFixedArray(1);
    for (int i = 1; i < trimmed->length(); i++) {
      trimmed->set(i, *dead);
    }
    heap->RightTrimFixedArray(*trimmed, trimmed->length() - 1);
  }
  CcTest::CollectGarbage(i::NEW_SPACE);
  CcTest::CollectGarbage(i::OLD_SPACE);
}

HEAP_TEST(InvalidatedSlotsLeftTrimFixedArray) {
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_parallel_compaction = false;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  HandleScope scope(isolate);
  PagedSpace* old_space = heap->old_space();
  // Allocate a dummy page to be swept be the sweeper during evacuation.
  AllocateArrayOnFreshPage(isolate, old_space, 1);
  Handle<FixedArray> evacuated =
      AllocateArrayOnEvacuationCandidate(isolate, old_space, 1);
  Handle<FixedArray> trimmed = AllocateArrayOnFreshPage(isolate, old_space, 10);
  heap::SimulateIncrementalMarking(heap);
  for (int i = 0; i + 1 < trimmed->length(); i++) {
    trimmed->set(i, *evacuated);
  }
  {
    HandleScope scope(isolate);
    Handle<HeapObject> dead = factory->NewFixedArray(1);
    for (int i = 1; i < trimmed->length(); i++) {
      trimmed->set(i, *dead);
    }
    heap->LeftTrimFixedArray(*trimmed, trimmed->length() - 1);
  }
  CcTest::CollectGarbage(i::NEW_SPACE);
  CcTest::CollectGarbage(i::OLD_SPACE);
}

HEAP_TEST(InvalidatedSlotsFastToSlow) {
  FLAG_manual_evacuation_candidates_selection = true;
  FLAG_parallel_compaction = false;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = CcTest::heap();
  PagedSpace* old_space = heap->old_space();

  HandleScope scope(isolate);

  Handle<String> name = factory->InternalizeUtf8String("TestObject");
  Handle<String> prop_name1 = factory->InternalizeUtf8String("prop1");
  Handle<String> prop_name2 = factory->InternalizeUtf8String("prop2");
  Handle<String> prop_name3 = factory->InternalizeUtf8String("prop3");
  // Allocate a dummy page to be swept be the sweeper during evacuation.
  AllocateArrayOnFreshPage(isolate, old_space, 1);
  Handle<FixedArray> evacuated =
      AllocateArrayOnEvacuationCandidate(isolate, old_space, 1);
  // Allocate a dummy page to ensure that the JSObject is allocated on
  // a fresh page.
  AllocateArrayOnFreshPage(isolate, old_space, 1);
  Handle<JSObject> obj;
  {
    AlwaysAllocateScope always_allocate(isolate);
    Handle<JSFunction> function = factory->NewFunctionForTest(name);
    function->shared()->set_expected_nof_properties(3);
    obj = factory->NewJSObject(function, TENURED);
  }
  // Start incremental marking.
  heap::SimulateIncrementalMarking(heap);
  // Set properties to point to the evacuation candidate.
  Object::SetProperty(isolate, obj, prop_name1, evacuated).Check();
  Object::SetProperty(isolate, obj, prop_name2, evacuated).Check();
  Object::SetProperty(isolate, obj, prop_name3, evacuated).Check();

  {
    HandleScope scope(isolate);
    Handle<HeapObject> dead = factory->NewFixedArray(1);
    Object::SetProperty(isolate, obj, prop_name1, dead).Check();
    Object::SetProperty(isolate, obj, prop_name2, dead).Check();
    Object::SetProperty(isolate, obj, prop_name3, dead).Check();
    Handle<Map> map(obj->map(), isolate);
    Handle<Map> normalized_map =
        Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES, "testing");
    JSObject::MigrateToMap(obj, normalized_map);
  }
  CcTest::CollectGarbage(i::NEW_SPACE);
  CcTest::CollectGarbage(i::OLD_SPACE);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
