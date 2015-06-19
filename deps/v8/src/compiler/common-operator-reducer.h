// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_REDUCER_H_
#define V8_COMPILER_COMMON_OPERATOR_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;
class JSGraph;
class MachineOperatorBuilder;
class Operator;


// Performs strength reduction on nodes that have common operators.
class CommonOperatorReducer final : public Reducer {
 public:
  explicit CommonOperatorReducer(JSGraph* jsgraph) : jsgraph_(jsgraph) {}
  ~CommonOperatorReducer() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReducePhi(Node* node);
  Reduction ReduceSelect(Node* node);

  Reduction Change(Node* node, Operator const* op, Node* a);
  Reduction Change(Node* node, Operator const* op, Node* a, Node* b);

  CommonOperatorBuilder* common() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  MachineOperatorBuilder* machine() const;

  JSGraph* const jsgraph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMMON_OPERATOR_REDUCER_H_
