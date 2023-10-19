// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_REDUCER_H_
#define V8_COMPILER_COMMON_OPERATOR_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/common-operator.h"
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
class V8_EXPORT_PRIVATE CommonOperatorReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  CommonOperatorReducer(Editor* editor, Graph* graph, JSHeapBroker* broker,
                        CommonOperatorBuilder* common,
                        MachineOperatorBuilder* machine, Zone* temp_zone,
                        BranchSemantics default_branch_semantics);
  ~CommonOperatorReducer() final = default;

  const char* reducer_name() const override { return "CommonOperatorReducer"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceBranch(Node* node);
  Reduction ReduceDeoptimizeConditional(Node* node);
  Reduction ReduceMerge(Node* node);
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReducePhi(Node* node);
  Reduction ReduceReturn(Node* node);
  Reduction ReduceSelect(Node* node);
  Reduction ReduceSwitch(Node* node);
  Reduction ReduceStaticAssert(Node* node);
  Reduction ReduceTrapConditional(Node* node);

  Reduction Change(Node* node, Operator const* op, Node* a);
  Reduction Change(Node* node, Operator const* op, Node* a, Node* b);

  // Helper to determine if conditions are true or false.
  Decision DecideCondition(Node* const cond, BranchSemantics branch_semantics);
  BranchSemantics BranchSemanticsOf(const Node* branch) {
    BranchSemantics bs = BranchParametersOf(branch->op()).semantics();
    if (bs != BranchSemantics::kUnspecified) return bs;
    return default_branch_semantics_;
  }

  Graph* graph() const { return graph_; }
  JSHeapBroker* broker() const { return broker_; }
  CommonOperatorBuilder* common() const { return common_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  Node* dead() const { return dead_; }

  Graph* const graph_;
  JSHeapBroker* const broker_;
  CommonOperatorBuilder* const common_;
  MachineOperatorBuilder* const machine_;
  Node* const dead_;
  Zone* zone_;
  BranchSemantics default_branch_semantics_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMMON_OPERATOR_REDUCER_H_
