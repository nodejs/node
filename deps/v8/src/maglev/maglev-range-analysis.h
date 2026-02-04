// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_
#define V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_

#include <cstdint>

#include "src/base/logging.h"
#include "src/common/operation.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/zone/zone-containers.h"

#define TRACE_RANGE(...)                                     \
  do {                                                       \
    if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) { \
      StdoutStream{} << __VA_ARGS__ << std::endl;            \
    }                                                        \
  } while (false)

namespace v8::internal::maglev {

// {lhs_map} becomes the result of the intersection.
template <typename Key, typename Value, typename MergeFunc>
void DestructivelyIntersect(ZoneMap<Key, Value>& lhs_map,
                            const ZoneMap<Key, Value>& rhs_map,
                            MergeFunc&& func) {
  typename ZoneMap<Key, Value>::iterator lhs_it = lhs_map.begin();
  typename ZoneMap<Key, Value>::const_iterator rhs_it = rhs_map.begin();
  while (lhs_it != lhs_map.end() && rhs_it != rhs_map.end()) {
    if (lhs_it->first < rhs_it->first) {
      // Skip over elements that are only in LHS.
      ++lhs_it;
    } else if (rhs_it->first < lhs_it->first) {
      // Skip over elements that are only in RHS.
      ++rhs_it;
    } else {
      lhs_it->second = func(lhs_it->second, rhs_it->second);
      ++lhs_it;
      ++rhs_it;
    }
  }
}

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
  explicit NodeRanges(Graph* graph)
      : graph_(graph),
        ranges_(graph->max_block_id(), graph->zone()),
        less_equals_(graph->max_block_id(), graph->zone()) {}

  Range Get(BasicBlock* block, ValueNode* node) {
    auto*& map = ranges_[block->id()];
    DCHECK_NOT_NULL(map);
    auto it = map->find(node);
    if (it == map->end()) {
      return node->GetStaticRange();
    }
    return it->second;
  }

  void UnionUpdate(BasicBlock* block, ValueNode* node, Range range) {
    auto* map = ranges_[block->id()];
    DCHECK_NOT_NULL(map);
    auto it = map->find(node);
    if (it == map->end()) {
      map->emplace(node, range);
    } else {
      Range new_range = Range::Union(it->second, range);
      TRACE_RANGE("[range]: Union update: "
                  << PrintNodeLabel(node) << ": " << PrintNode(node)
                  << ", from: " << it->second << ", to: " << new_range);

      it->second = new_range;
    }
  }

  inline void ProcessGraph();

  void Print() {
    std::cout << "Node ranges:\n";
    for (BasicBlock* block : graph_->blocks()) {
      int id = block->id();
      std::cout << "Block b" << id << ":\n";
      auto* map = ranges_[id];
      if (!map) continue;
      for (auto& [node, range] : *map) {
        std::cout << "  " << PrintNodeLabel(node) << ": " << PrintNode(node)
                  << ": " << range << std::endl;
      }
    }
  }

  void EnsureMapExistsFor(BasicBlock* block) {
    if (ranges_[block->id()] == nullptr) {
      ranges_[block->id()] = zone()->New<ZoneMap<ValueNode*, Range>>(zone());
    }
  }

  void Join(BasicBlock* block, BasicBlock* pred) {
    auto*& map = ranges_[block->id()];
    auto*& pred_map = ranges_[pred->id()];
    DCHECK_NOT_NULL(pred_map);
    if (map == nullptr) {
      map = zone()->New<ZoneMap<ValueNode*, Range>>(*pred_map);
      return;
    }
    DestructivelyIntersect(*map, *pred_map, [&](Range& r1, const Range& r2) {
      return Range::Union(r1, r2);
    });
  }

  void NarrowUpdate(BasicBlock* block, ValueNode* node, Range narrowed_range) {
    if (IsConstantNode(node->opcode())) {
      // Don't narrow update constants.
      return;
    }
    auto* map = ranges_[block->id()];
    DCHECK_NOT_NULL(map);
    auto it = map->find(node);
    if (it == map->end()) {
      TRACE_RANGE("[range]: Narrow update: " << PrintNodeLabel(node) << ": "
                                             << PrintNode(node) << ": "
                                             << narrowed_range);
      map->emplace(node, narrowed_range);
    } else {
      if (!narrowed_range.is_empty()) {
        TRACE_RANGE("[range]: Narrow update: "
                    << PrintNodeLabel(node) << ": " << PrintNode(node)
                    << ", from: " << it->second << ", to: " << narrowed_range);
        it->second = narrowed_range;
      } else {
        TRACE_RANGE("[range]: Failed narrowing update: "
                    << PrintNodeLabel(node) << ": " << PrintNode(node)
                    << ", from: " << it->second << ", to: " << narrowed_range);
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
  // TODO(victorgomes): Use SnapshotTable.
  ZoneVector<ZoneMap<ValueNode*, Range>*> ranges_;

  // This is a very naive way to avoid CheckInt32Condition after a
  // BranchIfInt32Compare(LessThan).
  ZoneVector<LessEqualConstraint::List> less_equals_;

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
    TRACE_RANGE("[range] >>> Processing backedges for block b" << block->id());
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
      TRACE_RANGE("[ranges]: Processing " << PrintNodeLabel(phi) << ": "
                                          << PrintNode(phi) << ":");
      TRACE_RANGE("  before = " << range);
      TRACE_RANGE("  new    = " << backedge);
      TRACE_RANGE("  widen  = " << widened);
      if (range != widened) {
        TRACE_RANGE("[range] FIXPOINT NOT REACHED");
        is_done = false;
        ranges_.UnionUpdate(block, phi, widened);
      }
      TRACE_RANGE("[range] <<<< End of processing backedges for block b"
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
