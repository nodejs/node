// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/managed.h"

namespace v8 {
namespace internal {

namespace {
// Called by the GC in its second pass when a Managed<CppType> is
// garbage collected.
void ManagedObjectFinalizerSecondPass(const v8::WeakCallbackInfo<void>& data) {
  auto destructor =
      reinterpret_cast<ManagedPtrDestructor*>(data.GetParameter());
  int64_t adjustment = 0 - static_cast<int64_t>(destructor->estimated_size_);
  destructor->destructor_(destructor->shared_ptr_ptr_);
  delete destructor;
  data.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(adjustment);
}
}  // namespace

// Called by the GC in its first pass when a Managed<CppType> is
// garbage collected.
void ManagedObjectFinalizer(const v8::WeakCallbackInfo<void>& data) {
  auto destructor =
      reinterpret_cast<ManagedPtrDestructor*>(data.GetParameter());
  GlobalHandles::Destroy(destructor->global_handle_location_);
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  isolate->UnregisterManagedPtrDestructor(destructor);
  // We need to do the main work as a second pass callback because
  // it can trigger garbage collection. The first pass callbacks
  // are not allowed to invoke V8 API.
  data.SetSecondPassCallback(&ManagedObjectFinalizerSecondPass);
}

}  // namespace internal
}  // namespace v8
