// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_MAYBE_HANDLES_INL_H_
#define V8_HANDLES_MAYBE_HANDLES_INL_H_

#include "src/handles/maybe-handles.h"

#include "src/handles/handles-inl.h"
#include "src/objects/maybe-object-inl.h"

namespace v8 {
namespace internal {

template <typename T>
MaybeHandle<T>::MaybeHandle(T object, Isolate* isolate)
    : MaybeHandle(handle(object, isolate)) {}

MaybeObjectHandle::MaybeObjectHandle(MaybeObject object, Isolate* isolate) {
  HeapObject heap_object;
  DCHECK(!object->IsCleared());
  if (object->GetHeapObjectIfWeak(&heap_object)) {
    handle_ = handle(heap_object, isolate);
    reference_type_ = HeapObjectReferenceType::WEAK;
  } else {
    handle_ = handle(object->cast<Object>(), isolate);
    reference_type_ = HeapObjectReferenceType::STRONG;
  }
}

MaybeObjectHandle::MaybeObjectHandle(Handle<Object> object)
    : reference_type_(HeapObjectReferenceType::STRONG), handle_(object) {}

MaybeObjectHandle::MaybeObjectHandle(Object object, Isolate* isolate)
    : reference_type_(HeapObjectReferenceType::STRONG),
      handle_(object, isolate) {}

MaybeObjectHandle::MaybeObjectHandle(Object object,
                                     HeapObjectReferenceType reference_type,
                                     Isolate* isolate)
    : reference_type_(reference_type), handle_(handle(object, isolate)) {}

MaybeObjectHandle::MaybeObjectHandle(Handle<Object> object,
                                     HeapObjectReferenceType reference_type)
    : reference_type_(reference_type), handle_(object) {}

MaybeObjectHandle MaybeObjectHandle::Weak(Handle<Object> object) {
  return MaybeObjectHandle(object, HeapObjectReferenceType::WEAK);
}

MaybeObjectHandle MaybeObjectHandle::Weak(Object object, Isolate* isolate) {
  return MaybeObjectHandle(object, HeapObjectReferenceType::WEAK, isolate);
}

MaybeObject MaybeObjectHandle::operator*() const {
  if (reference_type_ == HeapObjectReferenceType::WEAK) {
    return HeapObjectReference::Weak(*handle_.ToHandleChecked());
  } else {
    return MaybeObject::FromObject(*handle_.ToHandleChecked());
  }
}

MaybeObject MaybeObjectHandle::operator->() const {
  if (reference_type_ == HeapObjectReferenceType::WEAK) {
    return HeapObjectReference::Weak(*handle_.ToHandleChecked());
  } else {
    return MaybeObject::FromObject(*handle_.ToHandleChecked());
  }
}

Handle<Object> MaybeObjectHandle::object() const {
  return handle_.ToHandleChecked();
}

inline MaybeObjectHandle handle(MaybeObject object, Isolate* isolate) {
  return MaybeObjectHandle(object, isolate);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_MAYBE_HANDLES_INL_H_
