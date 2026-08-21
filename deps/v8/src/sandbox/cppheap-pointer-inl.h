// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CPPHEAP_POINTER_INL_H_
#define V8_SANDBOX_CPPHEAP_POINTER_INL_H_

#include "src/sandbox/cppheap-pointer.h"
// Include the non-inl header before the rest of the headers.

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/objects/slots-inl.h"
#include "src/sandbox/cppheap-pointer-table-inl.h"
#include "src/sandbox/isolate.h"

namespace v8::internal {

// TODO(saelo): consider passing a CppHeapPointerTagRange as template parameter
// once C++20 is supported everywhere.
template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
V8_INLINE Address ReadCppHeapPointerField(
    Address field_address, IsolateForPointerCompression isolate) {
  CppHeapPointerSlot slot(field_address);
  CppHeapPointerTagRange tag_range(lower_bound, upper_bound);
#ifdef V8_COMPRESS_POINTERS
  // Handles may be written to objects from other threads so the handle needs
  // to be loaded atomically. We assume that the load from the table cannot
  // be reordered before the load of the handle due to the data dependency
  // between the two loads and therefore use relaxed memory ordering, but
  // technically we should use memory_order_consume here.
  CppHeapPointerHandle handle = slot.Relaxed_LoadHandle();
  return isolate.GetCppHeapPointerTable().Get(handle, tag_range);
#else   // !V8_COMPRESS_POINTERS
  return slot.load();
#endif  // !V8_COMPRESS_POINTERS
}

V8_INLINE Address ReadCppHeapPointerField(Address field_address,
                                          IsolateForPointerCompression isolate,
                                          CppHeapPointerTagRange tag_range) {
  CppHeapPointerSlot slot(field_address);
#ifdef V8_COMPRESS_POINTERS
  // Handles may be written to objects from other threads so the handle needs
  // to be loaded atomically. We assume that the load from the table cannot
  // be reordered before the load of the handle due to the data dependency
  // between the two loads and therefore use relaxed memory ordering, but
  // technically we should use memory_order_consume here.
  CppHeapPointerHandle handle = slot.Relaxed_LoadHandle();
  return isolate.GetCppHeapPointerTable().Get(handle, tag_range);
#else   // !V8_COMPRESS_POINTERS
  return slot.load();
#endif  // !V8_COMPRESS_POINTERS
}

V8_INLINE void WriteLazilyInitializedCppHeapPointerField(
    Address field_address, IsolateForPointerCompression isolate, Address value,
    CppHeapPointerTag tag) {
  CppHeapPointerSlot slot(field_address);
#ifdef V8_COMPRESS_POINTERS
  DCHECK_NE(tag, CppHeapPointerTag::kNullTag);
  // See comment above for why this uses a Relaxed_Load and Release_Store.
  CppHeapPointerTable& table = isolate.GetCppHeapPointerTable();
  const CppHeapPointerHandle handle = slot.Relaxed_LoadHandle();
  if (handle == kNullCppHeapPointerHandle) {
    // Field has not been initialized yet.
    const CppHeapPointerHandle new_handle = table.AllocateAndInitializeEntry(
        isolate.GetCppHeapPointerTableSpace(), value, tag);
    slot.Release_StoreHandle(new_handle);
  } else {
    table.Set(handle, value, tag);
  }
#else   // !V8_COMPRESS_POINTERS
  slot.store(value);
#endif  // !V8_COMPRESS_POINTERS
}

void CppHeapPointerMember::SetupLazilyInitialized() {
  CppHeapPointerSlot(storage_address()).init();
}

void CppHeapPointerMember::StoreLazy(IsolateForPointerCompression isolate,
                                     Address value, CppHeapPointerTag tag) {
  WriteLazilyInitializedCppHeapPointerField(storage_address(), isolate, value,
                                            tag);
}

template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
Address CppHeapPointerMember::load(IsolateForPointerCompression isolate) const {
  return ReadCppHeapPointerField<lower_bound, upper_bound>(storage_address(),
                                                           isolate);
}

Address CppHeapPointerMember::load(IsolateForPointerCompression isolate,
                                   CppHeapPointerTagRange tag_range) const {
  return ReadCppHeapPointerField(storage_address(), isolate, tag_range);
}

}  // namespace v8::internal

#endif  // V8_SANDBOX_CPPHEAP_POINTER_INL_H_
