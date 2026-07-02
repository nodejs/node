// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_

#include <initializer_list>

#include "src/base/logging.h"
#include "src/codegen/bailout-reason.h"
#include "src/common/scoped-modification.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-kna-processor.h"
#include "src/maglev/maglev-range-analysis.h"
#include "src/maglev/maglev-reducer.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphOptimizer {
 public:
  explicit MaglevGraphOptimizer(
      Graph* graph, RecomputeKnownNodeAspectsProcessor& kna_processor,
      NodeRanges* ranges = nullptr);

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
  void PostProcessBasicBlock(BasicBlock* block);
  void PostPhiProcessing() {}

#define DECLARE_PROCESS(NodeT)                                        \
  ProcessResult Visit##NodeT(NodeT*, const ProcessingState&);         \
  ProcessResult Process(NodeT* node, const ProcessingState& state) {  \
    ScopedModification<NodeBase*> current_node(&current_node_, node); \
    UnwrapInputs();                                                   \
    PreProcessNode(node, state);                                      \
    ProcessResult result = Visit##NodeT(node, state);                 \
    PostProcessNode(node);                                            \
    return result;                                                    \
  }
  NODE_BASE_LIST(DECLARE_PROCESS)
#undef DECLARE_PROCESS

  bool is_tracing() const {
    return v8_flags.trace_maglev_graph_optimizer &&
           reducer_.graph()->compilation_info()->is_tracing_enabled();
  }

  KnownNodeAspects& known_node_aspects() {
    return kna_processor_.known_node_aspects();
  }
  void set_known_node_aspects(KnownNodeAspects* known_node_aspects) {
    kna_processor_.set_known_node_aspects(known_node_aspects);
  }

  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    CHECK(current_node()->properties().has_eager_deopt_info());
    return &current_node()->eager_deopt_info()->top_frame();
  }

  std::tuple<DeoptFrame*, interpreter::Register, int> GetDeoptFrameForLazyDeopt(
      bool can_throw) {
    CHECK(current_node()->properties().can_lazy_deopt());
    return current_node()->lazy_deopt_info()->GetFrameForCloning();
  }

  void AttachExceptionHandlerInfo(NodeBase* node);

  ProcessResult DeoptAndTruncate(DeoptimizeReason reason) {
    ReduceResult result = reducer_.EmitUnconditionalDeopt(reason);
    CHECK(result.IsDoneWithAbort());
    return ProcessResult::kTruncateBlock;
  }
  ProcessResult ThrowAndTruncate(Throw::Function function,
                                 ValueNode* input = nullptr) {
    ReduceResult result = reducer_.EmitThrow(function, input);
    CHECK(result.IsDoneWithAbort());
    return ProcessResult::kTruncateBlock;
  }

  bool HasPendingSplice() const { return reducer_.HasPendingSplice(); }
  auto TakePendingSplice() { return reducer_.TakePendingSplice(); }

  Graph* graph() const { return reducer_.graph(); }

 private:
  MaglevReducer<MaglevGraphOptimizer> reducer_;
  RecomputeKnownNodeAspectsProcessor& kna_processor_;
  NodeRanges* ranges_;
  int loop_depth_ = 0;

  NodeBase* current_node_;

  NodeBase* current_node() const {
    CHECK_NOT_NULL(current_node_);
    return current_node_;
  }

  compiler::JSHeapBroker* broker() const;

  std::optional<Range> GetRange(ValueNode* node);
  bool IsRangeLessEqual(ValueNode* lhs, ValueNode* rhs);

  void UnwrapInputs();

  template <typename NodeT>
  ValueNode* TrySmiTag(Input input);

  ValueNode* GetConstantWithRepresentation(
      ValueNode* node, UseRepresentation repr,
      std::optional<TaggedToFloat64ConversionType> conversion_type);

  // Returns a variant of the node with the value representation given. It
  // returns nullptr if we need to emit a tagged conversion.
  MaybeReduceResult GetUntaggedValueWithRepresentation(
      ValueNode* node, UseRepresentation repr,
      std::optional<TaggedToFloat64ConversionType> conversion_type);

  // Records the untagged input of a tagging conversion as the matching
  // untagged alternative of `tagged`, so a later untagging use can reuse it.
  template <ValueRepresentation kRepresentation>
  void RegisterUntaggedAlternative(ValueNode* tagged);

  void PreProcessNode(Node*, const ProcessingState& state);
  void PostProcessNode(Node*);

  // Phis are treated differently since they are not stored directly in the
  // basic block.
  void PreProcessNode(Phi*, const ProcessingState& state);
  void PostProcessNode(Phi*);

  // Control nodes are singletons in the basic block.
  void PreProcessNode(ControlNode*, const ProcessingState& state);
  void PostProcessNode(ControlNode*);

  Jump* FoldBranch(BasicBlock* current, BranchControlNode* branch_node,
                   bool if_true);

  ProcessResult ReplaceWith(ValueNode* node);

  template <typename NodeT, typename... Args>
  ProcessResult ReplaceWith(std::initializer_list<ValueNode*> inputs,
                            Args&&...);

  ProcessResult RemoveCurrentNode();

  template <Operation kOperation>
  std::optional<ProcessResult> TryFoldInt32Operation(ValueNode* node);
  template <Operation kOperation>
  std::optional<ProcessResult> TryFoldFloat64Operation(ValueNode* node);

  template <typename NodeT>
  ProcessResult ProcessLoadContextSlot(NodeT* node);
  template <typename NodeT>
  ProcessResult ProcessCheckMaps(NodeT* node, ValueNode* object_map = nullptr);

  MaybeReduceResult EnsureType(
      ValueNode* node, NodeType type,
      DeoptimizeReason reason = DeoptimizeReason::kWrongValue);
};

// Optimizer-side Subgraph: synthesises an off-graph CFG (entry / branches /
// labels / join) and records a pending splice when it goes out of scope, so
// the GraphProcessor can stitch the new blocks into the live graph at the
// node currently being visited.
template <>
class Subgraph<MaglevGraphOptimizer>
    : public SubgraphBase<Subgraph<MaglevGraphOptimizer>,
                          MaglevGraphOptimizer> {
 public:
  using Base =
      SubgraphBase<Subgraph<MaglevGraphOptimizer>, MaglevGraphOptimizer>;
  using Variable = Base::Variable;
  using Label = Base::Label;

  // TODO(victorgomes): support loops.
  class LoopLabel {};

  Subgraph(MaglevReducer<MaglevGraphOptimizer>* reducer, int variable_count);
  ~Subgraph();

  template <typename ControlNodeT, typename... Args>
  ReduceResult GotoIfTrue(Label* true_target,
                          std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args);
  template <typename ControlNodeT, typename... Args>
  ReduceResult GotoIfFalse(Label* false_target,
                           std::initializer_list<ValueNode*> control_inputs,
                           Args&&... args);

  void Goto(Label* label);
  void Bind(Label* label);

  LoopLabel BeginLoop(std::initializer_list<Variable*> /*loop_vars*/) {
    UNREACHABLE();
  }
  void EndLoop(LoopLabel* /*loop_label*/) { UNREACHABLE(); }

 private:
  void MergeIntoLabel(Label* label, BasicBlock* predecessor);

  template <typename ControlNodeT, typename... Args>
  ReduceResult GotoIfImpl(Label* jump_target, bool jump_on_true,
                          std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args);

  Zone* zone_;
  ZoneVector<BasicBlock*> blocks_;
  BasicBlock* saved_block_;
  BasicBlockPosition saved_position_;
  KnownNodeAspects* saved_kna_;
  // Enclosing subgraph, or nullptr if this is a top-level subgraph.
  Subgraph<MaglevGraphOptimizer>* parent_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_
