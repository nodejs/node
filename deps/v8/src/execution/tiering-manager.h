// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_TIERING_MANAGER_H_
#define V8_EXECUTION_TIERING_MANAGER_H_

#include "src/common/assert-scope.h"
#include "src/handles/handles.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class Isolate;
class UnoptimizedFrame;
class JavaScriptFrame;
class JSFunction;
class OptimizationDecision;
enum class CodeKind : uint8_t;
enum class OptimizationReason : uint8_t;

void TraceManualRecompile(JSFunction function, CodeKind code_kind,
                          ConcurrencyMode concurrency_mode);

class TieringManager {
 public:
  explicit TieringManager(Isolate* isolate) : isolate_(isolate) {}

  void OnInterruptTick(Handle<JSFunction> function);

  void NotifyICChanged() { any_ic_changed_ = true; }

  void AttemptOnStackReplacement(UnoptimizedFrame* frame,
                                 int nesting_levels = 1);

  // For use when a JSFunction is available.
  static int InterruptBudgetFor(Isolate* isolate, JSFunction function);
  // For use when no JSFunction is available.
  static int InitialInterruptBudget();

 private:
  // Make the decision whether to optimize the given function, and mark it for
  // optimization if the decision was 'yes'.
  void MaybeOptimizeFrame(JSFunction function, JavaScriptFrame* frame,
                          CodeKind code_kind);

  // Potentially attempts OSR from and returns whether no other
  // optimization attempts should be made.
  bool MaybeOSR(JSFunction function, UnoptimizedFrame* frame);
  OptimizationDecision ShouldOptimize(JSFunction function, CodeKind code_kind,
                                      JavaScriptFrame* frame);
  void Optimize(JSFunction function, CodeKind code_kind,
                OptimizationDecision decision);
  void Baseline(JSFunction function, OptimizationReason reason);

  class V8_NODISCARD OnInterruptTickScope final {
   public:
    explicit OnInterruptTickScope(TieringManager* profiler);
    ~OnInterruptTickScope();

   private:
    TieringManager* const profiler_;
    DisallowGarbageCollection no_gc;
  };

  Isolate* const isolate_;
  bool any_ic_changed_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_TIERING_MANAGER_H_
