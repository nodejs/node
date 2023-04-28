// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_BOUNDED_SIZE_INL_H_
#define V8_SANDBOX_BOUNDED_SIZE_INL_H_

#include "include/v8-internal.h"
#include "src/common/ptr-compr-inl.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/sandboxed-pointer.h"

namespace v8::internal {

V8_INLINE size_t ReadBoundedSizeField(Address field_address) {
#ifdef V8_ENABLE_SANDBOX
  size_t raw_value = base::ReadUnalignedValue<size_t>(field_address);
  return raw_value >> kBoundedSizeShift;
#else
  return ReadMaybeUnalignedValue<size_t>(field_address);
#endif
}

V8_INLINE void WriteBoundedSizeField(Address field_address, size_t value) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_LE(value, kMaxSafeBufferSizeForSandbox);
  size_t raw_value = value << kBoundedSizeShift;
  base::WriteUnalignedValue<size_t>(field_address, raw_value);
#else
  WriteMaybeUnalignedValue<size_t>(field_address, value);
#endif
}

}  // namespace v8::internal

#endif  // V8_SANDBOX_BOUNDED_SIZE_INL_H_
