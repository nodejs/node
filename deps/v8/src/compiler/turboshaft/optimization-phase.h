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
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/reducer-traits.h"
#include "src/compiler/turboshaft/snapshot-table.h"

namespace v8::internal::compiler::turboshaft {

using MaybeVariable = base::Optional<Variable>;

int CountDecimalDigits(uint32_t value);
struct PaddingSpace {
  int spaces;
};
std::ostream& operator<<(std::ostream& os, PaddingSpace padding);

// All operations whose `saturated_use_count` is 0 are unused and can be
// skipped. Analyzers modify the input graph in-place when they want to mark
// some Operations as removeable. In order to make that work for operations that
// have no uses such as Goto and Branch, all operations that have the property
// `IsRequiredWhenUnused()` have a non-zero `saturated_use_count`.
V8_INLINE bool ShouldSkipOperation(const Operation& op) {
  return op.saturated_use_count.IsZero();
}

template <template <class> class... Reducers>
class OptimizationPhaseImpl {
 public:
  static void Run(Zone* phase_zone) {
    PipelineData& data = PipelineData::Get();
    Graph& input_graph = data.graph();
    Assembler<reducer_list<Reducers...>> phase(
        input_graph, input_graph.GetOrCreateCompanion(), phase_zone,
        data.node_origins());
    if (data.info()->turboshaft_trace_reduction()) {
      phase.template VisitGraph<true>();
    } else {
      phase.template VisitGraph<false>();
    }
  }
};

template <template <typename> typename... Reducers>
class OptimizationPhase {
 public:
  static void Run(Zone* phase_zone) {
    OptimizationPhaseImpl<Reducers...>::Run(phase_zone);
  }
};

template <typename Next>
class ReducerBaseForwarder;
template <typename Next>
class VariableReducer;

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

  // {InlineOp} introduces two limitations unlike {CloneAndInlineBlock}:
  // 1. The input operation must not be emitted anymore as part of its
  // regular input block;
  // 2. {InlineOp} must not be used multiple times for the same input op.
  bool InlineOp(OpIndex index, const Block* input_block) {
    return VisitOp<false>(index, input_block);
  }

  template <bool can_be_invalid = false>
  OpIndex MapToNewGraph(OpIndex old_index, int predecessor_index = -1) {
    DCHECK(old_index.valid());
    OpIndex result = op_mapping_[old_index.id()];

    if constexpr (reducer_list_contains<typename Assembler::ReducerList,
                                        VariableReducer>::value) {
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
          result = assembler().GetVariable(var.value());
        } else {
          result =
              assembler().GetPredecessorValue(var.value(), predecessor_index);
        }
      }
    }
    DCHECK_IMPLIES(!can_be_invalid, result.valid());
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
          assembler().FinalizeLoop(final_goto->destination->MapToNextGraph());
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
    current_block->SetOrigin(input_block);
    OpIndex first_output_index =
        assembler().output_graph().next_operation_index();
    USE(first_output_index);
    const Operation& op = input_graph().Get(index);
    if constexpr (trace_reduction) TraceReductionStart(index);
    if (!ShouldSkipOptimizationStep() && ShouldSkipOperation(op)) {
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
        const Operation& new_op = output_graph().Get(new_index);
        DCHECK_EQ(new_op.outputs_rep().size(), op.outputs_rep().size());
        for (size_t i = 0; i < new_op.outputs_rep().size(); ++i) {
          DCHECK(new_op.outputs_rep()[i].AllowImplicitRepresentationChangeTo(
              op.outputs_rep()[i]));
        }
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
      if (op.IsBlockTerminator() && assembler().current_block() &&
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
    if (destination->IsBound()) {
      DCHECK(destination->IsLoop());
      FixLoopPhis(op.destination);
    }
    // It is important that we first fix loop phis and then reduce the `Goto`,
    // because reducing the `Goto` can have side effects, in particular, it can
    // modify affect the SnapshotTable of `VariableReducer`, which is also used
    // by `FixLoopPhis()`.
    assembler().ReduceGoto(destination);
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
      return assembler().PendingLoopPhi(MapToNewGraph(op.input(0)), op.rep);
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
      // If {new_pred} is not nullptr, then the order of the predecessors
      // changed. This should only happen with blocks that were introduced in
      // the previous graph. For instance, consider this (partial) dominator
      // tree:
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
      for (auto& block : assembler().modifiable_input_graph().blocks()) {
        block.clear_custom_data();
      }
#endif
      uint32_t pos = current_input_block_->PredecessorCount() - 1;
      for (old_pred = current_input_block_->LastPredecessor();
           old_pred != nullptr; old_pred = old_pred->NeighboringPredecessor()) {
        // Store the current index of the {old_pred}.
        old_pred->set_custom_data(pos--, Block::CustomDataKind::kPhiInputIndex);
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
        OpIndex input = old_inputs[origin->get_custom_data(
            Block::CustomDataKind::kPhiInputIndex)];
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
                                  base::VectorOf(arguments), op.descriptor,
                                  op.Effects());
  }
  OpIndex AssembleOutputGraphDidntThrow(const DidntThrowOp& op) {
    const Operation& throwing_operation =
        input_graph().Get(op.throwing_operation());
    OpIndex result;
    switch (throwing_operation.opcode) {
#define CASE(Name)                                                           \
  case Opcode::k##Name:                                                      \
    result = AssembleOutputGraph##Name(throwing_operation.Cast<Name##Op>()); \
    break;
      TURBOSHAFT_THROWING_OPERATIONS_LIST(CASE)
#undef CASE
      default:
        UNREACHABLE();
    }
    return result;
  }
  OpIndex AssembleOutputGraphCheckException(const CheckExceptionOp& op) {
    Graph::OpIndexIterator it(op.didnt_throw_block->begin(),
                              &assembler().input_graph());
    Graph::OpIndexIterator end(op.didnt_throw_block->end(),
                               &assembler().input_graph());
    // To translate `CheckException` to the new graph, we reduce the throwing
    // operation (actually it's `DidntThrow` operation, but that triggers the
    // actual reduction) with a catch scope. If the reduction replaces the
    // throwing operation with other throwing operations, all of them will be
    // connected to the provided catch block. The reduction should automatically
    // bind a block that represents non-throwing control flow of the original
    // operation, so we can inline the rest of the `didnt_throw` block.
    {
      typename Assembler::CatchScope scope(assembler(),
                                           op.catch_block->MapToNextGraph());
      DCHECK(input_graph().Get(*it).template Is<DidntThrowOp>());
      if (!assembler().InlineOp(*it, op.didnt_throw_block)) {
        return OpIndex::Invalid();
      }
      ++it;
    }
    for (; it != end; ++it) {
      // Using `InlineOp` requires that the inlined operation is not emitted
      // multiple times. This is the case here because we just removed the
      // single predecessor of `didnt_throw_block`.
      if (!assembler().InlineOp(*it, op.didnt_throw_block)) {
        break;
      }
    }
    return OpIndex::Invalid();
  }
  OpIndex AssembleOutputGraphCatchBlockBegin(const CatchBlockBeginOp& op) {
    return assembler().ReduceCatchBlockBegin();
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
  OpIndex AssembleOutputGraphBitcastWord32PairToFloat64(
      const BitcastWord32PairToFloat64Op& op) {
    return assembler().ReduceBitcastWord32PairToFloat64(
        MapToNewGraph(op.high_word32()), MapToNewGraph(op.low_word32()));
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
  OpIndex AssembleOutputGraphObjectIsNumericValue(
      const ObjectIsNumericValueOp& op) {
    return assembler().ReduceObjectIsNumericValue(MapToNewGraph(op.input()),
                                                  op.kind, op.input_rep);
  }
  OpIndex AssembleOutputGraphConvert(const ConvertOp& op) {
    return assembler().ReduceConvert(MapToNewGraph(op.input()), op.from, op.to);
  }
  OpIndex AssembleOutputGraphConvertUntaggedToJSPrimitive(
      const ConvertUntaggedToJSPrimitiveOp& op) {
    return assembler().ReduceConvertUntaggedToJSPrimitive(
        MapToNewGraph(op.input()), op.kind, op.input_rep,
        op.input_interpretation, op.minus_zero_mode);
  }
  OpIndex AssembleOutputGraphConvertUntaggedToJSPrimitiveOrDeopt(
      const ConvertUntaggedToJSPrimitiveOrDeoptOp& op) {
    return assembler().ReduceConvertUntaggedToJSPrimitiveOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()), op.kind,
        op.input_rep, op.input_interpretation, op.feedback);
  }
  OpIndex AssembleOutputGraphConvertJSPrimitiveToUntagged(
      const ConvertJSPrimitiveToUntaggedOp& op) {
    return assembler().ReduceConvertJSPrimitiveToUntagged(
        MapToNewGraph(op.input()), op.kind, op.input_assumptions);
  }
  OpIndex AssembleOutputGraphConvertJSPrimitiveToUntaggedOrDeopt(
      const ConvertJSPrimitiveToUntaggedOrDeoptOp& op) {
    return assembler().ReduceConvertJSPrimitiveToUntaggedOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()),
        op.from_kind, op.to_kind, op.minus_zero_mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTruncateJSPrimitiveToUntagged(
      const TruncateJSPrimitiveToUntaggedOp& op) {
    return assembler().ReduceTruncateJSPrimitiveToUntagged(
        MapToNewGraph(op.input()), op.kind, op.input_assumptions);
  }
  OpIndex AssembleOutputGraphTruncateJSPrimitiveToUntaggedOrDeopt(
      const TruncateJSPrimitiveToUntaggedOrDeoptOp& op) {
    return assembler().ReduceTruncateJSPrimitiveToUntaggedOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()), op.kind,
        op.input_requirement, op.feedback);
  }
  OpIndex AssembleOutputGraphConvertJSPrimitiveToObject(
      const ConvertJSPrimitiveToObjectOp& op) {
    return assembler().ReduceConvertJSPrimitiveToObject(
        MapToNewGraph(op.value()), MapToNewGraph(op.global_proxy()), op.mode);
  }
  OpIndex AssembleOutputGraphSelect(const SelectOp& op) {
    return assembler().ReduceSelect(
        MapToNewGraph(op.cond()), MapToNewGraph(op.vtrue()),
        MapToNewGraph(op.vfalse()), op.rep, op.hint, op.implem);
  }
  OpIndex AssembleOutputGraphConstant(const ConstantOp& op) {
    return assembler().ReduceConstant(op.kind, op.storage);
  }
  OpIndex AssembleOutputGraphAtomicRMW(const AtomicRMWOp& op) {
    return assembler().ReduceAtomicRMW(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), MapToNewGraphIfValid(op.expected()),
        op.bin_op, op.result_rep, op.input_rep, op.memory_access_kind);
  }

  OpIndex AssembleOutputGraphAtomicWord32Pair(const AtomicWord32PairOp& op) {
    return assembler().ReduceAtomicWord32Pair(
        MapToNewGraph(op.base()), MapToNewGraphIfValid(op.index()),
        MapToNewGraphIfValid(op.value_low()),
        MapToNewGraphIfValid(op.value_high()),
        MapToNewGraphIfValid(op.expected_low()),
        MapToNewGraphIfValid(op.expected_high()), op.op_kind, op.offset);
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
        op.offset, op.element_size_log2,
        op.maybe_initializing_or_transitioning);
  }
  OpIndex AssembleOutputGraphAllocate(const AllocateOp& op) {
    return assembler().FinishInitialization(
        assembler().Allocate(MapToNewGraph(op.size()), op.type));
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
    return assembler().ReduceTrapIf(MapToNewGraph(op.condition()),
                                    MapToNewGraphIfValid(op.frame_state()),
                                    op.negated, op.trap_id);
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
    return assembler().ReduceStaticAssert(MapToNewGraph(op.condition()),
                                          op.source);
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
  OpIndex AssembleOutputGraphDebugPrint(const DebugPrintOp& op) {
    return assembler().ReduceDebugPrint(MapToNewGraph(op.input()), op.rep);
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
  OpIndex AssembleOutputGraphStringConcat(const StringConcatOp& op) {
    return assembler().ReduceStringConcat(MapToNewGraph(op.left()),
                                          MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphStringEqual(const StringEqualOp& op) {
    return assembler().ReduceStringEqual(MapToNewGraph(op.left()),
                                         MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphStringComparison(const StringComparisonOp& op) {
    return assembler().ReduceStringComparison(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind);
  }
  OpIndex AssembleOutputGraphArgumentsLength(const ArgumentsLengthOp& op) {
    return assembler().ReduceArgumentsLength(op.kind,
                                             op.formal_parameter_count);
  }
  OpIndex AssembleOutputGraphNewArgumentsElements(
      const NewArgumentsElementsOp& op) {
    return assembler().ReduceNewArgumentsElements(
        MapToNewGraph(op.arguments_count()), op.type,
        op.formal_parameter_count);
  }
  OpIndex AssembleOutputGraphLoadTypedElement(const LoadTypedElementOp& op) {
    return assembler().ReduceLoadTypedElement(
        MapToNewGraph(op.buffer()), MapToNewGraph(op.base()),
        MapToNewGraph(op.external()), MapToNewGraph(op.index()), op.array_type);
  }
  OpIndex AssembleOutputGraphLoadDataViewElement(
      const LoadDataViewElementOp& op) {
    return assembler().ReduceLoadDataViewElement(
        MapToNewGraph(op.object()), MapToNewGraph(op.storage()),
        MapToNewGraph(op.index()), MapToNewGraph(op.is_little_endian()),
        op.element_type);
  }
  OpIndex AssembleOutputGraphLoadStackArgument(const LoadStackArgumentOp& op) {
    return assembler().ReduceLoadStackArgument(MapToNewGraph(op.base()),
                                               MapToNewGraph(op.index()));
  }
  OpIndex AssembleOutputGraphStoreTypedElement(const StoreTypedElementOp& op) {
    return assembler().ReduceStoreTypedElement(
        MapToNewGraph(op.buffer()), MapToNewGraph(op.base()),
        MapToNewGraph(op.external()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.array_type);
  }
  OpIndex AssembleOutputGraphStoreDataViewElement(
      const StoreDataViewElementOp& op) {
    return assembler().ReduceStoreDataViewElement(
        MapToNewGraph(op.object()), MapToNewGraph(op.storage()),
        MapToNewGraph(op.index()), MapToNewGraph(op.value()),
        MapToNewGraph(op.is_little_endian()), op.element_type);
  }
  OpIndex AssembleOutputGraphTransitionAndStoreArrayElement(
      const TransitionAndStoreArrayElementOp& op) {
    return assembler().ReduceTransitionAndStoreArrayElement(
        MapToNewGraph(op.array()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.kind, op.fast_map, op.double_map);
  }
  OpIndex AssembleOutputGraphCompareMaps(const CompareMapsOp& op) {
    return assembler().ReduceCompareMaps(MapToNewGraph(op.heap_object()),
                                         op.maps);
  }
  OpIndex AssembleOutputGraphCheckMaps(const CheckMapsOp& op) {
    return assembler().ReduceCheckMaps(MapToNewGraph(op.heap_object()),
                                       MapToNewGraph(op.frame_state()), op.maps,
                                       op.flags, op.feedback);
  }
  OpIndex AssembleOutputGraphAssumeMap(const AssumeMapOp& op) {
    return assembler().ReduceAssumeMap(MapToNewGraph(op.heap_object()),
                                       op.maps);
  }
  OpIndex AssembleOutputGraphCheckedClosure(const CheckedClosureOp& op) {
    return assembler().ReduceCheckedClosure(MapToNewGraph(op.input()),
                                            MapToNewGraph(op.frame_state()),
                                            op.feedback_cell);
  }
  OpIndex AssembleOutputGraphCheckEqualsInternalizedString(
      const CheckEqualsInternalizedStringOp& op) {
    return assembler().ReduceCheckEqualsInternalizedString(
        MapToNewGraph(op.expected()), MapToNewGraph(op.value()),
        MapToNewGraph(op.frame_state()));
  }
  OpIndex AssembleOutputGraphLoadMessage(const LoadMessageOp& op) {
    return assembler().ReduceLoadMessage(MapToNewGraph(op.offset()));
  }
  OpIndex AssembleOutputGraphStoreMessage(const StoreMessageOp& op) {
    return assembler().ReduceStoreMessage(MapToNewGraph(op.offset()),
                                          MapToNewGraph(op.object()));
  }
  OpIndex AssembleOutputGraphSameValue(const SameValueOp& op) {
    return assembler().ReduceSameValue(MapToNewGraph(op.left()),
                                       MapToNewGraph(op.right()), op.mode);
  }
  OpIndex AssembleOutputGraphFloat64SameValue(const Float64SameValueOp& op) {
    return assembler().ReduceFloat64SameValue(MapToNewGraph(op.left()),
                                              MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphFastApiCall(const FastApiCallOp& op) {
    auto arguments = MapToNewGraph<8>(op.arguments());
    return assembler().ReduceFastApiCall(MapToNewGraph(op.data_argument()),
                                         base::VectorOf(arguments),
                                         op.parameters);
  }
  OpIndex AssembleOutputGraphRuntimeAbort(const RuntimeAbortOp& op) {
    return assembler().ReduceRuntimeAbort(op.reason);
  }
  OpIndex AssembleOutputGraphEnsureWritableFastElements(
      const EnsureWritableFastElementsOp& op) {
    return assembler().ReduceEnsureWritableFastElements(
        MapToNewGraph(op.object()), MapToNewGraph(op.elements()));
  }
  OpIndex AssembleOutputGraphMaybeGrowFastElements(
      const MaybeGrowFastElementsOp& op) {
    return assembler().ReduceMaybeGrowFastElements(
        MapToNewGraph(op.object()), MapToNewGraph(op.elements()),
        MapToNewGraph(op.index()), MapToNewGraph(op.elements_length()),
        MapToNewGraph(op.frame_state()), op.mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTransitionElementsKind(
      const TransitionElementsKindOp& op) {
    return assembler().ReduceTransitionElementsKind(MapToNewGraph(op.object()),
                                                    op.transition);
  }
  OpIndex AssembleOutputGraphFindOrderedHashEntry(
      const FindOrderedHashEntryOp& op) {
    return assembler().ReduceFindOrderedHashEntry(
        MapToNewGraph(op.data_structure()), MapToNewGraph(op.key()), op.kind);
  }
  OpIndex AssembleOutputGraphWord32PairBinop(const Word32PairBinopOp& op) {
    return assembler().ReduceWord32PairBinop(op.left_low(), op.left_high(),
                                             op.right_low(), op.right_high(),
                                             op.kind);
  }

#ifdef V8_ENABLE_WEBASSEMBLY
  OpIndex AssembleOutputGraphGlobalGet(const GlobalGetOp& op) {
    return assembler().ReduceGlobalGet(MapToNewGraph(op.instance()), op.global);
  }

  OpIndex AssembleOutputGraphGlobalSet(const GlobalSetOp& op) {
    return assembler().ReduceGlobalSet(MapToNewGraph(op.instance()),
                                       MapToNewGraph(op.value()), op.global);
  }

  OpIndex AssembleOutputGraphNull(const NullOp& op) {
    return assembler().ReduceNull(op.type);
  }

  OpIndex AssembleOutputGraphIsNull(const IsNullOp& op) {
    return assembler().ReduceIsNull(MapToNewGraph(op.object()), op.type);
  }

  OpIndex AssembleOutputGraphAssertNotNull(const AssertNotNullOp& op) {
    return assembler().ReduceAssertNotNull(MapToNewGraph(op.object()), op.type,
                                           op.trap_id);
  }

  OpIndex AssembleOutputGraphRttCanon(const RttCanonOp& op) {
    return assembler().ReduceRttCanon(MapToNewGraph(op.instance()),
                                      op.type_index);
  }

  OpIndex AssembleOutputGraphWasmTypeCheck(const WasmTypeCheckOp& op) {
    return assembler().ReduceWasmTypeCheck(
        MapToNewGraph(op.object()), MapToNewGraphIfValid(op.rtt()), op.config);
  }

  OpIndex AssembleOutputGraphWasmTypeCast(const WasmTypeCastOp& op) {
    return assembler().ReduceWasmTypeCast(
        MapToNewGraph(op.object()), MapToNewGraphIfValid(op.rtt()), op.config);
  }

  OpIndex AssembleOutputGraphStructGet(const StructGetOp& op) {
    return assembler().ReduceStructGet(MapToNewGraph(op.object()), op.type,
                                       op.field_index, op.is_signed,
                                       op.null_check);
  }

  OpIndex AssembleOutputGraphStructSet(const StructSetOp& op) {
    return assembler().ReduceStructSet(MapToNewGraph(op.object()),
                                       MapToNewGraph(op.value()), op.type,
                                       op.field_index, op.null_check);
  }

  OpIndex AssembleOutputGraphArrayGet(const ArrayGetOp& op) {
    return assembler().ReduceArrayGet(MapToNewGraph(op.array()),
                                      MapToNewGraph(op.index()),
                                      op.element_type, op.is_signed);
  }

  OpIndex AssembleOutputGraphArraySet(const ArraySetOp& op) {
    return assembler().ReduceArraySet(
        MapToNewGraph(op.array()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.element_type);
  }

  OpIndex AssembleOutputGraphArrayLength(const ArrayLengthOp& op) {
    return assembler().ReduceArrayLength(MapToNewGraph(op.array()),
                                         op.null_check);
  }

  OpIndex AssembleOutputGraphSimd128Constant(const Simd128ConstantOp& op) {
    return assembler().ReduceSimd128Constant(op.value);
  }

  OpIndex AssembleOutputGraphSimd128Binop(const Simd128BinopOp& op) {
    return assembler().ReduceSimd128Binop(MapToNewGraph(op.left()),
                                          MapToNewGraph(op.right()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Unary(const Simd128UnaryOp& op) {
    return assembler().ReduceSimd128Unary(MapToNewGraph(op.input()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Shift(const Simd128ShiftOp& op) {
    return assembler().ReduceSimd128Shift(MapToNewGraph(op.input()),
                                          MapToNewGraph(op.shift()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Test(const Simd128TestOp& op) {
    return assembler().ReduceSimd128Test(MapToNewGraph(op.input()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Splat(const Simd128SplatOp& op) {
    return assembler().ReduceSimd128Splat(MapToNewGraph(op.input()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Ternary(const Simd128TernaryOp& op) {
    return assembler().ReduceSimd128Ternary(MapToNewGraph(op.first()),
                                            MapToNewGraph(op.second()),
                                            MapToNewGraph(op.third()), op.kind);
  }
  OpIndex AssembleOutputGraphSimd128ExtractLane(
      const Simd128ExtractLaneOp& op) {
    return assembler().ReduceSimd128ExtractLane(MapToNewGraph(op.input()),
                                                op.kind, op.lane);
  }
  OpIndex AssembleOutputGraphSimd128ReplaceLane(
      const Simd128ReplaceLaneOp& op) {
    return assembler().ReduceSimd128ReplaceLane(MapToNewGraph(op.into()),
                                                MapToNewGraph(op.new_lane()),
                                                op.kind, op.lane);
  }
  OpIndex AssembleOutputGraphSimd128LaneMemory(const Simd128LaneMemoryOp& op) {
    return assembler().ReduceSimd128LaneMemory(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.mode, op.kind, op.lane_kind, op.lane,
        op.offset);
  }
  OpIndex AssembleOutputGraphSimd128LoadTransform(
      const Simd128LoadTransformOp& op) {
    return assembler().ReduceSimd128LoadTransform(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()), op.load_kind,
        op.transform_kind, op.offset);
  }
  OpIndex AssembleOutputGraphSimd128Shuffle(const Simd128ShuffleOp& op) {
    return assembler().ReduceSimd128Shuffle(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.shuffle);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  void CreateOldToNewMapping(OpIndex old_index, OpIndex new_index) {
    if constexpr (reducer_list_contains<typename Assembler::ReducerList,
                                        VariableReducer>::value) {
      if (current_block_needs_variables_) {
        MaybeVariable var = GetVariableFor(old_index);
        if (!var.has_value()) {
          MaybeRegisterRepresentation rep =
              input_graph().Get(old_index).outputs_rep().size() == 1
                  ? static_cast<const MaybeRegisterRepresentation&>(
                        input_graph().Get(old_index).outputs_rep()[0])
                  : MaybeRegisterRepresentation::None();
          var = assembler().NewLoopInvariantVariable(rep);
          SetVariableFor(old_index, *var);
        }
        assembler().SetVariable(*var, new_index);
        return;
      }
    } else {
      DCHECK(!current_block_needs_variables_);
    }
    DCHECK(!op_mapping_[old_index.id()].valid());
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

  void FixLoopPhis(Block* input_graph_loop) {
    DCHECK(input_graph_loop->IsLoop());
    Block* output_graph_loop = input_graph_loop->MapToNextGraph();
    DCHECK(output_graph_loop->IsLoop());
    for (const Operation& op : assembler().input_graph().operations(
             input_graph_loop->begin(), input_graph_loop->end())) {
      if (auto* input_phi = op.TryCast<PhiOp>()) {
        OpIndex phi_index =
            MapToNewGraph<true>(assembler().input_graph().Index(*input_phi));
        if (!phi_index.valid() || !output_graph_loop->Contains(phi_index)) {
          // Unused phis are skipped, so they are not be mapped to anything in
          // the new graph. If the phi is reduced to an operation from a
          // different block, then there is no loop phi in the current loop
          // header to take care of.
          continue;
        }
        assembler().FixLoopPhi(*input_phi, phi_index, output_graph_loop);
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
