// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_BOUNDED_SIZE_H_
#define V8_SANDBOX_BOUNDED_SIZE_H_

#include "src/common/globals.h"

namespace v8::internal {

//
// BoundedSize accessors.
//
// A BoundedSize is just a regular size_t when the sandbox is disabled.
// However, when the sandbox is enabled, a BoundedLength is guaranteed to be in
// the range [0, kMaxSafeBufferSizeForSandbox]. This property is required to
// ensure safe access to variable-sized buffers, in particular ArrayBuffers and
// their views, located inside the sandbox.
//

V8_INLINE size_t ReadBoundedSizeField(Address field_address);

V8_INLINE void WriteBoundedSizeField(Address field_address, size_t value);

}  // namespace v8::internal

#endif  // V8_SANDBOX_BOUNDED_SIZE_H_
