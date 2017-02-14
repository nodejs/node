// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MANAGED_H_
#define V8_WASM_MANAGED_H_

#include "src/factory.h"
#include "src/global-handles.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
// An object that wraps a pointer to a C++ object and optionally deletes it
// when the managed wrapper object is garbage collected.
template <class CppType>
class Managed : public Foreign {
 public:
  V8_INLINE CppType* get() {
    return reinterpret_cast<CppType*>(foreign_address());
  }

  static Handle<Managed<CppType>> New(Isolate* isolate, CppType* ptr,
                                      bool delete_on_gc = true) {
    Handle<Foreign> foreign =
        isolate->factory()->NewForeign(reinterpret_cast<Address>(ptr));
    Handle<Managed<CppType>> handle(
        reinterpret_cast<Managed<CppType>*>(*foreign), isolate);
    if (delete_on_gc) {
      RegisterWeakCallbackForDelete(isolate, handle);
    }
    return handle;
  }

 private:
  static void RegisterWeakCallbackForDelete(Isolate* isolate,
                                            Handle<Managed<CppType>> handle) {
    Handle<Object> global_handle = isolate->global_handles()->Create(*handle);
    GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                            &Managed<CppType>::Delete,
                            v8::WeakCallbackType::kFinalizer);
  }
  static void Delete(const v8::WeakCallbackInfo<void>& data) {
    Managed<CppType>** p =
        reinterpret_cast<Managed<CppType>**>(data.GetParameter());
    delete (*p)->get();
    (*p)->set_foreign_address(0);
    GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  }
};
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MANAGED_H_
