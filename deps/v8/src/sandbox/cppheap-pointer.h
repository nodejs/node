// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CPPHEAP_POINTER_H_
#define V8_SANDBOX_CPPHEAP_POINTER_H_

#include "include/v8-sandbox.h"
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

template <CppHeapPointerTag tag>
V8_INLINE void WriteLazilyInitializedCppHeapPointerField(
    Address field_address, IsolateForPointerCompression isolate, Address value);

V8_INLINE void WriteLazilyInitializedCppHeapPointerField(
    Address field_address, IsolateForPointerCompression isolate, Address value,
    CppHeapPointerTag tag);

}  // namespace v8::internal

#endif  // V8_SANDBOX_CPPHEAP_POINTER_H_
