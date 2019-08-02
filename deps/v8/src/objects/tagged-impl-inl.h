// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TAGGED_IMPL_INL_H_
#define V8_OBJECTS_TAGGED_IMPL_INL_H_

#include "src/objects/tagged-impl.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/execution/isolate.h"
#endif
#include "src/common/ptr-compr-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/smi.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::ToSmi(Smi* value) const {
  if (HAS_SMI_TAG(ptr_)) {
    *value = ToSmi();
    return true;
  }
  return false;
}

template <HeapObjectReferenceType kRefType, typename StorageType>
Smi TaggedImpl<kRefType, StorageType>::ToSmi() const {
  DCHECK(HAS_SMI_TAG(ptr_));
  if (kIsFull) {
    return Smi(ptr_);
  }
  // Implementation for compressed pointers.
  return Smi(DecompressTaggedSigned(static_cast<Tagged_t>(ptr_)));
}

//
// TaggedImpl::GetHeapObject(HeapObject* result) implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObject(
    HeapObject* result) const {
  CHECK(kIsFull);
  if (!IsStrongOrWeak()) return false;
  *result = GetHeapObject();
  return true;
}

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObject(
    ROOT_PARAM, HeapObject* result) const {
  if (kIsFull) return GetHeapObject(result);
  // Implementation for compressed pointers.
  if (!IsStrongOrWeak()) return false;
  *result = GetHeapObject(ROOT_VALUE);
  return true;
}

//
// TaggedImpl::GetHeapObject(HeapObject* result,
//                           HeapObjectReferenceType* reference_type)
// implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObject(
    HeapObject* result, HeapObjectReferenceType* reference_type) const {
  CHECK(kIsFull);
  if (!IsStrongOrWeak()) return false;
  *reference_type = IsWeakOrCleared() ? HeapObjectReferenceType::WEAK
                                      : HeapObjectReferenceType::STRONG;
  *result = GetHeapObject();
  return true;
}

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObject(
    ROOT_PARAM, HeapObject* result,
    HeapObjectReferenceType* reference_type) const {
  if (kIsFull) return GetHeapObject(result, reference_type);
  // Implementation for compressed pointers.
  if (!IsStrongOrWeak()) return false;
  *reference_type = IsWeakOrCleared() ? HeapObjectReferenceType::WEAK
                                      : HeapObjectReferenceType::STRONG;
  *result = GetHeapObject(ROOT_VALUE);
  return true;
}

//
// TaggedImpl::GetHeapObjectIfStrong(HeapObject* result) implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObjectIfStrong(
    HeapObject* result) const {
  CHECK(kIsFull);
  if (IsStrong()) {
    *result = HeapObject::cast(Object(ptr_));
    return true;
  }
  return false;
}

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObjectIfStrong(
    ROOT_PARAM, HeapObject* result) const {
  if (kIsFull) return GetHeapObjectIfStrong(result);
  // Implementation for compressed pointers.
  if (IsStrong()) {
    *result =
        HeapObject::cast(Object(DecompressTaggedPointer(ROOT_VALUE, ptr_)));
    return true;
  }
  return false;
}

//
// TaggedImpl::GetHeapObjectAssumeStrong() implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
HeapObject TaggedImpl<kRefType, StorageType>::GetHeapObjectAssumeStrong()
    const {
  CHECK(kIsFull);
  DCHECK(IsStrong());
  return HeapObject::cast(Object(ptr_));
}

template <HeapObjectReferenceType kRefType, typename StorageType>
HeapObject TaggedImpl<kRefType, StorageType>::GetHeapObjectAssumeStrong(
    ROOT_PARAM) const {
  if (kIsFull) return GetHeapObjectAssumeStrong();
  // Implementation for compressed pointers.
  DCHECK(IsStrong());
  return HeapObject::cast(Object(DecompressTaggedPointer(ROOT_VALUE, ptr_)));
}

//
// TaggedImpl::GetHeapObjectIfWeak(HeapObject* result) implementation
//

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObjectIfWeak(
    HeapObject* result) const {
  CHECK(kIsFull);
  if (kCanBeWeak) {
    if (IsWeak()) {
      *result = GetHeapObject();
      return true;
    }
    return false;
  } else {
    DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(ptr_));
    return false;
  }
}

template <HeapObjectReferenceType kRefType, typename StorageType>
bool TaggedImpl<kRefType, StorageType>::GetHeapObjectIfWeak(
    ROOT_PARAM, HeapObject* result) const {
  if (kIsFull) return GetHeapObjectIfWeak(result);
  // Implementation for compressed pointers.
  if (kCanBeWeak) {
    if (IsWeak()) {
      *result = GetHeapObject(ROOT_VALUE);
      return true;
    }
    return false;
  } else {
    DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(ptr_));
    return false;
  }
}

//
// TaggedImpl::GetHeapObjectAssumeWeak() implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
HeapObject TaggedImpl<kRefType, StorageType>::GetHeapObjectAssumeWeak() const {
  CHECK(kIsFull);
  DCHECK(IsWeak());
  return GetHeapObject();
}

template <HeapObjectReferenceType kRefType, typename StorageType>
HeapObject TaggedImpl<kRefType, StorageType>::GetHeapObjectAssumeWeak(
    ROOT_PARAM) const {
  if (kIsFull) return GetHeapObjectAssumeWeak();
  // Implementation for compressed pointers.
  DCHECK(IsWeak());
  return GetHeapObject(ROOT_VALUE);
}

//
// TaggedImpl::GetHeapObject() implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
HeapObject TaggedImpl<kRefType, StorageType>::GetHeapObject() const {
  CHECK(kIsFull);
  DCHECK(!IsSmi());
  if (kCanBeWeak) {
    DCHECK(!IsCleared());
    return HeapObject::cast(Object(ptr_ & ~kWeakHeapObjectMask));
  } else {
    DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(ptr_));
    return HeapObject::cast(Object(ptr_));
  }
}

template <HeapObjectReferenceType kRefType, typename StorageType>
HeapObject TaggedImpl<kRefType, StorageType>::GetHeapObject(ROOT_PARAM) const {
  if (kIsFull) return GetHeapObject();
  // Implementation for compressed pointers.
  DCHECK(!IsSmi());
  if (kCanBeWeak) {
    DCHECK(!IsCleared());
    return HeapObject::cast(Object(
        DecompressTaggedPointer(ROOT_VALUE, ptr_ & ~kWeakHeapObjectMask)));
  } else {
    DCHECK(!HAS_WEAK_HEAP_OBJECT_TAG(ptr_));
    return HeapObject::cast(Object(DecompressTaggedPointer(ROOT_VALUE, ptr_)));
  }
}

//
// TaggedImpl::GetHeapObjectOrSmi() implementation.
//

template <HeapObjectReferenceType kRefType, typename StorageType>
Object TaggedImpl<kRefType, StorageType>::GetHeapObjectOrSmi() const {
  CHECK(kIsFull);
  if (IsSmi()) {
    return Object(ptr_);
  }
  return GetHeapObject();
}

template <HeapObjectReferenceType kRefType, typename StorageType>
Object TaggedImpl<kRefType, StorageType>::GetHeapObjectOrSmi(ROOT_PARAM) const {
  if (kIsFull) return GetHeapObjectOrSmi();
  // Implementation for compressed pointers.
  if (IsSmi()) {
    return Object(DecompressTaggedSigned(ptr_));
  }
  return GetHeapObject(ROOT_VALUE);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_TAGGED_IMPL_INL_H_
