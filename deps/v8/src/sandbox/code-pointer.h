// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_POINTER_H_
#define V8_SANDBOX_CODE_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Creates and initializes an entry in the external pointer table and writes the
// handle for that entry to the field.
V8_INLINE void InitCodePointerField(Address field_address, Isolate* isolate,
                                    Address value);

// If the sandbox is enabled: reads the CodePointerHandle from the field and
// loads the corresponding external pointer from the external pointer table. If
// the sandbox is disabled: load the external pointer from the field.
V8_INLINE Address ReadCodePointerField(Address field_address);

// If the sandbox is enabled: reads the CodePointerHandle from the field and
// stores the external pointer to the corresponding entry in the external
// pointer table. If the sandbox is disabled: stores the external pointer to the
// field.
V8_INLINE void WriteCodePointerField(Address field_address, Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_CODE_POINTER_H_
