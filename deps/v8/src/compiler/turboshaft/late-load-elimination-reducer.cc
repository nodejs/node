// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/late-load-elimination-reducer.h"

#include "src/compiler/js-heap-broker.h"
#include "src/objects/code-inl.h"

namespace v8::internal::compiler::turboshaft {

void LateLoadEliminationAnalyzer::ProcessBlock(const Block& block,
                                               bool compute_start_snapshot) {
  if (compute_start_snapshot) {
    BeginBlock(&block);
  }

  for (OpIndex op_idx : graph_.OperationIndices(block)) {
    Operation& op = graph_.Get(op_idx);
    if (ShouldSkipOptimizationStep()) continue;
    if (ShouldSkipOperation(op)) continue;
    switch (op.opcode) {
      case Opcode::kLoad:
        // Eliminate load or update state
        ProcessLoad(op_idx, op.Cast<LoadOp>());
        break;
      case Opcode::kStore:
        // Update state (+ maybe invalidate aliases)
        ProcessStore(op_idx, op.Cast<StoreOp>());
        break;
      case Opcode::kAllocate:
        // Create new non-alias
        ProcessAllocate(op_idx, op.Cast<AllocateOp>());
        break;
      case Opcode::kCall:
        // Invalidate state (+ maybe invalidate aliases)
        ProcessCall(op_idx, op.Cast<CallOp>());
        break;
      case Opcode::kPhi:
        // Invalidate aliases
        ProcessPhi(op_idx, op.Cast<PhiOp>());
        break;
      case Opcode::kAssumeMap:
        // Update known maps
        ProcessAssumeMap(op_idx, op.Cast<AssumeMapOp>());
        break;
      case Opcode::kCatchBlockBegin:
      case Opcode::kRetain:
      case Opcode::kDidntThrow:
      case Opcode::kCheckException:
        // We explicitely break for those operations that have can_write effects
        // but don't actually write.
        break;
      default:
        // Operations that `can_write` should invalidate the state. All such
        // operations should be already handled above, which means that we don't
        // need a `if (can_write) { Invalidate(); }` here.
        CHECK(!op.Effects().can_write());
        break;
    }
  }

  FinishBlock(&block);
}

namespace {

// Returns true if replacing a Load with a RegisterReprsentation
// {expected_reg_rep} and MemoryRepresentation of {expected_loaded_repr} with an
// operation with RegisterRepresentation {actual} is valid. For instance,
// replacing a operation that returns a Float64 by one that returns a Word64 is
// not valid. Similarly, replacing a Tagged with an untagged value is probably
// not valid because of the GC.
bool RepIsCompatible(RegisterRepresentation actual,
                     RegisterRepresentation expected_reg_repr,
                     MemoryRepresentation expected_loaded_repr) {
  if (expected_loaded_repr.SizeInBytes() !=
      MemoryRepresentation::FromRegisterRepresentation(actual, true)
          .SizeInBytes()) {
    // The replacement was truncated when being stored or should be truncated
    // (or sign-extended) during the load. Since we don't have enough
    // truncations operators in Turboshaft (eg, we don't have Int32 to Int8
    // truncation), we just prevent load elimination in this case.

    // TODO(dmercadier): add more truncations operators to Turboshaft, and
    // insert the correct truncation when there is a missmatch between
    // {expected_loaded_repr} and {actual}.

    return false;
  }

  return expected_reg_repr == actual;
}

}  // namespace

void LateLoadEliminationAnalyzer::ProcessLoad(OpIndex op_idx,
                                              const LoadOp& load) {
  if (!load.kind.always_canonically_accessed) {
    // We don't optimize Loads/Stores to addresses that could be accessed
    // non-canonically.
    return;
  }

  if (OpIndex existing = memory_.Find(load); existing.valid()) {
    const Operation& replacement = graph_.Get(existing);
    // We need to make sure that {load} and {replacement} have the same output
    // representation. In particular, in unreachable code, it's possible that
    // the two of them have incompatible representations (like one could be
    // Tagged and the other one Float64).
    DCHECK_EQ(replacement.outputs_rep().size(), 1);
    DCHECK_EQ(load.outputs_rep().size(), 1);
    if (RepIsCompatible(replacement.outputs_rep()[0], load.outputs_rep()[0],
                        load.loaded_rep)) {
      replacements_[op_idx] = existing;
      return;
    }
  }
  // Reset the replacement of {op_idx} to Invalid, in case a previous visit of a
  // loop has set it to something else.
  replacements_[op_idx] = OpIndex::Invalid();

  // TODO(dmercadier): if we precisely track maps, then we could know from the
  // map what we are loading in some cases. For instance, if the elements_kind
  // of the map is *_DOUBLE_ELEMENTS, then a load at offset
  // JSObject::kElementsOffset always load a FixedDoubleArray, with map
  // fixed_double_array_map.

  if (const ConstantOp* base = graph_.Get(load.base()).TryCast<ConstantOp>();
      base != nullptr && base->kind == ConstantOp::Kind::kExternal) {
    // External constants can be written by other threads, so we don't
    // load-eliminate them, in order to always reload them.
    return;
  }

  memory_.Insert(load, op_idx);
}

void LateLoadEliminationAnalyzer::ProcessStore(OpIndex op_idx,
                                               const StoreOp& store) {
  if (!store.kind.always_canonically_accessed) {
    // We don't optimize Loads/Stores to addresses that could be accessed
    // non-canonically.
    return;
  }

  OpIndex value = store.value();

  // Updating the known stored values
  memory_.Invalidate(store);
  memory_.Insert(store);

  // Updating aliases if the value stored was known as non-aliasing.
  if (non_aliasing_objects_.HasKeyFor(value)) {
    non_aliasing_objects_.Set(value, false);
  }
}

namespace {

// If {target} is a HeapObject representing a builtin, return that builtin's ID.
base::Optional<Builtin> TryGetBuiltinId(ConstantOp* target,
                                        JSHeapBroker* broker) {
  if (!target) return base::nullopt;
  if (target->kind != ConstantOp::Kind::kHeapObject) return base::nullopt;
  HeapObjectRef ref = MakeRef(broker, target->handle());
  if (ref.IsCode()) {
    CodeRef code = ref.AsCode();
    if (code.object()->is_builtin()) {
      return code.object()->builtin_id();
    }
  }
  return base::nullopt;
}

}  // namespace

// Since we only loosely keep track of what can or can't alias, we assume that
// anything that was guaranteed to not alias with anything (because it's in
// {non_aliasing_objects_}) can alias with anything when coming back from the
// call if it was an argument of the call.
void LateLoadEliminationAnalyzer::ProcessCall(OpIndex op_idx,
                                              const CallOp& op) {
  // Some builtins do not create aliases and do not invalidate existing
  // memory, and some even return fresh objects. For such cases, we don't
  // invalidate the state, and record the non-alias if any.
  if (!op.Effects().can_write()) return;

  if (auto builtin_id = TryGetBuiltinId(
          graph_.Get(op.callee()).TryCast<ConstantOp>(), broker_)) {
    switch (*builtin_id) {
      // TODO(dmercadier): extend this list.
      case Builtin::kCopyFastSmiOrObjectElements:
        // This function just replaces the Elements array of an object.
        // It doesn't invalidate any alias or any other memory than this
        // Elements array.
        memory_.Invalidate(op.arguments()[0], OpIndex::Invalid(),
                           JSObject::kElementsOffset);
        return;
      case Builtin::kCEntry_Return1_ArgvOnStack_NoBuiltinExit: {
        DCHECK_GE(op.input_count, 3);
        if (const ConstantOp* real_callee =
                graph_.Get(op.input(2)).TryCast<ConstantOp>();
            real_callee != nullptr &&
            real_callee->kind == ConstantOp::Kind::kExternal &&
            real_callee->external_reference() ==
                ExternalReference::Create(
                    Runtime::kHandleNoHeapWritesInterrupts)) {
          // This is a stack check that cannot write heap memory.
          return;
        }
        break;
      }
      default:
        break;
    }
  }
  // Not a builtin call, or not a builtin that we know doesn't invalidate
  // memory.

  for (OpIndex input : op.inputs()) {
    InvalidateIfAlias(input);
  }

  // The call could modify arbitrary memory, so we invalidate every
  // potentially-aliasing object.
  memory_.InvalidateMaybeAliasing();
}

void LateLoadEliminationAnalyzer::InvalidateIfAlias(OpIndex op_idx) {
  if (auto key = non_aliasing_objects_.TryGetKeyFor(op_idx);
      key.has_value() && non_aliasing_objects_.Get(*key)) {
    // An known non-aliasing object was passed as input to the Call; the Call
    // could create aliases, so we have to consider going forward that this
    // object could actually have aliases.
    non_aliasing_objects_.Set(*key, false);
  }
}

void LateLoadEliminationAnalyzer::ProcessAllocate(OpIndex op_idx,
                                                  const AllocateOp&) {
  non_aliasing_objects_.Set(op_idx, true);
}

void LateLoadEliminationAnalyzer::ProcessPhi(OpIndex op_idx, const PhiOp& phi) {
  for (OpIndex input : phi.inputs()) {
    if (auto key = non_aliasing_objects_.TryGetKeyFor(input)) {
      non_aliasing_objects_.Set(*key, false);
    }
    // If {non_aliasing_objects_} has no key for {input}, then {input} was not
    // known as non-aliasing (and is thus considered by default as
    // maybe-aliasing), so there is no need to create an entry for it in
    // {non_aliasing_objects_}.
  }
}

void LateLoadEliminationAnalyzer::ProcessAssumeMap(
    OpIndex op_idx, const AssumeMapOp& assume_map) {
  OpIndex object = assume_map.heap_object();
  object_maps_.Set(object, CombineMinMax(object_maps_.Get(object),
                                         ComputeMinMaxHash(assume_map.maps)));
}

void LateLoadEliminationAnalyzer::FinishBlock(const Block* block) {
  block_to_snapshot_mapping_[block->index()] = Snapshot{
      non_aliasing_objects_.Seal(), object_maps_.Seal(), memory_.Seal()};
}

void LateLoadEliminationAnalyzer::SealAndDiscard() {
  non_aliasing_objects_.Seal();
  object_maps_.Seal();
  memory_.Seal();
}

template <bool for_loop_revisit>
bool LateLoadEliminationAnalyzer::BeginBlock(const Block* block) {
  DCHECK_IMPLIES(
      for_loop_revisit,
      block->IsLoop() &&
          block_to_snapshot_mapping_[block->LastPredecessor()->index()]
              .has_value());

  // Collect the snapshots of all predecessors.
  {
    predecessor_alias_snapshots_.clear();
    predecessor_maps_snapshots_.clear();
    predecessor_memory_snapshots_.clear();
    for (const Block* p = block->LastPredecessor(); p != nullptr;
         p = p->NeighboringPredecessor()) {
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
      if (p->NeighboringPredecessor() != nullptr || !block->IsLoop() ||
          block->LastPredecessor() != p) {
        // We only add a MapSnapshot predecessor for non-backedge predecessor.
        // This is because maps coming from inside of the loop may be wrong
        // until a specific check has been executed.
        predecessor_maps_snapshots_.push_back(pred_snapshots->maps_snapshot);
      }
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

  auto merge_maps =
      [&](MapKey key,
          base::Vector<const MapMaskAndOr> predecessors) -> MapMaskAndOr {
    MapMaskAndOr minmax;
    for (const MapMaskAndOr pred : predecessors) {
      if (is_empty(pred)) {
        // One of the predecessors doesn't have maps for this object, so we have
        // to assume that this object could have any map.
        return MapMaskAndOr{};
      }
      minmax = CombineMinMax(minmax, pred);
    }
    return minmax;
  };
  object_maps_.StartNewSnapshot(base::VectorOf(predecessor_maps_snapshots_),
                                merge_maps);

  // Merging for {memory_} means setting values to Invalid unless all
  // predecessors have the same value.
  // TODO(dmercadier): we could insert of Phis during the pass to merge existing
  // information. This is a bit hard, because we are currently in an analyzer
  // rather than a reducer. Still, we could "prepare" the insertion now and then
  // really insert them during the Reduce phase of the OptimizationPhase.
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

  if (block->IsLoop()) return loop_needs_revisit;
  return false;
}

template bool LateLoadEliminationAnalyzer::BeginBlock<true>(const Block* block);
template bool LateLoadEliminationAnalyzer::BeginBlock<false>(
    const Block* block);

}  // namespace v8::internal::compiler::turboshaft
