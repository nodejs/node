// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_EXTERNAL_POINTER_H_
#define V8_COMMON_EXTERNAL_POINTER_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// See v8:10391 for details about V8 heap sandbox.
constexpr uint32_t kExternalPointerSalt =
    0x7fffffff & ~static_cast<uint32_t>(kHeapObjectTagMask);

static_assert(static_cast<int32_t>(kExternalPointerSalt) >= 0,
              "Salt value must be positive for better assembly code");

// Convert external pointer value into encoded form suitable for being stored
// on V8 heap.
V8_INLINE ExternalPointer_t EncodeExternalPointer(Isolate* isolate,
                                                  Address external_pointer);

// Convert external pointer from on-V8-heap representation to an actual external
// pointer value.
V8_INLINE Address DecodeExternalPointer(const Isolate* isolate,
                                        ExternalPointer_t encoded_pointer);

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_EXTERNAL_POINTER_H_
