// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_H_
#define V8_SANDBOX_EXTERNAL_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

constexpr ExternalPointer_t kNullExternalPointer = 0;
constexpr ExternalPointerHandle kNullExternalPointerHandle = 0;

// Creates zero-initialized entry in external pointer table and writes the entry
// id to the field. When sandbox is not enabled, it's a no-op.
template <ExternalPointerTag tag>
V8_INLINE void InitExternalPointerField(Address field_address,
                                        Isolate* isolate);

// Creates and initializes entry in external pointer table and writes the entry
// id to the field.
// Basically, it's InitExternalPointerField() followed by
// WriteExternalPointerField().
template <ExternalPointerTag tag>
V8_INLINE void InitExternalPointerField(Address field_address, Isolate* isolate,
                                        Address value);

// Reads external pointer for the field, and decodes it if the sandbox is
// enabled.
template <ExternalPointerTag tag>
V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           const Isolate* isolate);

// Encodes value if the sandbox is enabled and writes it into the field.
template <ExternalPointerTag tag>
V8_INLINE void WriteExternalPointerField(Address field_address,
                                         Isolate* isolate, Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_EXTERNAL_POINTER_H_
