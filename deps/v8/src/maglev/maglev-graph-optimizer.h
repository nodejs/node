// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_

#include <initializer_list>

#include "src/base/logging.h"
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

  KnownNodeAspects& known_node_aspects() {
    return kna_processor_.known_node_aspects();
  }

  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    return &current_node()->eager_deopt_info()->top_frame();
  }

  ReduceResult EmitUnconditionalDeopt(DeoptimizeReason);

 private:
  MaglevReducer<MaglevGraphOptimizer> reducer_;
  RecomputeKnownNodeAspectsProcessor& kna_processor_;
  NodeRanges* ranges_;

  NodeBase* current_node_;

  NodeBase* current_node() const {
    CHECK_NOT_NULL(current_node_);
    return current_node_;
  }

  compiler::JSHeapBroker* broker() const;

  std::optional<Range> GetRange(ValueNode* node);

  // Iterates the deopt frames unwrapping its inputs, ie, removing Identity or
  // ReturnedValue nodes.
  void UnwrapDeoptFrames();
  void UnwrapInputs();

  ValueNode* GetConstantWithRepresentation(
      ValueNode* node, UseRepresentation repr,
      std::optional<TaggedToFloat64ConversionType> conversion_type);

  // Returns a variant of the node with the value representation given. It
  // returns nullptr if we need to emit a tagged conversion.
  ValueNode* GetUntaggedValueWithRepresentation(
      ValueNode* node, UseRepresentation repr,
      std::optional<TaggedToFloat64ConversionType> conversion_type);

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

  template <Operation kOperation>
  std::optional<ProcessResult> TryFoldInt32Operation(ValueNode* node);
  template <Operation kOperation>
  std::optional<ProcessResult> TryFoldFloat64Operation(ValueNode* node);

  template <typename NodeT>
  ProcessResult ProcessLoadContextSlot(NodeT* node);
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_
