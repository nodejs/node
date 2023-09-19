// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_

#include "src/base/memory.h"
#include "src/common/globals.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/embedder-data-array.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/sandbox/external-pointer-inl.h"

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

EmbedderDataSlot::EmbedderDataSlot(const EmbedderDataSlotSnapshot& snapshot)
    : SlotBase(reinterpret_cast<Address>(&snapshot)) {}

void EmbedderDataSlot::Initialize(Object initial_value) {
  // TODO(v8) initialize the slot with Smi::zero() instead. This'll also
  // guarantee that we don't need a write barrier.
  DCHECK(initial_value.IsSmi() ||
         ReadOnlyHeap::Contains(HeapObject::cast(initial_value)));
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(initial_value);
#ifdef V8_COMPRESS_POINTERS
  ObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(Smi::zero());
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
#ifdef V8_COMPRESS_POINTERS
  CHECK(value.IsSmi() ||
        V8HeapCompressionScheme::GetPtrComprCageBaseAddress(value.ptr()) ==
            V8HeapCompressionScheme::GetPtrComprCageBaseAddress(array.ptr()));
#endif
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
#ifdef V8_COMPRESS_POINTERS
  CHECK(value.IsSmi() ||
        V8HeapCompressionScheme::GetPtrComprCageBaseAddress(value.ptr()) ==
            V8HeapCompressionScheme::GetPtrComprCageBaseAddress(object.ptr()));
#endif
  int slot_offset = object.GetEmbedderFieldOffset(embedder_field_index);
  ObjectSlot(FIELD_ADDR(object, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(value);
  WRITE_BARRIER(object, slot_offset + kTaggedPayloadOffset, value);
#ifdef V8_COMPRESS_POINTERS
  // See gc_safe_store() for the reasons behind two stores.
  ObjectSlot(FIELD_ADDR(object, slot_offset + kRawPayloadOffset))
      .Relaxed_Store(Smi::zero());
#endif
}

bool EmbedderDataSlot::ToAlignedPointer(Isolate* isolate,
                                        void** out_pointer) const {
  // We don't care about atomicity of access here because embedder slots
  // are accessed this way only from the main thread via API during "mutator"
  // phase which is propely synched with GC (concurrent marker may still look
  // at the tagged part of the embedder slot but read-only access is ok).
#ifdef V8_ENABLE_SANDBOX
  // The raw part must always contain a valid external pointer table index.
  *out_pointer = reinterpret_cast<void*>(
      ReadExternalPointerField<kEmbedderDataSlotPayloadTag>(
          address() + kExternalPointerOffset, isolate));
  return true;
#else
  Address raw_value;
  if (COMPRESS_POINTERS_BOOL) {
    // TODO(ishell, v8:8875): When pointer compression is enabled 8-byte size
    // fields (external pointers, doubles and BigInt data) are only kTaggedSize
    // aligned so we have to use unaligned pointer friendly way of accessing
    // them in order to avoid undefined behavior in C++ code.
    raw_value = base::ReadUnalignedValue<Address>(address());
  } else {
    raw_value = *location();
  }
  *out_pointer = reinterpret_cast<void*>(raw_value);
  return HAS_SMI_TAG(raw_value);
#endif  // V8_ENABLE_SANDBOX
}

bool EmbedderDataSlot::store_aligned_pointer(Isolate* isolate, void* ptr) {
  Address value = reinterpret_cast<Address>(ptr);
  if (!HAS_SMI_TAG(value)) return false;
#ifdef V8_ENABLE_SANDBOX
  DCHECK_EQ(0, value & kExternalPointerTagMask);
  // When the sandbox is enabled, the external pointer handles in
  // EmbedderDataSlots are lazily initialized: initially they contain the null
  // external pointer handle (see EmbedderDataSlot::Initialize), and only once
  // an external pointer is stored in them are they properly initialized.
  WriteLazilyInitializedExternalPointerField<kEmbedderDataSlotPayloadTag>(
      address() + kExternalPointerOffset, isolate, value);
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi::zero());
  return true;
#else
  gc_safe_store(isolate, value);
  return true;
#endif  // V8_ENABLE_SANDBOX
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
  static_assert(kSmiShiftSize == 0);
  static_assert(SmiValuesAre31Bits());
  static_assert(kTaggedSize == kInt32Size);

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
  // Here we use MaybeObjectSlot because ObjectSlot expects a valid `Object`.
  // This allows us to store a non-smi, that is not a valid `HeapObject`.
  MaybeObjectSlot(address() + kRawPayloadOffset).Relaxed_Store(MaybeObject(hi));
#else
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi(value));
#endif
}

// static
void EmbedderDataSlot::PopulateEmbedderDataSnapshot(
    Map map, JSObject js_object, int entry_index,
    EmbedderDataSlotSnapshot& snapshot) {
#ifdef V8_COMPRESS_POINTERS
  static_assert(sizeof(EmbedderDataSlotSnapshot) == sizeof(AtomicTagged_t) * 2);
#else   // !V8_COMPRESS_POINTERS
  static_assert(sizeof(EmbedderDataSlotSnapshot) == sizeof(AtomicTagged_t));
#endif  // !V8_COMPRESS_POINTERS
  static_assert(sizeof(EmbedderDataSlotSnapshot) == kEmbedderDataSlotSize);

  const Address field_base =
      FIELD_ADDR(js_object, js_object.GetEmbedderFieldOffset(entry_index));

#if defined(V8_TARGET_BIG_ENDIAN) && defined(V8_COMPRESS_POINTERS)
  const int index = 1;
#else
  const int index = 0;
#endif

  reinterpret_cast<AtomicTagged_t*>(&snapshot)[index] =
      AsAtomicTagged::Relaxed_Load(
          reinterpret_cast<AtomicTagged_t*>(field_base + kTaggedPayloadOffset));
#ifdef V8_COMPRESS_POINTERS
  reinterpret_cast<AtomicTagged_t*>(&snapshot)[1 - index] =
      AsAtomicTagged::Relaxed_Load(
          reinterpret_cast<AtomicTagged_t*>(field_base + kRawPayloadOffset));
#endif  // V8_COMPRESS_POINTERS
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
