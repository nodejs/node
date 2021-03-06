// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/setup-isolate.h"

#include "src/base/logging.h"
#include "src/debug/debug-evaluate.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/interpreter/interpreter.h"

namespace v8 {
namespace internal {

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate) {
  if (create_heap_objects_) {
    SetupBuiltinsInternal(isolate);
#ifdef DEBUG
    DebugEvaluate::VerifyTransitiveBuiltins(isolate);
#endif  // DEBUG
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
