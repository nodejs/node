// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_POINTER_H_
#define V8_SANDBOX_CODE_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Creates and initializes an entry in the CodePointerTable and writes a handle
// for that entry into the field. The entry will contain a pointer to the
// owning code object as well as to the entrypoint.
//
// Only available when the sandbox is enabled.
V8_INLINE void InitCodePointerTableEntryField(Address field_address,
                                              Isolate* isolate,
                                              Tagged<HeapObject> owning_code,
                                              Address entrypoint);

// Reads an indirect pointer to a Code object (a CodePointerHandle) from the
// field and reads the entrypoint pointer from the corresponding
// CodePointerTable entry.
//
// Only available when the sandbox is enabled.
V8_INLINE Address
ReadCodeEntrypointViaIndirectPointerField(Address field_address);

// Reads an indirect pointer to a Code object (a CodePointerHandle) from the
// field and writes the entrypoint pointer to the corresponding
// CodePointerTable entry.
//
// Only available when the sandbox is enabled.
V8_INLINE void WriteCodeEntrypointViaIndirectPointerField(Address field_address,
                                                          Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_CODE_POINTER_H_
