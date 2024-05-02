// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/debug/debug-evaluate.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/init/setup-isolate.h"

namespace v8 {
namespace internal {

bool SetupIsolateDelegate::SetupHeap(Isolate* isolate,
                                     bool create_heap_objects) {
  if (!create_heap_objects) {
    CHECK(isolate->snapshot_available());
    return true;
  }
  return SetupHeapInternal(isolate);
}

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate,
                                         bool compile_builtins) {
  if (!compile_builtins) {
    CHECK(isolate->snapshot_available());
    return;
  }
  SetupBuiltinsInternal(isolate);
#ifdef DEBUG
  DebugEvaluate::VerifyTransitiveBuiltins(isolate);
#endif  // DEBUG
}

}  // namespace internal
}  // namespace v8
