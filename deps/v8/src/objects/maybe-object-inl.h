// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_INL_H_
#define V8_OBJECTS_MAYBE_OBJECT_INL_H_

#include "src/objects/maybe-object.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

bool MaybeObject::ToSmi(Smi** value) {
  if (HAS_SMI_TAG(this)) {
    *value = Smi::cast(reinterpret_cast<Object*>(this));
    return true;
  }
  return false;
}

Smi* MaybeObject::ToSmi() {
  DCHECK(HAS_SMI_TAG(this));
  return Smi::cast(reinterpret_cast<Object*>(this));
}

bool MaybeObject::IsStrongOrWeakHeapObject() const {
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

bool MaybeObject::IsStrongHeapObject() const {
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

bool MaybeObject::IsWeakHeapObject() const {
  return HasWeakHeapObjectTag(this) && !IsClearedWeakHeapObject();
}

bool MaybeObject::IsWeakOrClearedHeapObject() const {
  return HasWeakHeapObjectTag(this);
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

Object* MaybeObject::GetHeapObjectOrSmi() {
  if (IsSmi()) {
    return reinterpret_cast<Object*>(this);
  }
  return GetHeapObject();
}

bool MaybeObject::IsObject() const { return IsSmi() || IsStrongHeapObject(); }

Object* MaybeObject::ToObject() {
  DCHECK(!HasWeakHeapObjectTag(this));
  return reinterpret_cast<Object*>(this);
}

MaybeObject* MaybeObject::MakeWeak(MaybeObject* object) {
  DCHECK(object->IsStrongOrWeakHeapObject());
  return AddWeakHeapObjectMask(object);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_INL_H_
