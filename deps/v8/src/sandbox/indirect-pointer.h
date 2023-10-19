// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_H_
#define V8_SANDBOX_INDIRECT_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Reads the IndirectPointerHandle from the field and loads the Object
// referenced by this handle from the pointer table. Currently, only Code
// objects are referenced through indirect pointers, so this function will
// always use the code pointer table. If we ever have multiple tables for
// storing indirect pointers, then the table identifier could be passed as a
// template parameter, or the table itself could be provided as a through a
// (regular) parameter. Alternatively, if only Code objects use the code
// pointer table, and all other indirectly-referenced objects use another
// table, then we might want to move the function to load a Code object into
// code-pointer.h instead.
// Only available when the sandbox is enabled.
V8_INLINE Object ReadIndirectPointerField(Address field_address);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_H_
