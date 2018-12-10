// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_PROFILER_H_
#define V8_RUNTIME_PROFILER_H_

#include "src/allocation.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class Isolate;
class InterpretedFrame;
class JSFunction;
enum class OptimizationReason : uint8_t;

class RuntimeProfiler {
 public:
  explicit RuntimeProfiler(Isolate* isolate);

  void MarkCandidatesForOptimization();

  void NotifyICChanged() { any_ic_changed_ = true; }

  void AttemptOnStackReplacement(InterpretedFrame* frame,
                                 int nesting_levels = 1);

 private:
  void MaybeOptimize(JSFunction function, InterpretedFrame* frame);
  // Potentially attempts OSR from and returns whether no other
  // optimization attempts should be made.
  bool MaybeOSR(JSFunction function, InterpretedFrame* frame);
  OptimizationReason ShouldOptimize(JSFunction function,
                                    BytecodeArray bytecode_array);
  void Optimize(JSFunction function, OptimizationReason reason);
  void Baseline(JSFunction function, OptimizationReason reason);

  Isolate* isolate_;
  bool any_ic_changed_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_RUNTIME_PROFILER_H_
