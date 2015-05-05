// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INTRINSIC_LOWERING_H_
#define V8_COMPILER_JS_INTRINSIC_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class MachineOperatorBuilder;


// Lowers certain JS-level runtime calls.
class JSIntrinsicLowering FINAL : public Reducer {
 public:
  explicit JSIntrinsicLowering(JSGraph* jsgraph);
  ~JSIntrinsicLowering() FINAL {}

  Reduction Reduce(Node* node) FINAL;

 private:
  Reduction ReduceConstructDouble(Node* node);
  Reduction ReduceDeoptimizeNow(Node* node);
  Reduction ReduceDoubleHi(Node* node);
  Reduction ReduceDoubleLo(Node* node);
  Reduction ReduceHeapObjectGetMap(Node* node);
  Reduction ReduceIncrementStatsCounter(Node* node);
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
  Reduction ReduceValueOf(Node* node);

  Reduction Change(Node* node, const Operator* op);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b, Node* c);
  Reduction ChangeToUndefined(Node* node, Node* effect = nullptr);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INTRINSIC_LOWERING_H_
