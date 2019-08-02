// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_INL_H_
#define V8_OBJECTS_MAYBE_OBJECT_INL_H_

#include "src/objects/maybe-object.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/execution/isolate.h"
#endif
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-impl-inl.h"

namespace v8 {
namespace internal {

//
// MaybeObject implementation.
//

// static
MaybeObject MaybeObject::FromSmi(Smi smi) {
  DCHECK(HAS_SMI_TAG(smi.ptr()));
  return MaybeObject(smi.ptr());
}

// static
MaybeObject MaybeObject::FromObject(Object object) {
  DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(object.ptr()));
  return MaybeObject(object.ptr());
}

MaybeObject MaybeObject::MakeWeak(MaybeObject object) {
  DCHECK(object.IsStrongOrWeak());
  return MaybeObject(object.ptr() | kWeakHeapObjectMask);
}

//
// HeapObjectReference implementation.
//

HeapObjectReference::HeapObjectReference(Object object)
    : MaybeObject(object.ptr()) {}

// static
HeapObjectReference HeapObjectReference::Strong(Object object) {
  DCHECK(!object.IsSmi());
  DCHECK(!HasWeakHeapObjectTag(object));
  return HeapObjectReference(object);
}

// static
HeapObjectReference HeapObjectReference::Weak(Object object) {
  DCHECK(!object.IsSmi());
  DCHECK(!HasWeakHeapObjectTag(object));
  return HeapObjectReference(object.ptr() | kWeakHeapObjectMask);
}

// static
HeapObjectReference HeapObjectReference::ClearedValue(Isolate* isolate) {
  // Construct cleared weak ref value.
  Address raw_value = kClearedWeakHeapObjectLower32;
#ifdef V8_COMPRESS_POINTERS
  // This is necessary to make pointer decompression computation also
  // suitable for cleared weak references.
  Address isolate_root = isolate->isolate_root();
  raw_value |= isolate_root;
  DCHECK_EQ(raw_value & (~static_cast<Address>(kClearedWeakHeapObjectLower32)),
            isolate_root);
#endif
  // The rest of the code will check only the lower 32-bits.
  DCHECK_EQ(kClearedWeakHeapObjectLower32, static_cast<uint32_t>(raw_value));
  return HeapObjectReference(raw_value);
}

template <typename THeapObjectSlot>
void HeapObjectReference::Update(THeapObjectSlot slot, HeapObject value) {
  static_assert(std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
                    std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  Address old_value = (*slot).ptr();
  DCHECK(!HAS_SMI_TAG(old_value));
  Address new_value = value.ptr();
  DCHECK(Internals::HasHeapObjectTag(new_value));

#ifdef DEBUG
  bool weak_before = HAS_WEAK_HEAP_OBJECT_TAG(old_value);
#endif

  slot.store(
      HeapObjectReference(new_value | (old_value & kWeakHeapObjectMask)));

#ifdef DEBUG
  bool weak_after = HAS_WEAK_HEAP_OBJECT_TAG((*slot).ptr());
  DCHECK_EQ(weak_before, weak_after);
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_INL_H_
