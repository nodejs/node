// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/managed.h"

#include "src/handles/global-handles-inl.h"
#include "src/sandbox/external-pointer-table-inl.h"

namespace v8::internal {

namespace {
// Called by the GC in its second pass when a Managed<CppType> is
// garbage collected.
void ManagedObjectFinalizerSecondPass(const v8::WeakCallbackInfo<void>& data) {
  auto destructor =
      reinterpret_cast<ManagedPtrDestructor*>(data.GetParameter());
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  isolate->UnregisterManagedPtrDestructor(destructor);
  destructor->destructor_(destructor->shared_ptr_ptr_);
  Isolate* accounter_isolate = destructor->shared_ == SharedFlag::kYes
                                   ? isolate->shared_space_isolate()
                                   : isolate;
  destructor->external_memory_accounter_.Decrease(
      reinterpret_cast<v8::Isolate*>(accounter_isolate),
      destructor->estimated_size_);
#ifdef V8_ENABLE_SANDBOX
  destructor->ZapExternalPointerTableEntry();
#endif  // V8_ENABLE_SANDBOX
  delete destructor;
}
}  // namespace

void ManagedPtrDestructor::UpdateEstimatedSize(size_t new_estimated_size,
                                               Isolate* isolate) {
  if (estimated_size_ == new_estimated_size) return;

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (estimated_size_ < new_estimated_size) {
    external_memory_accounter_.Increase(v8_isolate,
                                        new_estimated_size - estimated_size_);
  } else {
    external_memory_accounter_.Decrease(v8_isolate,
                                        estimated_size_ - new_estimated_size);
  }
  estimated_size_ = new_estimated_size;
}

void ManagedObjectFinalizer(const v8::WeakCallbackInfo<void>& data) {
  auto destructor =
      reinterpret_cast<ManagedPtrDestructor*>(data.GetParameter());
  GlobalHandles::Destroy(destructor->global_handle_location_);
  // We need to do the main work as a second pass callback because
  // it can trigger garbage collection. The first pass callbacks
  // are not allowed to invoke V8 API.
  data.SetSecondPassCallback(&ManagedObjectFinalizerSecondPass);
}

}  // namespace v8::internal
