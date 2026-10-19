// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_INL_H_
#define V8_SANDBOX_INDIRECT_POINTER_INL_H_

#include "src/sandbox/indirect-pointer.h"
// Include the non-inl header before the rest of the headers.

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/heap/heap-write-barrier.h"
#include "src/sandbox/isolate-inl.h"
#include "src/sandbox/trusted-pointer-table-inl.h"

namespace v8 {
namespace internal {

V8_INLINE void InitSelfTrustedPointerField(
    Address field_address, IsolateForSandbox isolate, Tagged<HeapObject> host,
    TrustedPointerTag tag,
    TrustedPointerPublishingScope* opt_publishing_scope) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kTrustedPointerNullTag);

  TrustedPointerTable::Space* space =
      isolate.GetTrustedPointerTableSpaceFor(tag, host.address());
  TrustedPointerHandle handle =
      isolate.GetTrustedPointerTableFor(tag).AllocateAndInitializeEntry(
          space, host.ptr(), tag, opt_publishing_scope);

  // Use a Release_Store to ensure that the store of the pointer into the table
  // is not reordered after the store of the handle. Otherwise, other threads
  // may access an uninitialized table entry and crash.
  auto location = reinterpret_cast<TrustedPointerHandle*>(field_address);
  base::AsAtomic32::Release_Store(location, handle);
#else
  UNREACHABLE();
#endif
}

#ifdef V8_ENABLE_SANDBOX
template <TrustedPointerTagRange tag_range>
V8_INLINE Tagged<Object> ResolveTrustedPointerHandle(
    TrustedPointerHandle handle, IsolateForSandbox isolate) {
  const TrustedPointerTable& table =
      isolate.GetTrustedPointerTableFor(tag_range);
  return Tagged<Object>(table.Get(handle, tag_range));
}

template <TrustedPointerTagRange tag_range>
V8_INLINE Tagged<Object> ReadTrustedPointerHandle(TrustedPointerHandle handle,
                                                  IsolateForSandbox isolate) {
  return ResolveTrustedPointerHandle<tag_range>(handle, isolate);
}

#endif  // V8_ENABLE_SANDBOX

template <TrustedPointerTagRange tag_range>
V8_INLINE Tagged<Object> ReadTrustedPointerField(Address field_address,
                                                 IsolateForSandbox isolate,
                                                 AcquireLoadTag) {
#ifdef V8_ENABLE_SANDBOX
  // Load the indirect pointer handle from the object.
  // Technically, we could use memory_order_consume here as the loads are
  // dependent, but that appears to be deprecated in favor of acquire ordering.
  auto location = reinterpret_cast<TrustedPointerHandle*>(field_address);
  TrustedPointerHandle handle = base::AsAtomic32::Acquire_Load(location);
  if (handle == kNullTrustedPointerHandle) return Smi::zero();
  return ReadTrustedPointerHandle<tag_range>(handle, isolate);
#else
  return Tagged<Object>(*reinterpret_cast<Address*>(field_address));
#endif
}

template <TrustedPointerTagRange tag_range>
V8_INLINE void WriteTrustedPointerField(Address field_address,
                                        Tagged<ExposedTrustedObject> value,
                                        ReleaseStoreTag) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(!tag_range.IsEmpty());
  TrustedPointerHandle handle = value->self_trusted_pointer_handle();
  DCHECK_NE(handle, kNullTrustedPointerHandle);
  auto location = reinterpret_cast<TrustedPointerHandle*>(field_address);
  base::AsAtomic32::Release_Store(location, handle);
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_INL_H_
