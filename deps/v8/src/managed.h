// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MANAGED_H_
#define V8_MANAGED_H_

#include "src/factory.h"
#include "src/global-handles.h"
#include "src/handles.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {
// An object that wraps a pointer to a C++ object and manages its lifetime.
// The C++ object will be deleted when the managed wrapper object is
// garbage collected, or, last resort, if the isolate is torn down before GC,
// as part of Isolate::Dispose().
// Managed<CppType> may be used polymorphically as Foreign, where the held
// address is typed as CppType**. The double indirection is due to the
// use, by Managed, of Isolate::ManagedObjectFinalizer, which has a CppType*
// first field.
// Calling Foreign::set_foreign_address is not allowed on a Managed object.
template <class CppType>
class Managed : public Foreign {
  class FinalizerWithHandle : public Isolate::ManagedObjectFinalizer {
   public:
    FinalizerWithHandle(void* value,
                        Isolate::ManagedObjectFinalizer::Deleter deleter)
        : Isolate::ManagedObjectFinalizer(value, deleter) {}

    Object** global_handle_location;
  };

 public:
  V8_INLINE CppType* get() {
    return reinterpret_cast<CppType*>(GetFinalizer()->value());
  }

  static Managed<CppType>* cast(Object* obj) {
    SLOW_DCHECK(obj->IsForeign());
    return reinterpret_cast<Managed<CppType>*>(obj);
  }

  // Allocate a new CppType and wrap it in a Managed.
  template <typename... Args>
  static Handle<Managed<CppType>> Allocate(Isolate* isolate, Args&&... args) {
    CppType* ptr = new CppType(std::forward<Args>(args)...);
    return From(isolate, ptr);
  }

  // Create a Managed from an existing CppType*. Takes ownership of the passed
  // object.
  static Handle<Managed<CppType>> From(Isolate* isolate, CppType* ptr) {
    FinalizerWithHandle* finalizer =
        new FinalizerWithHandle(ptr, &NativeDelete);
    isolate->RegisterForReleaseAtTeardown(finalizer);
    Handle<Managed<CppType>> handle = Handle<Managed<CppType>>::cast(
        isolate->factory()->NewForeign(reinterpret_cast<Address>(finalizer)));
    Handle<Object> global_handle = isolate->global_handles()->Create(*handle);
    finalizer->global_handle_location = global_handle.location();
    GlobalHandles::MakeWeak(finalizer->global_handle_location,
                            handle->GetFinalizer(), &Managed<CppType>::GCDelete,
                            v8::WeakCallbackType::kParameter);

    return handle;
  }

 private:
  static void GCDelete(const v8::WeakCallbackInfo<void>& data) {
    FinalizerWithHandle* finalizer =
        reinterpret_cast<FinalizerWithHandle*>(data.GetParameter());

    Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
    isolate->UnregisterFromReleaseAtTeardown(finalizer);

    GlobalHandles::Destroy(finalizer->global_handle_location);
    NativeDelete(finalizer);
  }

  static void NativeDelete(Isolate::ManagedObjectFinalizer* finalizer) {
    CppType* typed_value = reinterpret_cast<CppType*>(finalizer->value());
    delete typed_value;
    FinalizerWithHandle* finalizer_with_handle =
        static_cast<FinalizerWithHandle*>(finalizer);
    delete finalizer_with_handle;
  }

  FinalizerWithHandle* GetFinalizer() {
    return reinterpret_cast<FinalizerWithHandle*>(foreign_address());
  }
};
}  // namespace internal
}  // namespace v8

#endif  // V8_MANAGED_H_
