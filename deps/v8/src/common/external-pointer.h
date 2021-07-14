// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_EXTERNAL_POINTER_H_
#define V8_COMMON_EXTERNAL_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Convert external pointer from on-V8-heap representation to an actual external
// pointer value.
V8_INLINE Address DecodeExternalPointer(const Isolate* isolate,
                                        ExternalPointer_t encoded_pointer,
                                        ExternalPointerTag tag);

constexpr ExternalPointer_t kNullExternalPointer = 0;

// Creates uninitialized entry in external pointer table and writes the entry id
// to the field.
// When sandbox is not enabled, it's a no-op.
V8_INLINE void InitExternalPointerField(Address field_address,
                                        Isolate* isolate);

// Creates and initializes entry in external pointer table and writes the entry
// id to the field.
// Basically, it's InitExternalPointerField() followed by
// WriteExternalPointerField().
V8_INLINE void InitExternalPointerField(Address field_address, Isolate* isolate,
                                        Address value, ExternalPointerTag tag);

// Reads external pointer for the field, and decodes it if the sandbox is
// enabled.
V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           const Isolate* isolate,
                                           ExternalPointerTag tag);

// Encodes value if the sandbox is enabled and writes it into the field.
V8_INLINE void WriteExternalPointerField(Address field_address,
                                         Isolate* isolate, Address value,
                                         ExternalPointerTag tag);

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_EXTERNAL_POINTER_H_
