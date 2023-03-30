// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_FIELD_INL_H_
#define V8_OBJECTS_TAGGED_FIELD_INL_H_

#include "src/common/ptr-compr-inl.h"
#include "src/objects/tagged-field.h"

namespace v8 {
namespace internal {

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
Address TaggedField<T, kFieldOffset, CompressionScheme>::address(
    HeapObject host, int offset) {
  return host.address() + kFieldOffset + offset;
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
Tagged_t* TaggedField<T, kFieldOffset, CompressionScheme>::location(
    HeapObject host, int offset) {
  return reinterpret_cast<Tagged_t*>(address(host, offset));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
template <typename TOnHeapAddress>
Address TaggedField<T, kFieldOffset, CompressionScheme>::tagged_to_full(
    TOnHeapAddress on_heap_addr, Tagged_t tagged_value) {
#ifdef V8_COMPRESS_POINTERS
  if (kIsSmi) {
    return CompressionScheme::DecompressTaggedSigned(tagged_value);
  } else if (kIsHeapObject) {
    return CompressionScheme::DecompressTagged(on_heap_addr, tagged_value);
  } else {
    return CompressionScheme::DecompressTagged(on_heap_addr, tagged_value);
  }
#else
  return tagged_value;
#endif
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
Tagged_t TaggedField<T, kFieldOffset, CompressionScheme>::full_to_tagged(
    Address value) {
#ifdef V8_COMPRESS_POINTERS
  if (std::is_base_of<MaybeObject, T>::value) {
    return CompressionScheme::CompressAny(value);
  }
  return CompressionScheme::CompressObject(value);
#else
  return value;
#endif
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::load(HeapObject host,
                                                        int offset) {
  Tagged_t value = *location(host, offset);
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(host.ptr(), value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::load(
    PtrComprCageBase cage_base, HeapObject host, int offset) {
  Tagged_t value = *location(host, offset);
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(cage_base, value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::store(HeapObject host,
                                                            T value) {
#ifdef V8_ATOMIC_OBJECT_FIELD_WRITES
  Relaxed_Store(host, value);
#else
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset, HeapObject::kMapOffset);
  *location(host) = full_to_tagged(ptr);
#endif
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::store(HeapObject host,
                                                            int offset,
                                                            T value) {
#ifdef V8_ATOMIC_OBJECT_FIELD_WRITES
  Relaxed_Store(host, offset, value);
#else
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  *location(host, offset) = full_to_tagged(ptr);
#endif
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::Relaxed_Load(HeapObject host,
                                                                int offset) {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location(host, offset));
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(host.ptr(), value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::Relaxed_Load(
    PtrComprCageBase cage_base, HeapObject host, int offset) {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location(host, offset));
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(cage_base, value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::Relaxed_Load_Map_Word(
    PtrComprCageBase cage_base, HeapObject host) {
  AtomicTagged_t value = AsAtomicTagged::Relaxed_Load(location(host, 0));
  return T(tagged_to_full(cage_base, value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::Relaxed_Store_Map_Word(
    HeapObject host, T value) {
  AsAtomicTagged::Relaxed_Store(location(host), full_to_tagged(value.ptr()));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::Relaxed_Store(
    HeapObject host, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset, HeapObject::kMapOffset);
  AsAtomicTagged::Relaxed_Store(location(host), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::Relaxed_Store(
    HeapObject host, int offset, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  AsAtomicTagged::Relaxed_Store(location(host, offset), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::Acquire_Load(HeapObject host,
                                                                int offset) {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(location(host, offset));
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(host.ptr(), value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::Acquire_Load_No_Unpack(
    PtrComprCageBase cage_base, HeapObject host, int offset) {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(location(host, offset));
  return T(tagged_to_full(cage_base, value));
}

template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::Acquire_Load(
    PtrComprCageBase cage_base, HeapObject host, int offset) {
  AtomicTagged_t value = AsAtomicTagged::Acquire_Load(location(host, offset));
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(cage_base, value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::Release_Store(
    HeapObject host, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset, HeapObject::kMapOffset);
  AsAtomicTagged::Release_Store(location(host), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::Release_Store_Map_Word(
    HeapObject host, T value) {
  Address ptr = value.ptr();
  AsAtomicTagged::Release_Store(location(host), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::Release_Store(
    HeapObject host, int offset, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  AsAtomicTagged::Release_Store(location(host, offset), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
Tagged_t TaggedField<T, kFieldOffset,
                     CompressionScheme>::Release_CompareAndSwap(HeapObject host,
                                                                T old,
                                                                T value) {
  Tagged_t old_value = full_to_tagged(old.ptr());
  Tagged_t new_value = full_to_tagged(value.ptr());
  Tagged_t result = AsAtomicTagged::Release_CompareAndSwap(
      location(host), old_value, new_value);
  return result;
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::SeqCst_Load(HeapObject host,
                                                               int offset) {
  AtomicTagged_t value = AsAtomicTagged::SeqCst_Load(location(host, offset));
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(host.ptr(), value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::SeqCst_Load(
    PtrComprCageBase cage_base, HeapObject host, int offset) {
  AtomicTagged_t value = AsAtomicTagged::SeqCst_Load(location(host, offset));
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  return T(tagged_to_full(cage_base, value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::SeqCst_Store(
    HeapObject host, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset, HeapObject::kMapOffset);
  AsAtomicTagged::SeqCst_Store(location(host), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
void TaggedField<T, kFieldOffset, CompressionScheme>::SeqCst_Store(
    HeapObject host, int offset, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  AsAtomicTagged::SeqCst_Store(location(host, offset), full_to_tagged(ptr));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::SeqCst_Swap(HeapObject host,
                                                               int offset,
                                                               T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  AtomicTagged_t old_value =
      AsAtomicTagged::SeqCst_Swap(location(host, offset), full_to_tagged(ptr));
  return T(tagged_to_full(host.ptr(), old_value));
}

// static
template <typename T, int kFieldOffset, typename CompressionScheme>
T TaggedField<T, kFieldOffset, CompressionScheme>::SeqCst_Swap(
    PtrComprCageBase cage_base, HeapObject host, int offset, T value) {
  Address ptr = value.ptr();
  DCHECK_NE(kFieldOffset + offset, HeapObject::kMapOffset);
  AtomicTagged_t old_value =
      AsAtomicTagged::SeqCst_Swap(location(host, offset), full_to_tagged(ptr));
  return T(tagged_to_full(cage_base, old_value));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_TAGGED_FIELD_INL_H_
