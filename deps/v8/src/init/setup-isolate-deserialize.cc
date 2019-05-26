// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/setup-isolate.h"

#include "src/base/logging.h"
#include "src/execution/isolate.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate) {
  CHECK(!create_heap_objects_);
  // No actual work to be done; builtins will be deserialized from the snapshot.
}

bool SetupIsolateDelegate::SetupHeap(Heap* heap) {
  CHECK(!create_heap_objects_);
  // No actual work to be done; heap will be deserialized from the snapshot.
  return true;
}

}  // namespace internal
}  // namespace v8
