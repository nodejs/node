// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_

#include "src/objects/embedder-data-slot.h"
// Include the non-inl header before the rest of the headers.

#include "include/v8-cppgc.h"
#include "include/v8-isolate.h"
#include "src/base/memory.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/embedder-data-array.h"
#include "src/objects/heap-object-field-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/sandbox/cppheap-pointer-inl.h"
#include "src/sandbox/external-pointer-inl.h"
#include "src/sandbox/isolate.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

EmbedderDataSlot::EmbedderDataSlot(Tagged<EmbedderDataArray> array,
                                   int entry_index)
    : SlotBase(FIELD_ADDR(array,
                          EmbedderDataArray::OffsetOfElementAt(entry_index))) {}

EmbedderDataSlot::EmbedderDataSlot(Tagged<JSObject> object,
                                   int embedder_field_index)
    : SlotBase(FIELD_ADDR(
          object, object->GetEmbedderFieldOffset(embedder_field_index))) {}

void EmbedderDataSlot::Initialize(Tagged<Object> initial_value) {
  // TODO(v8) initialize the slot with Smi::zero() instead. This'll also
  // guarantee that we don't need a write barrier.
  DCHECK(IsSmi(initial_value) ||
         ReadOnlyHeap::Contains(Cast<HeapObject>(initial_value)));
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(initial_value);
  clear_cpp_heap_pointer_field(address());
}

Tagged<Object> EmbedderDataSlot::load_tagged() const {
  return ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load();
}

void EmbedderDataSlot::store_smi(Tagged<Smi> value) {
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(value);
  // See store_raw() for the reasons behind two stores.
  clear_cpp_heap_pointer_field(address());
}

// static
void EmbedderDataSlot::store_tagged(Tagged<EmbedderDataArray> array,
                                    int entry_index, Tagged<Object> value) {
#ifdef V8_COMPRESS_POINTERS
  CHECK(IsSmi(value) ||
        V8HeapCompressionScheme::GetPtrComprCageBaseAddress(value.ptr()) ==
            V8HeapCompressionScheme::GetPtrComprCageBaseAddress(array.ptr()));
#endif
  int slot_offset = EmbedderDataArray::OffsetOfElementAt(entry_index);
  Address tagged_addr = FIELD_ADDR(array, slot_offset + kTaggedPayloadOffset);
  ObjectSlot(tagged_addr).Relaxed_Store(value);
  WriteBarrier::ForValue(&*array, MaybeObjectSlot(tagged_addr), value,
                         UPDATE_WRITE_BARRIER);
  // See store_raw() for the reasons behind two stores.
  clear_cpp_heap_pointer_field(FIELD_ADDR(array, slot_offset));
}

// static
void EmbedderDataSlot::store_tagged(Tagged<JSObject> object,
                                    int embedder_field_index,
                                    Tagged<Object> value) {
#ifdef V8_COMPRESS_POINTERS
  CHECK(IsSmi(value) ||
        V8HeapCompressionScheme::GetPtrComprCageBaseAddress(value.ptr()) ==
            V8HeapCompressionScheme::GetPtrComprCageBaseAddress(object.ptr()));
#endif
  int slot_offset = object->GetEmbedderFieldOffset(embedder_field_index);
  ObjectSlot(FIELD_ADDR(object, slot_offset + kTaggedPayloadOffset))
      .Relaxed_Store(value);
  WRITE_BARRIER(object, slot_offset + kTaggedPayloadOffset, value);
  // See store_raw() for the reasons behind two stores.
  clear_cpp_heap_pointer_field(FIELD_ADDR(object, slot_offset));
}

// static
void EmbedderDataSlot::clear_cpp_heap_pointer_field(Address slot_address) {
#ifdef V8_COMPRESS_POINTERS
  CppHeapPointerSlot(slot_address + kCppHeapPointerOffset)
      .Relaxed_StoreHandle(kNullCppHeapPointerHandle);
#else
  base::WriteUnalignedValue<Address>(slot_address + kCppHeapPointerOffset,
                                     kNullAddress);
#endif
}

bool EmbedderDataSlot::ToAlignedPointer(
    IsolateForPointerCompression isolate, void** out_pointer,
    CppHeapPointerTagRange tag_range) const {
  // We don't care about atomicity of access here because embedder slots
  // are accessed this way only from the main thread via API during "mutator"
  // phase which is properly synched with GC (concurrent marker may still look
  // at the tagged part of the embedder slot but read-only access is ok).
#ifdef V8_COMPRESS_POINTERS
  CppHeapPointerSlot slot(address() + kCppHeapPointerOffset);
  CppHeapPointerHandle handle = slot.Relaxed_LoadHandle();
  if (handle == kNullCppHeapPointerHandle) {
    *out_pointer = nullptr;
    return true;
  }
  Address wrapper_addr =
      isolate.GetCppHeapPointerTable().Get(handle, tag_range);
  if (wrapper_addr == kNullAddress) {
    *out_pointer = nullptr;
    return true;
  }
  *out_pointer = reinterpret_cast<void*>(wrapper_addr);
  return true;
#else
  Address raw_value =
      base::ReadUnalignedValue<Address>(address() + kCppHeapPointerOffset);
  *out_pointer = reinterpret_cast<void*>(raw_value);
  return true;
#endif  // V8_COMPRESS_POINTERS
}

bool EmbedderDataSlot::ToAlignedPointer(
    IsolateForPointerCompression isolate, void** out_pointer,
    ExternalPointerTagRange tag_range) const {
  void* raw_ptr = nullptr;
  if (!ToAlignedPointer(isolate, &raw_ptr,
                        {CppHeapPointerTag::kEmbedderDataSlotTag})) {
    *out_pointer = nullptr;
    return false;
  }
  if (!raw_ptr) {
    *out_pointer = nullptr;
    return true;
  }
  auto* wrapper = reinterpret_cast<EmbedderDataSlotWrapper*>(raw_ptr);
  if (tag_range.Contains(wrapper->tag())) {
    *out_pointer = wrapper->pointer();
    return true;
  }
  *out_pointer = nullptr;
  return true;
}

bool EmbedderDataSlot::ToGenericAlignedPointer(
    IsolateForPointerCompression isolate, void** out_pointer) const {
  return ToAlignedPointer(
      isolate, out_pointer, {kFirstEmbedderDataTag, kLastEmbedderDataTag});
}

bool EmbedderDataSlot::DeprecatedToAlignedPointer(
    IsolateForPointerCompression isolate, void** out_pointer) const {
  return ToAlignedPointer(
      isolate, out_pointer, {kFirstEmbedderDataTag, kLastEmbedderDataTag});
}

bool EmbedderDataSlot::store_aligned_pointer(Isolate* isolate,
                                             Tagged<HeapObject> host, void* ptr,
                                             CppHeapPointerTag tag) {
  Address value = reinterpret_cast<Address>(ptr);
#ifdef V8_COMPRESS_POINTERS
  CppHeapPointerSlot slot(address() + kCppHeapPointerOffset);
  CppHeapPointerHandle handle = slot.Relaxed_LoadHandle();
  CppHeapPointerTable& table =
      IsolateForPointerCompression(isolate).GetCppHeapPointerTable();

  if (ptr == nullptr) {
    if (handle != kNullCppHeapPointerHandle) {
      table.Set(handle, kNullAddress, tag);
    }
  } else {
    if (handle == kNullCppHeapPointerHandle) {
      CppHeapPointerTable::Space* space =
          IsolateForPointerCompression(isolate).GetCppHeapPointerTableSpace();
      handle = table.AllocateAndInitializeEntry(space, value, tag);
      slot.Relaxed_StoreHandle(handle);
    } else {
      table.Set(handle, value, tag);
    }
    WriteBarrier::ForCppHeapPointer(Cast<CppHeapPointerWrapperObjectT>(host),
                                    slot, ptr);
  }
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi::zero());
  return true;
#else
  base::WriteUnalignedValue<Address>(address() + kCppHeapPointerOffset, value);
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi::zero());
  if (ptr != nullptr) {
    CppHeapPointerSlot slot(address() + kCppHeapPointerOffset);
    WriteBarrier::ForCppHeapPointer(Cast<CppHeapPointerWrapperObjectT>(host),
                                    slot, ptr);
  }
  return true;
#endif  // V8_COMPRESS_POINTERS
}

// static
template <typename T>
  requires std::is_same_v<EmbedderDataArray, T> ||
           std::is_same_v<JSObject, T>
bool EmbedderDataSlot::store_aligned_pointer(
    Isolate* isolate, DirectHandle<T> host, int entry_or_embedder_field_index,
    void* ptr, ExternalPointerTag tag) {
  EmbedderDataSlotWrapper* wrapper = nullptr;
  if (ptr != nullptr) {
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    cppgc::AllocationHandle& alloc_handle =
        v8_isolate->GetCppHeap()->GetAllocationHandle();
    wrapper = cppgc::MakeGarbageCollected<EmbedderDataSlotWrapper>(alloc_handle,
                                                                   ptr, tag);
  }
  DisallowGarbageCollection no_gc;
  EmbedderDataSlot slot(*host, entry_or_embedder_field_index);
  return slot.store_aligned_pointer(isolate, *host, wrapper,
                                    CppHeapPointerTag::kEmbedderDataSlotTag);
}

#ifdef V8_COMPRESS_POINTERS
void EmbedderDataSlot::store_tagged_without_barrier(Tagged<Object> value) {
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(value);
  clear_cpp_heap_pointer_field(address());
}

bool EmbedderDataSlot::store_handle_without_barrier(
    IsolateForPointerCompression isolate, CppHeapPointerHandle handle) {
  DCHECK_NE(handle, kNullCppHeapPointerHandle);
  CppHeapPointerTable& table = isolate.GetCppHeapPointerTable();
  CppHeapPointerTable::Space* space = isolate.GetCppHeapPointerTableSpace();

  CppHeapPointerHandle new_handle = table.DuplicateEntry(space, handle);
  if (new_handle == kNullCppHeapPointerHandle) return false;

  CppHeapPointerSlot slot(address() + kCppHeapPointerOffset);
  DCHECK_EQ(slot.Relaxed_LoadHandle(), kNullCppHeapPointerHandle);
  slot.Relaxed_StoreHandle(new_handle);

  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Smi::zero());
  return true;
}
#endif  // V8_COMPRESS_POINTERS

EmbedderDataSlot::RawData EmbedderDataSlot::load_raw(
    IsolateForPointerCompression isolate,
    const DisallowGarbageCollection& no_gc) const {
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
  return RawData{
      base::ReadUnalignedValue<Address>(address() + kCppHeapPointerOffset),
      ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Load().ptr()};
#endif
}

void EmbedderDataSlot::store_raw(IsolateForPointerCompression isolate,
                                 EmbedderDataSlot::RawData value,
                                 const DisallowGarbageCollection& no_gc) {
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
  ObjectSlot(address() + kTaggedPayloadOffset).Relaxed_Store(Tagged<Smi>(lo));
  Tagged_t hi = static_cast<Tagged_t>(value >> 32);
  // The raw part of the payload does not contain a valid tagged value, so we
  // need to use a raw store operation for it here.
  AsAtomicTagged::Relaxed_Store(
      reinterpret_cast<AtomicTagged_t*>(address() + kCppHeapPointerOffset), hi);
#else
  ObjectSlot(address() + kTaggedPayloadOffset)
      .Relaxed_Store(Tagged<Object>(value.tagged));
  base::WriteUnalignedValue<Address>(address() + kCppHeapPointerOffset,
                                     value.pointer);
#endif
}

bool EmbedderDataSlot::MustClearDuringSerialization(
    const DisallowGarbageCollection& no_gc) {
  // Serialization must avoid writing external pointer handles.  If we were to
  // accidentally write an external pointer handle, that ends up deserializing
  // as a dangling pointer.  For consistency it would be nice to avoid writing
  // external pointers also in the wide-pointer case, but as we can't
  // distinguish between Smi values and pointers we just leave them be.
#ifdef V8_COMPRESS_POINTERS
  auto* location = reinterpret_cast<CppHeapPointerHandle*>(
      address() + kCppHeapPointerOffset);
  return base::AsAtomic32::Relaxed_Load(location) != kNullCppHeapPointerHandle;
#else   // !V8_COMPRESS_POINTERS
  Address raw_value =
      base::ReadUnalignedValue<Address>(address() + kCppHeapPointerOffset);
  return raw_value != kNullAddress;
#endif  // !V8_COMPRESS_POINTERS
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_INL_H_
