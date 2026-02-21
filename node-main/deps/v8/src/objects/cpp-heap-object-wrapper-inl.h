// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CPP_HEAP_OBJECT_WRAPPER_INL_H_
#define V8_OBJECTS_CPP_HEAP_OBJECT_WRAPPER_INL_H_

#include "src/objects/cpp-heap-object-wrapper.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object-inl.h"

namespace v8::internal {

// static
CppHeapObjectWrapper CppHeapObjectWrapper::From(
    Tagged<CppHeapPointerWrapperObjectT> object) {
  DCHECK(IsCppHeapPointerWrapperObject(object));
  if (IsCppHeapExternalObject(object)) {
    return CppHeapObjectWrapper(Cast<CppHeapExternalObject>(object));
  }
  DCHECK(IsJSObject(object));
  return CppHeapObjectWrapper(Cast<JSObject>(object));
}

CppHeapObjectWrapper::CppHeapObjectWrapper(Tagged<CppHeapExternalObject> object)
    : object_(object), offset_(CppHeapExternalObject::kCppHeapWrappableOffset) {
  DCHECK(IsCppHeapPointerWrapperObject(object));
}

CppHeapObjectWrapper::CppHeapObjectWrapper(Tagged<JSObject> object)
    : object_(object),
      offset_(JSAPIObjectWithEmbedderSlots::kCppHeapWrappableOffset) {
  DCHECK(IsCppHeapPointerWrapperObject(object));
  DCHECK(IsJSApiWrapperObject(object));
  DCHECK(IsJSAPIObjectWithEmbedderSlots(object_) || IsJSSpecialObject(object_));
}

void CppHeapObjectWrapper::InitializeCppHeapWrapper() {
  object_->SetupLazilyInitializedCppHeapPointerField(offset_);
}

template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
void* CppHeapObjectWrapper::GetCppHeapWrappable(
    IsolateForPointerCompression isolate) const {
  return reinterpret_cast<void*>(
      object_->ReadCppHeapPointerField<lower_bound, upper_bound>(offset_,
                                                                 isolate));
}

void* CppHeapObjectWrapper::GetCppHeapWrappable(
    IsolateForPointerCompression isolate,
    CppHeapPointerTagRange tag_range) const {
  return reinterpret_cast<void*>(
      object_->ReadCppHeapPointerField(offset_, isolate, tag_range));
}

template <CppHeapPointerTag tag>
void CppHeapObjectWrapper::SetCppHeapWrappable(
    IsolateForPointerCompression isolate, void* instance) {
  object_->WriteLazilyInitializedCppHeapPointerField<tag>(
      offset_, isolate, reinterpret_cast<Address>(instance));
  WriteBarrier::ForCppHeapPointer(
      object_, object_->RawCppHeapPointerField(offset_), instance);
}

void CppHeapObjectWrapper::SetCppHeapWrappable(
    IsolateForPointerCompression isolate, void* instance,
    CppHeapPointerTag tag) {
  object_->WriteLazilyInitializedCppHeapPointerField(
      offset_, isolate, reinterpret_cast<Address>(instance), tag);
  WriteBarrier::ForCppHeapPointer(
      object_, object_->RawCppHeapPointerField(offset_), instance);
}

}  // namespace v8::internal

#endif  // V8_OBJECTS_CPP_HEAP_OBJECT_WRAPPER_INL_H_
