// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INTRINSIC_LOWERING_H_
#define V8_COMPILER_JS_INTRINSIC_LOWERING_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSOperatorBuilder;
class JSGraph;
class MachineOperatorBuilder;


// Lowers certain JS-level runtime calls.
class JSIntrinsicLowering final : public AdvancedReducer {
 public:
  enum DeoptimizationMode { kDeoptimizationEnabled, kDeoptimizationDisabled };

  JSIntrinsicLowering(Editor* editor, JSGraph* jsgraph,
                      DeoptimizationMode mode);
  ~JSIntrinsicLowering() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceConstructDouble(Node* node);
  Reduction ReduceDateField(Node* node);
  Reduction ReduceDeoptimizeNow(Node* node);
  Reduction ReduceDoubleHi(Node* node);
  Reduction ReduceDoubleLo(Node* node);
  Reduction ReduceHeapObjectGetMap(Node* node);
  Reduction ReduceIncrementStatsCounter(Node* node);
  Reduction ReduceIsMinusZero(Node* node);
  Reduction ReduceIsInstanceType(Node* node, InstanceType instance_type);
  Reduction ReduceIsNonNegativeSmi(Node* node);
  Reduction ReduceIsSmi(Node* node);
  Reduction ReduceJSValueGetValue(Node* node);
  Reduction ReduceMapGetInstanceType(Node* node);
  Reduction ReduceMathClz32(Node* node);
  Reduction ReduceMathFloor(Node* node);
  Reduction ReduceMathSqrt(Node* node);
  Reduction ReduceSeqStringGetChar(Node* node, String::Encoding encoding);
  Reduction ReduceSeqStringSetChar(Node* node, String::Encoding encoding);
  Reduction ReduceStringGetLength(Node* node);
  Reduction ReduceUnLikely(Node* node, BranchHint hint);
  Reduction ReduceValueOf(Node* node);
  Reduction ReduceFixedArrayGet(Node* node);
  Reduction ReduceFixedArraySet(Node* node);
  Reduction ReduceGetTypeFeedbackVector(Node* node);
  Reduction ReduceGetCallerJSFunction(Node* node);
  Reduction ReduceThrowNotDateError(Node* node);
  Reduction ReduceCallFunction(Node* node);

  Reduction Change(Node* node, const Operator* op);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b, Node* c);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b, Node* c,
                   Node* d);
  Reduction ChangeToUndefined(Node* node, Node* effect = nullptr);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  MachineOperatorBuilder* machine() const;
  DeoptimizationMode mode() const { return mode_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  JSGraph* const jsgraph_;
  DeoptimizationMode const mode_;
  SimplifiedOperatorBuilder simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INTRINSIC_LOWERING_H_
