// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRUSTED_POINTER_INL_H_
#define V8_OBJECTS_TRUSTED_POINTER_INL_H_

#include "src/objects/trusted-pointer.h"
// Include the non-inl header before the rest of the headers.

#include <atomic>

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/trusted-object.h"
#include "src/sandbox/indirect-pointer-inl.h"
#include "src/sandbox/trusted-pointer-table-inl.h"

namespace v8 {
namespace internal {

template <IndirectPointerTagRange tag_range>
Tagged<TrustedTypeFor<tag_range>> TrustedPointerField::ReadTrustedPointerField(
    Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate) {
  // Currently, trusted pointer loads always use acquire semantics as the
  // under-the-hood indirect pointer loads use acquire loads anyway.
  return ReadTrustedPointerField<tag_range>(host, offset, isolate,
                                            kAcquireLoad);
}

template <IndirectPointerTagRange tag_range>
Tagged<TrustedTypeFor<tag_range>> TrustedPointerField::ReadTrustedPointerField(
    Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate,
    AcquireLoadTag acquire_load) {
  Tagged<Object> object = ReadMaybeEmptyTrustedPointerField<tag_range>(
      host, offset, isolate, acquire_load);
  using ReturnType = TrustedTypeFor<tag_range>;
  return TrustedCast<ReturnType>(object);
}

template <IndirectPointerTagRange tag_range>
Tagged<Object> TrustedPointerField::ReadMaybeEmptyTrustedPointerField(
    Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate,
    AcquireLoadTag) {
#ifdef V8_ENABLE_SANDBOX
  // Reading a TrustedPointer field is just a ReadIndirectPointerField with the
  // proper tag.
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  return ReadIndirectPointerField<tag_range>(field_address, isolate,
                                             kAcquireLoad);
#else
  return TaggedField<Object>::Acquire_Load(host, static_cast<int>(offset));
#endif
}

template <IndirectPointerTagRange tag_range>
void TrustedPointerField::WriteTrustedPointerField(
    Tagged<HeapObject> host, size_t offset,
    Tagged<ExposedTrustedObject> value) {
#ifdef V8_ENABLE_SANDBOX
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  WriteIndirectPointerField<tag_range>(field_address, value, kReleaseStore);
#else
  TaggedField<ExposedTrustedObject>::Release_Store(
      host, static_cast<int>(offset), value);
#endif
}

bool TrustedPointerField::IsTrustedPointerFieldEmpty(Tagged<HeapObject> host,
                                                     size_t offset) {
#ifdef V8_ENABLE_SANDBOX
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  IndirectPointerHandle handle = base::AsAtomic32::Acquire_Load(
      reinterpret_cast<IndirectPointerHandle*>(field_address));
  return handle == kNullIndirectPointerHandle;
#else
  return IsSmi(
      TaggedField<Object>::Acquire_Load(host, static_cast<int>(offset)));
#endif
}

template <typename IsolateT>
bool TrustedPointerField::IsTrustedPointerFieldUnpublished(
    Tagged<HeapObject> host, size_t offset, IndirectPointerTagRange tag_range,
    IsolateT isolate) {
#ifdef V8_ENABLE_SANDBOX
  IndirectPointerHandle handle =
      base::AsAtomic32::Acquire_Load(reinterpret_cast<IndirectPointerHandle*>(
          host->ptr() + offset - kHeapObjectTag));
  if (handle == kNullIndirectPointerHandle) return false;
  if (handle & kCodePointerHandleMarker) return false;
  const TrustedPointerTable& table =
      isolate.GetTrustedPointerTableFor(tag_range);
  return table.IsUnpublished(handle);
#else
  return false;
#endif
}

void TrustedPointerField::ClearTrustedPointerField(Tagged<HeapObject> host,
                                                   size_t offset) {
  ClearTrustedPointerField(host, offset, kReleaseStore);
}

void TrustedPointerField::ClearTrustedPointerField(Tagged<HeapObject> host,
                                                   size_t offset,
                                                   ReleaseStoreTag) {
#ifdef V8_ENABLE_SANDBOX
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  base::AsAtomic32::Release_Store(
      reinterpret_cast<IndirectPointerHandle*>(field_address),
      kNullIndirectPointerHandle);
#else
  TaggedField<Smi>::Release_Store(host, static_cast<int>(offset), Smi::zero());
#endif
}

template <typename T, IndirectPointerTagRange kTagRange>
Tagged<T> TrustedPointerMember<T, kTagRange>::load(
    IsolateForSandbox isolate) const {
#ifdef V8_ENABLE_SANDBOX
  return TrustedCast<T>(ReadIndirectPointerHandle<kTagRange>(
      handle_.load(std::memory_order::acquire), isolate));
#else
  return member_.load();
#endif
}

template <typename T, IndirectPointerTagRange kTagRange>
Tagged<Object> TrustedPointerMember<T, kTagRange>::load_maybe_empty(
    IsolateForSandbox isolate, AcquireLoadTag) const {
#ifdef V8_ENABLE_SANDBOX
  IndirectPointerHandle handle = handle_.load(std::memory_order::acquire);
  if (handle == kNullIndirectPointerHandle) return Smi::zero();
  return ReadIndirectPointerHandle<kTagRange>(handle, isolate);
#else
  return member_.Acquire_Load();
#endif
}

template <typename T, IndirectPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::store(HeapObjectLayout* host,
                                               Tagged<T> value,
                                               WriteBarrierMode mode) {
#ifdef V8_ENABLE_SANDBOX
  handle_.store(
      Cast<ExposedTrustedObject>(value)->self_indirect_pointer_handle(),
      std::memory_order_release);
  WriteBarrier::ForIndirectPointer(host, this, value, mode);
#else
  member_.store(host, value, mode);
#endif
}

template <typename T, IndirectPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::store_no_write_barrier(
    HeapObjectLayout* host, Tagged<T> value) {
#ifdef V8_ENABLE_SANDBOX
  store(host, value, SKIP_WRITE_BARRIER);
#else
  member_.store_no_write_barrier(value);
#endif
}

template <typename T, IndirectPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::clear(HeapObjectLayout* host) {
#ifdef V8_ENABLE_SANDBOX
  handle_.store(kNullIndirectPointerHandle, std::memory_order_release);
#else
  member_.store_no_write_barrier(Tagged<T>());
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_TRUSTED_POINTER_INL_H_
