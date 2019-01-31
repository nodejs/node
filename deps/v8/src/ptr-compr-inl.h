// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PTR_COMPR_INL_H_
#define V8_PTR_COMPR_INL_H_

#if V8_TARGET_ARCH_64_BIT

#include "src/objects/heap-object-inl.h"
#include "src/ptr-compr.h"

namespace v8 {
namespace internal {

// Compresses full-pointer representation of a tagged value to on-heap
// representation.
V8_INLINE Tagged_t CompressTagged(Address tagged) {
  // The compression is no-op while we are using checked decompression.
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  // TODO(ishell): implement once kTaggedSize is equal to kInt32Size.
  return tagged;
}

// Calculates isolate root value from any on-heap address.
V8_INLINE Address GetRootFromOnHeapAddress(Address addr) {
  return RoundDown(addr + kPtrComprIsolateRootBias,
                   kPtrComprIsolateRootAlignment);
}

// Decompresses weak or strong heap object pointer or forwarding pointer,
// preserving both weak- and smi- tags.
V8_INLINE Address DecompressTaggedPointerImpl(Address on_heap_addr,
                                              int32_t value) {
  Address root = GetRootFromOnHeapAddress(on_heap_addr);
  // Current compression scheme requires value to be sign-extended to inptr_t
  // before adding the |root|.
  return root + static_cast<Address>(static_cast<intptr_t>(value));
}

// Decompresses weak or strong heap object pointer or forwarding pointer,
// preserving both weak- and smi- tags and checks that the result of
// decompression matches full value stored in the field.
// Checked decompression helps to find misuses of XxxSlots and FullXxxSlots.
// TODO(ishell): remove in favour of DecompressTaggedPointerImpl() once
// kTaggedSize is equal to kInt32Size.
V8_INLINE Address DecompressTaggedPointer(Address on_heap_addr,
                                          Tagged_t full_value) {
  // Use only lower 32-bits of the value for decompression.
  int32_t compressed = static_cast<int32_t>(full_value);
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  Address result = DecompressTaggedPointerImpl(on_heap_addr, compressed);
#ifdef DEBUG
  if (full_value != result) {
    base::OS::DebugBreak();
    result = DecompressTaggedPointerImpl(on_heap_addr, compressed);
  }
#endif
  DCHECK_EQ(full_value, result);
  return result;
}

// Decompresses any tagged value, preserving both weak- and smi- tags.
V8_INLINE Address DecompressTaggedAnyImpl(Address on_heap_addr, int32_t value) {
  // |root_mask| is 0 if the |value| was a smi or -1 otherwise.
  Address root_mask = -static_cast<Address>(value & kSmiTagMask);
  Address root_or_zero = root_mask & GetRootFromOnHeapAddress(on_heap_addr);
  // Current compression scheme requires value to be sign-extended to inptr_t
  // before adding the |root_or_zero|.
  return root_or_zero + static_cast<Address>(static_cast<intptr_t>(value));
}

// Decompresses any tagged value, preserving both weak- and smi- tags and checks
// that the result of decompression matches full value stored in the field.
// Checked decompression helps to find misuses of XxxSlots and FullXxxSlots.
// TODO(ishell): remove in favour of DecompressTaggedAnyImpl() once
// kTaggedSize is equal to kInt32Size.
V8_INLINE Address DecompressTaggedAny(Address on_heap_addr,
                                      Tagged_t full_value) {
  // Use only lower 32-bits of the value for decompression.
  int32_t compressed = static_cast<int32_t>(full_value);
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  Address result = DecompressTaggedAnyImpl(on_heap_addr, compressed);
#ifdef DEBUG
  if (full_value != result) {
    base::OS::DebugBreak();
    result = DecompressTaggedAnyImpl(on_heap_addr, compressed);
  }
#endif
  DCHECK_EQ(full_value, result);
  return result;
}

//
// CompressedObjectSlot implementation.
//

CompressedObjectSlot::CompressedObjectSlot(Object* object)
    : SlotBase(reinterpret_cast<Address>(&object->ptr_)) {}

Object CompressedObjectSlot::operator*() const {
  Tagged_t value = *location();
  return Object(DecompressTaggedAny(address(), value));
}

void CompressedObjectSlot::store(Object value) const {
  *location() = CompressTagged(value->ptr());
}

Object CompressedObjectSlot::Acquire_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(location());
  return Object(DecompressTaggedAny(address(), value));
}

Object CompressedObjectSlot::Relaxed_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return Object(DecompressTaggedAny(address(), value));
}

void CompressedObjectSlot::Relaxed_Store(Object value) const {
  Tagged_t ptr = CompressTagged(value->ptr());
  AsAtomicTagged::Relaxed_Store(location(), ptr);
}

void CompressedObjectSlot::Release_Store(Object value) const {
  Tagged_t ptr = CompressTagged(value->ptr());
  AsAtomicTagged::Release_Store(location(), ptr);
}

Object CompressedObjectSlot::Release_CompareAndSwap(Object old,
                                                    Object target) const {
  Tagged_t old_ptr = CompressTagged(old->ptr());
  Tagged_t target_ptr = CompressTagged(target->ptr());
  Tagged_t result =
      AsAtomicTagged::Release_CompareAndSwap(location(), old_ptr, target_ptr);
  return Object(DecompressTaggedAny(address(), result));
}

//
// CompressedMapWordSlot implementation.
//

bool CompressedMapWordSlot::contains_value(Address raw_value) const {
  Tagged_t value = *location();
  return value == static_cast<Tagged_t>(raw_value);
}

Object CompressedMapWordSlot::operator*() const {
  Tagged_t value = *location();
  return Object(DecompressTaggedPointer(address(), value));
}

void CompressedMapWordSlot::store(Object value) const {
  *location() = CompressTagged(value.ptr());
}

Object CompressedMapWordSlot::Relaxed_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return Object(DecompressTaggedPointer(address(), value));
}

void CompressedMapWordSlot::Relaxed_Store(Object value) const {
  Tagged_t ptr = CompressTagged(value.ptr());
  AsAtomicTagged::Relaxed_Store(location(), ptr);
}

Object CompressedMapWordSlot::Acquire_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(location());
  return Object(DecompressTaggedPointer(address(), value));
}

void CompressedMapWordSlot::Release_Store(Object value) const {
  Tagged_t ptr = CompressTagged(value->ptr());
  AsAtomicTagged::Release_Store(location(), ptr);
}

Object CompressedMapWordSlot::Release_CompareAndSwap(Object old,
                                                     Object target) const {
  Tagged_t old_ptr = CompressTagged(old->ptr());
  Tagged_t target_ptr = CompressTagged(target->ptr());
  Tagged_t result =
      AsAtomicTagged::Release_CompareAndSwap(location(), old_ptr, target_ptr);
  return Object(DecompressTaggedPointer(address(), result));
}

//
// CompressedMaybeObjectSlot implementation.
//

MaybeObject CompressedMaybeObjectSlot::operator*() const {
  Tagged_t value = *location();
  return MaybeObject(DecompressTaggedAny(address(), value));
}

void CompressedMaybeObjectSlot::store(MaybeObject value) const {
  *location() = CompressTagged(value->ptr());
}

MaybeObject CompressedMaybeObjectSlot::Relaxed_Load() const {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location());
  return MaybeObject(DecompressTaggedAny(address(), value));
}

void CompressedMaybeObjectSlot::Relaxed_Store(MaybeObject value) const {
  Tagged_t ptr = CompressTagged(value->ptr());
  AsAtomicTagged::Relaxed_Store(location(), ptr);
}

void CompressedMaybeObjectSlot::Release_CompareAndSwap(
    MaybeObject old, MaybeObject target) const {
  Tagged_t old_ptr = CompressTagged(old->ptr());
  Tagged_t target_ptr = CompressTagged(target->ptr());
  AsAtomicTagged::Release_CompareAndSwap(location(), old_ptr, target_ptr);
}

//
// CompressedHeapObjectSlot implementation.
//

HeapObjectReference CompressedHeapObjectSlot::operator*() const {
  Tagged_t value = *location();
  return HeapObjectReference(DecompressTaggedPointer(address(), value));
}

void CompressedHeapObjectSlot::store(HeapObjectReference value) const {
  *location() = CompressTagged(value.ptr());
}

HeapObject CompressedHeapObjectSlot::ToHeapObject() const {
  DCHECK((*location() & kHeapObjectTagMask) == kHeapObjectTag);
  return HeapObject::cast(Object(*location()));
}

void CompressedHeapObjectSlot::StoreHeapObject(HeapObject value) const {
  *location() = value->ptr();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_64_BIT

#endif  // V8_PTR_COMPR_INL_H_
