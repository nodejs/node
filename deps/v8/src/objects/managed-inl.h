// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MANAGED_INL_H_
#define V8_OBJECTS_MANAGED_INL_H_

#include "src/handles/global-handles-inl.h"
#include "src/objects/managed.h"

namespace v8 {
namespace internal {

// static
template <class CppType>
template <typename... Args>
Handle<Managed<CppType>> Managed<CppType>::Allocate(Isolate* isolate,
                                                    size_t estimated_size,
                                                    Args&&... args) {
  return FromSharedPtr(isolate, estimated_size,
                       std::make_shared<CppType>(std::forward<Args>(args)...));
}

// static
template <class CppType>
Handle<Managed<CppType>> Managed<CppType>::FromRawPtr(Isolate* isolate,
                                                      size_t estimated_size,
                                                      CppType* ptr) {
  return FromSharedPtr(isolate, estimated_size, std::shared_ptr<CppType>{ptr});
}

// static
template <class CppType>
Handle<Managed<CppType>> Managed<CppType>::FromUniquePtr(
    Isolate* isolate, size_t estimated_size,
    std::unique_ptr<CppType> unique_ptr, AllocationType allocation_type) {
  return FromSharedPtr(isolate, estimated_size, std::move(unique_ptr),
                       allocation_type);
}

// static
template <class CppType>
Handle<Managed<CppType>> Managed<CppType>::FromSharedPtr(
    Isolate* isolate, size_t estimated_size,
    std::shared_ptr<CppType> shared_ptr, AllocationType allocation_type) {
  reinterpret_cast<v8::Isolate*>(isolate)
      ->AdjustAmountOfExternalAllocatedMemory(estimated_size);
  auto destructor = new ManagedPtrDestructor(
      estimated_size, new std::shared_ptr<CppType>{std::move(shared_ptr)},
      Destructor);
  Handle<Managed<CppType>> handle =
      Handle<Managed<CppType>>::cast(isolate->factory()->NewForeign(
          reinterpret_cast<Address>(destructor), allocation_type));
  Handle<Object> global_handle = isolate->global_handles()->Create(*handle);
  destructor->global_handle_location_ = global_handle.location();
  GlobalHandles::MakeWeak(destructor->global_handle_location_, destructor,
                          &ManagedObjectFinalizer,
                          v8::WeakCallbackType::kParameter);
  isolate->RegisterManagedPtrDestructor(destructor);
  return handle;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_MANAGED_INL_H_
