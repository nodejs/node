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

  KnownNodeAspects& known_node_aspects() {
    return kna_processor_.known_node_aspects();
  }

  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    DCHECK(current_node()->properties().can_eager_deopt() ||
           current_node()->properties().is_deopt_checkpoint());
    return &current_node()->eager_deopt_info()->top_frame();
  }

  std::tuple<DeoptFrame*, interpreter::Register, int> GetDeoptFrameForLazyDeopt(
      bool can_throw) {
    DCHECK(current_node()->properties().can_lazy_deopt());
    LazyDeoptInfo* info = current_node()->lazy_deopt_info();
    return std::make_tuple(&info->top_frame(), info->result_location(),
                           info->result_size());
  }

  void AttachExceptionHandlerInfo(NodeBase* node) {
    DCHECK(node->properties().can_throw());
    DCHECK(current_node()->properties().can_throw());
    DCHECK(!node->Is<CallKnownJSFunction>());
    ExceptionHandlerInfo* info = current_node()->exception_handler_info();
    if (info->ShouldLazyDeopt()) {
      new (node->exception_handler_info())
          ExceptionHandlerInfo(ExceptionHandlerInfo::kLazyDeopt);
    } else if (!info->HasExceptionHandler()) {
      new (node->exception_handler_info()) ExceptionHandlerInfo();
    } else {
      new (node->exception_handler_info())
          ExceptionHandlerInfo(info->catch_block(), info->depth());
    }
  }

  ReduceResult EmitUnconditionalDeopt(DeoptimizeReason);
  ReduceResult EmitThrow(Throw::Function function, ValueNode* input);

  ProcessResult DeoptAndTruncate(DeoptimizeReason reason) {
    ReduceResult result = EmitUnconditionalDeopt(reason);
    CHECK(result.IsDoneWithAbort());
    return ProcessResult::kTruncateBlock;
  }
  ProcessResult ThrowAndTruncate(Throw::Function function,
                                 ValueNode* input = nullptr) {
    ReduceResult result = EmitThrow(function, input);
    CHECK(result.IsDoneWithAbort());
    return ProcessResult::kTruncateBlock;
  }

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
  bool IsRangeLessEqual(ValueNode* lhs, ValueNode* rhs);

  // Iterates the deopt frames unwrapping its inputs, ie, removing Identity or
  // ReturnedValue nodes.
  void UnwrapDeoptFrames();
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
  template <typename NodeT>
  ProcessResult ProcessCheckMaps(NodeT* node, ValueNode* object_map = nullptr);

  MaybeReduceResult EnsureType(
      ValueNode* node, NodeType type,
      DeoptimizeReason reason = DeoptimizeReason::kWrongValue);
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_OPTIMIZER_H_
