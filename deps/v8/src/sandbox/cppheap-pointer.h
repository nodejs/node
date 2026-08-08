// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CPPHEAP_POINTER_H_
#define V8_SANDBOX_CPPHEAP_POINTER_H_

#include "include/v8-sandbox.h"
#include "src/common/globals.h"
#include "src/sandbox/isolate.h"

namespace v8::internal {

// TODO(saelo): consider passing a CppHeapPointerTagRange as template parameter
// once C++20 is supported everywhere.
template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
V8_INLINE Address ReadCppHeapPointerField(Address field_address,
                                          IsolateForPointerCompression isolate);

V8_INLINE Address ReadCppHeapPointerField(Address field_address,
                                          IsolateForPointerCompression isolate,
                                          CppHeapPointerTagRange tag_range);

V8_INLINE void WriteLazilyInitializedCppHeapPointerField(
    Address field_address, IsolateForPointerCompression isolate, Address value,
    CppHeapPointerTag tag);

// In-object member holding a CppHeapPointer. Mirror of ExternalPointerMember
// (see src/sandbox/external-pointer.h), but backed by the CppHeapPointerTable.
// The tag is always provided at the call site since cpp_heap_wrappable fields
// are shared across multiple embedder tag ranges.
class CppHeapPointerMember {
 public:
  CppHeapPointerMember() = default;

  // Set the slot to its "lazy-null" state: kNullCppHeapPointerHandle under
  // pointer compression, kNullAddress otherwise. No table entry is allocated.
  inline void SetupLazilyInitialized();

  // Write a value. If the slot is still in its lazy-null state, a table entry
  // is allocated on the fly.
  inline void StoreLazy(IsolateForPointerCompression isolate, Address value,
                        CppHeapPointerTag tag);

  template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
  inline Address load(IsolateForPointerCompression isolate) const;
  inline Address load(IsolateForPointerCompression isolate,
                      CppHeapPointerTagRange tag_range) const;

  Address storage_address() const {
    return reinterpret_cast<Address>(storage_);
  }

 private:
  alignas(alignof(Tagged_t)) char storage_[sizeof(CppHeapPointer_t)];
};

static_assert(sizeof(CppHeapPointerMember) == kCppHeapPointerSlotSize);

}  // namespace v8::internal

#endif  // V8_SANDBOX_CPPHEAP_POINTER_H_
