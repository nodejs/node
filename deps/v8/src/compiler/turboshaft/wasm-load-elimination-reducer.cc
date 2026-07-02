// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-load-elimination-reducer.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/analyzer-iterator.h"
#include "src/compiler/turboshaft/loop-finder.h"
#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

void WasmLoadEliminationAnalyzer::Run() {
  LoopFinder loop_finder(phase_zone_, &graph_, LoopFinder::Config{});
  AnalyzerIterator iterator(phase_zone_, graph_, loop_finder);

  bool compute_start_snapshot = true;
  while (iterator.HasNext()) {
    const Block* block = iterator.Next();

    ProcessBlock(*block, compute_start_snapshot);
    compute_start_snapshot = true;

    // Consider re-processing for loops.
    if (const GotoOp* last = block->LastOperation(graph_).TryCast<GotoOp>()) {
      if (last->destination->IsLoop() &&
          last->destination->LastPredecessor() == block) {
        const Block* loop_header = last->destination;
        // {block} is the backedge of a loop. We recompute the loop header's
        // initial snapshots, and if they differ from its original snapshot,
        // then we revisit the loop.
        if (BeginBlock<true>(loop_header)) {
          // We set the snapshot of the loop's 1st predecessor to the newly
          // computed snapshot. It's not quite correct, but this predecessor
          // is guaranteed to end with a Goto, and we are now visiting the
          // loop, which means that we don't really care about this
          // predecessor anymore.
          // The reason for saving this snapshot is to prevent infinite
          // looping, since the next time we reach this point, the backedge
          // snapshot could still invalidate things from the forward edge
          // snapshot. By restricting the forward edge snapshot, we prevent
          // this.
          const Block* loop_1st_pred =
              loop_header->LastPredecessor()->NeighboringPredecessor();
          FinishBlock(loop_1st_pred);
          // And we start a new fresh snapshot from this predecessor.
          auto pred_snapshots =
              block_to_snapshot_mapping_[loop_1st_pred->index()];
          non_aliasing_objects_.StartNewSnapshot(
              pred_snapshots->alias_snapshot);
          memory_.StartNewSnapshot(pred_snapshots->memory_snapshot);

          iterator.MarkLoopForRevisit();
          compute_start_snapshot = false;
        } else {
          SealAndDiscard();
        }
      }
    }
  }
}

void WasmLoadEliminationAnalyzer::ProcessBlock(const Block& block,
                                               bool compute_start_snapshot) {
  if (compute_start_snapshot) {
    BeginBlock(&block);
  }
  if (block.IsLoop() && BackedgeHasSnapshot(block)) {
    // Update the associated snapshot for the forward edge with the merged
    // snapshot information from the forward- and backward edge.
    // This will make sure that when evaluating whether a loop needs to be
    // revisited, the inner loop compares the merged state with the backedge
    // preventing us from exponential revisits for loops where the backedge
    // invalidates loads which are eliminatable on the forward edge.
    StoreLoopSnapshotInForwardPredecessor(block);
  }

  for (OpIndex op_idx : graph_.OperationIndices(block)) {
    Operation& op = graph_.Get(op_idx);
    if (ShouldSkipOptimizationStep()) continue;
    if (ShouldSkipOperation(op)) continue;
    switch (op.opcode) {
      case Opcode::kStructGet:
        ProcessStructGet(op_idx, op.Cast<StructGetOp>());
        break;
      case Opcode::kStructSet:
        ProcessStructSet(op_idx, op.Cast<StructSetOp>());
        break;
      case Opcode::kStructAtomicRMW:
        ProcessAtomicRMW(op_idx, op.Cast<StructAtomicRMWOp>());
        break;
      case Opcode::kArrayAtomicRMW:
        // Nothing to be done. We don't eliminate loads on wasm arrays at all.
        break;
      case Opcode::kArrayLength:
        ProcessArrayLength(op_idx, op.Cast<ArrayLengthOp>());
        break;
      case Opcode::kWasmAllocateArray:
        ProcessWasmAllocateArray(op_idx, op.Cast<WasmAllocateArrayOp>());
        break;
      case Opcode::kStringAsWtf16:
        ProcessStringAsWtf16(op_idx, op.Cast<StringAsWtf16Op>());
        break;
      case Opcode::kStringPrepareForGetCodeUnit:
        ProcessStringPrepareForGetCodeUnit(
            op_idx, op.Cast<StringPrepareForGetCodeUnitOp>());
        break;
      case Opcode::kAnyConvertExtern:
        ProcessAnyConvertExtern(op_idx, op.Cast<AnyConvertExternOp>());
        break;
      case Opcode::kAssertNotNull:
        // TODO(14108): We'll probably want to handle WasmTypeCast as
        // a "load-like" instruction too, to eliminate repeated casts.
        ProcessAssertNotNull(op_idx, op.Cast<AssertNotNullOp>());
        break;
      case Opcode::kArraySet:
        break;
      case Opcode::kAllocate:
        // Create new non-alias.
        ProcessAllocate(op_idx, op.Cast<AllocateOp>());
        break;
      case Opcode::kCall:
        // Invalidate state (+ maybe invalidate aliases).
        ProcessCall(op_idx, op.Cast<CallOp>());
        break;
      case Opcode::kPhi:
        // Invalidate aliases.
        ProcessPhi(op_idx, op.Cast<PhiOp>());
        break;
      case Opcode::kLoad:
        // Atomic loads have the "can_write" bit set, because they make
        // writes on other threads visible. At any rate, we have to
        // explicitly skip them here.
      case Opcode::kStore:
        // We rely on having no raw "Store" operations operating on Wasm
        // objects at this point in the pipeline.
        // TODO(jkummerow): Is there any way to DCHECK that?
      case Opcode::kMemoryCopy:
      case Opcode::kMemoryFill:
        // These should never operate on GC objects.
      case Opcode::kAssumeMap:
      case Opcode::kCatchBlockBegin:
      case Opcode::kRetain:
      case Opcode::kDidntThrow:
      case Opcode::kCheckException:
      case Opcode::kAtomicRMW:
      case Opcode::kAtomicWord32Pair:
      case Opcode::kMemoryBarrier:
      case Opcode::kJSStackCheck:
      case Opcode::kWasmStackCheck:
      case Opcode::kSimd128LaneMemory:
      case Opcode::kGlobalSet:
      case Opcode::kWasmIncCoverageCounter:
      case Opcode::kParameter:
      case Opcode::kSetStackPointer:
      case Opcode::kWasmFXArgBuffer:
        // We explicitly break for those operations that have can_write effects
        // but don't actually write, or cannot interfere with load elimination.
        break;

      case Opcode::kWordBinop:
        // A WordBinop should never invalidate aliases (since the only time when
        // it should take a non-aliasing object as input is for Smi checks).
        DcheckWordBinop(op_idx, op.Cast<WordBinopOp>());
        break;

      case Opcode::kFrameState:
      case Opcode::kDeoptimizeIf:
      case Opcode::kComparison:
      case Opcode::kTrapIf:
      case Opcode::kWasmTrap:
        // We explicitly break for these opcodes so that we don't call
        // InvalidateAllNonAliasingInputs on their inputs, since they don't
        // really create aliases. (and also, they don't write so it's
        // fine to break)
        DCHECK(!op.Effects().can_write());
        break;

      case Opcode::kDeoptimize:
      case Opcode::kReturn:
        // We explicitly break for these opcodes so that we don't call
        // InvalidateAllNonAliasingInputs on their inputs, since they are block
        // terminators without successors, meaning that it's not useful for the
        // rest of the analysis to invalidate anything here.
        DCHECK(op.IsBlockTerminator() && SuccessorBlocks(op).empty());
        break;

      case Opcode::kArrayGet:
        if (op.Effects().can_write()) {
          DCHECK(op.Cast<ArrayGetOp>().memory_order.has_value());
          // For now invalidate "everything", similar to the effect of a call.
          // TODO(manoskouk): What's the desired behavior here?
          InvalidateAllNonAliasingInputs(op);
          memory_.InvalidateMaybeAliasing();
        }
        break;

      default:
        // Operations that `can_write` should invalidate the state. In the Wasm
        // pipeline, all such Wasm-only operations should be already handled
        // explicitly above, so the CHECK ensures that we didn't forget any. In
        // the JS pipeline, we run this reducer as part of Wasm-in-JS inlining
        // and before `MachineLoweringReducer`. Thus, there are several more JS
        // and Simplified operations that we didn't handle above and that shall
        // invalidate the state.
        if (memory_.data_->pipeline_kind() == TurboshaftPipelineKind::kWasm) {
          CHECK(!op.Effects().can_write());
        } else {
          DCHECK_EQ(memory_.data_->pipeline_kind(),
                    TurboshaftPipelineKind::kJS);
          if (op.Effects().can_write()) {
            memory_.InvalidateMaybeAliasing();
          }
        }

        // Even if the operation doesn't write, it could create an alias to its
        // input by returning it. This happens for instance in Phis and in
        // Change (although ChangeOp is already handled earlier by calling
        // ProcessChange). We are conservative here by calling
        // InvalidateAllNonAliasingInputs for all operations even though only
        // few can actually create aliases to fresh allocations, the reason
        // being that missing such a case would be a security issue, and it
        // should be rare for fresh allocations to be used outside of
        // Call/Store/Load/Change anyways.
        InvalidateAllNonAliasingInputs(op);

        break;
    }
  }

  FinishBlock(&block);
}

namespace {
// Returns true if replacing a load with a RegisterRepresentation
// {expected_reg_rep} and size {in_memory_size} with an
// operation with RegisterRepresentation {actual} is valid. For instance,
// replacing an operation that returns a Float64 by one that returns a Word64 is
// not valid. Similarly, replacing a Tagged with an untagged value is probably
// not valid because of the GC.
bool RepIsCompatible(RegisterRepresentation actual,
                     RegisterRepresentation expected_reg_repr,
                     uint8_t in_memory_size) {
  if (in_memory_size !=
      MemoryRepresentation::FromRegisterRepresentation(actual, true)
          .SizeInBytes()) {
    // The replacement was truncated when being stored or should be truncated
    // (or sign-extended) during the load. Since we don't have enough
    // truncations operators in Turboshaft (eg, we don't have Int32 to Int8
    // truncation), we just prevent load elimination in this case.

    // TODO(jkummerow): support eliminating repeated loads of the same i8/i16
    // field.
    return false;
  }

  return expected_reg_repr == actual;
}
}  // namespace

void WasmLoadEliminationAnalyzer::ProcessStructGet(OpIndex op_idx,
                                                   const StructGetOp& get) {
  // TODO(mliedtke): struct.atomic.get also participates in load-elimination by
  // providing values that can be used to load-eliminate struct.get operations
  // for the same field. Is this the desired behavior?
  OpIndex existing = memory_.Find(get);
  if (existing.valid()) {
    const Operation& replacement = graph_.Get(existing);
    DCHECK_EQ(replacement.outputs_rep().size(), 1);
    DCHECK_EQ(get.outputs_rep().size(), 1);
    if (get.is_get_desc() ||
        RepIsCompatible(replacement.outputs_rep()[0], get.outputs_rep()[0],
                        get.type->field(get.field_index).value_kind_size())) {
      replacements_[op_idx] = existing;
      return;
    }
  }
  replacements_[op_idx] = OpIndex::Invalid();
  memory_.Insert(get, op_idx);
}

void WasmLoadEliminationAnalyzer::ProcessStructSet(OpIndex op_idx,
                                                   const StructSetOp& set) {
  if (memory_.HasValueWithIncorrectMutability(set)) {
    // This struct.set is unreachable. We don't have a good way to annotate
    // it as such, so we use "replace with itself" as a sentinel.
    // TODO(jkummerow): Check how often this case is triggered in practice.
    replacements_[op_idx] = op_idx;
    return;
  }

  memory_.Invalidate(set);
  memory_.Insert(set);

  // Updating aliases if the value stored was known as non-aliasing.
  OpIndex value = set.value();
  if (non_aliasing_objects_.HasKeyFor(value)) {
    non_aliasing_objects_.Set(value, false);
  }
}

void WasmLoadEliminationAnalyzer::ProcessAtomicRMW(
    OpIndex op_idx, const StructAtomicRMWOp& op) {
  memory_.Invalidate(op);
}

void WasmLoadEliminationAnalyzer::ProcessArrayLength(
    OpIndex op_idx, const ArrayLengthOp& length) {
  static constexpr int offset = wle::kArrayLengthFieldIndex;
  OpIndex existing = memory_.FindLoadLike(length.array(), offset);
  if (existing.valid()) {
#if DEBUG
    const Operation& replacement = graph_.Get(existing);
    DCHECK_EQ(replacement.outputs_rep().size(), 1);
    DCHECK_EQ(length.outputs_rep().size(), 1);
    DCHECK_EQ(replacement.outputs_rep()[0], length.outputs_rep()[0]);
#endif
    replacements_[op_idx] = existing;
    return;
  }
  replacements_[op_idx] = OpIndex::Invalid();
  memory_.InsertLoadLike(length.array(), offset, op_idx);
}

void WasmLoadEliminationAnalyzer::ProcessWasmAllocateArray(
    OpIndex op_idx, const WasmAllocateArrayOp& alloc) {
  non_aliasing_objects_.Set(op_idx, true);
  static constexpr int offset = wle::kArrayLengthFieldIndex;
  memory_.InsertLoadLike(op_idx, offset, alloc.length());
}

void WasmLoadEliminationAnalyzer::ProcessStringAsWtf16(
    OpIndex op_idx, const StringAsWtf16Op& op) {
  static constexpr int offset = wle::kStringAsWtf16Index;
  OpIndex existing = memory_.FindLoadLike(op.string(), offset);
  if (existing.valid()) {
    DCHECK_EQ(Opcode::kStringAsWtf16, graph_.Get(existing).opcode);
    replacements_[op_idx] = existing;
    return;
  }
  replacements_[op_idx] = OpIndex::Invalid();
  memory_.InsertLoadLike(op.string(), offset, op_idx);
}

void WasmLoadEliminationAnalyzer::ProcessStringPrepareForGetCodeUnit(
    OpIndex op_idx, const StringPrepareForGetCodeUnitOp& prep) {
  static constexpr int offset = wle::kStringPrepareForGetCodeunitIndex;
  OpIndex existing = memory_.FindLoadLike(prep.string(), offset);
  if (existing.valid()) {
    DCHECK_EQ(Opcode::kStringPrepareForGetCodeUnit,
              graph_.Get(existing).opcode);
    replacements_[op_idx] = existing;
    return;
  }
  replacements_[op_idx] = OpIndex::Invalid();
  memory_.InsertLoadLike(prep.string(), offset, op_idx);
}

void WasmLoadEliminationAnalyzer::ProcessAnyConvertExtern(
    OpIndex op_idx, const AnyConvertExternOp& convert) {
  static constexpr int offset = wle::kAnyConvertExternIndex;
  OpIndex existing = memory_.FindLoadLike(convert.object(), offset);
  if (existing.valid()) {
    DCHECK_EQ(Opcode::kAnyConvertExtern, graph_.Get(existing).opcode);
    replacements_[op_idx] = existing;
    return;
  }
  replacements_[op_idx] = OpIndex::Invalid();
  memory_.InsertLoadLike(convert.object(), offset, op_idx);
}

void WasmLoadEliminationAnalyzer::ProcessAssertNotNull(
    OpIndex op_idx, const AssertNotNullOp& assert) {
  static constexpr int offset = wle::kAssertNotNullIndex;
  OpIndex existing = memory_.FindLoadLike(assert.object(), offset);
  if (existing.valid()) {
    DCHECK_EQ(Opcode::kAssertNotNull, graph_.Get(existing).opcode);
    replacements_[op_idx] = existing;
    return;
  }
  replacements_[op_idx] = OpIndex::Invalid();
  memory_.InsertLoadLike(assert.object(), offset, op_idx);
}

// Since we only loosely keep track of what can or can't alias, we assume that
// anything that was guaranteed to not alias with anything (because it's in
// {non_aliasing_objects_}) can alias with anything when coming back from the
// call if it was an argument of the call.
void WasmLoadEliminationAnalyzer::ProcessCall(OpIndex op_idx,
                                              const CallOp& op) {
  // Some builtins do not create aliases and do not invalidate existing
  // memory, and some even return fresh objects. For such cases, we don't
  // invalidate the state, and record the non-alias if any.
  if (!op.Effects().can_write()) {
    return;
  }
  // TODO(jkummerow): Add special handling to builtins that are known not to
  // have relevant side effects. Alternatively, specify their effects to not
  // include `CanWriteMemory()`.
#if 0
  if (auto builtin_id = TryGetBuiltinId(
          graph_.Get(op.callee()).TryCast<ConstantOp>(), broker_)) {
    switch (*builtin_id) {
      case Builtin::kExample:
        // This builtin touches no Wasm objects, and calls no other functions.
        return;
      default:
        break;
    }
  }
#endif
  // Not a builtin call, or not a builtin that we know doesn't invalidate
  // memory.

  InvalidateAllNonAliasingInputs(op);

  // The call could modify arbitrary memory, so we invalidate every
  // potentially-aliasing object.
  memory_.InvalidateMaybeAliasing();
}

void WasmLoadEliminationAnalyzer::InvalidateAllNonAliasingInputs(
    const Operation& op) {
  for (OpIndex input : op.inputs()) {
    InvalidateIfAlias(input);
  }
}

void WasmLoadEliminationAnalyzer::InvalidateIfAlias(OpIndex op_idx) {
  if (auto key = non_aliasing_objects_.TryGetKeyFor(op_idx);
      key.has_value() && non_aliasing_objects_.Get(*key)) {
    // An known non-aliasing object was passed as input to the Call; the Call
    // could create aliases, so we have to consider going forward that this
    // object could actually have aliases.
    non_aliasing_objects_.Set(*key, false);
  }
}

// The only time an Allocate should flow into a WordBinop is for Smi checks
// (which, by the way, should be removed by MachineOptimizationReducer (since
// Allocate never returns a Smi), but there is no guarantee that this happens
// before load elimination). So, there is no need to invalidate non-aliases, and
// we just DCHECK in this function that indeed, nothing else than a Smi check
// happens on non-aliasing objects.
void WasmLoadEliminationAnalyzer::DcheckWordBinop(OpIndex op_idx,
                                                  const WordBinopOp& binop) {
#ifdef DEBUG
  auto check = [&](V<Word> left, V<Word> right) {
    if (auto key = non_aliasing_objects_.TryGetKeyFor(left);
        key.has_value() && non_aliasing_objects_.Get(*key)) {
      int64_t cst;
      DCHECK_EQ(binop.kind, WordBinopOp::Kind::kBitwiseAnd);
      DCHECK(OperationMatcher(graph_).MatchSignedIntegralConstant(right, &cst));
      DCHECK_EQ(cst, kSmiTagMask);
    }
  };
  check(binop.left(), binop.right());
  check(binop.right(), binop.left());
#endif
}

void WasmLoadEliminationAnalyzer::ProcessAllocate(OpIndex op_idx,
                                                  const AllocateOp&) {
  // In particular, this handles {struct.new}.
  non_aliasing_objects_.Set(op_idx, true);
}

OpIndex WasmLoadEliminationAnalyzer::MaybeReplacePhi(const PhiOp& phi) {
  base::Vector<const OpIndex> inputs = phi.inputs();
  // This copies some of the functionality of {RequiredOptimizationReducer}:
  // Phis whose inputs are all the same value can be replaced by that value.
  // We need to have this logic here because interleaving it with other cases
  // of load elimination can unlock further optimizations: simplifying Phis
  // can allow elimination of more loads, which can then allow simplification
  // of even more Phis.
  DCHECK_GT(inputs.size(), 0);

  bool same_inputs = true;
  OpIndex first = memory_.ResolveBase(inputs.first());
  for (const OpIndex& input : inputs.SubVectorFrom(1)) {
    if (memory_.ResolveBase(input) != first) {
      same_inputs = false;
      break;
    }
  }
  if (same_inputs) {
    return first;
  }
  return OpIndex::Invalid();
}

void WasmLoadEliminationAnalyzer::ProcessPhi(OpIndex op_idx, const PhiOp& phi) {
  InvalidateAllNonAliasingInputs(phi);
  replacements_[op_idx] = MaybeReplacePhi(phi);
}

void WasmLoadEliminationAnalyzer::FinishBlock(const Block* block) {
  block_to_snapshot_mapping_[block->index()] =
      Snapshot{non_aliasing_objects_.Seal(), memory_.Seal()};
}

void WasmLoadEliminationAnalyzer::SealAndDiscard() {
  non_aliasing_objects_.Seal();
  memory_.Seal();
}

void WasmLoadEliminationAnalyzer::StoreLoopSnapshotInForwardPredecessor(
    const Block& loop_header) {
  auto non_aliasing_snapshot = non_aliasing_objects_.Seal();
  auto memory_snapshot = memory_.Seal();

  block_to_snapshot_mapping_
      [loop_header.LastPredecessor()->NeighboringPredecessor()->index()] =
          Snapshot{non_aliasing_snapshot, memory_snapshot};

  non_aliasing_objects_.StartNewSnapshot(non_aliasing_snapshot);
  memory_.StartNewSnapshot(memory_snapshot);
}

bool WasmLoadEliminationAnalyzer::BackedgeHasSnapshot(
    const Block& loop_header) const {
  DCHECK(loop_header.IsLoop());
  return block_to_snapshot_mapping_[loop_header.LastPredecessor()->index()]
      .has_value();
}

template <bool for_loop_revisit>
bool WasmLoadEliminationAnalyzer::BeginBlock(const Block* block) {
  DCHECK_IMPLIES(
      for_loop_revisit,
      block->IsLoop() &&
          block_to_snapshot_mapping_[block->LastPredecessor()->index()]
              .has_value());

  // Collect the snapshots of all predecessors.
  {
    predecessor_alias_snapshots_.clear();
    predecessor_memory_snapshots_.clear();
    for (const Block* p : block->PredecessorsIterable()) {
      auto pred_snapshots = block_to_snapshot_mapping_[p->index()];
      // When we visit the loop for the first time, the loop header hasn't
      // been visited yet, so we ignore it.
      DCHECK_IMPLIES(!pred_snapshots.has_value(),
                     block->IsLoop() && block->LastPredecessor() == p);
      if (!pred_snapshots.has_value()) {
        DCHECK(!for_loop_revisit);
        continue;
      }
      // Note that the backedge snapshot of an inner loop in kFirstVisit will
      // also be taken into account if we are in the kSecondVisit of an outer
      // loop. The data in the backedge snapshot could be out-dated, but if it
      // is, then it's fine: if the backedge of the outer-loop was more
      // restrictive than its forward incoming edge, then the forward incoming
      // edge of the inner loop should reflect this restriction.
      predecessor_alias_snapshots_.push_back(pred_snapshots->alias_snapshot);
      predecessor_memory_snapshots_.push_back(pred_snapshots->memory_snapshot);
    }
  }

  // Note that predecessors are in reverse order, which means that the backedge
  // is at offset 0.
  constexpr int kBackedgeOffset = 0;
  constexpr int kForwardEdgeOffset = 1;

  bool loop_needs_revisit = false;
  // Start a new snapshot for this block by merging information from
  // predecessors.
  auto merge_aliases = [&](AliasKey key,
                           base::Vector<const bool> predecessors) -> bool {
    if (for_loop_revisit && predecessors[kForwardEdgeOffset] &&
        !predecessors[kBackedgeOffset]) {
      // The backedge doesn't think that {key} is no-alias, but the loop
      // header previously thought it was --> need to revisit.
      loop_needs_revisit = true;
    }
    return base::all_of(predecessors);
  };
  non_aliasing_objects_.StartNewSnapshot(
      base::VectorOf(predecessor_alias_snapshots_), merge_aliases);

  // Merging for {memory_} means setting values to Invalid unless all
  // predecessors have the same value.
  // TODO(dmercadier): we could insert of Phis during the pass to merge existing
  // information. This is a bit hard, because we are currently in an analyzer
  // rather than a reducer. Still, we could "prepare" the insertion now and then
  // really insert them during the Reduce phase of the CopyingPhase.
  auto merge_memory = [&](MemoryKey key,
                          base::Vector<const OpIndex> predecessors) -> OpIndex {
    if (for_loop_revisit && predecessors[kForwardEdgeOffset].valid() &&
        predecessors[kBackedgeOffset] != predecessors[kForwardEdgeOffset]) {
      // {key} had a value in the loop header, but the backedge and the forward
      // edge don't agree on its value, which means that the loop invalidated
      // some memory data, and thus needs to be revisited.
      loop_needs_revisit = true;
    }
    return base::all_equal(predecessors) ? predecessors[0] : OpIndex::Invalid();
  };
  memory_.StartNewSnapshot(base::VectorOf(predecessor_memory_snapshots_),
                           merge_memory);

  if constexpr (for_loop_revisit) {
    phi_replacements_backups_.clear();
    // Back up and clear all existing loop Phi replacements. Clearing is
    // necessary because loop Phis can depend on each other, and are
    // conceptually processed in parallel.
    for (OpIndex op_idx : graph_.OperationIndices(*block)) {
      if (graph_.Get(op_idx).Is<PhiOp>() && replacements_[op_idx].valid()) {
        phi_replacements_backups_.push_back({op_idx, replacements_[op_idx]});
        replacements_[op_idx] = OpIndex::Invalid();
      }
    }
    // Check if any loop Phi replacement would change.
    //
    // Note that we need to update {replacements_} while doing so in order to
    // emulate what the actual loop revisitation will do, so that dependent Phis
    // are properly updated. For instance, if we have
    //
    //     phi1: phi(0, 0)
    //     phi2: phi(0, phi1)
    //
    // The the main loop (re)visitation will update replacements_[phi1], which
    // will allow phi2 to be replaced in turn. If, here, we don't update
    // replacements_, then we will think that phi2 cannot be replaced, but given
    // that its old replacements_ was valid, we'll think that it got
    // invalidated, which will trigger revisiting the loop infinitely.
    //
    // However, it's important to also wipe the {replacements_} once we've
    // determined that we need to revisit the loop. The reason is that looking
    // at Phis multiple times can find more replacements. For instance:
    //
    //     phi1: phi(0, phi2)
    //     phi2: phi(0, 0)
    //
    // Starting with empty {replacements_}, we'll compute Invalid for {phi1}
    // because we don't have a replacement for {phi2}, and then we'll compute
    // `0` for {phi2}. If we then compute replacements again starting with those
    // non-empty replacements_, we will compute `0` for {phi1} as well. So if
    // here (to decide whether we need to revisit the loop or not), we start
    // from empty {replacements_} and we do actually decide to revisit the loop
    // and don't wipe {replacements_}, then ProcessPhi could compute additional
    // replacements, which we won't be able to recompute here at the next
    // revisit check, leading to an infinite loop.
    //
    // We could be more ambitious: whenever a loop Phi is replaced, we could
    // revisit any other loop Phis for which it is an input, until we find no
    // more replacements. That would cost more time, but would yield more
    // optimizations. For example, consider a chain of Phis, which before the
    // loop are all set to o.x, and inside the loop we have:
    //   phi1 = phi2; phi2 = phi3; ...; phiN-1 = phiN; phiN = o.x.
    // Checking them all only once, in order, we'll replace only phiN. Visiting
    // either the entire block N times, or visiting each Phi consumer of a
    // replaced Phi, we could replace all of them.
    for (auto [phi_idx, backup] : phi_replacements_backups_) {
      const PhiOp& phi = graph_.Get(phi_idx).Cast<PhiOp>();
      OpIndex new_replacement = MaybeReplacePhi(phi);
      replacements_[phi_idx] = new_replacement;
      if (new_replacement != backup) {
        loop_needs_revisit = true;
        break;
      }
    }
    if (!loop_needs_revisit) {
      // If we're not going to revisit, restore the replacements we had.
      for (auto [phi_idx, backup] : phi_replacements_backups_) {
        replacements_[phi_idx] = backup;
      }
    } else {
      // Otherwise, clear the {replacements_} (cf explanation above).
      for (auto [phi_idx, backup] : phi_replacements_backups_) {
        replacements_[phi_idx] = OpIndex::Invalid();
      }
    }
  }

  if (block->IsLoop()) return loop_needs_revisit;
  return false;
}

// Explicit template instantiations for BeginBlock
template bool WasmLoadEliminationAnalyzer::BeginBlock<true>(const Block* block);
template bool WasmLoadEliminationAnalyzer::BeginBlock<false>(
    const Block* block);

}  // namespace v8::internal::compiler::turboshaft
