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

bool MaybeObject::IsStrongOrWeak() const {
  if (IsSmi() || IsCleared()) {
    return false;
  }
  return true;
}

bool MaybeObject::GetHeapObject(HeapObject** result) {
  if (IsSmi() || IsCleared()) {
    return false;
  }
  *result = GetHeapObject();
  return true;
}

bool MaybeObject::GetHeapObject(HeapObject** result,
                                HeapObjectReferenceType* reference_type) {
  if (IsSmi() || IsCleared()) {
    return false;
  }
  *reference_type = HasWeakHeapObjectTag(this)
                        ? HeapObjectReferenceType::WEAK
                        : HeapObjectReferenceType::STRONG;
  *result = GetHeapObject();
  return true;
}

bool MaybeObject::IsStrong() const {
  return !HasWeakHeapObjectTag(this) && !IsSmi();
}

bool MaybeObject::GetHeapObjectIfStrong(HeapObject** result) {
  if (!HasWeakHeapObjectTag(this) && !IsSmi()) {
    *result = reinterpret_cast<HeapObject*>(this);
    return true;
  }
  return false;
}

HeapObject* MaybeObject::GetHeapObjectAssumeStrong() {
  DCHECK(IsStrong());
  return reinterpret_cast<HeapObject*>(this);
}

bool MaybeObject::IsWeak() const {
  return HasWeakHeapObjectTag(this) && !IsCleared();
}

bool MaybeObject::IsWeakOrCleared() const { return HasWeakHeapObjectTag(this); }

bool MaybeObject::GetHeapObjectIfWeak(HeapObject** result) {
  if (IsWeak()) {
    *result = GetHeapObject();
    return true;
  }
  return false;
}

HeapObject* MaybeObject::GetHeapObjectAssumeWeak() {
  DCHECK(IsWeak());
  return GetHeapObject();
}

HeapObject* MaybeObject::GetHeapObject() {
  DCHECK(!IsSmi());
  DCHECK(!IsCleared());
  return RemoveWeakHeapObjectMask(reinterpret_cast<HeapObjectReference*>(this));
}

Object* MaybeObject::GetHeapObjectOrSmi() {
  if (IsSmi()) {
    return reinterpret_cast<Object*>(this);
  }
  return GetHeapObject();
}

bool MaybeObject::IsObject() const { return IsSmi() || IsStrong(); }

MaybeObject* MaybeObject::MakeWeak(MaybeObject* object) {
  DCHECK(object->IsStrongOrWeak());
  return AddWeakHeapObjectMask(object);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_INL_H_
