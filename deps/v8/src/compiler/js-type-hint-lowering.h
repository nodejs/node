// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPE_HINT_LOWERING_H_
#define V8_COMPILER_JS_TYPE_HINT_LOWERING_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/deoptimize-reason.h"
#include "src/handles.h"

namespace v8 {
namespace internal {

// Forward declarations.
class FeedbackNexus;
class FeedbackSlot;

namespace compiler {

// Forward declarations.
class JSGraph;
class Node;
class Operator;

// The type-hint lowering consumes feedback about high-level operations in order
// to potentially emit nodes using speculative simplified operators in favor of
// the generic JavaScript operators.
//
// This lowering is implemented as an early reduction and can be applied before
// nodes are placed into the initial graph. It provides the ability to shortcut
// the JavaScript-level operators and directly emit simplified-level operators
// even during initial graph building. This is the reason this lowering doesn't
// follow the interface of the reducer framework used after graph construction.
//
// Also note that all reductions returned by this lowering will not produce any
// control-output, but might very well produce an effect-output. The one node
// returned as a replacement must fully describe the effect (i.e. produce the
// effect and carry {Operator::Property} for the entire lowering). Use-sites
// rely on this invariant, if it ever changes we need to switch the interface
// away from using the {Reduction} class.
class JSTypeHintLowering {
 public:
  // Flags that control the mode of operation.
  enum Flag { kNoFlags = 0u, kBailoutOnUninitialized = 1u << 1 };
  typedef base::Flags<Flag> Flags;

  JSTypeHintLowering(JSGraph* jsgraph, Handle<FeedbackVector> feedback_vector,
                     Flags flags);

  // Potential reduction of binary (arithmetic, logical, shift and relational
  // comparison) operations.
  Reduction ReduceBinaryOperation(const Operator* op, Node* left, Node* right,
                                  Node* effect, Node* control,
                                  FeedbackSlot slot) const;

  // Potential reduction to ToNumber operations
  Reduction ReduceToNumberOperation(Node* value, Node* effect, Node* control,
                                    FeedbackSlot slot) const;

  // Potential reduction of property access operations.
  Reduction ReduceLoadNamedOperation(const Operator* op, Node* obj,
                                     Node* effect, Node* control,
                                     FeedbackSlot slot) const;
  Reduction ReduceLoadKeyedOperation(const Operator* op, Node* obj, Node* key,
                                     Node* effect, Node* control,
                                     FeedbackSlot slot) const;
  Reduction ReduceStoreNamedOperation(const Operator* op, Node* obj, Node* val,
                                      Node* effect, Node* control,
                                      FeedbackSlot slot) const;
  Reduction ReduceStoreKeyedOperation(const Operator* op, Node* obj, Node* key,
                                      Node* val, Node* effect, Node* control,
                                      FeedbackSlot slot) const;

 private:
  friend class JSSpeculativeBinopBuilder;
  Node* TryBuildSoftDeopt(FeedbackNexus& nexus, Node* effect, Node* control,
                          DeoptimizeReason reson) const;

  JSGraph* jsgraph() const { return jsgraph_; }
  Flags flags() const { return flags_; }
  const Handle<FeedbackVector>& feedback_vector() const {
    return feedback_vector_;
  }

  JSGraph* jsgraph_;
  Flags const flags_;
  Handle<FeedbackVector> feedback_vector_;

  DISALLOW_COPY_AND_ASSIGN(JSTypeHintLowering);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_TYPE_HINT_LOWERING_H_
