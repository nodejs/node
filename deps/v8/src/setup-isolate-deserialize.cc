// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/base/logging.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate) {
  CHECK(!create_heap_objects_);
  // No actual work to be done; builtins will be deserialized from the snapshot.
}

void SetupIsolateDelegate::SetupInterpreter(
    interpreter::Interpreter* interpreter) {
#if defined(V8_USE_SNAPSHOT) && !defined(V8_USE_SNAPSHOT_WITH_UNWINDING_INFO)
  if (FLAG_perf_prof_unwinding_info) {
    OFStream os(stdout);
    os << "Warning: The --perf-prof-unwinding-info flag can be passed at "
          "mksnapshot time to get better results."
       << std::endl;
  }
#endif
  CHECK(interpreter->IsDispatchTableInitialized());
}

bool SetupIsolateDelegate::SetupHeap(Heap* heap) {
  CHECK(!create_heap_objects_);
  // No actual work to be done; heap will be deserialized from the snapshot.
  return true;
}

}  // namespace internal
}  // namespace v8
