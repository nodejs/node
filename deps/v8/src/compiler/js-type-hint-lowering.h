// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPE_HINT_LOWERING_H_
#define V8_COMPILER_JS_TYPE_HINT_LOWERING_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/handles/handles.h"

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
// The result of the lowering is encapsulated in
// {the JSTypeHintLowering::LoweringResult} class.
class JSTypeHintLowering {
 public:
  // Flags that control the mode of operation.
  enum Flag { kNoFlags = 0u, kBailoutOnUninitialized = 1u << 1 };
  using Flags = base::Flags<Flag>;

  JSTypeHintLowering(JSGraph* jsgraph, Handle<FeedbackVector> feedback_vector,
                     Flags flags);

  // {LoweringResult} describes the result of lowering. The following outcomes
  // are possible:
  //
  // - operation was lowered to a side-effect-free operation, the resulting
  //   value, effect and control can be obtained by the {value}, {effect} and
  //   {control} methods.
  //
  // - operation was lowered to a graph exit (deoptimization). The caller
  //   should connect {effect} and {control} nodes to the end.
  //
  // - no lowering happened. The caller needs to create the generic version
  //   of the operation.
  class LoweringResult {
   public:
    Node* value() const { return value_; }
    Node* effect() const { return effect_; }
    Node* control() const { return control_; }

    bool Changed() const { return kind_ != LoweringResultKind::kNoChange; }
    bool IsExit() const { return kind_ == LoweringResultKind::kExit; }
    bool IsSideEffectFree() const {
      return kind_ == LoweringResultKind::kSideEffectFree;
    }

    static LoweringResult SideEffectFree(Node* value, Node* effect,
                                         Node* control) {
      DCHECK_NOT_NULL(effect);
      DCHECK_NOT_NULL(control);
      return LoweringResult(LoweringResultKind::kSideEffectFree, value, effect,
                            control);
    }

    static LoweringResult NoChange() {
      return LoweringResult(LoweringResultKind::kNoChange, nullptr, nullptr,
                            nullptr);
    }

    static LoweringResult Exit(Node* control) {
      return LoweringResult(LoweringResultKind::kExit, nullptr, nullptr,
                            control);
    }

   private:
    enum class LoweringResultKind { kNoChange, kSideEffectFree, kExit };

    LoweringResult(LoweringResultKind kind, Node* value, Node* effect,
                   Node* control)
        : kind_(kind), value_(value), effect_(effect), control_(control) {}

    LoweringResultKind kind_;
    Node* value_;
    Node* effect_;
    Node* control_;
  };

  // Potential reduction of unary operations (e.g. negation).
  LoweringResult ReduceUnaryOperation(const Operator* op, Node* operand,
                                      Node* effect, Node* control,
                                      FeedbackSlot slot) const;

  // Potential reduction of binary (arithmetic, logical, shift and relational
  // comparison) operations.
  LoweringResult ReduceBinaryOperation(const Operator* op, Node* left,
                                       Node* right, Node* effect, Node* control,
                                       FeedbackSlot slot) const;

  // Potential reduction to for..in operations
  LoweringResult ReduceForInNextOperation(Node* receiver, Node* cache_array,
                                          Node* cache_type, Node* index,
                                          Node* effect, Node* control,
                                          FeedbackSlot slot) const;
  LoweringResult ReduceForInPrepareOperation(Node* enumerator, Node* effect,
                                             Node* control,
                                             FeedbackSlot slot) const;

  // Potential reduction to ToNumber operations
  LoweringResult ReduceToNumberOperation(Node* value, Node* effect,
                                         Node* control,
                                         FeedbackSlot slot) const;

  // Potential reduction of call operations.
  LoweringResult ReduceCallOperation(const Operator* op, Node* const* args,
                                     int arg_count, Node* effect, Node* control,
                                     FeedbackSlot slot) const;

  // Potential reduction of construct operations.
  LoweringResult ReduceConstructOperation(const Operator* op, Node* const* args,
                                          int arg_count, Node* effect,
                                          Node* control,
                                          FeedbackSlot slot) const;
  // Potential reduction of property access operations.
  LoweringResult ReduceLoadNamedOperation(const Operator* op, Node* obj,
                                          Node* effect, Node* control,
                                          FeedbackSlot slot) const;
  LoweringResult ReduceLoadKeyedOperation(const Operator* op, Node* obj,
                                          Node* key, Node* effect,
                                          Node* control,
                                          FeedbackSlot slot) const;
  LoweringResult ReduceStoreNamedOperation(const Operator* op, Node* obj,
                                           Node* val, Node* effect,
                                           Node* control,
                                           FeedbackSlot slot) const;
  LoweringResult ReduceStoreKeyedOperation(const Operator* op, Node* obj,
                                           Node* key, Node* val, Node* effect,
                                           Node* control,
                                           FeedbackSlot slot) const;

 private:
  friend class JSSpeculativeBinopBuilder;
  Node* TryBuildSoftDeopt(FeedbackNexus& nexus,  // NOLINT(runtime/references)
                          Node* effect, Node* control,
                          DeoptimizeReason reson) const;

  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
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
