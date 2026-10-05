// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MANAGED_INL_H_
#define V8_OBJECTS_MANAGED_INL_H_

#include "src/objects/managed.h"
// Include the non-inl header before the rest of the headers.

#include "include/cppgc/allocation.h"
#include "include/cppgc/heap.h"
#include "include/v8-isolate.h"
#include "include/v8-sandbox.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object-field-inl.h"

namespace v8::internal {

namespace detail {
// Called by either isolate shutdown or the {ManagedObjectFinalizer} in order
// to actually delete the shared pointer and decrement the shared refcount.
template <typename CppType>
void Destructor(void* ptr) {
  auto shared_ptr_ptr = reinterpret_cast<std::shared_ptr<CppType>*>(ptr);
  delete shared_ptr_ptr;
}
}  // namespace detail

// static
template <class CppType>
DirectHandle<Managed<CppType>> Managed<CppType>::From(
    Isolate* isolate, size_t estimated_size,
    std::shared_ptr<CppType> shared_ptr, AllocationType allocation_type) {
  SharedObjectConditionalSafePublishGuard publish_guard(allocation_type);
  static constexpr ExternalPointerTag kTag = TagForManaged<CppType>::value;
  static_assert(IsManagedExternalPointerType(kTag));
  SharedFlag shared = SharedFlag(IsSharedAllocationType(allocation_type));
  auto destructor = new ManagedPtrDestructor(
      estimated_size, new std::shared_ptr<CppType>{std::move(shared_ptr)},
      detail::Destructor<CppType>, shared);
  destructor->external_memory_accounter_.Increase(
      reinterpret_cast<v8::Isolate*>(shared ? isolate->shared_space_isolate()
                                            : isolate),
      estimated_size);
  DirectHandle<Managed<CppType>> handle =
      Cast<Managed<CppType>>(isolate->factory()->NewForeign<kTag>(
          reinterpret_cast<Address>(destructor), allocation_type));
  IndirectHandle<Object> global_handle =
      isolate->global_handles()->Create(*handle);
  destructor->global_handle_location_ = global_handle.location();
  GlobalHandles::MakeWeak(destructor->global_handle_location_, destructor,
                          &ManagedObjectFinalizer,
                          v8::WeakCallbackType::kParameter);
  isolate->RegisterManagedPtrDestructor(destructor);
  return handle;
}

// static
template <class CppType>
DirectHandle<TrustedManaged<CppType>> TrustedManaged<CppType>::From(
    Isolate* isolate, size_t estimated_size,
    std::shared_ptr<CppType> shared_ptr) {
  auto destructor = new ManagedPtrDestructor(
      estimated_size, new std::shared_ptr<CppType>{std::move(shared_ptr)},
      detail::Destructor<CppType>, SharedFlag{false});
  destructor->external_memory_accounter_.Increase(
      reinterpret_cast<v8::Isolate*>(isolate), estimated_size);
  DirectHandle<TrustedManaged<CppType>> handle =
      TrustedCast<TrustedManaged<CppType>>(
          isolate->factory()->NewTrustedForeign(
              reinterpret_cast<Address>(destructor)));
  IndirectHandle<Object> global_handle =
      isolate->global_handles()->Create(Cast<Object>(*handle));
  destructor->global_handle_location_ = global_handle.location();
  GlobalHandles::MakeWeak(destructor->global_handle_location_, destructor,
                          &ManagedObjectFinalizer,
                          v8::WeakCallbackType::kParameter);
  isolate->RegisterManagedPtrDestructor(destructor);
  return handle;
}

inline CppGCManagedWrapper* CppGCManagedBase::GetWrapper() const {
  return reinterpret_cast<CppGCManagedWrapper*>(ReadCppHeapPointerField(
      offsetof(CppGCManagedBase, cpp_gc_wrapper_), Isolate::Current(),
      CppHeapPointerTag::kCppGCManagedTag));
}

inline size_t CppGCManagedBase::estimated_size() const {
  return GetWrapper()->estimated_size();
}

// static
template <class CppType>
Handle<CppGCManaged<CppType>> CppGCManaged<CppType>::Create(
    Isolate* isolate, size_t estimated_size,
    std::shared_ptr<CppType> shared_ptr, AllocationType allocation_type) {
  auto* shared_ptr_ptr = new std::shared_ptr<CppType>(std::move(shared_ptr));
  auto* destructor = cppgc::MakeGarbageCollected<CppGCManagedWrapper>(
      reinterpret_cast<v8::Isolate*>(isolate)
          ->GetCppHeap()
          ->GetAllocationHandle(),
      TypeIdForManaged<CppType>::value, estimated_size, shared_ptr_ptr,
      detail::Destructor<CppType>, reinterpret_cast<v8::Isolate*>(isolate),
      isolate->cpp_heap_isolate_alive_token());
  Tagged<CppGCManagedBase> raw =
      *isolate->factory()->NewCppGCManagedBase(allocation_type);
  raw->WriteLazilyInitializedCppHeapPointerField(
      offsetof(CppGCManagedBase, cpp_gc_wrapper_), isolate,
      reinterpret_cast<Address>(destructor),
      CppHeapPointerTag::kCppGCManagedTag);
  WriteBarrier::ForCppHeapPointer(
      raw,
      raw->RawCppHeapPointerField(offsetof(CppGCManagedBase, cpp_gc_wrapper_)),
      destructor);
  return handle(Cast<CppGCManaged<CppType>>(raw), isolate);
}

template <class CppType>
typename CppGCManaged<CppType>::Ptr CppGCManaged<CppType>::ptr() const {
  return Ptr(get());
}

template <class CppType>
CppType* CppGCManaged<CppType>::raw(
    const DisallowGarbageCollection& no_gc) const {
  return GetSharedPtr()->get();
}

template <class CppType>
std::shared_ptr<CppType> CppGCManaged<CppType>::get() const {
  return *GetSharedPtr();
}

template <class CppType>
std::shared_ptr<CppType>* CppGCManaged<CppType>::GetSharedPtr() const {
  auto wrapper = GetWrapper();
  CHECK_EQ(wrapper->type_id(), TypeIdForManaged<CppType>::value);
  return reinterpret_cast<std::shared_ptr<CppType>*>(wrapper->shared_ptr_ptr());
}

template <class CppType>
void CppGCManaged<CppType>::SetManagedObject(
    std::shared_ptr<CppType> new_managed, Isolate* isolate,
    size_t new_estimated_size) {
  CppGCManagedWrapper* wrapper = GetWrapper();
  *GetSharedPtr() = std::move(new_managed);
  wrapper->UpdateEstimatedSize(new_estimated_size, isolate);
}

}  // namespace v8::internal

#endif  // V8_OBJECTS_MANAGED_INL_H_
