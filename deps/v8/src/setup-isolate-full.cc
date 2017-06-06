// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/base/logging.h"
#include "src/interpreter/setup-interpreter.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate,
                                         bool create_heap_objects) {
#ifdef V8_GYP_BUILD
  // Compatibility hack to keep the deprecated GYP build working.
  if (create_heap_objects) {
    SetupBuiltinsInternal(isolate);
  } else {
    isolate->builtins()->MarkInitialized();
  }
  return;
#endif
  DCHECK(create_heap_objects);
  SetupBuiltinsInternal(isolate);
}

void SetupIsolateDelegate::SetupInterpreter(
    interpreter::Interpreter* interpreter, bool create_heap_objects) {
#ifdef V8_GYP_BUILD
  // Compatibility hack to keep the deprecated GYP build working.
  if (create_heap_objects) {
    interpreter::SetupInterpreter::InstallBytecodeHandlers(interpreter);
  }
  return;
#endif
  DCHECK(create_heap_objects);
  interpreter::SetupInterpreter::InstallBytecodeHandlers(interpreter);
}

}  // namespace internal
}  // namespace v8
