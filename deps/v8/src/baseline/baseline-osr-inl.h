// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef V8_BASELINE_BASELINE_OSR_INL_H_
#define V8_BASELINE_BASELINE_OSR_INL_H_

#include "src/execution/frames.h"
#include "src/execution/isolate-inl.h"

namespace v8 {
namespace internal {

inline void OSRInterpreterFrameToBaseline(Isolate* isolate,
                                          Handle<JSFunction> function,
                                          UnoptimizedFrame* frame) {
  IsCompiledScope is_compiled_scope(
      function->shared().is_compiled_scope(isolate));
  if (Compiler::CompileBaseline(isolate, function, Compiler::CLEAR_EXCEPTION,
                                &is_compiled_scope)) {
    if (V8_LIKELY(FLAG_use_osr)) {
      DCHECK_NOT_NULL(frame);
      if (FLAG_trace_osr) {
        CodeTracer::Scope scope(isolate->GetCodeTracer());
        PrintF(scope.file(),
               "[OSR - Entry at OSR bytecode offset %d into baseline code]\n",
               frame->GetBytecodeOffset());
      }
      frame->GetBytecodeArray().set_osr_loop_nesting_level(
          AbstractCode::kMaxLoopNestingMarker);
    }
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_BASELINE_OSR_INL_H_
