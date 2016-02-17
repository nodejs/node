// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BINARY_OPERATOR_REDUCER_H_
#define V8_COMPILER_BINARY_OPERATOR_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;
class MachineOperatorBuilder;
class Operator;


// Performs strength reduction on nodes that have common operators.
class BinaryOperatorReducer final : public AdvancedReducer {
 public:
  BinaryOperatorReducer(Editor* editor, Graph* graph,
                        CommonOperatorBuilder* common,
                        MachineOperatorBuilder* machine);
  ~BinaryOperatorReducer() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceFloat52Mul(Node* node);
  Reduction ReduceFloat52Div(Node* node);

  Reduction Change(Node* node, Operator const* op, Node* a);

  Graph* graph() const { return graph_; }
  CommonOperatorBuilder* common() const { return common_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  Node* dead() const { return dead_; }

  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  MachineOperatorBuilder* const machine_;
  Node* const dead_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BINARY_OPERATOR_REDUCER_H_
