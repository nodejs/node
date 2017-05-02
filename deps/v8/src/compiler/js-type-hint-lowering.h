// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPE_HINT_LOWERING_H_
#define V8_COMPILER_JS_TYPE_HINT_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/handles.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class JSGraph;

// The type-hint lowering consumes feedback about data operations (i.e. unary
// and binary operations) to emit nodes using speculative simplified operators
// in favor of the generic JavaScript operators.
//
// This lowering is implemented as an early reduction and can be applied before
// nodes are placed into the initial graph. It provides the ability to shortcut
// the JavaScript-level operators and directly emit simplified-level operators
// even during initial graph building. This is the reason this lowering doesn't
// follow the interface of the reducer framework used after graph construction.
class JSTypeHintLowering {
 public:
  JSTypeHintLowering(JSGraph* jsgraph, Handle<FeedbackVector> feedback_vector);

  // Potential reduction of binary (arithmetic, logical and shift) operations.
  Reduction ReduceBinaryOperation(const Operator* op, Node* left, Node* right,
                                  Node* effect, Node* control,
                                  FeedbackSlot slot);

 private:
  friend class JSSpeculativeBinopBuilder;

  JSGraph* jsgraph() const { return jsgraph_; }
  const Handle<FeedbackVector>& feedback_vector() const {
    return feedback_vector_;
  }

  JSGraph* jsgraph_;
  Handle<FeedbackVector> feedback_vector_;

  DISALLOW_COPY_AND_ASSIGN(JSTypeHintLowering);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_TYPE_HINT_LOWERING_H_
