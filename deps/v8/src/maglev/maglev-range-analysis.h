// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_
#define V8_MAGLEV_MAGLEV_RANGE_ANALYSIS_H_

#include <optional>

#include "src/base/logging.h"
#include "src/common/operation.h"
#include "src/maglev/hamt.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-node-type.h"

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
        block_states_(graph->zone()->NewVector<BlockState>(
            graph->max_block_id(), BlockState::kUninitialized)),
        ranges_(graph->zone()->NewVector<RangeMap>(graph->max_block_id())),
        less_equals_(graph->zone()->NewVector<LessEqualConstraint::List>(
            graph->max_block_id())) {}

  Range Get(BasicBlock* block, ValueNode* node) {
    if (!IsBlockTracked(block)) return node->GetStaticRange();
    DCHECK(IsInitialized(block));
    RangeMap& map = ranges_[block->id()];
    const Range* range = map.find(node);
    if (!range) {
      return node->GetStaticRange();
    }
    return *range;
  }

  void InitializeRangeIfMissing(BasicBlock* block, ValueNode* node,
                                Range range) {
    DCHECK(IsInitialized(block));
    RangeMap& map = ranges_[block->id()];
    if (!map.find(node)) {
      map = map.insert(zone(), node, range);
      TRACE_RANGE("Initializing in Block b" << block->id() << ": "
                                            << PrintNodeBrief(node) << ": "
                                            << range);
    }
  }

  void UnionUpdate(BasicBlock* block, ValueNode* node, Range range) {
    DCHECK(IsInitialized(block));
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
    if (block_states_[id] == BlockState::kUninitialized) {
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

  bool IsInitialized(BasicBlock* block) const {
    return block_states_[block->id()] != BlockState::kUninitialized;
  }

  bool IsProcessed(BasicBlock* block) const {
    return block_states_[block->id()] == BlockState::kProcessed;
  }

  void MarkBlockProcessed(BasicBlock* block) {
    block_states_[block->id()] = BlockState::kProcessed;
  }

  void Join(BasicBlock* block, BasicBlock* pred) {
    RangeMap& map = ranges_[block->id()];
    RangeMap pred_map = ranges_[pred->id()];
    auto union_ranges = [](Range r1, Range r2) { return Range::Union(r1, r2); };
    switch (block_states_[block->id()]) {
      case BlockState::kUninitialized:
        map = pred_map;
        block_states_[block->id()] = BlockState::kInitialized;
        if (V8_UNLIKELY(v8_flags.trace_maglev_range_analysis)) {
          TRACE_RANGE("Initializing Block b" << block->id() << " from Block b"
                                             << pred->id());
          PrintRangesForBlock(block->id());
        }
        return;
      case BlockState::kInitialized:
        // Absent keys mean GetStaticRange(), so drop non-shared keys.
        map = map.intersect(zone(), pred_map, union_ranges);
        break;
      case BlockState::kProcessed:
        // Inherited keys are already shared by all preds (which never lose
        // keys), so merge_into is equivalent and keeps `block`'s definitions.
        map = map.merge_into(zone(), pred_map, union_ranges);
        break;
    }
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
    DCHECK(IsInitialized(block));
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
    if (!IsBlockTracked(block)) return false;
    for (LessEqualConstraint* constraint : less_equals_[block->id()]) {
      if (constraint->is(lhs, rhs)) return true;
    }
    return false;
  }

 private:
  // The graph optimizer that consumes these ranges can splice in new basic
  // blocks (e.g. when expanding a node into a subgraph). Since range analysis
  // does not run again over those blocks, they fall outside the analyzed id
  // range and have no recorded ranges. Such untracked blocks are handled
  // conservatively by returning the widest possible (static) range for their
  // nodes.
  bool IsBlockTracked(BasicBlock* block) const {
    return block->id() < static_cast<BasicBlock::Id>(block_states_.size());
  }

  Graph* graph_;
  // A block's RangeMap is first copied from a predecessor (kInitialized),
  // then also holds the block's own Phis and definitions (kProcessed).
  enum class BlockState : uint8_t { kUninitialized, kInitialized, kProcessed };
  base::Vector<BlockState> block_states_;
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
    ranges_.MarkBlockProcessed(block);
    if (block->has_phi()) {
      ProcessPhis(block);
      if (block->is_loop()) {
        // Resumable loop headers might not have a live forward predecessor:
        // that part of the code might be dead even though the loop is still
        // reachable via a resume point. Make sure we initialize phis in such
        // case. If the loop has a live predecessor, ProcessPhis is called
        // above, and this code is a no-op.
        InitializeLoopPhisIfMissing(block);
      }
    }
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    if (JumpLoop* control = block->control_node()->TryCast<JumpLoop>()) {
      if (!ProcessLoopPhisBackedge(control->target(), block)) {
        // We didn't reach a fixpoint for this loop, try this loop header
        // again.
        is_done_ = false;
      }
    } else if (UnconditionalControlNode* unconditional =
                   block->control_node()->TryCast<UnconditionalControlNode>()) {
      BasicBlock* succ = unconditional->target();
      // Resume edges into a resumable loop bypass its forward predecessor.
      if (!(succ->is_loop() && succ->state()->is_resumable_loop())) {
        ranges_.Join(succ, block);
      }
    } else {
      block->ForEachSuccessor([&](BasicBlock* succ) {
        DCHECK(!(succ->is_loop() && succ->state()->is_resumable_loop()));
        ranges_.Join(succ, block);
        // Because of split-edge, {succ} cannot have phis.
        DCHECK_IMPLIES(succ->has_state(), !succ->has_phi());
        ProcessNodeBase(block->control_node(), succ);
      });
    }
    return BlockProcessResult::kContinue;
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
  ProcessResult Process(Float64Add* node, const ProcessingState&) {
    UnionUpdate(node,
                Range::Add(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Float64SpeculateSafeAdd* node, const ProcessingState&) {
    UnionUpdate(node,
                Range::Add(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Float64Subtract* node, const ProcessingState&) {
    UnionUpdate(node,
                Range::Sub(Get(node->input_node(0)), Get(node->input_node(1))));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Float64Multiply* node, const ProcessingState&) {
    UnionUpdate(node,
                Range::Mul(Get(node->input_node(0)), Get(node->input_node(1))));
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
  ProcessResult Process(Int32ModulusWithOverflow* node,
                        const ProcessingState&) {
    UnionUpdateInt32(
        node, Range::Mod(Get(node->input_node(0)), Get(node->input_node(1))));
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
  ProcessResult Process(BuiltinStringPrototypeCharCodeOrCodePointAt* node,
                        const ProcessingState&) {
    UnionUpdateInt32(
        node,
        node->mode() == BuiltinStringPrototypeCharCodeOrCodePointAt::kCharCodeAt
            ? Range(0, String::kMaxUtf16CodeUnit)
            : Range(0, String::kMaxCodePoint));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(LoadUnsignedIntTypedArrayElement* node,
                        const ProcessingState&) {
    UnionUpdateUint32(node,
                      UnsignedTypedArrayElementRange(node->elements_kind()));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(LoadUnsignedIntConstantTypedArrayElement* node,
                        const ProcessingState&) {
    UnionUpdateUint32(node,
                      UnsignedTypedArrayElementRange(node->elements_kind()));
    return ProcessResult::kContinue;
  }
  ProcessResult Process(LoadTaggedField* node, const ProcessingState&) {
    if (node->type() == NodeType::kSmi) {
      UnionUpdate(node, node->is_array_length() == IsArrayLength::kYes
                            ? Range::NonNegativeSmi()
                            : Range::Smi());
    }
    return ProcessResult::kContinue;
  }
  ProcessResult Process(LoadFixedArrayElement* node, const ProcessingState&) {
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
    DCHECK_EQ(succ->predecessor_count(), 1);
    ValueNode* lhs = node->input_node(0);
    ValueNode* rhs = node->input_node(1);
    Range lhs_range = Get(lhs);
    Range rhs_range = Get(rhs);
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

  static Range UnsignedTypedArrayElementRange(ElementsKind kind) {
    switch (kind) {
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
        return Range(0, kMaxUInt8);
      case UINT16_ELEMENTS:
        return Range(0, kMaxUInt16);
      default:
        return Range::Uint32();
    }
  }

  void UnionUpdateUint32(ValueNode* node, Range range) {
    // WARNING: This entails that the current range analysis cannot be used to
    // identify truncation, since we always intersect Int32 operations range.
    DCHECK_NOT_NULL(current_block_);
    ranges_.UnionUpdate(current_block_, node,
                        Range::Intersect(Range::Uint32(), range));
  }

  void ProcessPhis(BasicBlock* block) {
    int pred_count = block->is_loop() ? block->predecessor_count() - 1
                                      : block->predecessor_count();
    for (Phi* phi : *block->phis()) {
      // Union all inputs first, so the phi is inserted only once.
      std::optional<Range> phi_range;
      for (int i = 0; i < pred_count; ++i) {
        BasicBlock* pred = block->predecessor_at(i);
        if (!ranges_.IsInitialized(pred)) {
          // Live forward preds are processed before `block`.
          DCHECK(pred->is_dead());
          continue;
        }
        Range input_range = ranges_.Get(pred, phi->input_node(i));
        if (phi->is_int32()) {
          // Since phi representation selector promoted this phi to Int32,
          // take its range into consideration.
          input_range = Range::Intersect(Range::Int32(), input_range);
        }
        phi_range =
            phi_range ? Range::Union(*phi_range, input_range) : input_range;
      }
      if (phi_range) ranges_.UnionUpdate(block, phi, *phi_range);
    }
  }

  void InitializeLoopPhisIfMissing(BasicBlock* block) {
    if (!block->has_phi()) return;
    for (Phi* phi : *block->phis()) {
      ranges_.InitializeRangeIfMissing(
          block, phi, phi->is_int32() ? Range::Int32() : Range::All());
    }
  }

  // Returns true if the loop reach a fixpoint.
  bool ProcessLoopPhisBackedge(BasicBlock* block, BasicBlock* backedge_pred) {
    if (!block->has_phi()) return true;
    DCHECK_EQ(backedge_pred, block->backedge_predecessor());
    DCHECK(ranges_.IsProcessed(block));
    TRACE_RANGE(">>>> Processing backedges for Block b" << block->id());
    bool is_done = true;
    for (Phi* phi : *block->phis()) {
      Range range = ranges_.Get(block, phi);
      Range backedge = ranges_.Get(backedge_pred, phi->backedge());
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
