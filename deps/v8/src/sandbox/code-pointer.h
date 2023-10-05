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
V8_INLINE void InitCodePointerTableEntryField(Address field_address,
                                              Isolate* isolate,
                                              Tagged<HeapObject> owning_code,
                                              Address entrypoint);

// If the sandbox is enabled: reads the CodePointerHandle from the field and
// reads the entrypoint pointer from the corresponding CodePointerTable entry.
// If the sandbox is disabled: load the external pointer from the field.
V8_INLINE Address ReadCodeEntrypointField(Address field_address);

// If the sandbox is enabled: reads the CodePointerHandle from the field and
// writes the entrypoint pointer to the corresponding CodePointerTable entry.
// If the sandbox is disabled: writes the entrypoint pointer to the field.
V8_INLINE void WriteCodeEntrypointField(Address field_address, Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_CODE_POINTER_H_
