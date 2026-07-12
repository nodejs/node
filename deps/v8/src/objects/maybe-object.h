// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_H_
#define V8_OBJECTS_MAYBE_OBJECT_H_

#include <type_traits>

#include "src/objects/tagged-impl.h"
#include "src/objects/tagged.h"

namespace v8 {
namespace internal {

// Cleared weak value with unset upper part, i.e. which does not belong to
// any particular pointer compression cage.
static constexpr Tagged<ClearedWeakValue> kClearedWeakValue =
    Tagged<ClearedWeakValue>(kClearedWeakHeapObjectLower32);

// Returns cleared weak value belonging to main or trusted pointer compression
// cage. This is useful for ensuring that a cleared value does not accidentally
// leak into another pointer compression cage.
inline Tagged<ClearedWeakValue> ClearedValue();
inline Tagged<ClearedWeakValue> ClearedTrustedValue();

template <typename THeapObjectSlot>
inline void UpdateHeapObjectReferenceSlot(THeapObjectSlot slot,
                                          Tagged<HeapObject> value);

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_H_
