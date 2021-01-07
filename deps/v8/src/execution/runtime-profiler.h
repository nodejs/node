// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_RUNTIME_PROFILER_H_
#define V8_EXECUTION_RUNTIME_PROFILER_H_

#include "src/common/assert-scope.h"
#include "src/handles/handles.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class Isolate;
class InterpretedFrame;
class JavaScriptFrame;
class JSFunction;
enum class CodeKind;
enum class OptimizationReason : uint8_t;

class RuntimeProfiler {
 public:
  explicit RuntimeProfiler(Isolate* isolate);

  // Called from the interpreter when the bytecode interrupt has been exhausted.
  void MarkCandidatesForOptimizationFromBytecode();
  // Likewise, from generated code.
  void MarkCandidatesForOptimizationFromCode();

  void NotifyICChanged() { any_ic_changed_ = true; }

  void AttemptOnStackReplacement(InterpretedFrame* frame,
                                 int nesting_levels = 1);

 private:
  // Helper function called from MarkCandidatesForOptimization*
  void MarkCandidatesForOptimization(JavaScriptFrame* frame);

  // Make the decision whether to optimize the given function, and mark it for
  // optimization if the decision was 'yes'.
  void MaybeOptimizeFrame(JSFunction function, JavaScriptFrame* frame,
                          CodeKind code_kind);

  // Potentially attempts OSR from and returns whether no other
  // optimization attempts should be made.
  bool MaybeOSR(JSFunction function, InterpretedFrame* frame);
  OptimizationReason ShouldOptimize(JSFunction function,
                                    BytecodeArray bytecode_array);
  void Optimize(JSFunction function, OptimizationReason reason,
                CodeKind code_kind);
  void Baseline(JSFunction function, OptimizationReason reason);

  class V8_NODISCARD MarkCandidatesForOptimizationScope final {
   public:
    explicit MarkCandidatesForOptimizationScope(RuntimeProfiler* profiler);
    ~MarkCandidatesForOptimizationScope();

   private:
    HandleScope handle_scope_;
    RuntimeProfiler* const profiler_;
    DisallowGarbageCollection no_gc;
  };

  Isolate* isolate_;
  bool any_ic_changed_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_RUNTIME_PROFILER_H_
