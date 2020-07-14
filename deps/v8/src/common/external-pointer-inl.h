// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_EXTERNAL_POINTER_INL_H_
#define V8_COMMON_EXTERNAL_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/common/external-pointer.h"
#include "src/execution/isolate.h"

namespace v8 {
namespace internal {

V8_INLINE ExternalPointer_t EncodeExternalPointer(Isolate* isolate,
                                                  Address external_pointer) {
  STATIC_ASSERT(kExternalPointerSize == kSystemPointerSize);
  if (!V8_HEAP_SANDBOX_BOOL) return external_pointer;
  return external_pointer ^ kExternalPointerSalt;
}

V8_INLINE Address DecodeExternalPointer(const Isolate* isolate,
                                        ExternalPointer_t encoded_pointer) {
  STATIC_ASSERT(kExternalPointerSize == kSystemPointerSize);
  if (!V8_HEAP_SANDBOX_BOOL) return encoded_pointer;
  return encoded_pointer ^ kExternalPointerSalt;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_EXTERNAL_POINTER_INL_H_
