// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_INL_H_
#define V8_SANDBOX_INDIRECT_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/sandbox/code-pointer-table-inl.h"
#include "src/sandbox/indirect-pointer.h"
#include "src/sandbox/isolate-inl.h"
#include "src/sandbox/trusted-pointer-table-inl.h"

namespace v8 {
namespace internal {

V8_INLINE void InitSelfIndirectPointerField(Address field_address,
                                            IsolateForSandbox isolate,
                                            Tagged<HeapObject> host,
                                            IndirectPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kUnknownIndirectPointerTag);
  // TODO(saelo): in the future, we might want to CHECK here or in
  // AllocateAndInitializeEntry that the host lives in trusted space.

  IndirectPointerHandle handle;
  if (tag == kCodeIndirectPointerTag) {
    CodePointerTable::Space* space =
        isolate.GetCodePointerTableSpaceFor(field_address);
    handle =
        IsolateGroup::current()
            ->code_pointer_table()
            ->AllocateAndInitializeEntry(space, host.address(), kNullAddress,
                                         kDefaultCodeEntrypointTag);
  } else {
    TrustedPointerTable::Space* space =
        isolate.GetTrustedPointerTableSpaceFor(tag);
    handle = isolate.GetTrustedPointerTableFor(tag).AllocateAndInitializeEntry(
        space, host.ptr(), tag, isolate.GetTrustedPointerPublishingScope());
  }

  // Use a Release_Store to ensure that the store of the pointer into the table
  // is not reordered after the store of the handle. Otherwise, other threads
  // may access an uninitialized table entry and crash.
  auto location = reinterpret_cast<IndirectPointerHandle*>(field_address);
  base::AsAtomic32::Release_Store(location, handle);
#else
  UNREACHABLE();
#endif
}

namespace {
#ifdef V8_ENABLE_SANDBOX
template <IndirectPointerTag tag>
V8_INLINE Tagged<Object> ResolveTrustedPointerHandle(
    IndirectPointerHandle handle, IsolateForSandbox isolate) {
  const TrustedPointerTable& table = isolate.GetTrustedPointerTableFor(tag);
  return Tagged<Object>(table.Get(handle, tag));
}

V8_INLINE Tagged<Object> ResolveCodePointerHandle(
    IndirectPointerHandle handle) {
  CodePointerTable* table = IsolateGroup::current()->code_pointer_table();
  return Tagged<Object>(table->GetCodeObject(handle));
}
#endif  // V8_ENABLE_SANDBOX
}  // namespace

template <IndirectPointerTag tag>
V8_INLINE Tagged<Object> ReadIndirectPointerField(Address field_address,
                                                  IsolateForSandbox isolate,
                                                  AcquireLoadTag) {
#ifdef V8_ENABLE_SANDBOX
  // Load the indirect pointer handle from the object.
  // Technically, we could use memory_order_consume here as the loads are
  // dependent, but that appears to be deprecated in favor of acquire ordering.
  auto location = reinterpret_cast<IndirectPointerHandle*>(field_address);
  IndirectPointerHandle handle = base::AsAtomic32::Acquire_Load(location);

  // Resolve the handle. The tag implies the pointer table to use.
  if constexpr (tag == kUnknownIndirectPointerTag) {
    // In this case we need to check if the handle is a code pointer handle and
    // select the appropriate table based on that.
    if (handle & kCodePointerHandleMarker) {
      return ResolveCodePointerHandle(handle);
    } else {
      // TODO(saelo): once we have type tagging for entries in the trusted
      // pointer table, we could ASSUME that the top bits of the tag match the
      // instance type, which might allow the compiler to optimize subsequent
      // instance type checks.
      return ResolveTrustedPointerHandle<tag>(handle, isolate);
    }
  } else if constexpr (tag == kCodeIndirectPointerTag) {
    return ResolveCodePointerHandle(handle);
  } else {
    return ResolveTrustedPointerHandle<tag>(handle, isolate);
  }
#else
  UNREACHABLE();
#endif
}

template <IndirectPointerTag tag>
V8_INLINE void WriteIndirectPointerField(Address field_address,
                                         Tagged<ExposedTrustedObject> value,
                                         ReleaseStoreTag) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(tag != kIndirectPointerNullTag);
  IndirectPointerHandle handle = value->self_indirect_pointer_handle();
  DCHECK_NE(handle, kNullIndirectPointerHandle);
  auto location = reinterpret_cast<IndirectPointerHandle*>(field_address);
  base::AsAtomic32::Release_Store(location, handle);
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_INL_H_
