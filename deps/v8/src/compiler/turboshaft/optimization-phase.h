// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_OPTIMIZATION_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_OPTIMIZATION_PHASE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/reducer-traits.h"
#include "src/compiler/turboshaft/snapshot-table.h"

namespace v8::internal::compiler::turboshaft {

using Variable =
    SnapshotTable<OpIndex, base::Optional<RegisterRepresentation>>::Key;
using MaybeVariable = base::Optional<Variable>;

int CountDecimalDigits(uint32_t value);
struct PaddingSpace {
  int spaces;
};
std::ostream& operator<<(std::ostream& os, PaddingSpace padding);

struct AnalyzerBase {
  Zone* phase_zone;
  const Graph& graph;

  void Run() {}
  bool OpIsUsed(OpIndex i) const {
    const Operation& op = graph.Get(i);
    return op.saturated_use_count > 0 ||
           op.Properties().is_required_when_unused;
  }

  explicit AnalyzerBase(const Graph& graph, Zone* phase_zone)
      : phase_zone(phase_zone), graph(graph) {}
};

// All operations whose `saturated_use_count` are unused and can be skipped.
// Analyzers modify the input graph in-place when they want to mark some
// Operations as removeable. In order to make that work for operations that have
// no uses such as Goto and Branch, all operations that have the property
// `is_required_when_unused` have a non-zero `saturated_use_count`.
V8_INLINE bool ShouldSkipOperation(const Operation& op) {
  return op.saturated_use_count == 0;
}

template <template <class> class... Reducers>
class OptimizationPhaseImpl {
 public:
  static void Run(Graph* input, Zone* phase_zone, NodeOriginTable* origins,
                  const typename Assembler<reducer_list<Reducers...>>::ArgT&
                      reducer_args = std::tuple<>{}) {
    Assembler<reducer_list<Reducers...>> phase(
        *input, input->GetOrCreateCompanion(), phase_zone, origins,
        reducer_args);
    if (v8_flags.turboshaft_trace_reduction) {
      phase.template VisitGraph<true>();
    } else {
      phase.template VisitGraph<false>();
    }
  }
};

template <template <typename> typename... Reducers>
class OptimizationPhase {
  using impl_t = OptimizationPhaseImpl<Reducers...>;

 public:
  static void Run(Isolate* isolate, Graph* input, Zone* phase_zone,
                  NodeOriginTable* origins,
                  const typename Assembler<reducer_list<Reducers...>>::ArgT&
                      reducer_args = std::tuple<>{}) {
    impl_t::Run(input, phase_zone, origins, reducer_args);
  }
};

template <typename Next>
class ReducerBaseForwarder;

template <class Assembler>
class GraphVisitor {
  template <typename Next>
  friend class ReducerBaseForwarder;

 public:
  GraphVisitor(Graph& input_graph, Graph& output_graph, Zone* phase_zone,
               compiler::NodeOriginTable* origins = nullptr)
      : input_graph_(input_graph),
        output_graph_(output_graph),
        phase_zone_(phase_zone),
        origins_(origins),
        current_input_block_(nullptr),
        op_mapping_(input_graph.op_id_count(), OpIndex::Invalid(), phase_zone),
        blocks_needing_variables(phase_zone),
        old_opindex_to_variables(input_graph.op_id_count(), phase_zone) {
    output_graph_.Reset();
  }

  // `trace_reduction` is a template parameter to avoid paying for tracing at
  // runtime.
  template <bool trace_reduction>
  void VisitGraph() {
    assembler().Analyze();

    // Creating initial old-to-new Block mapping.
    for (Block& input_block : modifiable_input_graph().blocks()) {
      output_graph().NewMappedBlock(&input_block);
    }

    // Visiting the graph.
    VisitAllBlocks<trace_reduction>();

    // Updating the source_positions.
    if (!input_graph().source_positions().empty()) {
      for (OpIndex index : output_graph_.AllOperationIndices()) {
        OpIndex origin = output_graph_.operation_origins()[index];
        output_graph_.source_positions()[index] =
            input_graph().source_positions()[origin];
      }
    }
    // Updating the operation origins.
    if (origins_) {
      for (OpIndex index : assembler().output_graph().AllOperationIndices()) {
        OpIndex origin = assembler().output_graph().operation_origins()[index];
        origins_->SetNodeOrigin(index.id(), origin.id());
      }
    }

    input_graph_.SwapWithCompanion();
  }

  Zone* graph_zone() const { return input_graph().graph_zone(); }
  const Graph& input_graph() const { return input_graph_; }
  Graph& output_graph() const { return output_graph_; }
  Zone* phase_zone() { return phase_zone_; }
  const Block* current_input_block() { return current_input_block_; }

  // Analyzers set Operations' saturated_use_count to zero when they are unused,
  // and thus need to have a non-const input graph.
  Graph& modifiable_input_graph() const { return input_graph_; }

  // Visits and emits {input_block} right now (ie, in the current block).
  void CloneAndInlineBlock(const Block* input_block) {
    // Computing which input of Phi operations to use when visiting
    // {input_block} (since {input_block} doesn't really have predecessors
    // anymore).
    int added_block_phi_input = input_block->GetPredecessorIndex(
        assembler().current_block()->OriginForBlockEnd());

    // There is no guarantees that {input_block} will be entirely removed just
    // because it's cloned/inlined, since it's possible that it has predecessors
    // for which this optimization didn't apply. As a result, we add it to
    // {blocks_needing_variables}, so that if it's ever generated
    // normally, Variables are used when emitting its content, so that
    // they can later be merged when control flow merges with the current
    // version of {input_block} that we just cloned.
    blocks_needing_variables.insert(input_block->index());

    // Updating the origin of "current_block", so that translating Phis can
    // still properly be done (in OptimizationPhase::ReducePhi).
    assembler().current_block()->SetOrigin(input_block);

    ScopedModification<bool> set_true(&current_block_needs_variables_, true);
    for (OpIndex index : input_graph().OperationIndices(*input_block)) {
      if (const PhiOp* phi =
              input_graph().Get(index).template TryCast<PhiOp>()) {
        // This Phi has been cloned/inlined, and has thus now a single
        // predecessor, and shouldn't be a Phi anymore.
        CreateOldToNewMapping(index,
                              MapToNewGraph(phi->input(added_block_phi_input)));
      } else {
        if (!VisitOp<false>(index, input_block)) break;
      }
    }
  }

  template <bool can_be_invalid = false>
  OpIndex MapToNewGraph(OpIndex old_index, int predecessor_index = -1) {
    DCHECK(old_index.valid());
    OpIndex result = op_mapping_[old_index.id()];
    if (!result.valid()) {
      // {op_mapping} doesn't have a mapping for {old_index}. The assembler
      // should provide the mapping.
      MaybeVariable var = GetVariableFor(old_index);
      if constexpr (can_be_invalid) {
        if (!var.has_value()) {
          return OpIndex::Invalid();
        }
      }
      DCHECK(var.has_value());
      if (predecessor_index == -1) {
        result = assembler().Get(var.value());
      } else {
        result =
            assembler().GetPredecessorValue(var.value(), predecessor_index);
      }
    }
    DCHECK(result.valid());
    return result;
  }

 private:
  template <bool trace_reduction>
  void VisitAllBlocks() {
    base::SmallVector<const Block*, 128> visit_stack;

    visit_stack.push_back(&input_graph().StartBlock());
    while (!visit_stack.empty()) {
      const Block* block = visit_stack.back();
      visit_stack.pop_back();
      VisitBlock<trace_reduction>(block);

      for (Block* child = block->LastChild(); child != nullptr;
           child = child->NeighboringChild()) {
        visit_stack.push_back(child);
      }
    }
  }

  template <bool trace_reduction>
  void VisitBlock(const Block* input_block) {
    current_input_block_ = input_block;
    current_block_needs_variables_ =
        blocks_needing_variables.find(input_block->index()) !=
        blocks_needing_variables.end();
    if constexpr (trace_reduction) {
      std::cout << "\nold " << PrintAsBlockHeader{*input_block} << "\n";
      std::cout
          << "new "
          << PrintAsBlockHeader{*input_block->MapToNextGraph(),
                                assembler().output_graph().next_block_index()}
          << "\n";
    }
    Block* new_block = input_block->MapToNextGraph();
    if (assembler().Bind(new_block)) {
      for (OpIndex index : input_graph().OperationIndices(*input_block)) {
        if (!VisitOp<trace_reduction>(index, input_block)) break;
      }
      if constexpr (trace_reduction) TraceBlockFinished();
    } else {
      if constexpr (trace_reduction) TraceBlockUnreachable();
    }

    // If we eliminate a loop backedge, we need to turn the loop into a
    // single-predecessor merge block.
    const Operation& last_op =
        *base::Reversed(input_graph().operations(*input_block)).begin();
    if (auto* final_goto = last_op.TryCast<GotoOp>()) {
      if (final_goto->destination->IsLoop()) {
        if (input_block->index() > final_goto->destination->index()) {
          Block* new_loop = final_goto->destination->MapToNextGraph();
          DCHECK(new_loop->IsLoop());
          if (new_loop->IsLoop() && new_loop->PredecessorCount() == 1) {
            output_graph_.TurnLoopIntoMerge(new_loop);
          }
        } else {
          // We have a forward jump to a loop, rather than a backedge. We
          // don't need to do anything.
        }
      }
    }
  }

  template <bool trace_reduction>
  bool VisitOp(OpIndex index, const Block* input_block) {
    Block* current_block = assembler().current_block();
    if (!current_block) return false;
    assembler().SetCurrentOrigin(index);
    OpIndex first_output_index =
        assembler().output_graph().next_operation_index();
    USE(first_output_index);
    const Operation& op = input_graph().Get(index);
    if constexpr (trace_reduction) TraceReductionStart(index);
    if (ShouldSkipOperation(op)) {
      if constexpr (trace_reduction) TraceOperationSkipped();
      return true;
    }
    OpIndex new_index;
    switch (op.opcode) {
#define EMIT_INSTR_CASE(Name)                                           \
  case Opcode::k##Name:                                                 \
    new_index =                                                         \
        assembler().ReduceInputGraph##Name(index, op.Cast<Name##Op>()); \
    if (CanBeUsedAsInput(op.Cast<Name##Op>())) {                        \
      if (!new_index.valid()) {                                         \
        if constexpr (trace_reduction) TraceOperationSkipped();         \
        return true;                                                    \
      }                                                                 \
      CreateOldToNewMapping(index, new_index);                          \
    }                                                                   \
    break;
      TURBOSHAFT_OPERATION_LIST(EMIT_INSTR_CASE)
#undef EMIT_INSTR_CASE
    }
      if constexpr (trace_reduction) {
        TraceReductionResult(current_block, first_output_index, new_index);
      }
#ifdef DEBUG
    if (V8_UNLIKELY(v8_flags.turboshaft_verify_reductions)) {
      if (new_index.valid()) {
        assembler().Verify(index, new_index);
      }
    }
#endif  // DEBUG
    return true;
  }

  void TraceReductionStart(OpIndex index) {
    std::cout << "╭── o" << index.id() << ": "
              << PaddingSpace{5 - CountDecimalDigits(index.id())}
              << OperationPrintStyle{input_graph().Get(index), "#o"} << "\n";
  }
  void TraceOperationSkipped() { std::cout << "╰─> skipped\n\n"; }
  void TraceBlockUnreachable() { std::cout << "╰─> unreachable\n\n"; }
  void TraceReductionResult(Block* current_block, OpIndex first_output_index,
                            OpIndex new_index) {
    if (new_index < first_output_index) {
      // The operation was replaced with an already existing one.
      std::cout << "╰─> #n" << new_index.id() << "\n";
    }
    bool before_arrow = new_index >= first_output_index;
    for (const Operation& op : output_graph_.operations(
             first_output_index, output_graph_.next_operation_index())) {
      OpIndex index = output_graph_.Index(op);
      const char* prefix;
      if (index == new_index) {
        prefix = "╰─>";
        before_arrow = false;
      } else if (before_arrow) {
        prefix = "│  ";
      } else {
        prefix = "   ";
      }
      std::cout << prefix << " n" << index.id() << ": "
                << PaddingSpace{5 - CountDecimalDigits(index.id())}
                << OperationPrintStyle{output_graph_.Get(index), "#n"} << "\n";
      if (op.Properties().is_block_terminator && assembler().current_block() &&
          assembler().current_block() != current_block) {
        current_block = &assembler().output_graph().Get(
            BlockIndex(current_block->index().id() + 1));
        std::cout << "new " << PrintAsBlockHeader{*current_block} << "\n";
      }
    }
    std::cout << "\n";
  }
  void TraceBlockFinished() { std::cout << "\n"; }

  // These functions take an operation from the old graph and use the assembler
  // to emit a corresponding operation in the new graph, translating inputs and
  // blocks accordingly.
  V8_INLINE OpIndex AssembleOutputGraphGoto(const GotoOp& op) {
    Block* destination = op.destination->MapToNextGraph();
    assembler().ReduceGoto(destination);
    if (destination->IsBound()) {
      DCHECK(destination->IsLoop());
      FixLoopPhis(destination);
    }
    return OpIndex::Invalid();
  }
  V8_INLINE OpIndex AssembleOutputGraphBranch(const BranchOp& op) {
    Block* if_true = op.if_true->MapToNextGraph();
    Block* if_false = op.if_false->MapToNextGraph();
    return assembler().ReduceBranch(MapToNewGraph(op.condition()), if_true,
                                    if_false, op.hint);
  }
  OpIndex AssembleOutputGraphSwitch(const SwitchOp& op) {
    base::SmallVector<SwitchOp::Case, 16> cases;
    for (SwitchOp::Case c : op.cases) {
      cases.emplace_back(c.value, c.destination->MapToNextGraph(), c.hint);
    }
    return assembler().ReduceSwitch(
        MapToNewGraph(op.input()),
        graph_zone()->CloneVector(base::VectorOf(cases)),
        op.default_case->MapToNextGraph(), op.default_hint);
  }
  OpIndex AssembleOutputGraphPhi(const PhiOp& op) {
    OpIndex ig_index = input_graph().Index(op);
    if (current_input_block_->IsLoop()) {
      if (ig_index == op.input(PhiOp::kLoopPhiBackEdgeIndex)) {
        // Avoid emitting a Loop Phi which points to itself, instead
        // emit it's 0'th input.
        return MapToNewGraph(op.input(0));
      }
      return assembler().PendingLoopPhi(MapToNewGraph(op.input(0)), op.rep,
                                        op.input(PhiOp::kLoopPhiBackEdgeIndex));
    }

    base::Vector<const OpIndex> old_inputs = op.inputs();
    base::SmallVector<OpIndex, 8> new_inputs;
    int predecessor_count = assembler().current_block()->PredecessorCount();
    Block* old_pred = current_input_block_->LastPredecessor();
    Block* new_pred = assembler().current_block()->LastPredecessor();
    // Control predecessors might be missing after the optimization phase. So we
    // need to skip phi inputs that belong to control predecessors that have no
    // equivalent in the new graph.

    // We first assume that the order if the predecessors of the current block
    // did not change. If it did, {new_pred} won't be nullptr at the end of this
    // loop, and we'll instead fall back to the slower code below to compute the
    // inputs of the Phi.
    int predecessor_index = predecessor_count - 1;
    for (OpIndex input : base::Reversed(old_inputs)) {
      if (new_pred && new_pred->OriginForBlockEnd() == old_pred) {
        // Phis inputs have to come from predecessors. We thus have to
        // MapToNewGraph with {predecessor_index} so that we get an OpIndex that
        // is from a predecessor rather than one that comes from a Variable
        // merged in the current block.
        new_inputs.push_back(MapToNewGraph(input, predecessor_index));
        new_pred = new_pred->NeighboringPredecessor();
        predecessor_index--;
      }
      old_pred = old_pred->NeighboringPredecessor();
    }
    DCHECK_IMPLIES(new_pred == nullptr, old_pred == nullptr);

    if (new_pred != nullptr) {
      // If {new_pred} is nullptr, then the order of the predecessors changed.
      // This should only happen with blocks that were introduced in the
      // previous graph. For instance, consider this (partial) dominator tree:
      //
      //     ╠ 7
      //     ║ ╠ 8
      //     ║ ╚ 10
      //     ╠ 9
      //     ╚ 11
      //
      // Where the predecessors of block 11 are blocks 9 and 10 (in that order).
      // In dominator visit order, block 10 will be visited before block 9.
      // Since blocks are added to predecessors when the predecessors are
      // visited, it means that in the new graph, the predecessors of block 11
      // are [10, 9] rather than [9, 10].
      // To account for this, we reorder the inputs of the Phi, and get rid of
      // inputs from blocks that vanished.

#ifdef DEBUG
      // To check that indices are set properly, we zap them in debug builds.
      const uint32_t invalid_custom_data = std::numeric_limits<uint32_t>::max();
      for (auto& block : assembler().modifiable_input_graph().blocks()) {
        block.custom_data() = invalid_custom_data;
      }
#endif
      uint32_t pos = current_input_block_->PredecessorCount() - 1;
      for (old_pred = current_input_block_->LastPredecessor();
           old_pred != nullptr; old_pred = old_pred->NeighboringPredecessor()) {
        // Store the current index of the {old_pred}.
        DCHECK_EQ(old_pred->custom_data(), invalid_custom_data);
        old_pred->custom_data() = pos--;
      }

      // Filling {new_inputs}: we iterate the new predecessors, and, for each
      // predecessor, we check the index of the input corresponding to the old
      // predecessor, and we put it next in {new_inputs}.
      new_inputs.clear();
      int predecessor_index = predecessor_count - 1;
      for (new_pred = assembler().current_block()->LastPredecessor();
           new_pred != nullptr; new_pred = new_pred->NeighboringPredecessor()) {
        const Block* origin = new_pred->OriginForBlockEnd();
        DCHECK_NOT_NULL(origin);
        DCHECK_NE(origin->custom_data(), invalid_custom_data);
        OpIndex input = old_inputs[origin->custom_data()];
        // Phis inputs have to come from predecessors. We thus have to
        // MapToNewGraph with {predecessor_index} so that we get an OpIndex that
        // is from a predecessor rather than one that comes from a Variable
        // merged in the current block.
        new_inputs.push_back(MapToNewGraph(input, predecessor_index));
        predecessor_index--;
      }
    }

    DCHECK_EQ(new_inputs.size(),
              assembler().current_block()->PredecessorCount());

    if (new_inputs.size() == 1) {
      // This Operation used to be a Phi in a Merge, but since one (or more) of
      // the inputs of the merge have been removed, there is no need for a Phi
      // anymore.
      return new_inputs[0];
    }

    std::reverse(new_inputs.begin(), new_inputs.end());
    return assembler().ReducePhi(base::VectorOf(new_inputs), op.rep);
  }
  OpIndex AssembleOutputGraphPendingLoopPhi(const PendingLoopPhiOp& op) {
    UNREACHABLE();
  }
  V8_INLINE OpIndex AssembleOutputGraphFrameState(const FrameStateOp& op) {
    auto inputs = MapToNewGraph<32>(op.inputs());
    return assembler().ReduceFrameState(base::VectorOf(inputs), op.inlined,
                                        op.data);
  }
  OpIndex AssembleOutputGraphCall(const CallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    OpIndex frame_state = MapToNewGraphIfValid(op.frame_state());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return assembler().ReduceCall(callee, frame_state,
                                  base::VectorOf(arguments), op.descriptor);
  }
  OpIndex AssembleOutputGraphCallAndCatchException(
      const CallAndCatchExceptionOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    Block* if_success = op.if_success->MapToNextGraph();
    Block* if_exception = op.if_exception->MapToNextGraph();
    OpIndex frame_state = MapToNewGraphIfValid(op.frame_state());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return assembler().ReduceCallAndCatchException(
        callee, frame_state, base::VectorOf(arguments), if_success,
        if_exception, op.descriptor);
  }
  OpIndex AssembleOutputGraphLoadException(const LoadExceptionOp& op) {
    return assembler().ReduceLoadException();
  }
  OpIndex AssembleOutputGraphTailCall(const TailCallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return assembler().ReduceTailCall(callee, base::VectorOf(arguments),
                                      op.descriptor);
  }
  OpIndex AssembleOutputGraphReturn(const ReturnOp& op) {
    // We very rarely have tuples longer than 4.
    auto return_values = MapToNewGraph<4>(op.return_values());
    return assembler().ReduceReturn(MapToNewGraph(op.pop_count()),
                                    base::VectorOf(return_values));
  }
  OpIndex AssembleOutputGraphOverflowCheckedBinop(
      const OverflowCheckedBinopOp& op) {
    return assembler().ReduceOverflowCheckedBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphWordUnary(const WordUnaryOp& op) {
    return assembler().ReduceWordUnary(MapToNewGraph(op.input()), op.kind,
                                       op.rep);
  }
  OpIndex AssembleOutputGraphFloatUnary(const FloatUnaryOp& op) {
    return assembler().ReduceFloatUnary(MapToNewGraph(op.input()), op.kind,
                                        op.rep);
  }
  OpIndex AssembleOutputGraphShift(const ShiftOp& op) {
    return assembler().ReduceShift(MapToNewGraph(op.left()),
                                   MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphEqual(const EqualOp& op) {
    return assembler().ReduceEqual(MapToNewGraph(op.left()),
                                   MapToNewGraph(op.right()), op.rep);
  }
  OpIndex AssembleOutputGraphComparison(const ComparisonOp& op) {
    return assembler().ReduceComparison(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphChange(const ChangeOp& op) {
    return assembler().ReduceChange(MapToNewGraph(op.input()), op.kind,
                                    op.assumption, op.from, op.to);
  }
  OpIndex AssembleOutputGraphChangeOrDeopt(const ChangeOrDeoptOp& op) {
    return assembler().ReduceChangeOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()), op.kind,
        op.minus_zero_mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTryChange(const TryChangeOp& op) {
    return assembler().ReduceTryChange(MapToNewGraph(op.input()), op.kind,
                                       op.from, op.to);
  }
  OpIndex AssembleOutputGraphTag(const TagOp& op) {
    return assembler().ReduceTag(MapToNewGraph(op.input()), op.kind);
  }
  OpIndex AssembleOutputGraphUntag(const UntagOp& op) {
    return assembler().ReduceUntag(MapToNewGraph(op.input()), op.kind, op.rep);
  }

  OpIndex AssembleOutputGraphFloat64InsertWord32(
      const Float64InsertWord32Op& op) {
    return assembler().ReduceFloat64InsertWord32(
        MapToNewGraph(op.float64()), MapToNewGraph(op.word32()), op.kind);
  }
  OpIndex AssembleOutputGraphTaggedBitcast(const TaggedBitcastOp& op) {
    return assembler().ReduceTaggedBitcast(MapToNewGraph(op.input()), op.from,
                                           op.to);
  }
  OpIndex AssembleOutputGraphObjectIs(const ObjectIsOp& op) {
    return assembler().ReduceObjectIs(MapToNewGraph(op.input()), op.kind,
                                      op.input_assumptions);
  }
  OpIndex AssembleOutputGraphFloatIs(const FloatIsOp& op) {
    return assembler().ReduceFloatIs(MapToNewGraph(op.input()), op.kind,
                                     op.input_rep);
  }
  OpIndex AssembleOutputGraphConvertToObject(const ConvertToObjectOp& op) {
    return assembler().ReduceConvertToObject(
        MapToNewGraph(op.input()), op.kind, op.input_rep,
        op.input_interpretation, op.minus_zero_mode);
  }
  OpIndex AssembleOutputGraphConvertToObjectOrDeopt(
      const ConvertToObjectOrDeoptOp& op) {
    return assembler().ReduceConvertToObjectOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()), op.kind,
        op.input_rep, op.input_interpretation, op.feedback);
  }
  OpIndex AssembleOutputGraphConvertObjectToPrimitive(
      const ConvertObjectToPrimitiveOp& op) {
    return assembler().ReduceConvertObjectToPrimitive(
        MapToNewGraph(op.input()), op.kind, op.input_assumptions);
  }
  OpIndex AssembleOutputGraphConvertObjectToPrimitiveOrDeopt(
      const ConvertObjectToPrimitiveOrDeoptOp& op) {
    return assembler().ReduceConvertObjectToPrimitiveOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()),
        op.from_kind, op.to_kind, op.minus_zero_mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTruncateObjectToPrimitive(
      const TruncateObjectToPrimitiveOp& op) {
    return assembler().ReduceTruncateObjectToPrimitive(
        MapToNewGraph(op.input()), op.kind, op.input_assumptions);
  }
  OpIndex AssembleOutputGraphSelect(const SelectOp& op) {
    return assembler().ReduceSelect(
        MapToNewGraph(op.cond()), MapToNewGraph(op.vtrue()),
        MapToNewGraph(op.vfalse()), op.rep, op.hint, op.implem);
  }
  OpIndex AssembleOutputGraphConstant(const ConstantOp& op) {
    return assembler().ReduceConstant(op.kind, op.storage);
  }
  OpIndex AssembleOutputGraphLoad(const LoadOp& op) {
    return assembler().ReduceLoad(
        MapToNewGraph(op.base()), MapToNewGraphIfValid(op.index()), op.kind,
        op.loaded_rep, op.result_rep, op.offset, op.element_size_log2);
  }
  OpIndex AssembleOutputGraphStore(const StoreOp& op) {
    return assembler().ReduceStore(
        MapToNewGraph(op.base()), MapToNewGraphIfValid(op.index()),
        MapToNewGraph(op.value()), op.kind, op.stored_rep, op.write_barrier,
        op.offset, op.element_size_log2);
  }
  OpIndex AssembleOutputGraphAllocate(const AllocateOp& op) {
    return assembler().Allocate(MapToNewGraph(op.size()), op.type,
                                op.allow_large_objects);
  }
  OpIndex AssembleOutputGraphDecodeExternalPointer(
      const DecodeExternalPointerOp& op) {
    return assembler().DecodeExternalPointer(MapToNewGraph(op.handle()),
                                             op.tag);
  }
  OpIndex AssembleOutputGraphRetain(const RetainOp& op) {
    return assembler().ReduceRetain(MapToNewGraph(op.retained()));
  }
  OpIndex AssembleOutputGraphParameter(const ParameterOp& op) {
    return assembler().ReduceParameter(op.parameter_index, op.rep,
                                       op.debug_name);
  }
  OpIndex AssembleOutputGraphOsrValue(const OsrValueOp& op) {
    return assembler().ReduceOsrValue(op.index);
  }
  OpIndex AssembleOutputGraphStackPointerGreaterThan(
      const StackPointerGreaterThanOp& op) {
    return assembler().ReduceStackPointerGreaterThan(
        MapToNewGraph(op.stack_limit()), op.kind);
  }
  OpIndex AssembleOutputGraphStackSlot(const StackSlotOp& op) {
    return assembler().ReduceStackSlot(op.size, op.alignment);
  }
  OpIndex AssembleOutputGraphFrameConstant(const FrameConstantOp& op) {
    return assembler().ReduceFrameConstant(op.kind);
  }
  OpIndex AssembleOutputGraphDeoptimize(const DeoptimizeOp& op) {
    return assembler().ReduceDeoptimize(MapToNewGraph(op.frame_state()),
                                        op.parameters);
  }
  OpIndex AssembleOutputGraphDeoptimizeIf(const DeoptimizeIfOp& op) {
    return assembler().ReduceDeoptimizeIf(MapToNewGraph(op.condition()),
                                          MapToNewGraph(op.frame_state()),
                                          op.negated, op.parameters);
  }
  OpIndex AssembleOutputGraphTrapIf(const TrapIfOp& op) {
    return assembler().ReduceTrapIf(MapToNewGraph(op.condition()), op.negated,
                                    op.trap_id);
  }
  OpIndex AssembleOutputGraphTuple(const TupleOp& op) {
    return assembler().ReduceTuple(
        base::VectorOf(MapToNewGraph<4>(op.inputs())));
  }
  OpIndex AssembleOutputGraphProjection(const ProjectionOp& op) {
    return assembler().ReduceProjection(MapToNewGraph(op.input()), op.index,
                                        op.rep);
  }
  OpIndex AssembleOutputGraphWordBinop(const WordBinopOp& op) {
    return assembler().ReduceWordBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphFloatBinop(const FloatBinopOp& op) {
    return assembler().ReduceFloatBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphUnreachable(const UnreachableOp& op) {
    return assembler().ReduceUnreachable();
  }
  OpIndex AssembleOutputGraphStaticAssert(const StaticAssertOp& op) {
    return assembler().ReduceStaticAssert(op.condition(), op.source);
  }
  OpIndex AssembleOutputGraphCheckTurboshaftTypeOf(
      const CheckTurboshaftTypeOfOp& op) {
    return assembler().ReduceCheckTurboshaftTypeOf(
        MapToNewGraph(op.input()), op.rep, op.type, op.successful);
  }
  OpIndex AssembleOutputGraphNewConsString(const NewConsStringOp& op) {
    return assembler().ReduceNewConsString(MapToNewGraph(op.length()),
                                           MapToNewGraph(op.first()),
                                           MapToNewGraph(op.second()));
  }
  OpIndex AssembleOutputGraphNewArray(const NewArrayOp& op) {
    return assembler().ReduceNewArray(MapToNewGraph(op.length()), op.kind,
                                      op.allocation_type);
  }
  OpIndex AssembleOutputGraphDoubleArrayMinMax(const DoubleArrayMinMaxOp& op) {
    return assembler().ReduceDoubleArrayMinMax(MapToNewGraph(op.array()),
                                               op.kind);
  }
  OpIndex AssembleOutputGraphLoadFieldByIndex(const LoadFieldByIndexOp& op) {
    return assembler().ReduceLoadFieldByIndex(MapToNewGraph(op.object()),
                                              MapToNewGraph(op.index()));
  }
  OpIndex AssembleOutputGraphDebugBreak(const DebugBreakOp& op) {
    return assembler().ReduceDebugBreak();
  }
  OpIndex AssembleOutputGraphBigIntBinop(const BigIntBinopOp& op) {
    return assembler().ReduceBigIntBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()),
        MapToNewGraph(op.frame_state()), op.kind);
  }
  OpIndex AssembleOutputGraphBigIntEqual(const BigIntEqualOp& op) {
    return assembler().ReduceBigIntEqual(MapToNewGraph(op.left()),
                                         MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphBigIntComparison(const BigIntComparisonOp& op) {
    return assembler().ReduceBigIntComparison(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind);
  }
  OpIndex AssembleOutputGraphBigIntUnary(const BigIntUnaryOp& op) {
    return assembler().ReduceBigIntUnary(MapToNewGraph(op.input()), op.kind);
  }
  OpIndex AssembleOutputGraphLoadRootRegister(const LoadRootRegisterOp& op) {
    return assembler().ReduceLoadRootRegister();
  }
  OpIndex AssembleOutputGraphStringAt(const StringAtOp& op) {
    return assembler().ReduceStringAt(MapToNewGraph(op.string()),
                                      MapToNewGraph(op.position()), op.kind);
  }
#ifdef V8_INTL_SUPPORT
  OpIndex AssembleOutputGraphStringToCaseIntl(const StringToCaseIntlOp& op) {
    return assembler().ReduceStringToCaseIntl(MapToNewGraph(op.string()),
                                              op.kind);
  }
#endif  // V8_INTL_SUPPORT
  OpIndex AssembleOutputGraphStringLength(const StringLengthOp& op) {
    return assembler().ReduceStringLength(MapToNewGraph(op.string()));
  }
  OpIndex AssembleOutputGraphStringIndexOf(const StringIndexOfOp& op) {
    return assembler().ReduceStringIndexOf(MapToNewGraph(op.string()),
                                           MapToNewGraph(op.search()),
                                           MapToNewGraph(op.position()));
  }
  OpIndex AssembleOutputGraphStringFromCodePointAt(
      const StringFromCodePointAtOp& op) {
    return assembler().ReduceStringFromCodePointAt(MapToNewGraph(op.string()),
                                                   MapToNewGraph(op.index()));
  }
  OpIndex AssembleOutputGraphStringSubstring(const StringSubstringOp& op) {
    return assembler().ReduceStringSubstring(MapToNewGraph(op.string()),
                                             MapToNewGraph(op.start()),
                                             MapToNewGraph(op.end()));
  }
  OpIndex AssembleOutputGraphStringEqual(const StringEqualOp& op) {
    return assembler().ReduceStringEqual(MapToNewGraph(op.left()),
                                         MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphStringComparison(const StringComparisonOp& op) {
    return assembler().ReduceStringComparison(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind);
  }

  void CreateOldToNewMapping(OpIndex old_index, OpIndex new_index) {
    if (current_block_needs_variables_) {
      MaybeVariable var = GetVariableFor(old_index);
      if (!var.has_value()) {
        base::Optional<RegisterRepresentation> rep =
            input_graph().Get(old_index).outputs_rep().size() == 1
                ? base::Optional<RegisterRepresentation>{input_graph()
                                                             .Get(old_index)
                                                             .outputs_rep()[0]}
                : base::nullopt;
        var = assembler().NewFreshVariable(rep);
        SetVariableFor(old_index, *var);
      }
      assembler().Set(*var, new_index);
      return;
    }
    op_mapping_[old_index.id()] = new_index;
  }

  template <bool can_be_invalid = false>
  OpIndex MapToNewGraphIfValid(OpIndex old_index, int predecessor_index = -1) {
    return old_index.valid()
               ? MapToNewGraph<can_be_invalid>(old_index, predecessor_index)
               : OpIndex::Invalid();
  }

  MaybeVariable GetVariableFor(OpIndex old_index) const {
    return old_opindex_to_variables[old_index];
  }

  void SetVariableFor(OpIndex old_index, MaybeVariable var) {
    DCHECK(!old_opindex_to_variables[old_index].has_value());
    old_opindex_to_variables[old_index] = var;
  }

  template <size_t expected_size>
  base::SmallVector<OpIndex, expected_size> MapToNewGraph(
      base::Vector<const OpIndex> inputs) {
    base::SmallVector<OpIndex, expected_size> result;
    for (OpIndex input : inputs) {
      result.push_back(MapToNewGraph(input));
    }
    return result;
  }

  void FixLoopPhis(Block* loop) {
    DCHECK(loop->IsLoop());
    for (Operation& op : assembler().output_graph().operations(*loop)) {
      if (auto* pending_phi = op.TryCast<PendingLoopPhiOp>()) {
        assembler().output_graph().template Replace<PhiOp>(
            assembler().output_graph().Index(*pending_phi),
            base::VectorOf(
                {pending_phi->first(),
                 MapToNewGraph(pending_phi->data.old_backedge_index)}),
            pending_phi->rep);
      }
    }
  }

  // TODO(dmercadier,tebbi): unify the ways we refer to the Assembler.
  // Currently, we have Asm(), assembler(), and to a certain extent, stack().
  Assembler& assembler() { return static_cast<Assembler&>(*this); }

  Graph& input_graph_;
  Graph& output_graph_;
  Zone* phase_zone_;
  compiler::NodeOriginTable* origins_;

  const Block* current_input_block_;

  // Mappings from the old graph to the new graph.
  ZoneVector<OpIndex> op_mapping_;

  // {current_block_needs_variables_} is set to true if the current block should
  // use Variables to map old to new OpIndex rather than just {op_mapping}. This
  // is typically the case when the block has been cloned.
  bool current_block_needs_variables_ = false;

  // Set of Blocks for which Variables should be used rather than
  // {op_mapping}.
  ZoneSet<BlockIndex> blocks_needing_variables;

  // Mapping from old OpIndex to Variables.
  FixedSidetable<MaybeVariable> old_opindex_to_variables;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_OPTIMIZATION_PHASE_H_
