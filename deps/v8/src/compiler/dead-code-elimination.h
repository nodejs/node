// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DEAD_CODE_ELIMINATION_H_
#define V8_COMPILER_DEAD_CODE_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;

// Propagates {Dead} control and {DeadValue} values through the graph and
// thereby removes dead code.
// We detect dead values based on types, replacing uses of nodes with
// {Type::None()} with {DeadValue}. A pure node (other than a phi) using
// {DeadValue} is replaced by {DeadValue}. When {DeadValue} hits the effect
// chain, a crashing {Unreachable} node is inserted and the rest of the effect
// chain is collapsed. We wait for the {EffectControlLinearizer} to connect
// {Unreachable} nodes to the graph end, since this is much easier if there is
// no floating control.
// {DeadValue} has an input, which has to have {Type::None()}. This input is
// important to maintain the dependency on the cause of the unreachable code.
// {Unreachable} has a value output and {Type::None()} so it can be used by
// {DeadValue}.
// {DeadValue} nodes track a {MachineRepresentation} so they can be lowered to a
// value-producing node. {DeadValue} has the runtime semantics of crashing and
// behaves like a constant of its representation so it can be used in gap moves.
// Since phi nodes are the only remaining use of {DeadValue}, this
// representation is only adjusted for uses by phi nodes.
// In contrast to {DeadValue}, {Dead} can never remain in the graph.
class V8_EXPORT_PRIVATE DeadCodeElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  DeadCodeElimination(Editor* editor, Graph* graph,
                      CommonOperatorBuilder* common, Zone* temp_zone);
  ~DeadCodeElimination() final = default;
  DeadCodeElimination(const DeadCodeElimination&) = delete;
  DeadCodeElimination& operator=(const DeadCodeElimination&) = delete;

  const char* reducer_name() const override { return "DeadCodeElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceEnd(Node* node);
  Reduction ReduceLoopOrMerge(Node* node);
  Reduction ReduceLoopExit(Node* node);
  Reduction ReduceNode(Node* node);
  Reduction ReducePhi(Node* node);
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReducePureNode(Node* node);
  Reduction ReduceUnreachableOrIfException(Node* node);
  Reduction ReduceEffectNode(Node* node);
  Reduction ReduceDeoptimizeOrReturnOrTerminateOrTailCall(Node* node);
  Reduction ReduceBranchOrSwitch(Node* node);

  Reduction RemoveLoopExit(Node* node);
  Reduction PropagateDeadControl(Node* node);

  void TrimMergeOrPhi(Node* node, int size);

  Node* DeadValue(Node* none_node,
                  MachineRepresentation rep = MachineRepresentation::kNone);

  Graph* graph() const { return graph_; }
  CommonOperatorBuilder* common() const { return common_; }
  Node* dead() const { return dead_; }

  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  Node* const dead_;
  Zone* zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DEAD_CODE_ELIMINATION_H_
