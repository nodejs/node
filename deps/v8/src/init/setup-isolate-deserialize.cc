// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/execution/isolate.h"
#include "src/init/setup-isolate.h"

namespace v8 {
namespace internal {

bool SetupIsolateDelegate::SetupHeap(Isolate* isolate,
                                     bool create_heap_objects) {
  // No actual work to be done; heap will be deserialized from the snapshot.
  CHECK_WITH_MSG(!create_heap_objects,
                 "Heap setup supported only in mksnapshot");
  return true;
}

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate,
                                         bool compile_builtins) {
  // No actual work to be done; builtins will be deserialized from the snapshot.
  CHECK_WITH_MSG(!compile_builtins,
                 "Builtin compilation supported only in mksnapshot");
}

}  // namespace internal
}  // namespace v8
