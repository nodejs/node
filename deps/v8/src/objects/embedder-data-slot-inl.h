// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_

#include "src/objects/embedder-data-slot.h"

#include "src/base/memory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/embedder-data-array.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

EmbedderDataSlot::EmbedderDataSlot(EmbedderDataArray array, int entry_index)
    : SlotBase(FIELD_ADDR(array,
                          EmbedderDataArray::OffsetOfElementAt(entry_index))) {}

EmbedderDataSlot::EmbedderDataSlot(JSObject object, int embedder_field_index)
    : SlotBase(FIELD_ADDR(
          object, object.GetEmbedderFieldOffset(embedder_field_index))) {}

void EmbedderDataSlot::AllocateExternalPointerEntry(Isolate* isolate) {
#ifdef V8_HEAP_SANDBOX
  // TODO(v8:10391, saelo): Use InitExternalPointerField() once
  // ExternalPointer_t is 4-bytes.
  uint32_t index = isolate->external_pointer_table().allocate();
  // Object slots don't support storing raw values, so we just "reinterpret
  // cast" the index value to Object.
  Object index_as_object(index);
  ObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(index_as_object);
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi::zero());
#endif
}

Object EmbedderDataSlot::load_tagged() const {
  return ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load();
}

void EmbedderDataSlot::store_smi(Smi value) {
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(value);
#ifdef V8_COMPRESS_POINTERS
  // See gc_safe_store() for the reasons behind two stores.
  ObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(Smi::zero());
#endif
}

// static
void EmbedderDataSlot::store_tagged(EmbedderDataArray array, int entry_index,
                                    Object value) {
  int slot_offset = EmbedderDataArray::OffsetOfElementAt(entry_index);
  ObjectSlot(FIELD_ADDR(array, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(value);
  WRITE_BARRIER(array, slot_offset + kTaggedPayloadOffset, value);
#ifdef V8_COMPRESS_POINTERS
  // See gc_safe_store() for the reasons behind two stores.
  ObjectSlot(FIELD_ADDR(array, slot_offset + kRawPayloadOffset))
      .Relaxed_Store(Smi::zero());
#endif
}

// static
void EmbedderDataSlot::store_tagged(JSObject object, int embedder_field_index,
                                    Object value) {
  int slot_offset = object.GetEmbedderFieldOffset(embedder_field_index);
  ObjectSlot(FIELD_ADDR(object, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(value);
  WRITE_BARRIER(object, slot_offset + kTaggedPayloadOffset, value);
#ifdef V8_COMPRESS_POINTERS
  // See gc_safe_store() for the reasons behind two stores and why the second is
  // only done if !V8_HEAP_SANDBOX_BOOL
  ObjectSlot(FIELD_ADDR(object, slot_offset + kRawPayloadOffset))
      .Relaxed_Store(Smi::zero());
#endif
}

bool EmbedderDataSlot::ToAlignedPointer(PtrComprCageBase isolate_root,
                                        void** out_pointer) const {
  // We don't care about atomicity of access here because embedder slots
  // are accessed this way only from the main thread via API during "mutator"
  // phase which is propely synched with GC (concurrent marker may still look
  // at the tagged part of the embedder slot but read-only access is ok).
  Address raw_value;
#ifdef V8_HEAP_SANDBOX

  // TODO(syg): V8_HEAP_SANDBOX doesn't work with pointer cage
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
#error "V8_HEAP_SANDBOX requires per-Isolate pointer compression cage"
#endif

  uint32_t index = base::Memory<uint32_t>(address() + kRawPayloadOffset);
  const Isolate* isolate = Isolate::FromRootAddress(isolate_root.address());
  raw_value = isolate->external_pointer_table().get(index) ^
              kEmbedderDataSlotPayloadTag;
#else
  if (COMPRESS_POINTERS_BOOL) {
    // TODO(ishell, v8:8875): When pointer compression is enabled 8-byte size
    // fields (external pointers, doubles and BigInt data) are only kTaggedSize
    // aligned so we have to use unaligned pointer friendly way of accessing
    // them in order to avoid undefined behavior in C++ code.
    raw_value = base::ReadUnalignedValue<Address>(address());
  } else {
    raw_value = *location();
  }
#endif
  *out_pointer = reinterpret_cast<void*>(raw_value);
  return HAS_SMI_TAG(raw_value);
}

bool EmbedderDataSlot::ToAlignedPointerSafe(PtrComprCageBase isolate_root,
                                            void** out_pointer) const {
#ifdef V8_HEAP_SANDBOX

  // TODO(syg): V8_HEAP_SANDBOX doesn't work with pointer cage
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
#error "V8_HEAP_SANDBOX requires per-Isolate pointer compression cage"
#endif

  uint32_t index = base::Memory<uint32_t>(address() + kRawPayloadOffset);
  Address raw_value;
  const Isolate* isolate = Isolate::FromRootAddress(isolate_root.address());
  if (isolate->external_pointer_table().is_valid_index(index)) {
    raw_value = isolate->external_pointer_table().get(index) ^
                kEmbedderDataSlotPayloadTag;
    *out_pointer = reinterpret_cast<void*>(raw_value);
    return true;
  }
  return false;
#else
  return ToAlignedPointer(isolate_root, out_pointer);
#endif  // V8_HEAP_SANDBOX
}

bool EmbedderDataSlot::store_aligned_pointer(Isolate* isolate, void* ptr) {
  Address value = reinterpret_cast<Address>(ptr);
  if (!HAS_SMI_TAG(value)) return false;
#ifdef V8_HEAP_SANDBOX
  if (V8_HEAP_SANDBOX_BOOL) {
    AllocateExternalPointerEntry(isolate);
    // Raw payload contains the table index. Object slots don't support loading
    // of raw values, so we just "reinterpret cast" Object value to index.
    Object index_as_object =
        ObjectSlot(address() + kRawPayloadOffset).Relaxed_Load();
    uint32_t index = static_cast<uint32_t>(index_as_object.ptr());
    isolate->external_pointer_table().set(index,
                                          value ^ kEmbedderDataSlotPayloadTag);
    return true;
  }
#endif
  gc_safe_store(isolate, value);
  return true;
}

EmbedderDataSlot::RawData EmbedderDataSlot::load_raw(
    Isolate* isolate, const DisallowGarbageCollection& no_gc) const {
  // We don't care about atomicity of access here because embedder slots
  // are accessed this way only by serializer from the main thread when
  // GC is not active (concurrent marker may still look at the tagged part
  // of the embedder slot but read-only access is ok).
#ifdef V8_COMPRESS_POINTERS
  // TODO(ishell, v8:8875): When pointer compression is enabled 8-byte size
  // fields (external pointers, doubles and BigInt data) are only kTaggedSize
  // aligned so we have to use unaligned pointer friendly way of accessing them
  // in order to avoid undefined behavior in C++ code.
  return base::ReadUnalignedValue<EmbedderDataSlot::RawData>(address());
#else
  return *location();
#endif
}

void EmbedderDataSlot::store_raw(Isolate* isolate,
                                 EmbedderDataSlot::RawData data,
                                 const DisallowGarbageCollection& no_gc) {
  gc_safe_store(isolate, data);
}

void EmbedderDataSlot::gc_safe_store(Isolate* isolate, Address value) {
#ifdef V8_COMPRESS_POINTERS
  STATIC_ASSERT(kSmiShiftSize == 0);
  STATIC_ASSERT(SmiValuesAre31Bits());
  STATIC_ASSERT(kTaggedSize == kInt32Size);

  // We have to do two 32-bit stores here because
  // 1) tagged part modifications must be atomic to be properly synchronized
  //    with the concurrent marker.
  // 2) atomicity of full pointer store is not guaranteed for embedder slots
  //    since the address of the slot may not be kSystemPointerSize aligned
  //    (only kTaggedSize alignment is guaranteed).
  // TODO(ishell, v8:8875): revisit this once the allocation alignment
  // inconsistency is fixed.
  Address lo = static_cast<intptr_t>(static_cast<int32_t>(value));
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi(lo));
  Address hi = value >> 32;
  ObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(Object(hi));
#else
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi(value));
#endif
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
