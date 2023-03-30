// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_INL_H_
#define V8_SANDBOX_EXTERNAL_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/execution/isolate.h"
#include "src/sandbox/external-pointer-table-inl.h"
#include "src/sandbox/external-pointer.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX
template <ExternalPointerTag tag>
const ExternalPointerTable& GetExternalPointerTable(const Isolate* isolate) {
  return IsSharedExternalPointerType(tag)
             ? isolate->shared_external_pointer_table()
             : isolate->external_pointer_table();
}

template <ExternalPointerTag tag>
ExternalPointerTable& GetExternalPointerTable(Isolate* isolate) {
  return IsSharedExternalPointerType(tag)
             ? isolate->shared_external_pointer_table()
             : isolate->external_pointer_table();
}
#endif  // V8_ENABLE_SANDBOX

template <ExternalPointerTag tag>
V8_INLINE void InitExternalPointerField(Address field_address, Isolate* isolate,
                                        Address value) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(tag != kExternalPointerNullTag);
  ExternalPointerTable& table = GetExternalPointerTable<tag>(isolate);
  ExternalPointerHandle handle =
      table.AllocateAndInitializeEntry(isolate, value, tag);
  // Use a Release_Store to ensure that the store of the pointer into the
  // table is not reordered after the store of the handle. Otherwise, other
  // threads may access an uninitialized table entry and crash.
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  base::AsAtomic32::Release_Store(location, handle);
#else
  WriteExternalPointerField<tag>(field_address, isolate, value);
#endif  // V8_ENABLE_SANDBOX
}

template <ExternalPointerTag tag>
V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           const Isolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(tag != kExternalPointerNullTag);
  // Handles may be written to objects from other threads so the handle needs
  // to be loaded atomically. We assume that the load from the table cannot
  // be reordered before the load of the handle due to the data dependency
  // between the two loads and therefore use relaxed memory ordering.
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  return GetExternalPointerTable<tag>(isolate).Get(handle, tag);
#else
  return ReadMaybeUnalignedValue<Address>(field_address);
#endif  // V8_ENABLE_SANDBOX
}

template <ExternalPointerTag tag>
V8_INLINE void WriteExternalPointerField(Address field_address,
                                         Isolate* isolate, Address value) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(tag != kExternalPointerNullTag);
  // See comment above for why this is a Relaxed_Load.
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  GetExternalPointerTable<tag>(isolate).Set(handle, value, tag);
#else
  WriteMaybeUnalignedValue<Address>(field_address, value);
#endif  // V8_ENABLE_SANDBOX
}

template <ExternalPointerTag tag>
V8_INLINE void WriteLazilyInitializedExternalPointerField(Address field_address,
                                                          Isolate* isolate,
                                                          Address value) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(tag != kExternalPointerNullTag);
  // See comment above for why this uses a Relaxed_Load and Release_Store.
  ExternalPointerTable& table = GetExternalPointerTable<tag>(isolate);
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  if (handle == kNullExternalPointerHandle) {
    // Field has not been initialized yet.
    ExternalPointerHandle handle =
        table.AllocateAndInitializeEntry(isolate, value, tag);
    base::AsAtomic32::Release_Store(location, handle);
  } else {
    table.Set(handle, value, tag);
  }
#else
  WriteMaybeUnalignedValue<Address>(field_address, value);
#endif  // V8_ENABLE_SANDBOX
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_EXTERNAL_POINTER_INL_H_
