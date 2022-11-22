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
#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

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

// TODO(dmercadier, tebbi): transform this analyzer into a reducer, and plug in
// into some reducer stacks.
struct LivenessAnalyzer : AnalyzerBase {
  using Base = AnalyzerBase;
  // Using `uint8_t` instead of `bool` prevents `std::vector` from using a
  // bitvector, which has worse performance.
  std::vector<uint8_t> op_used;

  LivenessAnalyzer(const Graph& graph, Zone* phase_zone)
      : AnalyzerBase(graph, phase_zone), op_used(graph.op_id_count(), false) {}

  bool OpIsUsed(OpIndex i) { return op_used[i.id()]; }

  void Run() {
    for (uint32_t unprocessed_count = graph.block_count();
         unprocessed_count > 0;) {
      BlockIndex block_index = static_cast<BlockIndex>(unprocessed_count - 1);
      --unprocessed_count;
      const Block& block = graph.Get(block_index);
      if (V8_UNLIKELY(block.IsLoop())) {
        ProcessBlock<true>(block, &unprocessed_count);
      } else {
        ProcessBlock<false>(block, &unprocessed_count);
      }
    }
  }

  template <bool is_loop>
  void ProcessBlock(const Block& block, uint32_t* unprocessed_count) {
    auto op_range = graph.OperationIndices(block);
    for (auto it = op_range.end(); it != op_range.begin();) {
      --it;
      OpIndex index = *it;
      const Operation& op = graph.Get(index);
      if (op.Properties().is_required_when_unused) {
        op_used[index.id()] = true;
      } else if (!OpIsUsed(index)) {
        continue;
      }
      if constexpr (is_loop) {
        if (op.Is<PhiOp>()) {
          const PhiOp& phi = op.Cast<PhiOp>();
          // Mark the loop backedge as used. Trigger a revisit if it wasn't
          // marked as used already.
          if (!OpIsUsed(phi.inputs()[PhiOp::kLoopPhiBackEdgeIndex])) {
            Block* backedge = block.LastPredecessor();
            // Revisit the loop by increasing the `unprocessed_count` to include
            // all blocks of the loop.
            *unprocessed_count =
                std::max(*unprocessed_count, backedge->index().id() + 1);
          }
        }
      }
      for (OpIndex input : op.inputs()) {
        op_used[input.id()] = true;
      }
    }
  }
};

template <template <class> class... Reducers>
class OptimizationPhase {
 public:
  static void Run(Graph* input, Zone* phase_zone, NodeOriginTable* origins) {
    Assembler<Reducers...> phase(*input, input->GetOrCreateCompanion(),
                                 phase_zone, origins);
    if (v8_flags.turboshaft_trace_reduction) {
      phase.template VisitGraph<true>();
    } else {
      phase.template VisitGraph<false>();
    }
  }
  static void RunWithoutTracing(Graph* input, Zone* phase_zone) {
    Assembler<Reducers...> phase(input, input->GetOrCreateCompanion(),
                                 phase_zone);
    phase->template VisitGraph<false>();
  }
};

template <class Assembler>
class GraphVisitor {
 public:
  GraphVisitor(Graph& input_graph, Graph& output_graph, Zone* phase_zone,
               compiler::NodeOriginTable* origins = nullptr)
      : input_graph_(input_graph),
        output_graph_(output_graph),
        phase_zone_(phase_zone),
        origins_(origins),
        current_input_block_(nullptr),
        block_mapping_(input_graph.block_count(), nullptr, phase_zone),
        op_mapping_(input_graph.op_id_count(), OpIndex::Invalid(), phase_zone) {
    output_graph_.Reset();
  }

  // `trace_reduction` is a template parameter to avoid paying for tracing at
  // runtime.
  template <bool trace_reduction>
  void VisitGraph() {
    // Creating initial old-to-new Block mapping.
    for (const Block& input_block : input_graph().blocks()) {
      block_mapping_[input_block.index().id()] =
          assembler().NewBlock(input_block.kind());
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
    if constexpr (trace_reduction) {
      std::cout << PrintAsBlockHeader{*input_block} << "\n";
    }
    if (!assembler().Bind(MapToNewGraph(input_block->index()))) {
      if constexpr (trace_reduction) TraceBlockUnreachable();
      // If we eliminate a loop backedge, we need to turn the loop into a
      // single-predecessor merge block.
      const Operation& last_op =
          *base::Reversed(input_graph().operations(*input_block)).begin();
      if (auto* final_goto = last_op.TryCast<GotoOp>()) {
        if (final_goto->destination->IsLoop()) {
          Block* new_loop = MapToNewGraph(final_goto->destination->index());
          DCHECK(new_loop->IsLoop());
          if (new_loop->IsLoop() && new_loop->PredecessorCount() == 1) {
            output_graph_.TurnLoopIntoMerge(new_loop);
          }
        }
      }
      return;
    }
    assembler().current_block()->SetDeferred(input_block->IsDeferred());
    for (OpIndex index : input_graph().OperationIndices(*input_block)) {
      if (!VisitOp<trace_reduction>(index, input_block)) break;
    }
    if constexpr (trace_reduction) TraceBlockFinished();
  }

  template <bool trace_reduction>
  bool VisitOp(OpIndex index, const Block* input_block) {
    if (!assembler().current_block()) return false;
    assembler().SetCurrentOrigin(index);
    OpIndex first_output_index =
        assembler().output_graph().next_operation_index();
    USE(first_output_index);
    const Operation& op = input_graph().Get(index);
    if (op.saturated_use_count == 0 &&
        !op.Properties().is_required_when_unused) {
      if constexpr (trace_reduction) TraceOperationUnused();
      return true;
    }
    if constexpr (trace_reduction) TraceReductionStart(index);
    OpIndex new_index;
    if (input_block->IsLoop() && op.Is<PhiOp>()) {
      const PhiOp& phi = op.Cast<PhiOp>();
      new_index = assembler().PendingLoopPhi(MapToNewGraph(phi.inputs()[0]),
                                             phi.rep, phi.inputs()[1]);
      if constexpr (trace_reduction) {
        TraceReductionResult(first_output_index, new_index);
      }
    } else {
      switch (op.opcode) {
#define EMIT_INSTR_CASE(Name)                           \
  case Opcode::k##Name:                                 \
    new_index = this->Visit##Name(op.Cast<Name##Op>()); \
    break;
        TURBOSHAFT_OPERATION_LIST(EMIT_INSTR_CASE)
#undef EMIT_INSTR_CASE
      }
      if constexpr (trace_reduction) {
        TraceReductionResult(first_output_index, new_index);
      }
    }
    op_mapping_[index.id()] = new_index;
    return true;
  }

  void TraceReductionStart(OpIndex index) {
    std::cout << "╭── o" << index.id() << ": "
              << PaddingSpace{5 - CountDecimalDigits(index.id())}
              << OperationPrintStyle{input_graph().Get(index), "#o"} << "\n";
  }
  void TraceOperationUnused() { std::cout << "╰─> unused\n\n"; }
  void TraceBlockUnreachable() { std::cout << "╰─> unreachable\n\n"; }
  void TraceReductionResult(OpIndex first_output_index, OpIndex new_index) {
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
    }
    std::cout << "\n";
  }
  void TraceBlockFinished() { std::cout << "\n"; }

  // These functions take an operation from the old graph and use the assembler
  // to emit a corresponding operation in the new graph, translating inputs and
  // blocks accordingly.

  V8_INLINE OpIndex VisitGoto(const GotoOp& op) {
    Block* destination = MapToNewGraph(op.destination->index());
    assembler().current_block()->SetOrigin(current_input_block_);
    assembler().ReduceGoto(destination);
    if (destination->IsBound()) {
      DCHECK(destination->IsLoop());
      FixLoopPhis(destination);
    }
    return OpIndex::Invalid();
  }
  V8_INLINE OpIndex VisitBranch(const BranchOp& op) {
    Block* if_true = MapToNewGraph(op.if_true->index());
    Block* if_false = MapToNewGraph(op.if_false->index());
    return assembler().ReduceBranch(MapToNewGraph(op.condition()), if_true,
                                    if_false);
  }
  OpIndex VisitCatchException(const CatchExceptionOp& op) {
    Block* if_success = MapToNewGraph(op.if_success->index());
    Block* if_exception = MapToNewGraph(op.if_exception->index());
    return assembler().ReduceCatchException(MapToNewGraph(op.call()),
                                            if_success, if_exception);
  }
  OpIndex VisitSwitch(const SwitchOp& op) {
    base::SmallVector<SwitchOp::Case, 16> cases;
    for (SwitchOp::Case c : op.cases) {
      cases.emplace_back(c.value, MapToNewGraph(c.destination->index()));
    }
    return assembler().ReduceSwitch(
        MapToNewGraph(op.input()),
        graph_zone()->CloneVector(base::VectorOf(cases)),
        MapToNewGraph(op.default_case->index()));
  }
  OpIndex VisitPhi(const PhiOp& op) {
    base::Vector<const OpIndex> old_inputs = op.inputs();
    base::SmallVector<OpIndex, 8> new_inputs;
    Block* old_pred = current_input_block_->LastPredecessor();
    Block* new_pred = assembler().current_block()->LastPredecessor();
    // Control predecessors might be missing after the optimization phase. So we
    // need to skip phi inputs that belong to control predecessors that have no
    // equivalent in the new graph.

    // We first assume that the order if the predecessors of the current block
    // did not change. If it did, {new_pred} won't be nullptr at the end of this
    // loop, and we'll instead fall back to the slower code below to compute the
    // inputs of the Phi.
    for (OpIndex input : base::Reversed(old_inputs)) {
      if (new_pred && new_pred->Origin() == old_pred) {
        new_inputs.push_back(MapToNewGraph(input));
        new_pred = new_pred->NeighboringPredecessor();
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

      base::SmallVector<uint32_t, 16> old_pred_vec;
      for (old_pred = current_input_block_->LastPredecessor();
           old_pred != nullptr; old_pred = old_pred->NeighboringPredecessor()) {
        old_pred_vec.push_back(old_pred->index().id());
        // Checking that predecessors are indeed sorted.
        DCHECK_IMPLIES(old_pred->NeighboringPredecessor() != nullptr,
                       old_pred->index().id() >
                           old_pred->NeighboringPredecessor()->index().id());
      }
      std::reverse(old_pred_vec.begin(), old_pred_vec.end());

      // Filling {new_inputs}: we iterate the new predecessors, and, for each
      // predecessor, we check the index of the input corresponding to the old
      // predecessor, and we put it next in {new_inputs}.
      new_inputs.clear();
      for (new_pred = assembler().current_block()->LastPredecessor();
           new_pred != nullptr; new_pred = new_pred->NeighboringPredecessor()) {
        const Block* origin = new_pred->Origin();
        DCHECK_NOT_NULL(origin);
        // {old_pred_vec} is sorted. We can thus use a binary search to find the
        // index of {origin} in {old_pred_vec}: the index is the index of the
        // old input corresponding to {new_pred}.
        auto lower = std::lower_bound(old_pred_vec.begin(), old_pred_vec.end(),
                                      origin->index().id());
        DCHECK_NE(lower, old_pred_vec.end());
        new_inputs.push_back(
            MapToNewGraph(old_inputs[lower - old_pred_vec.begin()]));
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
  OpIndex VisitPendingLoopPhi(const PendingLoopPhiOp& op) { UNREACHABLE(); }
  V8_INLINE OpIndex VisitFrameState(const FrameStateOp& op) {
    auto inputs = MapToNewGraph<32>(op.inputs());
    return assembler().ReduceFrameState(base::VectorOf(inputs), op.inlined,
                                        op.data);
  }
  OpIndex VisitCall(const CallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return assembler().ReduceCall(callee, base::VectorOf(arguments),
                                  op.descriptor);
  }
  OpIndex VisitTailCall(const TailCallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return assembler().ReduceTailCall(callee, base::VectorOf(arguments),
                                      op.descriptor);
  }
  OpIndex VisitReturn(const ReturnOp& op) {
    // We very rarely have tuples longer than 4.
    auto return_values = MapToNewGraph<4>(op.return_values());
    return assembler().ReduceReturn(MapToNewGraph(op.pop_count()),
                                    base::VectorOf(return_values));
  }
  OpIndex VisitOverflowCheckedBinop(const OverflowCheckedBinopOp& op) {
    return assembler().ReduceOverflowCheckedBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex VisitWordUnary(const WordUnaryOp& op) {
    return assembler().ReduceWordUnary(MapToNewGraph(op.input()), op.kind,
                                       op.rep);
  }
  OpIndex VisitFloatUnary(const FloatUnaryOp& op) {
    return assembler().ReduceFloatUnary(MapToNewGraph(op.input()), op.kind,
                                        op.rep);
  }
  OpIndex VisitShift(const ShiftOp& op) {
    return assembler().ReduceShift(MapToNewGraph(op.left()),
                                   MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex VisitEqual(const EqualOp& op) {
    return assembler().ReduceEqual(MapToNewGraph(op.left()),
                                   MapToNewGraph(op.right()), op.rep);
  }
  OpIndex VisitComparison(const ComparisonOp& op) {
    return assembler().ReduceComparison(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex VisitChange(const ChangeOp& op) {
    return assembler().ReduceChange(MapToNewGraph(op.input()), op.kind,
                                    op.assumption, op.from, op.to);
  }
  OpIndex VisitTryChange(const TryChangeOp& op) {
    return assembler().ReduceTryChange(MapToNewGraph(op.input()), op.kind,
                                       op.from, op.to);
  }

  OpIndex VisitFloat64InsertWord32(const Float64InsertWord32Op& op) {
    return assembler().ReduceFloat64InsertWord32(
        MapToNewGraph(op.float64()), MapToNewGraph(op.word32()), op.kind);
  }
  OpIndex VisitTaggedBitcast(const TaggedBitcastOp& op) {
    return assembler().ReduceTaggedBitcast(MapToNewGraph(op.input()), op.from,
                                           op.to);
  }
  OpIndex VisitSelect(const SelectOp& op) {
    return assembler().ReduceSelect(
        MapToNewGraph(op.cond()), MapToNewGraph(op.vtrue()),
        MapToNewGraph(op.vfalse()), op.rep, op.hint, op.implem);
  }
  OpIndex VisitConstant(const ConstantOp& op) {
    return assembler().ReduceConstant(op.kind, op.storage);
  }
  OpIndex VisitLoad(const LoadOp& op) {
    return assembler().ReduceLoad(
        MapToNewGraph(op.base()),
        op.index().valid() ? MapToNewGraph(op.index()) : OpIndex::Invalid(),
        op.kind, op.loaded_rep, op.result_rep, op.offset, op.element_size_log2);
  }
  OpIndex VisitStore(const StoreOp& op) {
    return assembler().ReduceStore(
        MapToNewGraph(op.base()),
        op.index().valid() ? MapToNewGraph(op.index()) : OpIndex::Invalid(),
        MapToNewGraph(op.value()), op.kind, op.stored_rep, op.write_barrier,
        op.offset, op.element_size_log2);
  }
  OpIndex VisitRetain(const RetainOp& op) {
    return assembler().ReduceRetain(MapToNewGraph(op.retained()));
  }
  OpIndex VisitParameter(const ParameterOp& op) {
    return assembler().ReduceParameter(op.parameter_index, op.debug_name);
  }
  OpIndex VisitOsrValue(const OsrValueOp& op) {
    return assembler().ReduceOsrValue(op.index);
  }
  OpIndex VisitStackPointerGreaterThan(const StackPointerGreaterThanOp& op) {
    return assembler().ReduceStackPointerGreaterThan(
        MapToNewGraph(op.stack_limit()), op.kind);
  }
  OpIndex VisitStackSlot(const StackSlotOp& op) {
    return assembler().ReduceStackSlot(op.size, op.alignment);
  }
  OpIndex VisitFrameConstant(const FrameConstantOp& op) {
    return assembler().ReduceFrameConstant(op.kind);
  }
  OpIndex VisitCheckLazyDeopt(const CheckLazyDeoptOp& op) {
    return assembler().ReduceCheckLazyDeopt(MapToNewGraph(op.call()),
                                            MapToNewGraph(op.frame_state()));
  }
  OpIndex VisitDeoptimize(const DeoptimizeOp& op) {
    return assembler().ReduceDeoptimize(MapToNewGraph(op.frame_state()),
                                        op.parameters);
  }
  OpIndex VisitDeoptimizeIf(const DeoptimizeIfOp& op) {
    return assembler().ReduceDeoptimizeIf(MapToNewGraph(op.condition()),
                                          MapToNewGraph(op.frame_state()),
                                          op.negated, op.parameters);
  }
  OpIndex VisitTrapIf(const TrapIfOp& op) {
    return assembler().ReduceTrapIf(MapToNewGraph(op.condition()), op.negated,
                                    op.trap_id);
  }
  OpIndex VisitTuple(const TupleOp& op) {
    return assembler().ReduceTuple(
        base::VectorOf(MapToNewGraph<4>(op.inputs())));
  }
  OpIndex VisitProjection(const ProjectionOp& op) {
    return assembler().ReduceProjection(MapToNewGraph(op.input()), op.index);
  }
  OpIndex VisitWordBinop(const WordBinopOp& op) {
    return assembler().ReduceWordBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex VisitFloatBinop(const FloatBinopOp& op) {
    return assembler().ReduceFloatBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex VisitUnreachable(const UnreachableOp& op) {
    return assembler().ReduceUnreachable();
  }

  Block* MapToNewGraph(BlockIndex old_index) const {
    Block* result = block_mapping_[old_index.id()];
    DCHECK_NOT_NULL(result);
    return result;
  }

  OpIndex MapToNewGraph(OpIndex old_index) {
    OpIndex result = op_mapping_[old_index.id()];
    DCHECK(result.valid());
    return result;
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
            base::VectorOf({pending_phi->first(),
                            MapToNewGraph(pending_phi->old_backedge_index)}),
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
  ZoneVector<Block*> block_mapping_;
  ZoneVector<OpIndex> op_mapping_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_OPTIMIZATION_PHASE_H_
