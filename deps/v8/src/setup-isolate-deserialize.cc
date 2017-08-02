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

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate,
                                         bool create_heap_objects) {
  DCHECK(!create_heap_objects);
  // No actual work to be done; builtins will be deserialized from the snapshot.
}

void SetupIsolateDelegate::SetupInterpreter(
    interpreter::Interpreter* interpreter, bool create_heap_objects) {
  DCHECK(interpreter->IsDispatchTableInitialized());
}

}  // namespace internal
}  // namespace v8
