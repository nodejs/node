// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SANDBOXED_POINTER_H_
#define V8_SANDBOX_SANDBOXED_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

V8_INLINE Address ReadSandboxedPointerField(Address field_address,
                                            PtrComprCageBase cage_base);

V8_INLINE void WriteSandboxedPointerField(Address field_address,
                                          PtrComprCageBase cage_base,
                                          Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_SANDBOXED_POINTER_H_
