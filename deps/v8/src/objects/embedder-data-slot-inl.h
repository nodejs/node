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

EmbedderDataSlot::EmbedderDataSlot(JSObject object, int embedder_field_index)
    : SlotBase(FIELD_ADDR(
          object, object->GetEmbedderFieldOffset(embedder_field_index))) {}

Object EmbedderDataSlot::load_tagged() const {
  return ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load();
}

void EmbedderDataSlot::store_smi(Smi value) {
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(value);
#ifdef V8_COMPRESS_POINTERS
  ObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(Smi::kZero);
#endif
}

// static
void EmbedderDataSlot::store_tagged(EmbedderDataArray array, int entry_index,
                                    Object value) {
  int slot_offset = EmbedderDataArray::OffsetOfElementAt(entry_index);
  ObjectSlot(FIELD_ADDR(array, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(value);
  WRITE_BARRIER(array, slot_offset, value);
#ifdef V8_COMPRESS_POINTERS
  ObjectSlot(FIELD_ADDR(array, slot_offset + kRawPayloadOffset))
      .Relaxed_Store(Smi::kZero);
#endif
}

// static
void EmbedderDataSlot::store_tagged(JSObject object, int embedder_field_index,
                                    Object value) {
  int slot_offset = object->GetEmbedderFieldOffset(embedder_field_index);
  ObjectSlot(FIELD_ADDR(object, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(value);
  WRITE_BARRIER(object, slot_offset, value);
#ifdef V8_COMPRESS_POINTERS
  ObjectSlot(FIELD_ADDR(object, slot_offset + kRawPayloadOffset))
      .Relaxed_Store(Smi::kZero);
#endif
}

bool EmbedderDataSlot::ToAlignedPointer(void** out_pointer) const {
  Object tagged_value =
      ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load();
  if (!tagged_value->IsSmi()) return false;
#ifdef V8_COMPRESS_POINTERS
  STATIC_ASSERT(kSmiShiftSize == 0);
  STATIC_ASSERT(SmiValuesAre31Bits());
  Address value_lo = static_cast<uint32_t>(tagged_value->ptr());
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  Address value_hi =
      FullObjectSlot(address() + kRawPayloadOffset).Relaxed_Load()->ptr();
  Address value = value_lo | (value_hi << 32);
  *out_pointer = reinterpret_cast<void*>(value);
#else
  *out_pointer = reinterpret_cast<void*>(tagged_value->ptr());
#endif
  return true;
}

bool EmbedderDataSlot::store_aligned_pointer(void* ptr) {
  Address value = reinterpret_cast<Address>(ptr);
  if (!HAS_SMI_TAG(value)) return false;
#ifdef V8_COMPRESS_POINTERS
  STATIC_ASSERT(kSmiShiftSize == 0);
  STATIC_ASSERT(SmiValuesAre31Bits());
  // Sign-extend lower 32-bits in order to form a proper Smi value.
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  Address lo = static_cast<intptr_t>(static_cast<int32_t>(value));
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi(lo));
  Address hi = value >> 32;
  ObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(Object(hi));
#else
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi(value));
#endif
  return true;
}

EmbedderDataSlot::RawData EmbedderDataSlot::load_raw(
    const DisallowHeapAllocation& no_gc) const {
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  return RawData{
      ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load()->ptr(),
#ifdef V8_COMPRESS_POINTERS
      FullObjectSlot(address() + kRawPayloadOffset).Relaxed_Load()->ptr()
#endif
  };
}

void EmbedderDataSlot::store_raw(const EmbedderDataSlot::RawData& data,
                                 const DisallowHeapAllocation& no_gc) {
  ObjectSlot(address() + kTaggedPayloadOffset)
      .Relaxed_Store(Object(data.data_[0]));
#ifdef V8_COMPRESS_POINTERS
  ObjectSlot(address() + kRawPayloadOffset)
      .Relaxed_Store(Object(data.data_[1]));
#endif
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
