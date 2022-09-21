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

// Creates and initializes an entry in the external pointer table and writes the
// handle for that entry to the field.
template <ExternalPointerTag tag>
V8_INLINE void InitExternalPointerField(Address field_address, Isolate* isolate,
                                        Address value);

// If the sandbox is enabled: reads the ExternalPointerHandle from the field and
// loads the corresponding external pointer from the external pointer table. If
// the sandbox is disabled: load the external pointer from the field.
template <ExternalPointerTag tag>
V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           const Isolate* isolate);

// If the sandbox is enabled: reads the ExternalPointerHandle from the field and
// stores the external pointer to the corresponding entry in the external
// pointer table. If the sandbox is disabled: stores the external pointer to the
// field.
template <ExternalPointerTag tag>
V8_INLINE void WriteExternalPointerField(Address field_address,
                                         Isolate* isolate, Address value);

// Writes and possibly initialized a lazily initialized external pointer field.
// When the sandbox is enabled, a lazily initialized external pointer field
// initially contains the kNullExternalPointerHandle and will only be properly
// initialized (i.e. allocate an entry in the external pointer table) once a
// value is written into it for the first time.
// If the sandbox is disabled, this is equivalent to WriteExternalPointerField.
template <ExternalPointerTag tag>
V8_INLINE void WriteLazilyInitializedExternalPointerField(Address field_address,
                                                          Isolate* isolate,
                                                          Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_EXTERNAL_POINTER_H_
