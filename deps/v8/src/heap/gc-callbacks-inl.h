// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_CALLBACKS_INL_H_
#define V8_HEAP_GC_CALLBACKS_INL_H_

#include "src/heap/gc-callbacks.h"
// Include the non-inl header before the rest of the headers.

#include "src/execution/local-isolate.h"
#include "src/heap/local-heap-inl.h"

namespace v8 {
namespace internal {

GCRootsProviderScope::GCRootsProviderScope(LocalIsolate* local_isolate,
                                           GCRootsProvider* provider)
    : GCRootsProviderScope(local_isolate->heap(), provider) {}

GCRootsProviderScope::GCRootsProviderScope(LocalHeap* local_heap,
                                           GCRootsProvider* provider)
    : local_heap_(local_heap) {
  CHECK_NULL(local_heap_->roots_provider_);
  local_heap_->roots_provider_ = provider;
}

GCRootsProviderScope::~GCRootsProviderScope() {
  DCHECK_NOT_NULL(local_heap_->roots_provider_);
  local_heap_->roots_provider_ = nullptr;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_CALLBACKS_INL_H_
