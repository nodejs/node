// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/slots-buffer.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

TEST(SlotsBufferObjectSlotsRemoval) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  SlotsBuffer* buffer = new SlotsBuffer(NULL);
  void* fake_object[1];

  Handle<FixedArray> array = factory->NewFixedArray(2, TENURED);
  CHECK(heap->old_space()->Contains(*array));
  array->set(0, reinterpret_cast<Object*>(fake_object), SKIP_WRITE_BARRIER);

  // Firstly, let's test the regular slots buffer entry.
  buffer->Add(HeapObject::RawField(*array, FixedArray::kHeaderSize));
  CHECK(reinterpret_cast<void*>(buffer->Get(0)) ==
        HeapObject::RawField(*array, FixedArray::kHeaderSize));
  SlotsBuffer::RemoveObjectSlots(CcTest::i_isolate()->heap(), buffer,
                                 array->address(),
                                 array->address() + array->Size());
  CHECK(reinterpret_cast<void*>(buffer->Get(0)) ==
        HeapObject::RawField(heap->empty_fixed_array(),
                             FixedArrayBase::kLengthOffset));

  // Secondly, let's test the typed slots buffer entry.
  SlotsBuffer::AddTo(NULL, &buffer, SlotsBuffer::EMBEDDED_OBJECT_SLOT,
                     array->address() + FixedArray::kHeaderSize,
                     SlotsBuffer::FAIL_ON_OVERFLOW);
  CHECK(reinterpret_cast<void*>(buffer->Get(1)) ==
        reinterpret_cast<Object**>(SlotsBuffer::EMBEDDED_OBJECT_SLOT));
  CHECK(reinterpret_cast<void*>(buffer->Get(2)) ==
        HeapObject::RawField(*array, FixedArray::kHeaderSize));
  SlotsBuffer::RemoveObjectSlots(CcTest::i_isolate()->heap(), buffer,
                                 array->address(),
                                 array->address() + array->Size());
  CHECK(reinterpret_cast<void*>(buffer->Get(1)) ==
        HeapObject::RawField(heap->empty_fixed_array(),
                             FixedArrayBase::kLengthOffset));
  CHECK(reinterpret_cast<void*>(buffer->Get(2)) ==
        HeapObject::RawField(heap->empty_fixed_array(),
                             FixedArrayBase::kLengthOffset));
  delete buffer;
}


TEST(FilterInvalidSlotsBufferEntries) {
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  SlotsBuffer* buffer = new SlotsBuffer(NULL);

  // Set up a fake black object that will contain a recorded SMI, a recorded
  // pointer to a new space object, and a recorded pointer to a non-evacuation
  // candidate object. These object should be filtered out. Additionally,
  // we point to an evacuation candidate object which should not be filtered
  // out.

  // Create fake object and mark it black.
  Handle<FixedArray> fake_object = factory->NewFixedArray(23, TENURED);
  MarkBit mark_bit = Marking::MarkBitFrom(*fake_object);
  Marking::MarkBlack(mark_bit);

  // Write a SMI into field one and record its address;
  Object** field_smi = fake_object->RawFieldOfElementAt(0);
  *field_smi = Smi::FromInt(100);
  buffer->Add(field_smi);

  // Write a new space reference into field 2 and record its address;
  Handle<FixedArray> new_space_object = factory->NewFixedArray(23);
  mark_bit = Marking::MarkBitFrom(*new_space_object);
  Marking::MarkBlack(mark_bit);
  Object** field_new_space = fake_object->RawFieldOfElementAt(1);
  *field_new_space = *new_space_object;
  buffer->Add(field_new_space);

  // Write an old space reference into field 3 which points to an object not on
  // an evacuation candidate.
  Handle<FixedArray> old_space_object_non_evacuation =
      factory->NewFixedArray(23, TENURED);
  mark_bit = Marking::MarkBitFrom(*old_space_object_non_evacuation);
  Marking::MarkBlack(mark_bit);
  Object** field_old_space_object_non_evacuation =
      fake_object->RawFieldOfElementAt(2);
  *field_old_space_object_non_evacuation = *old_space_object_non_evacuation;
  buffer->Add(field_old_space_object_non_evacuation);

  // Write an old space reference into field 4 which points to an object on an
  // evacuation candidate.
  heap::SimulateFullSpace(heap->old_space());
  Handle<FixedArray> valid_object =
      isolate->factory()->NewFixedArray(23, TENURED);
  Page* page = Page::FromAddress(valid_object->address());
  page->SetFlag(MemoryChunk::EVACUATION_CANDIDATE);
  Object** valid_field = fake_object->RawFieldOfElementAt(3);
  *valid_field = *valid_object;
  buffer->Add(valid_field);

  SlotsBuffer::RemoveInvalidSlots(heap, buffer);
  Object** kRemovedEntry = HeapObject::RawField(heap->empty_fixed_array(),
                                                FixedArrayBase::kLengthOffset);
  CHECK_EQ(buffer->Get(0), kRemovedEntry);
  CHECK_EQ(buffer->Get(1), kRemovedEntry);
  CHECK_EQ(buffer->Get(2), kRemovedEntry);
  CHECK_EQ(buffer->Get(3), valid_field);

  // Clean-up to make verify heap happy.
  mark_bit = Marking::MarkBitFrom(*fake_object);
  Marking::MarkWhite(mark_bit);
  mark_bit = Marking::MarkBitFrom(*new_space_object);
  Marking::MarkWhite(mark_bit);
  mark_bit = Marking::MarkBitFrom(*old_space_object_non_evacuation);
  Marking::MarkWhite(mark_bit);

  delete buffer;
}

}  // namespace internal
}  // namespace v8
