// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAYBE_OBJECT_INL_H_
#define V8_OBJECTS_MAYBE_OBJECT_INL_H_

#include "src/objects/maybe-object.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/isolate.h"
#endif
#include "src/objects/heap-object-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi-inl.h"

namespace v8 {
namespace internal {

bool MaybeObject::ToSmi(Smi* value) {
  if (HAS_SMI_TAG(ptr_)) {
    *value = Smi::cast(Object(ptr_));
    return true;
  }
  return false;
}

Smi MaybeObject::ToSmi() const {
  DCHECK(HAS_SMI_TAG(ptr_));
  return Smi::cast(Object(ptr_));
}

bool MaybeObject::IsStrongOrWeak() const {
  if (IsSmi() || IsCleared()) {
    return false;
  }
  return true;
}

bool MaybeObject::GetHeapObject(HeapObject* result) const {
  if (IsSmi() || IsCleared()) {
    return false;
  }
  *result = GetHeapObject();
  return true;
}

bool MaybeObject::GetHeapObject(HeapObject* result,
                                HeapObjectReferenceType* reference_type) const {
  if (IsSmi() || IsCleared()) {
    return false;
  }
  *reference_type = HasWeakHeapObjectTag(ptr_)
                        ? HeapObjectReferenceType::WEAK
                        : HeapObjectReferenceType::STRONG;
  *result = GetHeapObject();
  return true;
}

bool MaybeObject::IsStrong() const {
  return !HasWeakHeapObjectTag(ptr_) && !IsSmi();
}

bool MaybeObject::GetHeapObjectIfStrong(HeapObject* result) const {
  if (!HasWeakHeapObjectTag(ptr_) && !IsSmi()) {
    *result = HeapObject::cast(Object(ptr_));
    return true;
  }
  return false;
}

HeapObject MaybeObject::GetHeapObjectAssumeStrong() const {
  DCHECK(IsStrong());
  return HeapObject::cast(Object(ptr_));
}

bool MaybeObject::IsWeak() const {
  return HasWeakHeapObjectTag(ptr_) && !IsCleared();
}

bool MaybeObject::IsWeakOrCleared() const { return HasWeakHeapObjectTag(ptr_); }

bool MaybeObject::GetHeapObjectIfWeak(HeapObject* result) const {
  if (IsWeak()) {
    *result = GetHeapObject();
    return true;
  }
  return false;
}

HeapObject MaybeObject::GetHeapObjectAssumeWeak() const {
  DCHECK(IsWeak());
  return GetHeapObject();
}

HeapObject MaybeObject::GetHeapObject() const {
  DCHECK(!IsSmi());
  DCHECK(!IsCleared());
  return HeapObject::cast(Object(ptr_ & ~kWeakHeapObjectMask));
}

Object MaybeObject::GetHeapObjectOrSmi() const {
  if (IsSmi()) {
    return Object(ptr_);
  }
  return GetHeapObject();
}

bool MaybeObject::IsObject() const { return IsSmi() || IsStrong(); }

MaybeObject MaybeObject::MakeWeak(MaybeObject object) {
  DCHECK(object.IsStrongOrWeak());
  return MaybeObject(object.ptr_ | kWeakHeapObjectMask);
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
  Address new_value = value->ptr();
  DCHECK(Internals::HasHeapObjectTag(new_value));

#ifdef DEBUG
  bool weak_before = HasWeakHeapObjectTag(old_value);
#endif

  slot.store(
      HeapObjectReference(new_value | (old_value & kWeakHeapObjectMask)));

#ifdef DEBUG
  bool weak_after = HasWeakHeapObjectTag((*slot).ptr());
  DCHECK_EQ(weak_before, weak_after);
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MAYBE_OBJECT_INL_H_
