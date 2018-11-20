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
  Reduction ReduceInlineIsSmi(Node* node);
  Reduction ReduceInlineIsNonNegativeSmi(Node* node);
  Reduction ReduceInlineIsInstanceType(Node* node, InstanceType instance_type);
  Reduction ReduceInlineValueOf(Node* node);

  Reduction Change(Node* node, const Operator* op);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b, Node* c);

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
