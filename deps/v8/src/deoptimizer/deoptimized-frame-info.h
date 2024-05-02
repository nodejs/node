// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_DEOPTIMIZED_FRAME_INFO_H_
#define V8_DEOPTIMIZER_DEOPTIMIZED_FRAME_INFO_H_

#include <vector>

#include "src/deoptimizer/translated-state.h"

namespace v8 {
namespace internal {

// Class used to represent an unoptimized frame when the debugger
// needs to inspect a frame that is part of an optimized frame. The
// internally used FrameDescription objects are not GC safe so for use
// by the debugger frame information is copied to an object of this type.
// Represents parameters in unadapted form so their number might mismatch
// formal parameter count.
class DeoptimizedFrameInfo : public Malloced {
 public:
  DeoptimizedFrameInfo(TranslatedState* state,
                       TranslatedState::iterator frame_it, Isolate* isolate);

  // Get the frame context.
  Handle<Object> GetContext() { return context_; }

  // Get an incoming argument.
  Handle<Object> GetParameter(int index) {
    DCHECK(0 <= index && index < parameters_count());
    return parameters_[index];
  }

  // Get an expression from the expression stack.
  Handle<Object> GetExpression(int index) {
    DCHECK(0 <= index && index < expression_count());
    return expression_stack_[index];
  }

 private:
  // Return the number of incoming arguments.
  int parameters_count() { return static_cast<int>(parameters_.size()); }

  // Return the height of the expression stack.
  int expression_count() { return static_cast<int>(expression_stack_.size()); }

  // Set an incoming argument.
  void SetParameter(int index, Handle<Object> obj) {
    DCHECK(0 <= index && index < parameters_count());
    parameters_[index] = obj;
  }

  // Set an expression on the expression stack.
  void SetExpression(int index, Handle<Object> obj) {
    DCHECK(0 <= index && index < expression_count());
    expression_stack_[index] = obj;
  }

  Handle<Object> context_;
  std::vector<Handle<Object>> parameters_;
  std::vector<Handle<Object>> expression_stack_;

  friend class Deoptimizer;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_DEOPTIMIZED_FRAME_INFO_H_
