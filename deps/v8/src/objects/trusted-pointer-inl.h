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
#include "src/objects/tagged-field-inl.h"
#include "src/objects/trusted-object.h"
#include "src/sandbox/indirect-pointer-inl.h"
#include "src/sandbox/trusted-pointer-table-inl.h"

namespace v8 {
namespace internal {

template <TrustedPointerTagRange tag_range>
Tagged<TrustedTypeFor<tag_range>> TrustedPointerField::ReadTrustedPointerField(
    Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate) {
  // Currently, trusted pointer loads always use acquire semantics as the
  // under-the-hood indirect pointer loads use acquire loads anyway.
  return ReadTrustedPointerField<tag_range>(host, offset, isolate,
                                            kAcquireLoad);
}

template <TrustedPointerTagRange tag_range>
Tagged<TrustedTypeFor<tag_range>> TrustedPointerField::ReadTrustedPointerField(
    Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate,
    AcquireLoadTag acquire_load) {
  Tagged<Object> object = ReadMaybeEmptyTrustedPointerField<tag_range>(
      host, offset, isolate, acquire_load);
  using ReturnType = TrustedTypeFor<tag_range>;
  return TrustedCast<ReturnType>(object);
}

template <TrustedPointerTagRange tag_range>
Tagged<Union<Smi, TrustedTypeFor<tag_range>>>
TrustedPointerField::ReadMaybeEmptyTrustedPointerField(
    Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate,
    AcquireLoadTag) {
#ifdef V8_ENABLE_SANDBOX
  // Reading a TrustedPointer field is just a ReadTrustedPointerField with the
  // proper tag.
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  Tagged<Object> object = v8::internal::ReadTrustedPointerField<tag_range>(
      field_address, isolate, kAcquireLoad);
#else
  Tagged<Object> object =
      TaggedField<Object>::Acquire_Load(host, static_cast<int>(offset));
#endif
  return TrustedCast<Union<Smi, TrustedTypeFor<tag_range>>>(object);
}

template <TrustedPointerTagRange tag_range>
void TrustedPointerField::WriteTrustedPointerField(
    Tagged<HeapObject> host, size_t offset,
    Tagged<ExposedTrustedObject> value) {
#ifdef V8_ENABLE_SANDBOX
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  v8::internal::WriteTrustedPointerField<tag_range>(field_address, value,
                                                    kReleaseStore);
#else
  TaggedField<ExposedTrustedObject>::Release_Store(
      host, static_cast<int>(offset), value);
#endif
}

bool TrustedPointerField::IsTrustedPointerFieldEmpty(Tagged<HeapObject> host,
                                                     size_t offset) {
#ifdef V8_ENABLE_SANDBOX
  Address field_address = host->ptr() + offset - kHeapObjectTag;
  TrustedPointerHandle handle = base::AsAtomic32::Acquire_Load(
      reinterpret_cast<TrustedPointerHandle*>(field_address));
  return handle == kNullTrustedPointerHandle;
#else
  return IsSmi(
      TaggedField<Object>::Acquire_Load(host, static_cast<int>(offset)));
#endif
}

template <typename IsolateT>
bool TrustedPointerField::IsTrustedPointerFieldUnpublished(
    Tagged<HeapObject> host, size_t offset, TrustedPointerTagRange tag_range,
    IsolateT isolate) {
#ifdef V8_ENABLE_SANDBOX
  TrustedPointerHandle handle =
      base::AsAtomic32::Acquire_Load(reinterpret_cast<TrustedPointerHandle*>(
          host->ptr() + offset - kHeapObjectTag));
  if (handle == kNullTrustedPointerHandle) return false;
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
      reinterpret_cast<TrustedPointerHandle*>(field_address),
      kNullTrustedPointerHandle);
#else
  TaggedField<Smi>::Release_Store(host, static_cast<int>(offset), Smi::zero());
#endif
}

// Currently, trusted pointer loads/stores always use acquire/release
// semantics as the under-the-hood indirect pointer loads/stores use
// acquire/release loads/stores anyway.
template <typename T, TrustedPointerTagRange kTagRange>
TrustedPointerMember<T, kTagRange>::TrustedPointerMember(
    const AllocationWitness& witness, Tagged<T> value)
    : TrustedPointerMember(witness, value, witness.write_barrier_mode()) {}

template <typename T, TrustedPointerTagRange kTagRange>
TrustedPointerMember<T, kTagRange>::TrustedPointerMember(
    const AllocationWitness& witness, Tagged<T> value, WriteBarrierMode mode) {
  store(witness.object(), value, mode);
}

template <typename T, TrustedPointerTagRange kTagRange>
TrustedPointerMember<T, kTagRange>::TrustedPointerMember(
    const AllocationWitness& witness, Tagged<UnionOf<T, Undefined>> maybe_value)
    : TrustedPointerMember(witness, maybe_value, witness.write_barrier_mode()) {
}

template <typename T, TrustedPointerTagRange kTagRange>
TrustedPointerMember<T, kTagRange>::TrustedPointerMember(
    const AllocationWitness& witness, Tagged<UnionOf<T, Undefined>> maybe_value,
    WriteBarrierMode mode) {
  if (Tagged<T> value; TryCast(maybe_value, &value)) {
    store(witness.object(), value, mode);
  } else {
    DCHECK(IsUndefined(maybe_value));
    clear(witness.object());
  }
}
template <typename T, TrustedPointerTagRange kTagRange>
Tagged<T> TrustedPointerMember<T, kTagRange>::load(
    IsolateForSandbox isolate) const {
  return Acquire_Load(isolate);
}
template <typename T, TrustedPointerTagRange kTagRange>
Tagged<Object> TrustedPointerMember<T, kTagRange>::load_maybe_empty(
    IsolateForSandbox isolate) const {
  return Acquire_Load_maybe_empty(isolate);
}
template <typename T, TrustedPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::store(HeapObject* host,
                                               Tagged<T> value,
                                               WriteBarrierMode mode) {
  Release_Store(host, value, mode);
}
template <typename T, TrustedPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::store_no_write_barrier(
    HeapObject* host, Tagged<T> value) {
  Release_Store_no_write_barrier(host, value);
}

template <typename T, TrustedPointerTagRange kTagRange>
Tagged<T> TrustedPointerMember<T, kTagRange>::Acquire_Load(
    IsolateForSandbox isolate) const {
#ifdef V8_ENABLE_SANDBOX
  return TrustedCast<T>(ReadTrustedPointerHandle<kTagRange>(
      handle_.load(std::memory_order::acquire), isolate));
#else
  return Base::Acquire_Load();
#endif
}

template <typename T, TrustedPointerTagRange kTagRange>
Tagged<Object> TrustedPointerMember<T, kTagRange>::Acquire_Load_maybe_empty(
    IsolateForSandbox isolate) const {
#ifdef V8_ENABLE_SANDBOX
  TrustedPointerHandle handle = handle_.load(std::memory_order::acquire);
  if (handle == kNullTrustedPointerHandle) return Smi::zero();
  return ReadTrustedPointerHandle<kTagRange>(handle, isolate);
#else
  return Base::Acquire_Load();
#endif
}

template <typename T, TrustedPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::Release_Store(HeapObject* host,
                                                       Tagged<T> value,
                                                       WriteBarrierMode mode) {
#ifdef V8_ENABLE_SANDBOX
  handle_.store(
      Cast<ExposedTrustedObject>(value)->self_trusted_pointer_handle(),
      std::memory_order_release);
  WriteBarrier::ForTrustedPointer(host, this, value, mode);
#else
  Base::Release_Store(host, value, mode);
#endif
}

template <typename T, TrustedPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::Release_Store_no_write_barrier(
    HeapObject* host, Tagged<T> value) {
#ifdef V8_ENABLE_SANDBOX
  store(host, value, SKIP_WRITE_BARRIER);
#else
  Base::Release_Store_no_write_barrier(value);
#endif
}

template <typename T, TrustedPointerTagRange kTagRange>
bool TrustedPointerMember<T, kTagRange>::is_empty() const {
#ifdef V8_ENABLE_SANDBOX
  return handle_.load(std::memory_order_acquire) == kNullTrustedPointerHandle;
#else
  return Base::Acquire_Load().is_null();
#endif
}

template <typename T, TrustedPointerTagRange kTagRange>
bool TrustedPointerMember<T, kTagRange>::is_unpublished(
    IsolateForSandbox isolate) const {
#ifdef V8_ENABLE_SANDBOX
  TrustedPointerHandle handle = handle_.load(std::memory_order_acquire);
  if (handle == kNullTrustedPointerHandle) return false;
  const TrustedPointerTable& table =
      isolate.GetTrustedPointerTableFor(kTagRange);
  return table.IsUnpublished(handle);
#else
  return false;
#endif
}

template <typename T, TrustedPointerTagRange kTagRange>
void TrustedPointerMember<T, kTagRange>::clear(HeapObject* host) {
#ifdef V8_ENABLE_SANDBOX
  handle_.store(kNullTrustedPointerHandle, std::memory_order_release);
#else
  Base::Release_Store_no_write_barrier(Tagged<T>());
#endif
}

#ifdef V8_ENABLE_SANDBOX
template <typename T, TrustedPointerTagRange kTagRange>
Address TrustedPointerMember<T, kTagRange>::storage_address() const {
  return reinterpret_cast<Address>(&handle_);
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_TRUSTED_POINTER_INL_H_
