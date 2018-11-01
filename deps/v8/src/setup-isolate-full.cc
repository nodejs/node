// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/base/logging.h"
#include "src/heap/heap-inl.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate) {
  if (create_heap_objects_) {
    SetupBuiltinsInternal(isolate);
  } else {
    CHECK(isolate->snapshot_available());
  }
}

bool SetupIsolateDelegate::SetupHeap(Heap* heap) {
  if (create_heap_objects_) {
    return SetupHeapInternal(heap);
  } else {
    CHECK(heap->isolate()->snapshot_available());
    return true;
  }
}

}  // namespace internal
}  // namespace v8
