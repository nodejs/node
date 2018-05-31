// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_INL_H_
#define V8_OBJECTS_MAYBE_OBJECT_INL_H_

#include "include/v8.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

bool MaybeObject::ToSmi(Smi** value) {
  if (HAS_SMI_TAG(this)) {
    *value = Smi::cast(reinterpret_cast<Object*>(this));
    return true;
  }
  return false;
}

bool MaybeObject::IsStrongOrWeakHeapObject() {
  if (IsSmi() || IsClearedWeakHeapObject()) {
    return false;
  }
  return true;
}

bool MaybeObject::ToStrongOrWeakHeapObject(HeapObject** result) {
  if (IsSmi() || IsClearedWeakHeapObject()) {
    return false;
  }
  *result = GetHeapObject();
  return true;
}

bool MaybeObject::ToStrongOrWeakHeapObject(
    HeapObject** result, HeapObjectReferenceType* reference_type) {
  if (IsSmi() || IsClearedWeakHeapObject()) {
    return false;
  }
  *reference_type = HasWeakHeapObjectTag(this)
                        ? HeapObjectReferenceType::WEAK
                        : HeapObjectReferenceType::STRONG;
  *result = GetHeapObject();
  return true;
}

bool MaybeObject::IsStrongHeapObject() {
  return !HasWeakHeapObjectTag(this) && !IsSmi();
}

bool MaybeObject::ToStrongHeapObject(HeapObject** result) {
  if (!HasWeakHeapObjectTag(this) && !IsSmi()) {
    *result = reinterpret_cast<HeapObject*>(this);
    return true;
  }
  return false;
}

HeapObject* MaybeObject::ToStrongHeapObject() {
  DCHECK(IsStrongHeapObject());
  return reinterpret_cast<HeapObject*>(this);
}

bool MaybeObject::IsWeakHeapObject() {
  return HasWeakHeapObjectTag(this) && !IsClearedWeakHeapObject();
}

bool MaybeObject::ToWeakHeapObject(HeapObject** result) {
  if (HasWeakHeapObjectTag(this) && !IsClearedWeakHeapObject()) {
    *result = GetHeapObject();
    return true;
  }
  return false;
}

HeapObject* MaybeObject::ToWeakHeapObject() {
  DCHECK(IsWeakHeapObject());
  return GetHeapObject();
}

HeapObject* MaybeObject::GetHeapObject() {
  DCHECK(!IsSmi());
  DCHECK(!IsClearedWeakHeapObject());
  return RemoveWeakHeapObjectMask(reinterpret_cast<HeapObjectReference*>(this));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_INL_H_
