// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_COPYING_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_COPYING_PHASE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/reducer-traits.h"
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

using MaybeVariable = base::Optional<Variable>;

V8_EXPORT_PRIVATE int CountDecimalDigits(uint32_t value);
struct PaddingSpace {
  int spaces;
};
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           PaddingSpace padding);

template <typename Next>
class ReducerBaseForwarder;
template <typename Next>
class VariableReducer;

template <class Next>
class GraphVisitor : public Next {
  template <typename N>
  friend class ReducerBaseForwarder;

 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  GraphVisitor()
      : input_graph_(Asm().modifiable_input_graph()),
        current_input_block_(nullptr),
        op_mapping_(Asm().input_graph().op_id_count(), OpIndex::Invalid(),
                    Asm().phase_zone(), &Asm().input_graph()),
        block_mapping_(Asm().input_graph().block_count(), nullptr,
                       Asm().phase_zone()),
        blocks_needing_variables_(Asm().input_graph().block_count(),
                                  Asm().phase_zone()),
        old_opindex_to_variables(Asm().input_graph().op_id_count(),
                                 Asm().phase_zone(), &Asm().input_graph()) {
    Asm().output_graph().Reset();
  }

  // `trace_reduction` is a template parameter to avoid paying for tracing at
  // runtime.
  template <bool trace_reduction>
  void VisitGraph() {
    Asm().Analyze();

    // Creating initial old-to-new Block mapping.
    for (Block& input_block : Asm().modifiable_input_graph().blocks()) {
      block_mapping_[input_block.index()] = Asm().output_graph().NewBlock(
          input_block.IsLoop() ? Block::Kind::kLoopHeader : Block::Kind::kMerge,
          &input_block);
    }

    // Visiting the graph.
    VisitAllBlocks<trace_reduction>();

    Finalize();
  }

  void Bind(Block* block) {
    Next::Bind(block);
    block->SetOrigin(current_input_block());
  }

  void Finalize() {
    // Updating the source_positions.
    if (!Asm().input_graph().source_positions().empty()) {
      for (OpIndex index : Asm().output_graph().AllOperationIndices()) {
        OpIndex origin = Asm().output_graph().operation_origins()[index];
        Asm().output_graph().source_positions()[index] =
            Asm().input_graph().source_positions()[origin];
      }
    }
    // Updating the operation origins.
    NodeOriginTable* origins = PipelineData::Get().node_origins();
    if (origins) {
      for (OpIndex index : Asm().output_graph().AllOperationIndices()) {
        OpIndex origin = Asm().output_graph().operation_origins()[index];
        origins->SetNodeOrigin(index.id(), origin.id());
      }
    }

    input_graph_.SwapWithCompanion();
  }

  const Block* current_input_block() { return current_input_block_; }

  bool* turn_loop_without_backedge_into_merge() {
    return &turn_loop_without_backedge_into_merge_;
  }

  // Visits and emits {input_block} right now (ie, in the current block).
  void CloneAndInlineBlock(const Block* input_block) {
    if (Asm().generating_unreachable_operations()) return;

    // Computing which input of Phi operations to use when visiting
    // {input_block} (since {input_block} doesn't really have predecessors
    // anymore).
    int added_block_phi_input = input_block->GetPredecessorIndex(
        Asm().current_block()->OriginForBlockEnd());

    // There is no guarantees that {input_block} will be entirely removed just
    // because it's cloned/inlined, since it's possible that it has predecessors
    // for which this optimization didn't apply. As a result, we add it to
    // {blocks_needing_variables_}, so that if it's ever generated
    // normally, Variables are used when emitting its content, so that
    // they can later be merged when control flow merges with the current
    // version of {input_block} that we just cloned.
    blocks_needing_variables_.Add(input_block->index().id());

    ScopedModification<bool> set_true(&current_block_needs_variables_, true);

    // Similarly as VisitBlock does, we visit the Phis first, then update all of
    // the Phi mappings at once and visit the rest of the block.
    base::SmallVector<OpIndex, 64> new_phi_values;
    // Emitting new phis and recording mapping.
    DCHECK_NOT_NULL(Asm().current_block());
    for (OpIndex index : Asm().input_graph().OperationIndices(*input_block)) {
      if (const PhiOp* phi =
              Asm().input_graph().Get(index).template TryCast<PhiOp>()) {
        if (ShouldSkipOperation(*phi)) continue;
        // This Phi has been cloned/inlined, and has thus now a single
        // predecessor, and shouldn't be a Phi anymore.
        OpIndex newval = MapToNewGraph(phi->input(added_block_phi_input));
        new_phi_values.push_back(newval);
      }
    }
    // Visiting the other operations of the block and emitting the new Phi
    // mappings.
    int phi_num = 0;
    for (OpIndex index : Asm().input_graph().OperationIndices(*input_block)) {
      const Operation& op = Asm().input_graph().Get(index);
      if (op.template Is<PhiOp>()) {
        if (ShouldSkipOperation(op)) continue;
        CreateOldToNewMapping(index, new_phi_values[phi_num++]);
      } else {
        if (!VisitOpAndUpdateMapping<false>(index, input_block)) break;
      }
    }
  }

  // {InlineOp} introduces two limitations unlike {CloneAndInlineBlock}:
  // 1. The input operation must not be emitted anymore as part of its
  // regular input block;
  // 2. {InlineOp} must not be used multiple times for the same input op.
  bool InlineOp(OpIndex index, const Block* input_block) {
    return VisitOpAndUpdateMapping<false>(index, input_block);
  }

  template <bool can_be_invalid = false>
  OpIndex MapToNewGraph(OpIndex old_index, int predecessor_index = -1) {
    DCHECK(old_index.valid());
    OpIndex result = op_mapping_[old_index];

    if constexpr (reducer_list_contains<typename Next::ReducerList,
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
          result = Asm().GetVariable(var.value());
        } else {
          result = Asm().GetPredecessorValue(var.value(), predecessor_index);
        }
      }
    }
    DCHECK_IMPLIES(!can_be_invalid, result.valid());
    return result;
  }

  Block* MapToNewGraph(const Block* block) const {
    Block* new_block = block_mapping_[block->index()];
    DCHECK_NOT_NULL(new_block);
    return new_block;
  }

  // The block from the input graph that corresponds to the current block as a
  // branch destination. Such a block might not exist, and this function uses a
  // trick to compute such a block in almost all cases, but might rarely fail
  // and return `nullptr` instead.
  const Block* OriginForBlockStart(Block* block) const {
    // Check that `block->origin_` is still valid as a block start and was not
    // changed to a semantically different block when inlining blocks.
    const Block* origin = block->origin_;
    if (origin && MapToNewGraph(origin) == block) return origin;
    return nullptr;
  }

  // Clone all of the blocks in {sub_graph} (which should be Blocks of the input
  // graph). If `keep_loop_kinds` is true, the loop headers are preserved, and
  // otherwise they are marked as Merge. An initial GotoOp jumping to the 1st
  // block of `sub_graph` is always emitted. The output Block corresponding to
  // the 1st block of `sub_graph` is returned.
  template <class Set>
  Block* CloneSubGraph(Set sub_graph, bool keep_loop_kinds,
                       bool is_loop_after_peeling = false) {
    // The BlockIndex of the blocks of `sub_graph` should be sorted so that
    // visiting them in order is correct (all of the predecessors of a block
    // should always be visited before the block itself).
    DCHECK(std::is_sorted(
        sub_graph.begin(), sub_graph.end(),
        [](Block* a, Block* b) { return a->index().id() <= b->index().id(); }));

    // 1. Create new blocks, and update old->new mapping. This is required to
    // emit multiple times the blocks of {sub_graph}: if a block `B1` in
    // {sub_graph} ends with a Branch/Goto to a block `B2` that is also in
    // {sub_graph}, then this Branch/Goto should go to the version of `B2` that
    // this CloneSubGraph will insert, rather than to a version inserted by a
    // previous call to CloneSubGraph or the version that the regular
    // VisitAllBlock function will emit.
    ZoneVector<Block*> old_mappings(sub_graph.size(), Asm().phase_zone());
    for (auto&& [input_block, old_mapping] :
         base::zip(sub_graph, old_mappings)) {
      old_mapping = block_mapping_[input_block->index()];
      Block::Kind kind = keep_loop_kinds && input_block->IsLoop()
                             ? Block::Kind::kLoopHeader
                             : Block::Kind::kMerge;
      block_mapping_[input_block->index()] =
          Asm().output_graph().NewBlock(kind, input_block);
    }

    // 2. Visit block in correct order (begin to end)

    // Emit a goto to 1st block.
    Block* start = block_mapping_[(*sub_graph.begin())->index()];
#ifdef DEBUG
    if (is_loop_after_peeling) start->set_has_peeled_iteration();
#endif
    Asm().Goto(start);
    // Visiting `sub_graph`.
    for (Block* block : sub_graph) {
      blocks_needing_variables_.Add(block->index().id());
      VisitBlock<false>(block);
    }

    // 3. Restore initial old->new mapping
    for (auto&& [input_block, old_mapping] :
         base::zip(sub_graph, old_mappings)) {
      block_mapping_[input_block->index()] = old_mapping;
    }

    return start;
  }

  template <bool can_be_invalid = false>
  OpIndex MapToNewGraphIfValid(OpIndex old_index, int predecessor_index = -1) {
    return old_index.valid()
               ? MapToNewGraph<can_be_invalid>(old_index, predecessor_index)
               : OpIndex::Invalid();
  }

  template <bool can_be_invalid = false>
  OptionalOpIndex MapToNewGraph(OptionalOpIndex old_index,
                                int predecessor_index = -1) {
    if (!old_index.has_value()) return OptionalOpIndex::Invalid();
    return MapToNewGraph<can_be_invalid>(old_index.value(), predecessor_index);
  }

 private:
  template <bool trace_reduction>
  void VisitAllBlocks() {
    base::SmallVector<const Block*, 128> visit_stack;

    visit_stack.push_back(&Asm().input_graph().StartBlock());
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
        blocks_needing_variables_.Contains(input_block->index().id());
    if constexpr (trace_reduction) {
      std::cout << "\nold " << PrintAsBlockHeader{*input_block} << "\n";
      std::cout << "new "
                << PrintAsBlockHeader{*MapToNewGraph(input_block),
                                      Asm().output_graph().next_block_index()}
                << "\n";
    }
    Block* new_block = MapToNewGraph(input_block);
    if (Asm().Bind(new_block)) {
      // Phis could be mutually recursive, for instance (in a loop header):
      //
      //     p1 = phi(a, p2)
      //     p2 = phi(b, p1)
      //
      // In this case, if we are currently unrolling the loop and visiting this
      // loop header that is now part of the loop body, then if we visit Phis
      // and emit new mapping (with CreateOldToNewMapping) as we go along, we
      // would visit p1 and emit a mapping saying "p1 = p2", and use this
      // mapping when visiting p2, then we'd map p2 to p2 instead of p1. To
      // overcome this issue, we first visit the Phis of the loop, emit the new
      // phis, and record the new mapping in a side-table ({new_phi_values}).
      // Then, we visit all of the operations of the loop and commit the new
      // mappings: phis were emitted before using the old mapping, and all of
      // the other operations will use the new mapping (as they should).

      // Visiting Phis and collecting their new OpIndices.
      base::SmallVector<OpIndex, 64> new_phi_values;
      for (OpIndex index : Asm().input_graph().OperationIndices(*input_block)) {
        DCHECK_NOT_NULL(Asm().current_block());
        if (Asm().input_graph().Get(index).template Is<PhiOp>()) {
          OpIndex new_index =
              VisitOpNoMappingUpdate<trace_reduction>(index, input_block);
          new_phi_values.push_back(new_index);
          if (!Asm().current_block()) {
            // A reducer has detected, based on the Phis of the block that were
            // visited so far, that we are in unreachable code (or, less likely,
            // decided, based on some Phis only, to jump away from this block?).
            break;
          }
        }
      }

      // Visiting everything, updating Phi mappings, and emitting non-phi
      // operations.
      if (Asm().current_block()) {
        int phi_num = 0;
        for (OpIndex index :
             Asm().input_graph().OperationIndices(*input_block)) {
          if (Asm().input_graph().Get(index).template Is<PhiOp>()) {
            CreateOldToNewMapping(index, new_phi_values[phi_num++]);
          } else {
            if (!VisitOpAndUpdateMapping<trace_reduction>(index, input_block)) {
              break;
            }
          }
        }
      }
      if constexpr (trace_reduction) TraceBlockFinished();
    } else {
      if constexpr (trace_reduction) TraceBlockUnreachable();
    }

    // If we eliminate a loop backedge, we need to turn the loop into a
    // single-predecessor merge block.
    if (!turn_loop_without_backedge_into_merge_) return;
    const Operation& last_op = input_block->LastOperation(Asm().input_graph());
    if (auto* final_goto = last_op.TryCast<GotoOp>()) {
      if (final_goto->destination->IsLoop()) {
        if (input_block->index() > final_goto->destination->index()) {
          Asm().FinalizeLoop(MapToNewGraph(final_goto->destination));
        } else {
          // We have a forward jump to a loop, rather than a backedge. We
          // don't need to do anything.
        }
      }
    }
  }

  template <bool trace_reduction>
  bool VisitOpAndUpdateMapping(OpIndex index, const Block* input_block) {
    if (Asm().current_block() == nullptr) return false;
    OpIndex new_index =
        VisitOpNoMappingUpdate<trace_reduction>(index, input_block);
    const Operation& op = Asm().input_graph().Get(index);
    if (CanBeUsedAsInput(op) && new_index.valid()) {
      CreateOldToNewMapping(index, new_index);
    }
    return true;
  }

  template <bool trace_reduction>
  OpIndex VisitOpNoMappingUpdate(OpIndex index, const Block* input_block) {
    Block* current_block = Asm().current_block();
    DCHECK_NOT_NULL(current_block);
    Asm().SetCurrentOrigin(index);
    current_block->SetOrigin(input_block);
    OpIndex first_output_index = Asm().output_graph().next_operation_index();
    USE(first_output_index);
    const Operation& op = Asm().input_graph().Get(index);
    if constexpr (trace_reduction) TraceReductionStart(index);
    if (ShouldSkipOperation(op)) {
      if constexpr (trace_reduction) TraceOperationSkipped();
      return OpIndex::Invalid();
    }
    OpIndex new_index;
    switch (op.opcode) {
#define EMIT_INSTR_CASE(Name)                                             \
  case Opcode::k##Name:                                                   \
    if (MayThrow(Opcode::k##Name)) return OpIndex::Invalid();             \
    new_index = Asm().ReduceInputGraph##Name(index, op.Cast<Name##Op>()); \
    break;
      TURBOSHAFT_OPERATION_LIST(EMIT_INSTR_CASE)
#undef EMIT_INSTR_CASE
    }
    if constexpr (trace_reduction) {
      if (CanBeUsedAsInput(op) && !new_index.valid()) {
        TraceOperationSkipped();
      } else {
        TraceReductionResult(current_block, first_output_index, new_index);
      }
    }
#ifdef DEBUG
    DCHECK_IMPLIES(new_index.valid(),
                   Asm().output_graph().BelongsToThisGraph(new_index));
    if (V8_UNLIKELY(v8_flags.turboshaft_verify_reductions)) {
      if (new_index.valid()) {
        const Operation& new_op = Asm().output_graph().Get(new_index);
        if (!new_op.Is<TupleOp>()) {
          // Checking that the outputs_rep of the new operation are the same as
          // the old operation. (except for tuples, since they don't have
          // outputs_rep)
          DCHECK_EQ(new_op.outputs_rep().size(), op.outputs_rep().size());
          for (size_t i = 0; i < new_op.outputs_rep().size(); ++i) {
            DCHECK(new_op.outputs_rep()[i].AllowImplicitRepresentationChangeTo(
                op.outputs_rep()[i]));
          }
        }
        Asm().Verify(index, new_index);
      }
    }
#endif  // DEBUG
    return new_index;
  }

  void TraceReductionStart(OpIndex index) {
    std::cout << "╭── o" << index.id() << ": "
              << PaddingSpace{5 - CountDecimalDigits(index.id())}
              << OperationPrintStyle{Asm().input_graph().Get(index), "#o"}
              << "\n";
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
    for (const Operation& op : Asm().output_graph().operations(
             first_output_index, Asm().output_graph().next_operation_index())) {
      OpIndex index = Asm().output_graph().Index(op);
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
                << OperationPrintStyle{Asm().output_graph().Get(index), "#n"}
                << "\n";
      if (op.IsBlockTerminator() && Asm().current_block() &&
          Asm().current_block() != current_block) {
        current_block = &Asm().output_graph().Get(
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
    Block* destination = MapToNewGraph(op.destination);
    if (op.is_backedge) {
      DCHECK(destination->IsBound());
      DCHECK(destination->IsLoop());
      FixLoopPhis(op.destination);
    }
    // It is important that we first fix loop phis and then reduce the `Goto`,
    // because reducing the `Goto` can have side effects, in particular, it can
    // modify affect the SnapshotTable of `VariableReducer`, which is also used
    // by `FixLoopPhis()`.
    Asm().ReduceGoto(destination, op.is_backedge);
    return OpIndex::Invalid();
  }
  V8_INLINE OpIndex AssembleOutputGraphBranch(const BranchOp& op) {
    Block* if_true = MapToNewGraph(op.if_true);
    Block* if_false = MapToNewGraph(op.if_false);
    return Asm().ReduceBranch(MapToNewGraph(op.condition()), if_true, if_false,
                              op.hint);
  }
  OpIndex AssembleOutputGraphSwitch(const SwitchOp& op) {
    base::SmallVector<SwitchOp::Case, 16> cases;
    for (SwitchOp::Case c : op.cases) {
      cases.emplace_back(c.value, MapToNewGraph(c.destination), c.hint);
    }
    return Asm().ReduceSwitch(
        MapToNewGraph(op.input()),
        Asm().graph_zone()->CloneVector(base::VectorOf(cases)),
        MapToNewGraph(op.default_case), op.default_hint);
  }
  OpIndex AssembleOutputGraphPhi(const PhiOp& op) {
    OpIndex ig_index = Asm().input_graph().Index(op);
    if (Asm().current_block()->IsLoop()) {
      if (ig_index == op.input(PhiOp::kLoopPhiBackEdgeIndex)) {
        // Avoid emitting a Loop Phi which points to itself, instead
        // emit it's 0'th input.
        return MapToNewGraph(op.input(0));
      }
      return Asm().PendingLoopPhi(MapToNewGraph(op.input(0)), op.rep);
    }

    base::Vector<const OpIndex> old_inputs = op.inputs();
    base::SmallVector<OpIndex, 64> new_inputs;
    int predecessor_count = Asm().current_block()->PredecessorCount();
    Block* old_pred = current_input_block_->LastPredecessor();
    Block* new_pred = Asm().current_block()->LastPredecessor();
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
      for (auto& block : Asm().modifiable_input_graph().blocks()) {
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
      for (new_pred = Asm().current_block()->LastPredecessor();
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

    DCHECK_EQ(new_inputs.size(), Asm().current_block()->PredecessorCount());

    if (new_inputs.size() == 1) {
      // This Operation used to be a Phi in a Merge, but since one (or more) of
      // the inputs of the merge have been removed, there is no need for a Phi
      // anymore.
      return new_inputs[0];
    }

    std::reverse(new_inputs.begin(), new_inputs.end());
    return Asm().ReducePhi(base::VectorOf(new_inputs), op.rep);
  }
  OpIndex AssembleOutputGraphPendingLoopPhi(const PendingLoopPhiOp& op) {
    UNREACHABLE();
  }
  V8_INLINE OpIndex AssembleOutputGraphFrameState(const FrameStateOp& op) {
    auto inputs = MapToNewGraph<32>(op.inputs());
    return Asm().ReduceFrameState(base::VectorOf(inputs), op.inlined, op.data);
  }
  OpIndex AssembleOutputGraphCall(const CallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    OpIndex frame_state = MapToNewGraphIfValid(op.frame_state());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return Asm().ReduceCall(callee, frame_state, base::VectorOf(arguments),
                            op.descriptor, op.Effects());
  }
  OpIndex AssembleOutputGraphDidntThrow(const DidntThrowOp& op) {
    const Operation& throwing_operation =
        Asm().input_graph().Get(op.throwing_operation());
    OpIndex result;
    switch (throwing_operation.opcode) {
#define CASE(Name)                                                     \
  case Opcode::k##Name:                                                \
    result = Asm().ReduceInputGraph##Name(                             \
        op.throwing_operation(), throwing_operation.Cast<Name##Op>()); \
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
                              &Asm().input_graph());
    Graph::OpIndexIterator end(op.didnt_throw_block->end(),
                               &Asm().input_graph());
    // To translate `CheckException` to the new graph, we reduce the throwing
    // operation (actually it's `DidntThrow` operation, but that triggers the
    // actual reduction) with a catch scope. If the reduction replaces the
    // throwing operation with other throwing operations, all of them will be
    // connected to the provided catch block. The reduction should automatically
    // bind a block that represents non-throwing control flow of the original
    // operation, so we can inline the rest of the `didnt_throw` block.
    {
      CatchScope scope(Asm(), MapToNewGraph(op.catch_block));
      DCHECK(Asm().input_graph().Get(*it).template Is<DidntThrowOp>());
      if (!Asm().InlineOp(*it, op.didnt_throw_block)) {
        return OpIndex::Invalid();
      }
      ++it;
    }
    for (; it != end; ++it) {
      // Using `InlineOp` requires that the inlined operation is not emitted
      // multiple times. This is the case here because we just removed the
      // single predecessor of `didnt_throw_block`.
      if (!Asm().InlineOp(*it, op.didnt_throw_block)) {
        break;
      }
    }
    return OpIndex::Invalid();
  }
  OpIndex AssembleOutputGraphCatchBlockBegin(const CatchBlockBeginOp& op) {
    return Asm().ReduceCatchBlockBegin();
  }
  OpIndex AssembleOutputGraphTailCall(const TailCallOp& op) {
    OpIndex callee = MapToNewGraph(op.callee());
    auto arguments = MapToNewGraph<16>(op.arguments());
    return Asm().ReduceTailCall(callee, base::VectorOf(arguments),
                                op.descriptor);
  }
  OpIndex AssembleOutputGraphReturn(const ReturnOp& op) {
    // We very rarely have tuples longer than 4.
    auto return_values = MapToNewGraph<4>(op.return_values());
    return Asm().ReduceReturn(MapToNewGraph(op.pop_count()),
                              base::VectorOf(return_values));
  }
  OpIndex AssembleOutputGraphOverflowCheckedBinop(
      const OverflowCheckedBinopOp& op) {
    return Asm().ReduceOverflowCheckedBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphWordUnary(const WordUnaryOp& op) {
    return Asm().ReduceWordUnary(MapToNewGraph(op.input()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphFloatUnary(const FloatUnaryOp& op) {
    return Asm().ReduceFloatUnary(MapToNewGraph(op.input()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphShift(const ShiftOp& op) {
    return Asm().ReduceShift(MapToNewGraph(op.left()),
                             MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphComparison(const ComparisonOp& op) {
    return Asm().ReduceComparison(MapToNewGraph(op.left()),
                                  MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphChange(const ChangeOp& op) {
    return Asm().ReduceChange(MapToNewGraph(op.input()), op.kind, op.assumption,
                              op.from, op.to);
  }
  OpIndex AssembleOutputGraphChangeOrDeopt(const ChangeOrDeoptOp& op) {
    return Asm().ReduceChangeOrDeopt(MapToNewGraph(op.input()),
                                     MapToNewGraph(op.frame_state()), op.kind,
                                     op.minus_zero_mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTryChange(const TryChangeOp& op) {
    return Asm().ReduceTryChange(MapToNewGraph(op.input()), op.kind, op.from,
                                 op.to);
  }
  OpIndex AssembleOutputGraphBitcastWord32PairToFloat64(
      const BitcastWord32PairToFloat64Op& op) {
    return Asm().ReduceBitcastWord32PairToFloat64(
        MapToNewGraph(op.high_word32()), MapToNewGraph(op.low_word32()));
  }
  OpIndex AssembleOutputGraphTaggedBitcast(const TaggedBitcastOp& op) {
    return Asm().ReduceTaggedBitcast(MapToNewGraph(op.input()), op.from, op.to,
                                     op.kind);
  }
  OpIndex AssembleOutputGraphObjectIs(const ObjectIsOp& op) {
    return Asm().ReduceObjectIs(MapToNewGraph(op.input()), op.kind,
                                op.input_assumptions);
  }
  OpIndex AssembleOutputGraphFloatIs(const FloatIsOp& op) {
    return Asm().ReduceFloatIs(MapToNewGraph(op.input()), op.kind,
                               op.input_rep);
  }
  OpIndex AssembleOutputGraphObjectIsNumericValue(
      const ObjectIsNumericValueOp& op) {
    return Asm().ReduceObjectIsNumericValue(MapToNewGraph(op.input()), op.kind,
                                            op.input_rep);
  }
  OpIndex AssembleOutputGraphConvert(const ConvertOp& op) {
    return Asm().ReduceConvert(MapToNewGraph(op.input()), op.from, op.to);
  }
  OpIndex AssembleOutputGraphConvertUntaggedToJSPrimitive(
      const ConvertUntaggedToJSPrimitiveOp& op) {
    return Asm().ReduceConvertUntaggedToJSPrimitive(
        MapToNewGraph(op.input()), op.kind, op.input_rep,
        op.input_interpretation, op.minus_zero_mode);
  }
  OpIndex AssembleOutputGraphConvertUntaggedToJSPrimitiveOrDeopt(
      const ConvertUntaggedToJSPrimitiveOrDeoptOp& op) {
    return Asm().ReduceConvertUntaggedToJSPrimitiveOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()), op.kind,
        op.input_rep, op.input_interpretation, op.feedback);
  }
  OpIndex AssembleOutputGraphConvertJSPrimitiveToUntagged(
      const ConvertJSPrimitiveToUntaggedOp& op) {
    return Asm().ReduceConvertJSPrimitiveToUntagged(
        MapToNewGraph(op.input()), op.kind, op.input_assumptions);
  }
  OpIndex AssembleOutputGraphConvertJSPrimitiveToUntaggedOrDeopt(
      const ConvertJSPrimitiveToUntaggedOrDeoptOp& op) {
    return Asm().ReduceConvertJSPrimitiveToUntaggedOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()),
        op.from_kind, op.to_kind, op.minus_zero_mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTruncateJSPrimitiveToUntagged(
      const TruncateJSPrimitiveToUntaggedOp& op) {
    return Asm().ReduceTruncateJSPrimitiveToUntagged(
        MapToNewGraph(op.input()), op.kind, op.input_assumptions);
  }
  OpIndex AssembleOutputGraphTruncateJSPrimitiveToUntaggedOrDeopt(
      const TruncateJSPrimitiveToUntaggedOrDeoptOp& op) {
    return Asm().ReduceTruncateJSPrimitiveToUntaggedOrDeopt(
        MapToNewGraph(op.input()), MapToNewGraph(op.frame_state()), op.kind,
        op.input_requirement, op.feedback);
  }
  OpIndex AssembleOutputGraphConvertJSPrimitiveToObject(
      const ConvertJSPrimitiveToObjectOp& op) {
    return Asm().ReduceConvertJSPrimitiveToObject(
        MapToNewGraph(op.value()), MapToNewGraph(op.native_context()),
        MapToNewGraph(op.global_proxy()), op.mode);
  }
  OpIndex AssembleOutputGraphSelect(const SelectOp& op) {
    return Asm().ReduceSelect(
        MapToNewGraph(op.cond()), MapToNewGraph(op.vtrue()),
        MapToNewGraph(op.vfalse()), op.rep, op.hint, op.implem);
  }
  OpIndex AssembleOutputGraphConstant(const ConstantOp& op) {
    return Asm().ReduceConstant(op.kind, op.storage);
  }
  OpIndex AssembleOutputGraphAtomicRMW(const AtomicRMWOp& op) {
    return Asm().ReduceAtomicRMW(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), MapToNewGraph(op.expected()), op.bin_op,
        op.result_rep, op.input_rep, op.memory_access_kind);
  }

  OpIndex AssembleOutputGraphAtomicWord32Pair(const AtomicWord32PairOp& op) {
    return Asm().ReduceAtomicWord32Pair(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value_low()), MapToNewGraph(op.value_high()),
        MapToNewGraph(op.expected_low()), MapToNewGraph(op.expected_high()),
        op.kind, op.offset);
  }

  OpIndex AssembleOutputGraphMemoryBarrier(const MemoryBarrierOp& op) {
    return Asm().MemoryBarrier(op.memory_order);
  }

  OpIndex AssembleOutputGraphLoad(const LoadOp& op) {
    return Asm().ReduceLoad(MapToNewGraph(op.base()), MapToNewGraph(op.index()),
                            op.kind, op.loaded_rep, op.result_rep, op.offset,
                            op.element_size_log2);
  }
  OpIndex AssembleOutputGraphStore(const StoreOp& op) {
    return Asm().ReduceStore(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.kind, op.stored_rep, op.write_barrier,
        op.offset, op.element_size_log2, op.maybe_initializing_or_transitioning,
        op.indirect_pointer_tag());
  }
  OpIndex AssembleOutputGraphAllocate(const AllocateOp& op) {
    return Asm().FinishInitialization(
        Asm().Allocate(MapToNewGraph(op.size()), op.type));
  }
  OpIndex AssembleOutputGraphDecodeExternalPointer(
      const DecodeExternalPointerOp& op) {
    return Asm().DecodeExternalPointer(MapToNewGraph(op.handle()), op.tag);
  }

  OpIndex AssembleOutputGraphStackCheck(const StackCheckOp& op) {
    return Asm().ReduceStackCheck(op.check_origin, op.check_kind);
  }

  OpIndex AssembleOutputGraphRetain(const RetainOp& op) {
    return Asm().ReduceRetain(MapToNewGraph(op.retained()));
  }
  OpIndex AssembleOutputGraphParameter(const ParameterOp& op) {
    return Asm().ReduceParameter(op.parameter_index, op.rep, op.debug_name);
  }
  OpIndex AssembleOutputGraphOsrValue(const OsrValueOp& op) {
    return Asm().ReduceOsrValue(op.index);
  }
  OpIndex AssembleOutputGraphStackPointerGreaterThan(
      const StackPointerGreaterThanOp& op) {
    return Asm().ReduceStackPointerGreaterThan(MapToNewGraph(op.stack_limit()),
                                               op.kind);
  }
  OpIndex AssembleOutputGraphStackSlot(const StackSlotOp& op) {
    return Asm().ReduceStackSlot(op.size, op.alignment);
  }
  OpIndex AssembleOutputGraphFrameConstant(const FrameConstantOp& op) {
    return Asm().ReduceFrameConstant(op.kind);
  }
  OpIndex AssembleOutputGraphDeoptimize(const DeoptimizeOp& op) {
    return Asm().ReduceDeoptimize(MapToNewGraph(op.frame_state()),
                                  op.parameters);
  }
  OpIndex AssembleOutputGraphDeoptimizeIf(const DeoptimizeIfOp& op) {
    return Asm().ReduceDeoptimizeIf(MapToNewGraph(op.condition()),
                                    MapToNewGraph(op.frame_state()), op.negated,
                                    op.parameters);
  }

#if V8_ENABLE_WEBASSEMBLY
  OpIndex AssembleOutputGraphTrapIf(const TrapIfOp& op) {
    return Asm().ReduceTrapIf(MapToNewGraph(op.condition()),
                              MapToNewGraphIfValid(op.frame_state()),
                              op.negated, op.trap_id);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  OpIndex AssembleOutputGraphTuple(const TupleOp& op) {
    return Asm().ReduceTuple(base::VectorOf(MapToNewGraph<4>(op.inputs())));
  }
  OpIndex AssembleOutputGraphProjection(const ProjectionOp& op) {
    return Asm().ReduceProjection(MapToNewGraph(op.input()), op.index, op.rep);
  }
  OpIndex AssembleOutputGraphWordBinop(const WordBinopOp& op) {
    return Asm().ReduceWordBinop(MapToNewGraph(op.left()),
                                 MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphFloatBinop(const FloatBinopOp& op) {
    return Asm().ReduceFloatBinop(MapToNewGraph(op.left()),
                                  MapToNewGraph(op.right()), op.kind, op.rep);
  }
  OpIndex AssembleOutputGraphUnreachable(const UnreachableOp& op) {
    return Asm().ReduceUnreachable();
  }
  OpIndex AssembleOutputGraphStaticAssert(const StaticAssertOp& op) {
    return Asm().ReduceStaticAssert(MapToNewGraph(op.condition()), op.source);
  }
  OpIndex AssembleOutputGraphCheckTurboshaftTypeOf(
      const CheckTurboshaftTypeOfOp& op) {
    return Asm().ReduceCheckTurboshaftTypeOf(MapToNewGraph(op.input()), op.rep,
                                             op.type, op.successful);
  }
  OpIndex AssembleOutputGraphNewConsString(const NewConsStringOp& op) {
    return Asm().ReduceNewConsString(MapToNewGraph(op.length()),
                                     MapToNewGraph(op.first()),
                                     MapToNewGraph(op.second()));
  }
  OpIndex AssembleOutputGraphNewArray(const NewArrayOp& op) {
    return Asm().ReduceNewArray(MapToNewGraph(op.length()), op.kind,
                                op.allocation_type);
  }
  OpIndex AssembleOutputGraphDoubleArrayMinMax(const DoubleArrayMinMaxOp& op) {
    return Asm().ReduceDoubleArrayMinMax(MapToNewGraph(op.array()), op.kind);
  }
  OpIndex AssembleOutputGraphLoadFieldByIndex(const LoadFieldByIndexOp& op) {
    return Asm().ReduceLoadFieldByIndex(MapToNewGraph(op.object()),
                                        MapToNewGraph(op.index()));
  }
  OpIndex AssembleOutputGraphDebugBreak(const DebugBreakOp& op) {
    return Asm().ReduceDebugBreak();
  }
  OpIndex AssembleOutputGraphDebugPrint(const DebugPrintOp& op) {
    return Asm().ReduceDebugPrint(MapToNewGraph(op.input()), op.rep);
  }
  OpIndex AssembleOutputGraphBigIntBinop(const BigIntBinopOp& op) {
    return Asm().ReduceBigIntBinop(MapToNewGraph(op.left()),
                                   MapToNewGraph(op.right()),
                                   MapToNewGraph(op.frame_state()), op.kind);
  }
  OpIndex AssembleOutputGraphBigIntComparison(const BigIntComparisonOp& op) {
    return Asm().ReduceBigIntComparison(MapToNewGraph(op.left()),
                                        MapToNewGraph(op.right()), op.kind);
  }
  OpIndex AssembleOutputGraphBigIntUnary(const BigIntUnaryOp& op) {
    return Asm().ReduceBigIntUnary(MapToNewGraph(op.input()), op.kind);
  }
  OpIndex AssembleOutputGraphLoadRootRegister(const LoadRootRegisterOp& op) {
    return Asm().ReduceLoadRootRegister();
  }
  OpIndex AssembleOutputGraphStringAt(const StringAtOp& op) {
    return Asm().ReduceStringAt(MapToNewGraph(op.string()),
                                MapToNewGraph(op.position()), op.kind);
  }
#ifdef V8_INTL_SUPPORT
  OpIndex AssembleOutputGraphStringToCaseIntl(const StringToCaseIntlOp& op) {
    return Asm().ReduceStringToCaseIntl(MapToNewGraph(op.string()), op.kind);
  }
#endif  // V8_INTL_SUPPORT
  OpIndex AssembleOutputGraphStringLength(const StringLengthOp& op) {
    return Asm().ReduceStringLength(MapToNewGraph(op.string()));
  }
  OpIndex AssembleOutputGraphStringIndexOf(const StringIndexOfOp& op) {
    return Asm().ReduceStringIndexOf(MapToNewGraph(op.string()),
                                     MapToNewGraph(op.search()),
                                     MapToNewGraph(op.position()));
  }
  OpIndex AssembleOutputGraphStringFromCodePointAt(
      const StringFromCodePointAtOp& op) {
    return Asm().ReduceStringFromCodePointAt(MapToNewGraph(op.string()),
                                             MapToNewGraph(op.index()));
  }
  OpIndex AssembleOutputGraphStringSubstring(const StringSubstringOp& op) {
    return Asm().ReduceStringSubstring(MapToNewGraph(op.string()),
                                       MapToNewGraph(op.start()),
                                       MapToNewGraph(op.end()));
  }
  OpIndex AssembleOutputGraphStringConcat(const StringConcatOp& op) {
    return Asm().ReduceStringConcat(MapToNewGraph(op.left()),
                                    MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphStringComparison(const StringComparisonOp& op) {
    return Asm().ReduceStringComparison(MapToNewGraph(op.left()),
                                        MapToNewGraph(op.right()), op.kind);
  }
  OpIndex AssembleOutputGraphArgumentsLength(const ArgumentsLengthOp& op) {
    return Asm().ReduceArgumentsLength(op.kind, op.formal_parameter_count);
  }
  OpIndex AssembleOutputGraphNewArgumentsElements(
      const NewArgumentsElementsOp& op) {
    return Asm().ReduceNewArgumentsElements(MapToNewGraph(op.arguments_count()),
                                            op.type, op.formal_parameter_count);
  }
  OpIndex AssembleOutputGraphLoadTypedElement(const LoadTypedElementOp& op) {
    return Asm().ReduceLoadTypedElement(
        MapToNewGraph(op.buffer()), MapToNewGraph(op.base()),
        MapToNewGraph(op.external()), MapToNewGraph(op.index()), op.array_type);
  }
  OpIndex AssembleOutputGraphLoadDataViewElement(
      const LoadDataViewElementOp& op) {
    return Asm().ReduceLoadDataViewElement(
        MapToNewGraph(op.object()), MapToNewGraph(op.storage()),
        MapToNewGraph(op.index()), MapToNewGraph(op.is_little_endian()),
        op.element_type);
  }
  OpIndex AssembleOutputGraphLoadStackArgument(const LoadStackArgumentOp& op) {
    return Asm().ReduceLoadStackArgument(MapToNewGraph(op.base()),
                                         MapToNewGraph(op.index()));
  }
  OpIndex AssembleOutputGraphStoreTypedElement(const StoreTypedElementOp& op) {
    return Asm().ReduceStoreTypedElement(
        MapToNewGraph(op.buffer()), MapToNewGraph(op.base()),
        MapToNewGraph(op.external()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.array_type);
  }
  OpIndex AssembleOutputGraphStoreDataViewElement(
      const StoreDataViewElementOp& op) {
    return Asm().ReduceStoreDataViewElement(
        MapToNewGraph(op.object()), MapToNewGraph(op.storage()),
        MapToNewGraph(op.index()), MapToNewGraph(op.value()),
        MapToNewGraph(op.is_little_endian()), op.element_type);
  }
  OpIndex AssembleOutputGraphTransitionAndStoreArrayElement(
      const TransitionAndStoreArrayElementOp& op) {
    return Asm().ReduceTransitionAndStoreArrayElement(
        MapToNewGraph(op.array()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.kind, op.fast_map, op.double_map);
  }
  OpIndex AssembleOutputGraphCompareMaps(const CompareMapsOp& op) {
    return Asm().ReduceCompareMaps(MapToNewGraph(op.heap_object()), op.maps);
  }
  OpIndex AssembleOutputGraphCheckMaps(const CheckMapsOp& op) {
    return Asm().ReduceCheckMaps(MapToNewGraph(op.heap_object()),
                                 MapToNewGraph(op.frame_state()), op.maps,
                                 op.flags, op.feedback);
  }
  OpIndex AssembleOutputGraphAssumeMap(const AssumeMapOp& op) {
    return Asm().ReduceAssumeMap(MapToNewGraph(op.heap_object()), op.maps);
  }
  OpIndex AssembleOutputGraphCheckedClosure(const CheckedClosureOp& op) {
    return Asm().ReduceCheckedClosure(MapToNewGraph(op.input()),
                                      MapToNewGraph(op.frame_state()),
                                      op.feedback_cell);
  }
  OpIndex AssembleOutputGraphCheckEqualsInternalizedString(
      const CheckEqualsInternalizedStringOp& op) {
    return Asm().ReduceCheckEqualsInternalizedString(
        MapToNewGraph(op.expected()), MapToNewGraph(op.value()),
        MapToNewGraph(op.frame_state()));
  }
  OpIndex AssembleOutputGraphLoadMessage(const LoadMessageOp& op) {
    return Asm().ReduceLoadMessage(MapToNewGraph(op.offset()));
  }
  OpIndex AssembleOutputGraphStoreMessage(const StoreMessageOp& op) {
    return Asm().ReduceStoreMessage(MapToNewGraph(op.offset()),
                                    MapToNewGraph(op.object()));
  }
  OpIndex AssembleOutputGraphSameValue(const SameValueOp& op) {
    return Asm().ReduceSameValue(MapToNewGraph(op.left()),
                                 MapToNewGraph(op.right()), op.mode);
  }
  OpIndex AssembleOutputGraphFloat64SameValue(const Float64SameValueOp& op) {
    return Asm().ReduceFloat64SameValue(MapToNewGraph(op.left()),
                                        MapToNewGraph(op.right()));
  }
  OpIndex AssembleOutputGraphFastApiCall(const FastApiCallOp& op) {
    auto arguments = MapToNewGraph<8>(op.arguments());
    return Asm().ReduceFastApiCall(MapToNewGraph(op.data_argument()),
                                   base::VectorOf(arguments), op.parameters);
  }
  OpIndex AssembleOutputGraphRuntimeAbort(const RuntimeAbortOp& op) {
    return Asm().ReduceRuntimeAbort(op.reason);
  }
  OpIndex AssembleOutputGraphEnsureWritableFastElements(
      const EnsureWritableFastElementsOp& op) {
    return Asm().ReduceEnsureWritableFastElements(MapToNewGraph(op.object()),
                                                  MapToNewGraph(op.elements()));
  }
  OpIndex AssembleOutputGraphMaybeGrowFastElements(
      const MaybeGrowFastElementsOp& op) {
    return Asm().ReduceMaybeGrowFastElements(
        MapToNewGraph(op.object()), MapToNewGraph(op.elements()),
        MapToNewGraph(op.index()), MapToNewGraph(op.elements_length()),
        MapToNewGraph(op.frame_state()), op.mode, op.feedback);
  }
  OpIndex AssembleOutputGraphTransitionElementsKind(
      const TransitionElementsKindOp& op) {
    return Asm().ReduceTransitionElementsKind(MapToNewGraph(op.object()),
                                              op.transition);
  }
  OpIndex AssembleOutputGraphFindOrderedHashEntry(
      const FindOrderedHashEntryOp& op) {
    return Asm().ReduceFindOrderedHashEntry(MapToNewGraph(op.data_structure()),
                                            MapToNewGraph(op.key()), op.kind);
  }
  OpIndex AssembleOutputGraphSpeculativeNumberBinop(
      const SpeculativeNumberBinopOp& op) {
    return Asm().ReduceSpeculativeNumberBinop(
        MapToNewGraph(op.left()), MapToNewGraph(op.right()),
        MapToNewGraph(op.frame_state()), op.kind);
  }
  OpIndex AssembleOutputGraphWord32PairBinop(const Word32PairBinopOp& op) {
    return Asm().ReduceWord32PairBinop(
        MapToNewGraph(op.left_low()), MapToNewGraph(op.left_high()),
        MapToNewGraph(op.right_low()), MapToNewGraph(op.right_high()), op.kind);
  }

  OpIndex AssembleOutputGraphComment(const CommentOp& op) {
    return Asm().ReduceComment(op.message);
  }

#ifdef V8_ENABLE_WEBASSEMBLY
  OpIndex AssembleOutputGraphGlobalGet(const GlobalGetOp& op) {
    return Asm().ReduceGlobalGet(MapToNewGraph(op.instance()), op.global);
  }

  OpIndex AssembleOutputGraphGlobalSet(const GlobalSetOp& op) {
    return Asm().ReduceGlobalSet(MapToNewGraph(op.instance()),
                                 MapToNewGraph(op.value()), op.global);
  }

  OpIndex AssembleOutputGraphNull(const NullOp& op) {
    return Asm().ReduceNull(op.type);
  }

  OpIndex AssembleOutputGraphIsNull(const IsNullOp& op) {
    return Asm().ReduceIsNull(MapToNewGraph(op.object()), op.type);
  }

  OpIndex AssembleOutputGraphAssertNotNull(const AssertNotNullOp& op) {
    return Asm().ReduceAssertNotNull(MapToNewGraph(op.object()), op.type,
                                     op.trap_id);
  }

  OpIndex AssembleOutputGraphRttCanon(const RttCanonOp& op) {
    return Asm().ReduceRttCanon(MapToNewGraph(op.rtts()), op.type_index);
  }

  OpIndex AssembleOutputGraphWasmTypeCheck(const WasmTypeCheckOp& op) {
    return Asm().ReduceWasmTypeCheck(MapToNewGraph(op.object()),
                                     MapToNewGraphIfValid(op.rtt()), op.config);
  }

  OpIndex AssembleOutputGraphWasmTypeCast(const WasmTypeCastOp& op) {
    return Asm().ReduceWasmTypeCast(MapToNewGraph(op.object()),
                                    MapToNewGraphIfValid(op.rtt()), op.config);
  }

  OpIndex AssembleOutputGraphAnyConvertExtern(const AnyConvertExternOp& op) {
    return Asm().ReduceAnyConvertExtern(MapToNewGraph(op.object()));
  }

  OpIndex AssembleOutputGraphExternConvertAny(const ExternConvertAnyOp& op) {
    return Asm().ReduceExternConvertAny(MapToNewGraph(op.object()));
  }

  OpIndex AssembleOutputGraphWasmTypeAnnotation(
      const WasmTypeAnnotationOp& op) {
    return Asm().ReduceWasmTypeAnnotation(MapToNewGraph(op.value()), op.type);
  }

  OpIndex AssembleOutputGraphStructGet(const StructGetOp& op) {
    return Asm().ReduceStructGet(MapToNewGraph(op.object()), op.type,
                                 op.type_index, op.field_index, op.is_signed,
                                 op.null_check);
  }

  OpIndex AssembleOutputGraphStructSet(const StructSetOp& op) {
    return Asm().ReduceStructSet(MapToNewGraph(op.object()),
                                 MapToNewGraph(op.value()), op.type,
                                 op.type_index, op.field_index, op.null_check);
  }

  OpIndex AssembleOutputGraphArrayGet(const ArrayGetOp& op) {
    return Asm().ReduceArrayGet(MapToNewGraph(op.array()),
                                MapToNewGraph(op.index()), op.array_type,
                                op.is_signed);
  }

  OpIndex AssembleOutputGraphArraySet(const ArraySetOp& op) {
    return Asm().ReduceArraySet(MapToNewGraph(op.array()),
                                MapToNewGraph(op.index()),
                                MapToNewGraph(op.value()), op.element_type);
  }

  OpIndex AssembleOutputGraphArrayLength(const ArrayLengthOp& op) {
    return Asm().ReduceArrayLength(MapToNewGraph(op.array()), op.null_check);
  }

  OpIndex AssembleOutputGraphWasmAllocateArray(const WasmAllocateArrayOp& op) {
    return Asm().ReduceWasmAllocateArray(
        MapToNewGraph(op.rtt()), MapToNewGraph(op.length()), op.array_type);
  }

  OpIndex AssembleOutputGraphWasmAllocateStruct(
      const WasmAllocateStructOp& op) {
    return Asm().ReduceWasmAllocateStruct(MapToNewGraph(op.rtt()),
                                          op.struct_type);
  }

  OpIndex AssembleOutputGraphWasmRefFunc(const WasmRefFuncOp& op) {
    return Asm().ReduceWasmRefFunc(MapToNewGraph(op.instance()),
                                   op.function_index);
  }

  OpIndex AssembleOutputGraphStringAsWtf16(const StringAsWtf16Op& op) {
    return Asm().ReduceStringAsWtf16(MapToNewGraph(op.string()));
  }

  OpIndex AssembleOutputGraphStringPrepareForGetCodeUnit(
      const StringPrepareForGetCodeUnitOp& op) {
    return Asm().ReduceStringPrepareForGetCodeUnit(MapToNewGraph(op.string()));
  }

  OpIndex AssembleOutputGraphSimd128Constant(const Simd128ConstantOp& op) {
    return Asm().ReduceSimd128Constant(op.value);
  }

  OpIndex AssembleOutputGraphSimd128Binop(const Simd128BinopOp& op) {
    return Asm().ReduceSimd128Binop(MapToNewGraph(op.left()),
                                    MapToNewGraph(op.right()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Unary(const Simd128UnaryOp& op) {
    return Asm().ReduceSimd128Unary(MapToNewGraph(op.input()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Shift(const Simd128ShiftOp& op) {
    return Asm().ReduceSimd128Shift(MapToNewGraph(op.input()),
                                    MapToNewGraph(op.shift()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Test(const Simd128TestOp& op) {
    return Asm().ReduceSimd128Test(MapToNewGraph(op.input()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Splat(const Simd128SplatOp& op) {
    return Asm().ReduceSimd128Splat(MapToNewGraph(op.input()), op.kind);
  }

  OpIndex AssembleOutputGraphSimd128Ternary(const Simd128TernaryOp& op) {
    return Asm().ReduceSimd128Ternary(MapToNewGraph(op.first()),
                                      MapToNewGraph(op.second()),
                                      MapToNewGraph(op.third()), op.kind);
  }
  OpIndex AssembleOutputGraphSimd128ExtractLane(
      const Simd128ExtractLaneOp& op) {
    return Asm().ReduceSimd128ExtractLane(MapToNewGraph(op.input()), op.kind,
                                          op.lane);
  }
  OpIndex AssembleOutputGraphSimd128ReplaceLane(
      const Simd128ReplaceLaneOp& op) {
    return Asm().ReduceSimd128ReplaceLane(MapToNewGraph(op.into()),
                                          MapToNewGraph(op.new_lane()), op.kind,
                                          op.lane);
  }
  OpIndex AssembleOutputGraphSimd128LaneMemory(const Simd128LaneMemoryOp& op) {
    return Asm().ReduceSimd128LaneMemory(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()),
        MapToNewGraph(op.value()), op.mode, op.kind, op.lane_kind, op.lane,
        op.offset);
  }
  OpIndex AssembleOutputGraphSimd128LoadTransform(
      const Simd128LoadTransformOp& op) {
    return Asm().ReduceSimd128LoadTransform(
        MapToNewGraph(op.base()), MapToNewGraph(op.index()), op.load_kind,
        op.transform_kind, op.offset);
  }
  OpIndex AssembleOutputGraphSimd128Shuffle(const Simd128ShuffleOp& op) {
    return Asm().ReduceSimd128Shuffle(MapToNewGraph(op.left()),
                                      MapToNewGraph(op.right()), op.shuffle);
  }
  OpIndex AssembleOutputGraphLoadStackPointer(const LoadStackPointerOp& op) {
    return Asm().ReduceLoadStackPointer();
  }
  OpIndex AssembleOutputGraphSetStackPointer(const SetStackPointerOp& op) {
    return Asm().ReduceSetStackPointer(MapToNewGraph(op.value()), op.fp_scope);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  void CreateOldToNewMapping(OpIndex old_index, OpIndex new_index) {
    DCHECK(old_index.valid());
    DCHECK(Asm().input_graph().BelongsToThisGraph(old_index));
    DCHECK_IMPLIES(new_index.valid(),
                   Asm().output_graph().BelongsToThisGraph(new_index));
    if constexpr (reducer_list_contains<typename Next::ReducerList,
                                        VariableReducer>::value) {
      if (current_block_needs_variables_) {
        MaybeVariable var = GetVariableFor(old_index);
        if (!var.has_value()) {
          MaybeRegisterRepresentation rep =
              Asm().input_graph().Get(old_index).outputs_rep().size() == 1
                  ? static_cast<const MaybeRegisterRepresentation&>(
                        Asm().input_graph().Get(old_index).outputs_rep()[0])
                  : MaybeRegisterRepresentation::None();
          var = Asm().NewLoopInvariantVariable(rep);
          SetVariableFor(old_index, *var);
        }
        Asm().SetVariable(*var, new_index);
        return;
      }
    } else {
      DCHECK(!current_block_needs_variables_);
    }
    DCHECK(!op_mapping_[old_index].valid());
    op_mapping_[old_index] = new_index;
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
    Block* output_graph_loop = MapToNewGraph(input_graph_loop);
    DCHECK(output_graph_loop->IsLoop());
    for (const Operation& op : Asm().input_graph().operations(
             input_graph_loop->begin(), input_graph_loop->end())) {
      if (auto* input_phi = op.TryCast<PhiOp>()) {
        OpIndex phi_index =
            MapToNewGraph<true>(Asm().input_graph().Index(*input_phi));
        if (!phi_index.valid() || !output_graph_loop->Contains(phi_index)) {
          // Unused phis are skipped, so they are not be mapped to anything in
          // the new graph. If the phi is reduced to an operation from a
          // different block, then there is no loop phi in the current loop
          // header to take care of.
          continue;
        }
        Asm().FixLoopPhi(*input_phi, phi_index, output_graph_loop);
      }
    }
  }

  Graph& input_graph_;

  const Block* current_input_block_;

  // Mappings from old OpIndices to new OpIndices.
  FixedOpIndexSidetable<OpIndex> op_mapping_;

  // Mappings from old blocks to new blocks.
  FixedBlockSidetable<Block*> block_mapping_;

  // {current_block_needs_variables_} is set to true if the current block should
  // use Variables to map old to new OpIndex rather than just {op_mapping}. This
  // is typically the case when the block has been cloned.
  bool current_block_needs_variables_ = false;

  // When {turn_loop_without_backedge_into_merge_} is true (the default), when
  // processing an input block that ended with a loop backedge but doesn't
  // anymore, the loop header is turned into a regular merge. This can be turned
  // off when unrolling a loop for instance.
  bool turn_loop_without_backedge_into_merge_ = true;

  // Set of Blocks for which Variables should be used rather than
  // {op_mapping}.
  BitVector blocks_needing_variables_;

  // Mapping from old OpIndex to Variables.
  FixedOpIndexSidetable<MaybeVariable> old_opindex_to_variables;
};

template <template <class> class... Reducers>
class TSAssembler;

template <template <class> class... Reducers>
class CopyingPhaseImpl {
 public:
  static void Run(Graph& input_graph, Zone* phase_zone,
                  bool trace_reductions = false) {
    TSAssembler<GraphVisitor, Reducers...> phase(
        input_graph, input_graph.GetOrCreateCompanion(), phase_zone);
#ifdef DEBUG
    if (trace_reductions) {
      phase.template VisitGraph<true>();
    } else {
      phase.template VisitGraph<false>();
    }
#else
    phase.template VisitGraph<false>();
#endif  // DEBUG
  }
};

template <template <typename> typename... Reducers>
class CopyingPhase {
 public:
  static void Run(Zone* phase_zone) {
    PipelineData& data = PipelineData::Get();
    Graph& input_graph = data.graph();
    CopyingPhaseImpl<Reducers...>::Run(
        input_graph, phase_zone, data.info()->turboshaft_trace_reduction());
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_COPYING_PHASE_H_
