// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_
#define V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_

#include "src/base/logging.h"
#include "src/common/operation.h"
#include "src/maglev/hamt.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/utils/bit-vector.h"

#define TRACE_RANGE(...)                                      \
  do {                                                        \
    if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) {  \
      StdoutStream{} << "[ranges] " __VA_ARGS__ << std::endl; \
    }                                                         \
  } while (false)

namespace v8::internal::maglev {

class LessEqualConstraint {
 public:
  LessEqualConstraint(ValueNode* lhs, ValueNode* rhs)
      : lhs_(lhs), rhs_(rhs), next_(nullptr) {}

  bool is(ValueNode* lhs, ValueNode* rhs) { return lhs_ == lhs && rhs_ == rhs; }

  using List = base::ThreadedList<LessEqualConstraint>;

 private:
  ValueNode* lhs_;
  ValueNode* rhs_;
  LessEqualConstraint* next_;

  LessEqualConstraint** next() { return &next_; }
  friend List;
  friend base::ThreadedListTraits<LessEqualConstraint>;
};

class NodeRanges {
 public:
  using RangeMap = HAMT<ValueNode*, Range>;

  explicit NodeRanges(Graph* graph)
      : graph_(graph),
        ranges_initialized_(graph->max_block_id(), graph->zone()),
        ranges_(graph->zone()->NewVector<RangeMap>(graph->max_block_id())),
        less_equals_(graph->zone()->NewVector<LessEqualConstraint::List>(
            graph->max_block_id())) {}

  Range Get(BasicBlock* block, ValueNode* node) {
    DCHECK(ranges_initialized_.Contains(block->id()));
    RangeMap& map = ranges_[block->id()];
    const Range* range = map.find(node);
    if (!range) {
      return node->GetStaticRange();
    }
    return *range;
  }

  void UnionUpdate(BasicBlock* block, ValueNode* node, Range range) {
    DCHECK(ranges_initialized_.Contains(block->id()));
    RangeMap& map = ranges_[block->id()];
    const Range* current_range = map.find(node);
    if (!current_range) {
      map = map.insert(zone(), node, range);
      TRACE_RANGE("Adding to Block b" << block->id() << ": "
                                      << PrintNodeBrief(node) << ": " << range);
    } else {
      Range new_range = Range::Union(*current_range, range);
      map = map.insert(zone(), node, new_range);
      TRACE_RANGE("Union update in Block b"
                  << block->id() << ": " << PrintNodeBrief(node)
                  << ", from: " << *current_range << " to: " << new_range);
    }
  }

  inline void ProcessGraph();

  void PrintRangesForBlock(int id) {
    std::cout << "Block b" << id << ":";
    if (!ranges_initialized_.Contains(id)) {
      std::cout << " ranges not initialized.\n";
    }
    std::cout << "\n";
    RangeMap map = ranges_[id];
    for (auto [node, range] : map) {
      std::cout << "  " << PrintNodeBrief(node) << ": " << range << std::endl;
    }
  }

  void Print() {
    std::cout << "Node ranges:\n";
    for (BasicBlock* block : graph_->blocks()) {
      PrintRangesForBlock(block->id());
    }
  }

  void EnsureMapExistsFor(BasicBlock* block) {
    if (!ranges_initialized_.Contains(block->id())) {
      ranges_initialized_.Add(block->id());
    }
  }

  void Join(BasicBlock* block, BasicBlock* pred) {
    RangeMap& map = ranges_[block->id()];
    RangeMap pred_map = ranges_[pred->id()];
    if (!ranges_initialized_.Contains(block->id())) {
      map = pred_map;
      ranges_initialized_.Add(block->id());
      if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) {
        TRACE_RANGE("Initializing Block b" << block->id() << " from Block b"
                                           << pred->id());
        PrintRangesForBlock(block->id());
      }
      return;
    }
    map = map.merge_into(zone(), pred_map, [&](Range r1, const Range r2) {
      return Range::Union(r1, r2);
    });
    if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) {
      TRACE_RANGE("Merging Block b" << pred->id() << " into Block b"
                                    << block->id());
      PrintRangesForBlock(block->id());
    }
  }

  void NarrowUpdate(BasicBlock* block, ValueNode* node, Range narrowed_range) {
    if (IsConstantNode(node->opcode())) {
      // Don't narrow update constants.
      return;
    }
    RangeMap& map = ranges_[block->id()];
    DCHECK(ranges_initialized_.Contains(block->id()));
    const Range* current_range = map.find(node);
    if (!current_range) {
      TRACE_RANGE("Narrow update in Block b" << block->id() << ": "
                                             << PrintNodeBrief(node) << ": "
                                             << narrowed_range);
      map = map.insert(zone(), node, narrowed_range);
    } else {
      if (!narrowed_range.is_empty()) {
        TRACE_RANGE("Narrow update in Block b"
                    << block->id() << ": " << PrintNodeBrief(node) << ", from: "
                    << *current_range << ", to: " << narrowed_range);
        map = map.insert(zone(), node, narrowed_range);
      } else {
        TRACE_RANGE("Failed narrowing update in Block b"
                    << block->id() << PrintNodeBrief(node) << ", from: "
                    << *current_range << ", to: " << narrowed_range);
      }
    }
  }

  void AddLessEqualConstraint(BasicBlock* block, ValueNode* lhs,
                              ValueNode* rhs) {
    less_equals_[block->id()].Add(zone()->New<LessEqualConstraint>(lhs, rhs));
  }

  bool IsLessEqualConstraint(BasicBlock* block, ValueNode* lhs,
                             ValueNode* rhs) {
    for (LessEqualConstraint* constraint : less_equals_[block->id()]) {
      if (constraint->is(lhs, rhs)) return true;
    }
    return false;
  }

 private:
  Graph* graph_;
  // A bitmask indicating whether a RangeMap has already been initialized
  // for a given block ID.
  BitVector ranges_initialized_;
  base::Vector<RangeMap> ranges_;

  // This is a very naive way to avoid CheckInt32Condition after a
  // BranchIfInt32Compare(LessThan).
  base::Vector<LessEqualConstraint::List> less_equals_;

  Zone* zone() const { return graph_->zone(); }
};

class RangeProcessor {
 public:
  explicit RangeProcessor(NodeRanges& node_ranges) : ranges_(node_ranges) {}

  void PreProcessGraph(Graph* graph) { is_done_ = true; }
  void PostProcessGraph(Graph* graph) {}

  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    current_block_ = block;
    ranges_.EnsureMapExistsFor(block);
    return BlockProcessResult::kContinue;
  }
  void PostProcessBasicBlock(BasicBlock* block) {
    if (JumpLoop* control = block->control_node()->TryCast<JumpLoop>()) {
      if (!ProcessLoopPhisBackedge(control->target(), block)) {
        // We didn't reach a fixpoint for this loop, try this loop header
        // again.
        is_done_ = false;
      }
    } else if (UnconditionalControlNode* unconditional =
                   block->control_node()->TryCast<UnconditionalControlNode>()) {
      BasicBlock* succ = unconditional->target();
      ranges_.Join(succ, block);
      if (succ->has_state() && succ->has_phi()) {
        ProcessPhis(succ, block);
      }
    } else {
      block->ForEachSuccessor([&](BasicBlock* succ) {
        ranges_.Join(succ, block);
        // Because of split-edge, {succ} cannot have phis.
        DCHECK_IMPLIES(succ->has_state(), !succ->has_phi());
        ProcessNodeBase(block->control_node(), succ);
      });
    }
  }

  void PostPhiProcessing() {}

  ProcessResult Process(UnsafeSmiUntag* node, const ProcessingState&) {
    UnionUpdate(node, Range::Intersect(Get(node->input_node(0)), Range::Smi()));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(CheckedSmiUntag* node, const ProcessingState&) {
    UnionUpdate(node, Range::Intersect(Get(node->input_node(0)), Range::Smi()));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(CheckedSmiSizedInt32* node, const ProcessingState&) {
    UnionUpdate(node, Range::Intersect(Get(node->input_node(0)), Range::Smi()));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(TruncateCheckedNumberOrOddballToInt32* node) {
    UnionUpdateInt32(node, Get(node->input_node(0)));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32Increment* node, const ProcessingState&) {
    UnionUpdateTruncatingInt32(node,
                               Range::Add(Get(node->input_node(0)), Range(1)));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32IncrementWithOverflow* node,
                        const ProcessingState&) {
    UnionUpdateInt32(node, Range::Add(Get(node->input_node(0)), Range(1)));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32Add* node, const ProcessingState&) {
    UnionUpdateTruncatingInt32(
        node, Range::Add(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32AddWithOverflow* node, const ProcessingState&) {
    UnionUpdateInt32(
        node, Range::Add(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32Decrement* node, const ProcessingState&) {
    UnionUpdateTruncatingInt32(node,
                               Range::Sub(Get(node->input_node(0)), Range(1)));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32DecrementWithOverflow* node,
                        const ProcessingState&) {
    UnionUpdateInt32(node, Range::Sub(Get(node->input_node(0)), Range(1)));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32Subtract* node, const ProcessingState&) {
    UnionUpdateTruncatingInt32(
        node, Range::Sub(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32SubtractWithOverflow* node,
                        const ProcessingState&) {
    UnionUpdateInt32(
        node, Range::Sub(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32Multiply* node, const ProcessingState&) {
    UnionUpdateTruncatingInt32(
        node, Range::Mul(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32MultiplyWithOverflow* node,
                        const ProcessingState&) {
    UnionUpdateInt32(
        node, Range::Mul(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32BitwiseAnd* node, const ProcessingState&) {
    UnionUpdateInt32(node, Range::BitwiseAnd(Get(node->input_node(0)),
                                             Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32BitwiseOr* node, const ProcessingState&) {
    UnionUpdateInt32(node, Range::BitwiseOr(Get(node->input_node(0)),
                                            Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32BitwiseXor* node, const ProcessingState&) {
    UnionUpdateInt32(node, Range::BitwiseXor(Get(node->input_node(0)),
                                             Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32ShiftLeft* node, const ProcessingState&) {
    UnionUpdateInt32(node, Range::ShiftLeft(Get(node->input_node(0)),
                                            Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32ShiftRight* node, const ProcessingState&) {
    UnionUpdateInt32(node, Range::ShiftRight(Get(node->input_node(0)),
                                             Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Int32ShiftRightLogical* node, const ProcessingState&) {
    UnionUpdateUint32(node, Range::ShiftRightLogical(Get(node->input_node(0)),
                                                     Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(LoadTaggedField* node, const ProcessingState&) {
    if (node->load_type() == LoadType::kSmi) {
      UnionUpdate(node, Range::Smi());
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState&) {
    if constexpr (!IsConstantNode(Node::opcode_of<NodeT>) &&
                  NodeT::kProperties.value_representation() ==
                      ValueRepresentation::kInt32) {
      UnionUpdate(node, Range::Int32());
    }
    if constexpr (NodeT::kProperties.can_throw()) {
      ExceptionHandlerInfo* info = node->exception_handler_info();
      if (info->HasExceptionHandler() && !info->ShouldLazyDeopt()) {
        BasicBlock* exception_handler =
            node->exception_handler_info()->catch_block();
        DCHECK(exception_handler->is_exception_handler_block());
        ranges_.Join(exception_handler, current_block_);
      }
    }
    return ProcessResult::kContinue;
  }

  void ProcessControlNodeFor(BranchIfInt32Compare* node, BasicBlock* succ) {
    ValueNode* lhs = node->input_node(0);
    ValueNode* rhs = node->input_node(1);
    Range lhs_range = ranges_.Get(succ, lhs);
    Range rhs_range = ranges_.Get(succ, rhs);
    switch (node->operation()) {
#define CASE(Op, InvOp, NegOp, NegInvOp)                                      \
  case Operation::k##Op:                                                      \
    if (node->if_true() == succ) {                                            \
      ranges_.NarrowUpdate(succ, lhs,                                         \
                           lhs_range.Constrain##Op(lhs_range, rhs_range));    \
      ranges_.NarrowUpdate(succ, rhs,                                         \
                           rhs_range.Constrain##InvOp(rhs_range, lhs_range)); \
    } else {                                                                  \
      DCHECK_EQ(node->if_false(), succ);                                      \
      ranges_.NarrowUpdate(succ, lhs,                                         \
                           lhs_range.Constrain##NegOp(lhs_range, rhs_range)); \
      ranges_.NarrowUpdate(                                                   \
          succ, rhs, rhs_range.Constrain##NegInvOp(rhs_range, lhs_range));    \
    }                                                                         \
    break;
      CASE(LessThan, GreaterThan, GreaterThanOrEqual, LessThanOrEqual)
      CASE(LessThanOrEqual, GreaterThanOrEqual, GreaterThan, LessThan)
      CASE(GreaterThan, LessThan, LessThanOrEqual, GreaterThanOrEqual)
      CASE(GreaterThanOrEqual, LessThanOrEqual, LessThan, GreaterThan)
#undef CASE
      case Operation::kEqual:
      case Operation::kStrictEqual:
        if (node->if_true() == succ) {
          Range eq = Range::Intersect(lhs_range, rhs_range);
          ranges_.NarrowUpdate(succ, lhs, eq);
          ranges_.NarrowUpdate(succ, rhs, eq);
        }
        break;
      default:
        UNREACHABLE();
    }
    if (node->operation() == Operation::kLessThan && node->if_true() == succ) {
      ranges_.AddLessEqualConstraint(succ, lhs, rhs);
    }
  }

  void ProcessControlNodeFor(ControlNode* node, BasicBlock* succ) {}

  void ProcessNodeBase(ControlNode* node, BasicBlock* succ) {
    switch (node->opcode()) {
#define CASE(OPCODE)                                   \
  case Opcode::k##OPCODE:                              \
    ProcessControlNodeFor(node->Cast<OPCODE>(), succ); \
    break;
      CONTROL_NODE_LIST(CASE)
#undef CASE
      default:
        UNREACHABLE();
    }
  }

  bool is_done() const { return is_done_; }

 private:
  NodeRanges& ranges_;
  BasicBlock* current_block_ = nullptr;
  bool is_done_ = false;

  Range Get(ValueNode* node) {
    DCHECK_NOT_NULL(current_block_);
    return ranges_.Get(current_block_, node);
  }

  void UnionUpdate(ValueNode* node, Range range) {
    DCHECK_NOT_NULL(current_block_);
    ranges_.UnionUpdate(current_block_, node, range);
  }

  void UnionUpdateInt32(ValueNode* node, Range range) {
    // WARNING: This entails that the current range analysis cannot be used to
    // identify truncation, since we always intersect Int32 operations range.
    DCHECK_NOT_NULL(current_block_);
    ranges_.UnionUpdate(current_block_, node,
                        Range::Intersect(Range::Int32(), range));
  }

  void UnionUpdateTruncatingInt32(ValueNode* node, Range range) {
    // Nodes with Int32 value representation without overflow check will
    // truncate their values back to Int32. If they overflow, their ranges are
    // approximated to Range::Int32().
    DCHECK_NOT_NULL(current_block_);
    ranges_.UnionUpdate(current_block_, node,
                        range.IsInt32() ? range : Range::Int32());
  }

  void UnionUpdateUint32(ValueNode* node, Range range) {
    // WARNING: This entails that the current range analysis cannot be used to
    // identify truncation, since we always intersect Int32 operations range.
    DCHECK_NOT_NULL(current_block_);
    ranges_.UnionUpdate(current_block_, node,
                        Range::Intersect(Range::Uint32(), range));
  }

  void ProcessPhis(BasicBlock* block, BasicBlock* pred) {
    int predecessor_id = -1;
    for (int i = 0; i < block->predecessor_count(); ++i) {
      if (block->predecessor_at(i) == pred) {
        predecessor_id = i;
        break;
      }
    }
    DCHECK_NE(predecessor_id, -1);
    for (Phi* phi : *block->phis()) {
      Range phi_range = ranges_.Get(pred, phi->input_node(predecessor_id));
      if (phi->is_int32()) {
        // Since phi representation selector promoted this phi to Int32,
        // take its range into consideration.
        phi_range = Range::Intersect(Range::Int32(), phi_range);
      }
      ranges_.UnionUpdate(block, phi, phi_range);
    }
  }

  // Returns true if the loop reach a fixpoint.
  bool ProcessLoopPhisBackedge(BasicBlock* block, BasicBlock* backedge_pred) {
    if (!block->has_phi()) return true;
    DCHECK_EQ(backedge_pred, block->backedge_predecessor());
    ranges_.EnsureMapExistsFor(block);  // TODO(victorgomes): not sure if needed
    TRACE_RANGE(">>>> Processing backedges for Block b" << block->id());
    int backedge_id = block->state()->predecessor_count() - 1;
    bool is_done = true;
    for (Phi* phi : *block->phis()) {
      Range range = ranges_.Get(block, phi);
      Range backedge = ranges_.Get(backedge_pred, phi->input_node(backedge_id));
      Range widened = Range::Widen(range, backedge);
      if (phi->is_int32()) {
        // Since phi representation selector promoted this phi to Int32,
        // take its range into consideration when widening.
        widened = Range::Intersect(Range::Int32(), widened);
      }
      TRACE_RANGE("Processing " << PrintNodeBrief(phi) << ":");
      TRACE_RANGE("    before = " << range);
      TRACE_RANGE("    new    = " << backedge);
      TRACE_RANGE("    widen  = " << widened);
      if (range != widened) {
        TRACE_RANGE("FIXPOINT NOT REACHED");
        is_done = false;
        ranges_.UnionUpdate(block, phi, widened);
      }
      TRACE_RANGE("<<<< End of processing backedges for Block b"
                  << block->id());
    }
    return is_done;
  }
};

inline void NodeRanges::ProcessGraph() {
  // TODO(victorgomes): The first pass could be shared with another
  // optimization.
  GraphProcessor<RangeProcessor> processor(*this);
  while (!processor.node_processor().is_done()) {
    processor.ProcessGraph(graph_);
  }
}

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_
