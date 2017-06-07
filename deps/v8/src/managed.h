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
template <class CppType>
class Managed : public Foreign {
 public:
  V8_INLINE CppType* get() {
    return *(reinterpret_cast<CppType**>(foreign_address()));
  }

  static Managed<CppType>* cast(Object* obj) {
    SLOW_DCHECK(obj->IsForeign());
    return reinterpret_cast<Managed<CppType>*>(obj);
  }

  static Handle<Managed<CppType>> New(Isolate* isolate, CppType* ptr) {
    Isolate::ManagedObjectFinalizer* node =
        isolate->RegisterForReleaseAtTeardown(ptr,
                                              Managed<CppType>::NativeDelete);
    Handle<Managed<CppType>> handle = Handle<Managed<CppType>>::cast(
        isolate->factory()->NewForeign(reinterpret_cast<Address>(node)));
    RegisterWeakCallbackForDelete(isolate, handle);
    return handle;
  }

 private:
  static void RegisterWeakCallbackForDelete(Isolate* isolate,
                                            Handle<Managed<CppType>> handle) {
    Handle<Object> global_handle = isolate->global_handles()->Create(*handle);
    GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                            &Managed<CppType>::GCDelete,
                            v8::WeakCallbackType::kFinalizer);
  }

  static void GCDelete(const v8::WeakCallbackInfo<void>& data) {
    Managed<CppType>** p =
        reinterpret_cast<Managed<CppType>**>(data.GetParameter());

    Isolate::ManagedObjectFinalizer* finalizer = (*p)->GetFinalizer();

    Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
    finalizer->Dispose();
    isolate->UnregisterFromReleaseAtTeardown(&finalizer);

    (*p)->set_foreign_address(static_cast<Address>(nullptr));
    GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  }

  static void NativeDelete(void* value) {
    CppType* typed_value = reinterpret_cast<CppType*>(value);
    delete typed_value;
  }

  Isolate::ManagedObjectFinalizer* GetFinalizer() {
    return reinterpret_cast<Isolate::ManagedObjectFinalizer*>(
        foreign_address());
  }
};
}  // namespace internal
}  // namespace v8

#endif  // V8_MANAGED_H_
