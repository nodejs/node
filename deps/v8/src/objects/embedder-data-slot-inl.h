// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/embedder-data-array.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

EmbedderDataSlot::EmbedderDataSlot(EmbedderDataArray array, int entry_index)
    : SlotBase(FIELD_ADDR(array,
                          EmbedderDataArray::OffsetOfElementAt(entry_index))) {}

EmbedderDataSlot::EmbedderDataSlot(JSObject* object, int embedder_field_index)
    : SlotBase(FIELD_ADDR(
          object, object->GetEmbedderFieldOffset(embedder_field_index))) {}

Object* EmbedderDataSlot::load_tagged() const {
  return ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load();
}

void EmbedderDataSlot::store_smi(Smi value) {
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(value);
}

// static
void EmbedderDataSlot::store_tagged(EmbedderDataArray array, int entry_index,
                                    Object* value) {
  int slot_offset = EmbedderDataArray::OffsetOfElementAt(entry_index);
  ObjectSlot(FIELD_ADDR(array, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(ObjectPtr(value->ptr()));
  WRITE_BARRIER(array, slot_offset, value);
}

// static
void EmbedderDataSlot::store_tagged(JSObject* object, int embedder_field_index,
                                    Object* value) {
  int slot_offset = object->GetEmbedderFieldOffset(embedder_field_index);
  ObjectSlot(FIELD_ADDR(object, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(ObjectPtr(value->ptr()));
  WRITE_BARRIER(object, slot_offset, value);
}

bool EmbedderDataSlot::ToAlignedPointer(void** out_pointer) const {
  Object* tagged_value =
      ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load();
  if (!tagged_value->IsSmi()) return false;
  *out_pointer = reinterpret_cast<void*>(tagged_value);
  return true;
}

bool EmbedderDataSlot::store_aligned_pointer(void* ptr) {
  Address value = reinterpret_cast<Address>(ptr);
  if (!HAS_SMI_TAG(value)) return false;
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi(value));
  return true;
}

EmbedderDataSlot::RawData EmbedderDataSlot::load_raw(
    const DisallowHeapAllocation& no_gc) const {
  return RawData{
      ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load()->ptr(),
  };
}

void EmbedderDataSlot::store_raw(const EmbedderDataSlot::RawData& data,
                                 const DisallowHeapAllocation& no_gc) {
  ObjectSlot(address() + kTaggedPayloadOffset)
      .Relaxed_Store(ObjectPtr(data.data_[0]));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
