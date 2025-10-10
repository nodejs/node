// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_INL_H_
#define V8_OBJECTS_MAYBE_OBJECT_INL_H_

#include "src/objects/maybe-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/ptr-compr-inl.h"
#include "src/objects/casting.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/objects/tagged.h"

namespace v8 {
namespace internal {

inline Tagged<ClearedWeakValue> ClearedValue() {
#ifdef V8_COMPRESS_POINTERS
  return Tagged<ClearedWeakValue>(
      V8HeapCompressionScheme::DecompressTagged(kClearedWeakHeapObjectLower32));
#else
  return kClearedWeakValue;
#endif
}

inline Tagged<ClearedWeakValue> ClearedTrustedValue() {
#ifdef V8_COMPRESS_POINTERS
  return Tagged<ClearedWeakValue>(
      TrustedSpaceCompressionScheme::DecompressTagged(
          kClearedWeakHeapObjectLower32));
#else
  return kClearedWeakValue;
#endif
}

template <typename THeapObjectSlot>
void UpdateHeapObjectReferenceSlot(THeapObjectSlot slot,
                                   Tagged<HeapObject> value) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  Address old_value = (*slot).ptr();
  DCHECK(!HAS_SMI_TAG(old_value));
  Address new_value = value.ptr();
  DCHECK(Internals::HasHeapObjectTag(new_value));

#ifdef DEBUG
  bool weak_before = HAS_WEAK_HEAP_OBJECT_TAG(old_value);
#endif

  slot.store(Cast<HeapObjectReference>(
      Tagged<MaybeObject>(new_value | (old_value & kWeakHeapObjectMask))));

#ifdef DEBUG
  bool weak_after = HAS_WEAK_HEAP_OBJECT_TAG((*slot).ptr());
  DCHECK_EQ(weak_before, weak_after);
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_INL_H_
