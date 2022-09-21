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
           op.properties().is_required_when_unused;
  }

  explicit AnalyzerBase(const Graph& graph, Zone* phase_zone)
      : phase_zone(phase_zone), graph(graph) {}
};

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
      if (op.properties().is_required_when_unused) {
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

enum class VisitOrder { kAsEmitted, kDominator };

template <class Analyzer, class Assembler>
class OptimizationPhase {
 private:
  struct Impl;

 public:
  static void Run(Graph* input, Zone* phase_zone, NodeOriginTable* origins,
                  VisitOrder visit_order = VisitOrder::kAsEmitted) {
    Impl phase{*input, phase_zone, origins, visit_order};
    if (FLAG_turboshaft_trace_reduction) {
      phase.template Run<true>();
    } else {
      phase.template Run<false>();
    }
  }
  static void RunWithoutTracing(
      Graph* input, Zone* phase_zone,
      VisitOrder visit_order = VisitOrder::kAsEmitted) {
    Impl phase{*input, phase_zone, visit_order};
    phase.template Run<false>();
  }
};

template <class Analyzer, class Assembler>
struct OptimizationPhase<Analyzer, Assembler>::Impl {
  Graph& input_graph;
  Zone* phase_zone;
  compiler::NodeOriginTable* origins;
  VisitOrder visit_order;

  Analyzer analyzer{input_graph, phase_zone};
  Assembler assembler{&input_graph.GetOrCreateCompanion(), phase_zone};
  const Block* current_input_block = nullptr;
  // Mappings from the old graph to the new graph.
  std::vector<Block*> block_mapping{input_graph.block_count(), nullptr};
  std::vector<OpIndex> op_mapping{input_graph.op_id_count(),
                                  OpIndex::Invalid()};

  // `trace_reduction` is a template parameter to avoid paying for tracing at
  // runtime.
  template <bool trace_reduction>
  void Run() {
    analyzer.Run();

    for (const Block& input_block : input_graph.blocks()) {
      block_mapping[input_block.index().id()] =
          assembler.NewBlock(input_block.kind());
    }

    if (visit_order == VisitOrder::kDominator) {
      RunDominatorOrder<trace_reduction>();
    } else {
      RunAsEmittedOrder<trace_reduction>();
    }

    if (!input_graph.source_positions().empty()) {
      for (OpIndex index : assembler.graph().AllOperationIndices()) {
        OpIndex origin = assembler.graph().operation_origins()[index];
        assembler.graph().source_positions()[index] =
            input_graph.source_positions()[origin];
      }
    }
    if (origins) {
      for (OpIndex index : assembler.graph().AllOperationIndices()) {
        OpIndex origin = assembler.graph().operation_origins()[index];
        origins->SetNodeOrigin(index.id(), origin.id());
      }
    }

    input_graph.SwapWithCompanion();
  }

  template <bool trace_reduction>
  void RunAsEmittedOrder() {
    for (const Block& input_block : input_graph.blocks()) {
      VisitBlock<trace_reduction>(input_block);
    }
  }

  template <bool trace_reduction>
  void RunDominatorOrder() {
    base::SmallVector<Block*, 128> dominator_visit_stack;
    input_graph.GenerateDominatorTree();

    dominator_visit_stack.push_back(input_graph.GetPtr(0));
    while (!dominator_visit_stack.empty()) {
      Block* block = dominator_visit_stack.back();
      dominator_visit_stack.pop_back();
      VisitBlock<trace_reduction>(*block);

      for (Block* child = block->LastChild(); child != nullptr;
           child = child->NeighboringChild()) {
        dominator_visit_stack.push_back(child);
      }
    }
  }

  template <bool trace_reduction>
  void VisitBlock(const Block& input_block) {
    assembler.EnterBlock(input_block);
    current_input_block = &input_block;
    if constexpr (trace_reduction) {
      std::cout << PrintAsBlockHeader{input_block} << "\n";
    }
    if (!assembler.Bind(MapToNewGraph(input_block.index()))) {
      if constexpr (trace_reduction) TraceBlockUnreachable();
      assembler.ExitBlock(input_block);
      return;
    }
    assembler.current_block()->SetDeferred(input_block.IsDeferred());
    for (OpIndex index : input_graph.OperationIndices(input_block)) {
      if (!assembler.current_block()) break;
      assembler.SetCurrentOrigin(index);
      OpIndex first_output_index = assembler.graph().next_operation_index();
      USE(first_output_index);
      if constexpr (trace_reduction) TraceReductionStart(index);
      if (!analyzer.OpIsUsed(index)) {
        if constexpr (trace_reduction) TraceOperationUnused();
        continue;
      }
      const Operation& op = input_graph.Get(index);
      OpIndex new_index;
      if (input_block.IsLoop() && op.Is<PhiOp>()) {
        const PhiOp& phi = op.Cast<PhiOp>();
        new_index = assembler.PendingLoopPhi(MapToNewGraph(phi.inputs()[0]),
                                             phi.rep, phi.inputs()[1]);
        if constexpr (trace_reduction) {
          TraceReductionResult(first_output_index, new_index);
        }
      } else {
        switch (op.opcode) {
#define EMIT_INSTR_CASE(Name)                            \
  case Opcode::k##Name:                                  \
    new_index = this->Reduce##Name(op.Cast<Name##Op>()); \
    break;
          TURBOSHAFT_OPERATION_LIST(EMIT_INSTR_CASE)
#undef EMIT_INSTR_CASE
        }
        if constexpr (trace_reduction) {
          TraceReductionResult(first_output_index, new_index);
        }
      }
      op_mapping[index.id()] = new_index;
    }
    assembler.ExitBlock(input_block);
    if constexpr (trace_reduction) TraceBlockFinished();
  }

  void TraceReductionStart(OpIndex index) {
    std::cout << "╭── o" << index.id() << ": "
              << PaddingSpace{5 - CountDecimalDigits(index.id())}
              << OperationPrintStyle{input_graph.Get(index), "#o"} << "\n";
  }
  void TraceOperationUnused() { std::cout << "╰─> unused\n\n"; }
  void TraceBlockUnreachable() { std::cout << "╰─> unreachable\n\n"; }
  void TraceReductionResult(OpIndex first_output_index, OpIndex new_index) {
    if (new_index < first_output_index) {
      // The operation was replaced with an already existing one.
      std::cout << "╰─> #n" << new_index.id() << "\n";
    }
    bool before_arrow = new_index >= first_output_index;
    for (const Operation& op : assembler.graph().operations(
             first_output_index, assembler.graph().next_operation_index())) {
      OpIndex index = assembler.graph().Index(op);
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
                << OperationPrintStyle{assembler.graph().Get(index), "#n"}
                << "\n";
    }
    std::cout << "\n";
  }
  void TraceBlockFinished() { std::cout << "\n"; }

  // These functions take an operation from the old graph and use the assembler
  // to emit a corresponding operation in the new graph, translating inputs and
  // blocks accordingly.

  V8_INLINE OpIndex ReduceGoto(const GotoOp& op) {
    Block* destination = MapToNewGraph(op.destination->index());
    if (destination->IsBound()) {
      DCHECK(destination->IsLoop());
      FixLoopPhis(destination);
    }
    assembler.current_block()->SetOrigin(current_input_block);
    return assembler.Goto(destination);
  }
  V8_INLINE OpIndex ReduceBranch(const BranchOp& op) {
    Block* if_true = MapToNewGraph(op.if_true->index());
    Block* if_false = MapToNewGraph(op.if_false->index());
    return assembler.Branch(MapToNewGraph(op.condition()), if_true, if_false);
  }
  OpIndex ReduceCatchException(const CatchExceptionOp& op) {
    Block* if_success = MapToNewGraph(op.if_success->index());
    Block* if_exception = MapToNewGraph(op.if_exception->index());
    return assembler.CatchException(MapToNewGraph(op.call()), if_success,
                                    if_exception);
  }
  OpIndex ReduceSwitch(const SwitchOp& op) {
    base::SmallVector<SwitchOp::Case, 16> cases;
    for (SwitchOp::Case c : op.cases) {
      cases.emplace_back(c.value, MapToNewGraph(c.destination->index()));
    }
    return assembler.Switch(
        MapToNewGraph(op.input()),
        assembler.graph_zone()->CloneVector(base::VectorOf(cases)),
        MapToNewGraph(op.default_case->index()));
  }
  OpIndex ReducePhi(const PhiOp& op) {
    base::Vector<const OpIndex> old_inputs = op.inputs();
    base::SmallVector<OpIndex, 8> new_inputs;
    Block* old_pred = current_input_block->LastPredecessor();
    Block* new_pred = assembler.current_block()->LastPredecessor();
    // Control predecessors might be missing after the optimization phase. So we
    // need to skip phi inputs that belong to control predecessors that have no
    // equivalent in the new graph.

    // When iterating the graph in kAsEmitted order (ie, going through all of
    // the blocks in linear order), we assume that the order of control
    // predecessors did not change. In kDominator order, the order of control
    // predecessor might or might not change.
    for (OpIndex input : base::Reversed(old_inputs)) {
      if (new_pred && new_pred->Origin() == old_pred) {
        new_inputs.push_back(MapToNewGraph(input));
        new_pred = new_pred->NeighboringPredecessor();
      }
      old_pred = old_pred->NeighboringPredecessor();
    }
    DCHECK_IMPLIES(new_pred == nullptr, old_pred == nullptr);

    if (new_pred != nullptr) {
      DCHECK_EQ(visit_order, VisitOrder::kDominator);
      // If {new_pred} is nullptr, then the order of the predecessors changed.
      // This should only happen when {visit_order} is kDominator. For instance,
      // consider this (partial) dominator tree:
      //
      //     ╠ 7
      //     ║ ╠ 8
      //     ║ ╚ 10
      //     ╠ 9
      //     ╚ 11
      //
      // Where the predecessors of block 11 are blocks 9 and 10 (in that order).
      // In kDominator visit order, block 10 will be visited before block 9.
      // Since blocks are added to predecessors when the predecessors are
      // visited, it means that in the new graph, the predecessors of block 11
      // are [10, 9] rather than [9, 10].
      // To account for this, we reorder the inputs of the Phi, and get rid of
      // inputs from blocks that vanished.

      base::SmallVector<uint32_t, 16> old_pred_vec;
      for (old_pred = current_input_block->LastPredecessor();
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
      for (new_pred = assembler.current_block()->LastPredecessor();
           new_pred != nullptr; new_pred = new_pred->NeighboringPredecessor()) {
        const Block* origin = new_pred->Origin();
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

    std::reverse(new_inputs.begin(), new_inputs.end());
    return assembler.Phi(base::VectorOf(new_inputs), op.rep);
  }
  OpIndex ReducePendingLoopPhi(const PendingLoopPhiOp& op) { UNREACHABLE(); }
  V8_INLINE OpIndex ReduceFrameState(const FrameStateOp& op) {
    auto inputs = MapToNewGraph<32>(op.inputs());
    return assembler.FrameState(base::VectorOf(inputs), op.inlined, op.data);
  }
  OpIndex ReduceCall(const CallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return assembler.Call(callee, base::VectorOf(arguments), op.descriptor);
  }
  OpIndex ReduceReturn(const ReturnOp& op) {
    // We very rarely have tuples longer than 4.
    auto return_values = MapToNewGraph<4>(op.return_values());
    return assembler.Return(MapToNewGraph(op.pop_count()),
                            base::VectorOf(return_values));
  }
  OpIndex ReduceOverflowCheckedBinop(const OverflowCheckedBinopOp& op) {
    return assembler.OverflowCheckedBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex ReduceWordUnary(const WordUnaryOp& op) {
    return assembler.WordUnary(MapToNewGraph(op.input()), op.kind, op.rep);
  }
  OpIndex ReduceFloatUnary(const FloatUnaryOp& op) {
    return assembler.FloatUnary(MapToNewGraph(op.input()), op.kind, op.rep);
  }
  OpIndex ReduceShift(const ShiftOp& op) {
    return assembler.Shift(MapToNewGraph(op.left()), MapToNewGraph(op.right()),
                           op.kind, op.rep);
  }
  OpIndex ReduceEqual(const EqualOp& op) {
    return assembler.Equal(MapToNewGraph(op.left()), MapToNewGraph(op.right()),
                           op.rep);
  }
  OpIndex ReduceComparison(const ComparisonOp& op) {
    return assembler.Comparison(MapToNewGraph(op.left()),
                                MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex ReduceChange(const ChangeOp& op) {
    return assembler.Change(MapToNewGraph(op.input()), op.kind, op.from, op.to);
  }
  OpIndex ReduceFloat64InsertWord32(const Float64InsertWord32Op& op) {
    return assembler.Float64InsertWord32(MapToNewGraph(op.float64()),
                                         MapToNewGraph(op.word32()), op.kind);
  }
  OpIndex ReduceTaggedBitcast(const TaggedBitcastOp& op) {
    return assembler.TaggedBitcast(MapToNewGraph(op.input()), op.from, op.to);
  }
  OpIndex ReduceConstant(const ConstantOp& op) {
    return assembler.Constant(op.kind, op.storage);
  }
  OpIndex ReduceLoad(const LoadOp& op) {
    return assembler.Load(MapToNewGraph(op.base()), op.kind, op.loaded_rep,
                          op.offset);
  }
  OpIndex ReduceIndexedLoad(const IndexedLoadOp& op) {
    return assembler.IndexedLoad(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()), op.kind,
        op.loaded_rep, op.offset, op.element_size_log2);
  }
  OpIndex ReduceStore(const StoreOp& op) {
    return assembler.Store(MapToNewGraph(op.base()), MapToNewGraph(op.value()),
                           op.kind, op.stored_rep, op.write_barrier, op.offset);
  }
  OpIndex ReduceIndexedStore(const IndexedStoreOp& op) {
    return assembler.IndexedStore(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.kind, op.stored_rep, op.write_barrier,
        op.offset, op.element_size_log2);
  }
  OpIndex ReduceRetain(const RetainOp& op) {
    return assembler.Retain(MapToNewGraph(op.retained()));
  }
  OpIndex ReduceParameter(const ParameterOp& op) {
    return assembler.Parameter(op.parameter_index, op.debug_name);
  }
  OpIndex ReduceOsrValue(const OsrValueOp& op) {
    return assembler.OsrValue(op.index);
  }
  OpIndex ReduceStackPointerGreaterThan(const StackPointerGreaterThanOp& op) {
    return assembler.StackPointerGreaterThan(MapToNewGraph(op.stack_limit()),
                                             op.kind);
  }
  OpIndex ReduceStackSlot(const StackSlotOp& op) {
    return assembler.StackSlot(op.size, op.alignment);
  }
  OpIndex ReduceFrameConstant(const FrameConstantOp& op) {
    return assembler.FrameConstant(op.kind);
  }
  OpIndex ReduceCheckLazyDeopt(const CheckLazyDeoptOp& op) {
    return assembler.CheckLazyDeopt(MapToNewGraph(op.call()),
                                    MapToNewGraph(op.frame_state()));
  }
  OpIndex ReduceDeoptimize(const DeoptimizeOp& op) {
    return assembler.Deoptimize(MapToNewGraph(op.frame_state()), op.parameters);
  }
  OpIndex ReduceDeoptimizeIf(const DeoptimizeIfOp& op) {
    return assembler.DeoptimizeIf(MapToNewGraph(op.condition()),
                                  MapToNewGraph(op.frame_state()), op.negated,
                                  op.parameters);
  }
  OpIndex ReduceTuple(const TupleOp& op) {
    return assembler.Tuple(base::VectorOf(MapToNewGraph<4>(op.inputs())));
  }
  OpIndex ReduceProjection(const ProjectionOp& op) {
    return assembler.Projection(MapToNewGraph(op.input()), op.index);
  }
  OpIndex ReduceWordBinop(const WordBinopOp& op) {
    return assembler.WordBinop(MapToNewGraph(op.left()),
                               MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex ReduceFloatBinop(const FloatBinopOp& op) {
    return assembler.FloatBinop(MapToNewGraph(op.left()),
                                MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex ReduceUnreachable(const UnreachableOp& op) {
    return assembler.Unreachable();
  }

  OpIndex MapToNewGraph(OpIndex old_index) {
    OpIndex result = op_mapping[old_index.id()];
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

  Block* MapToNewGraph(BlockIndex old_index) {
    Block* result = block_mapping[old_index.id()];
    DCHECK_NOT_NULL(result);
    return result;
  }

  void FixLoopPhis(Block* loop) {
    DCHECK(loop->IsLoop());
    for (Operation& op : assembler.graph().operations(*loop)) {
      if (auto* pending_phi = op.TryCast<PendingLoopPhiOp>()) {
        assembler.graph().template Replace<PhiOp>(
            assembler.graph().Index(*pending_phi),
            base::VectorOf({pending_phi->first(),
                            MapToNewGraph(pending_phi->old_backedge_index)}),
            pending_phi->rep);
      }
    }
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_OPTIMIZATION_PHASE_H_
