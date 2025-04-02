// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-gc-typed-optimization-reducer.h"

#include "src/base/logging.h"
#include "src/compiler/turboshaft/analyzer-iterator.h"
#include "src/compiler/turboshaft/loop-finder.h"

namespace v8::internal::compiler::turboshaft {

#define TRACE(...)                                      \
  do {                                                  \
    if (v8_flags.trace_wasm_typer) PrintF(__VA_ARGS__); \
  } while (false)

void WasmGCTypeAnalyzer::Run() {
  LoopFinder loop_finder(phase_zone_, &graph_);
  AnalyzerIterator iterator(phase_zone_, graph_, loop_finder);
  while (iterator.HasNext()) {
    const Block& block = *iterator.Next();
    ProcessBlock(block);

    // Finish snapshot.
    block_to_snapshot_[block.index()] = MaybeSnapshot(types_table_.Seal());

    // Consider re-processing for loops.
    if (const GotoOp* last = block.LastOperation(graph_).TryCast<GotoOp>()) {
      if (IsReachable(block) && last->destination->IsLoop() &&
          last->destination->LastPredecessor() == &block) {
        TRACE("[b%u] Reprocessing loop header b%u at backedge #%u\n",
              block.index().id(), last->destination->index().id(),
              graph_.Index(block.LastOperation(graph_)).id());
        const Block& loop_header = *last->destination;
        // Create a merged snapshot state for the forward- and backedge and
        // process all operations inside the loop header.
        ProcessBlock(loop_header);
        Snapshot old_snapshot = block_to_snapshot_[loop_header.index()].value();
        Snapshot snapshot = types_table_.Seal();
        // TODO(14108): The snapshot isn't needed at all, we only care about the
        // information if two snapshots are equivalent. Unfortunately, currently
        // this can only be answered by creating a merge snapshot.
        bool needs_revisit =
            CreateMergeSnapshot(base::VectorOf({old_snapshot, snapshot}),
                                base::VectorOf({true, true}));
        types_table_.Seal();  // Discard the snapshot.

        TRACE("[b%u] Loop header b%u reprocessed at backedge #%u: %s\n",
              block.index().id(), last->destination->index().id(),
              graph_.Index(block.LastOperation(graph_)).id(),
              needs_revisit ? "Scheduling loop body revisitation"
                            : "No revisit of loop body needed");

        // TODO(14108): This currently encodes a fixed point analysis where the
        // analysis is finished once the backedge doesn't provide updated type
        // information any more compared to the previous iteration. This could
        // be stopped in cases where the backedge only refines types (i.e. only
        // defines more precise types than the previous iteration).
        if (needs_revisit) {
          block_to_snapshot_[loop_header.index()] = MaybeSnapshot(snapshot);
          if (block.index() != loop_header.index()) {
            // This will push the successors of the loop header to the iterator
            // stack, so the loop body will be visited in the next iteration.
            iterator.MarkLoopForRevisitSkipHeader();
          } else {
            // A single-block loop doesn't have any successors which would be
            // re-evaluated and which might trigger another re-evaluation of the
            // loop header.
            // TODO(mliedtke): This is not a great design: We don't just
            // schedule revisiting the loop header but afterwards we revisit it
            // once again to evaluate whether we need to revisit it more times,
            // so for single block loops the revisitation count will always be a
            // multiple of 2. While this is inefficient, single-block loops are
            // rare and are either endless loops or need to trigger an exception
            // (e.g. a wasm trap) to terminate.
            iterator.MarkLoopForRevisit();
          }
        }
      }
    }
  }
}

void WasmGCTypeAnalyzer::ProcessBlock(const Block& block) {
  DCHECK_NULL(current_block_);
  current_block_ = &block;
  StartNewSnapshotFor(block);
  ProcessOperations(block);
  current_block_ = nullptr;
}

void WasmGCTypeAnalyzer::StartNewSnapshotFor(const Block& block) {
  is_first_loop_header_evaluation_ = false;
  // Reset reachability information. This can be outdated in case of loop
  // revisits. Below the reachability is calculated again and potentially
  // re-added.
  bool block_was_previously_reachable = IsReachable(block);
  if (!block_was_previously_reachable) {
    TRACE("[b%u] Removing unreachable flag as block is re-evaluated\n",
          block.index().id());
  }
  block_is_unreachable_.Remove(block.index().id());
  // Start new snapshot based on predecessor information.
  if (block.HasPredecessors() == 0) {
    // The first block just starts with an empty snapshot.
    DCHECK_EQ(block.index().id(), 0);
    types_table_.StartNewSnapshot();
  } else if (block.IsLoop()) {
    const Block& forward_predecessor =
        *block.LastPredecessor()->NeighboringPredecessor();
    if (!IsReachable(forward_predecessor)) {
      // If a loop isn't reachable through its forward edge, it can't possibly
      // become reachable via the backedge.
      TRACE(
          "[b%uu] Loop unreachable as forward predecessor b%u is unreachable\n",
          block.index().id(), forward_predecessor.index().id());
      block_is_unreachable_.Add(block.index().id());
    }
    MaybeSnapshot back_edge_snap =
        block_to_snapshot_[block.LastPredecessor()->index()];
    if (back_edge_snap.has_value() && block_was_previously_reachable) {
      // The loop was already visited at least once. In this case use the
      // available information from the backedge.
      // Note that we only do this if the loop wasn't marked as unreachable
      // before. This solves an issue where a single block loop would think the
      // backedge is reachable as we just removed the unreachable information
      // above. Once the analyzer hits the backedge, it will re-evaluate if the
      // backedge changes any analysis results and then potentially revisit
      // this loop with forward edge and backedge.
      CreateMergeSnapshot(block);
    } else {
      // The loop wasn't visited yet. There isn't any type information available
      // for the backedge.
      TRACE(
          "[b%u%s] First loop header evaluation: Ignoring all backedges on "
          "phis\n",
          block.index().id(), !IsReachable(*current_block_) ? "u" : "");
      is_first_loop_header_evaluation_ = true;
      Snapshot forward_edge_snap =
          block_to_snapshot_[forward_predecessor.index()].value();
      types_table_.StartNewSnapshot(forward_edge_snap);
    }
  } else if (block.IsBranchTarget()) {
    DCHECK_EQ(block.PredecessorCount(), 1);
    const Block& predecessor = *block.LastPredecessor();
    types_table_.StartNewSnapshot(
        block_to_snapshot_[predecessor.index()].value());
    if (IsReachable(predecessor)) {
      const BranchOp* branch =
          block.Predecessors()[0]->LastOperation(graph_).TryCast<BranchOp>();
      if (branch != nullptr) {
        ProcessBranchOnTarget(*branch, block);
      }
    } else {
      TRACE("[b%uu] Block unreachable as sole predecessor b%u is unreachable\n",
            block.index().id(), predecessor.index().id());
      block_is_unreachable_.Add(block.index().id());
    }
  } else {
    DCHECK_EQ(block.kind(), Block::Kind::kMerge);
    CreateMergeSnapshot(block);
  }
}

void WasmGCTypeAnalyzer::ProcessOperations(const Block& block) {
  for (OpIndex op_idx : graph_.OperationIndices(block)) {
    Operation& op = graph_.Get(op_idx);
    switch (op.opcode) {
      case Opcode::kWasmTypeCast:
        ProcessTypeCast(op.Cast<WasmTypeCastOp>());
        break;
      case Opcode::kWasmTypeCheck:
        ProcessTypeCheck(op.Cast<WasmTypeCheckOp>());
        break;
      case Opcode::kAssertNotNull:
        ProcessAssertNotNull(op.Cast<AssertNotNullOp>());
        break;
      case Opcode::kNull:
        ProcessNull(op.Cast<NullOp>());
        break;
      case Opcode::kIsNull:
        ProcessIsNull(op.Cast<IsNullOp>());
        break;
      case Opcode::kParameter:
        ProcessParameter(op.Cast<ParameterOp>());
        break;
      case Opcode::kStructGet:
        ProcessStructGet(op.Cast<StructGetOp>());
        break;
      case Opcode::kStructSet:
        ProcessStructSet(op.Cast<StructSetOp>());
        break;
      case Opcode::kArrayGet:
        ProcessArrayGet(op.Cast<ArrayGetOp>());
        break;
      case Opcode::kArrayLength:
        ProcessArrayLength(op.Cast<ArrayLengthOp>());
        break;
      case Opcode::kGlobalGet:
        ProcessGlobalGet(op.Cast<GlobalGetOp>());
        break;
      case Opcode::kWasmRefFunc:
        ProcessRefFunc(op.Cast<WasmRefFuncOp>());
        break;
      case Opcode::kWasmAllocateArray:
        ProcessAllocateArray(op.Cast<WasmAllocateArrayOp>());
        break;
      case Opcode::kWasmAllocateStruct:
        ProcessAllocateStruct(op.Cast<WasmAllocateStructOp>());
        break;
      case Opcode::kPhi:
        ProcessPhi(op.Cast<PhiOp>());
        break;
      case Opcode::kWasmTypeAnnotation:
        ProcessTypeAnnotation(op.Cast<WasmTypeAnnotationOp>());
        break;
      case Opcode::kBranch:
        // Handling branch conditions implying special values is handled on the
        // beginning of the successor block.
      default:
        break;
    }
  }
}

void WasmGCTypeAnalyzer::ProcessTypeCast(const WasmTypeCastOp& type_cast) {
  V<Object> object = type_cast.object();
  wasm::ValueType target_type = type_cast.config.to;
  wasm::ValueType known_input_type =
      RefineTypeKnowledge(object, target_type, type_cast);
  input_type_map_[graph_.Index(type_cast)] = known_input_type;
}

void WasmGCTypeAnalyzer::ProcessTypeCheck(const WasmTypeCheckOp& type_check) {
  wasm::ValueType type = GetResolvedType(type_check.object());
  input_type_map_[graph_.Index(type_check)] = type;
}

void WasmGCTypeAnalyzer::ProcessAssertNotNull(
    const AssertNotNullOp& assert_not_null) {
  V<Object> object = assert_not_null.object();
  wasm::ValueType new_type = assert_not_null.type.AsNonNull();
  wasm::ValueType known_input_type =
      RefineTypeKnowledge(object, new_type, assert_not_null);
  input_type_map_[graph_.Index(assert_not_null)] = known_input_type;
}

void WasmGCTypeAnalyzer::ProcessIsNull(const IsNullOp& is_null) {
  input_type_map_[graph_.Index(is_null)] = GetResolvedType(is_null.object());
}

void WasmGCTypeAnalyzer::ProcessParameter(const ParameterOp& parameter) {
  if (parameter.parameter_index != wasm::kWasmInstanceDataParameterIndex) {
    RefineTypeKnowledge(graph_.Index(parameter),
                        signature_->GetParam(parameter.parameter_index - 1),
                        parameter);
  }
}

void WasmGCTypeAnalyzer::ProcessStructGet(const StructGetOp& struct_get) {
  // struct.get performs a null check.
  wasm::ValueType type =
      RefineTypeKnowledgeNotNull(struct_get.object(), struct_get);
  input_type_map_[graph_.Index(struct_get)] = type;
  RefineTypeKnowledge(graph_.Index(struct_get),
                      struct_get.type->field(struct_get.field_index).Unpacked(),
                      struct_get);
}

void WasmGCTypeAnalyzer::ProcessStructSet(const StructSetOp& struct_set) {
  // struct.set performs a null check.
  wasm::ValueType type =
      RefineTypeKnowledgeNotNull(struct_set.object(), struct_set);
  input_type_map_[graph_.Index(struct_set)] = type;
}

void WasmGCTypeAnalyzer::ProcessArrayGet(const ArrayGetOp& array_get) {
  // array.get traps on null. (Typically already on the array length access
  // needed for the bounds check.)
  RefineTypeKnowledgeNotNull(array_get.array(), array_get);
  // The result type is at least the static array element type.
  RefineTypeKnowledge(graph_.Index(array_get),
                      array_get.array_type->element_type().Unpacked(),
                      array_get);
}

void WasmGCTypeAnalyzer::ProcessArrayLength(const ArrayLengthOp& array_length) {
  // array.len performs a null check.
  wasm::ValueType type =
      RefineTypeKnowledgeNotNull(array_length.array(), array_length);
  input_type_map_[graph_.Index(array_length)] = type;
}

void WasmGCTypeAnalyzer::ProcessGlobalGet(const GlobalGetOp& global_get) {
  RefineTypeKnowledge(graph_.Index(global_get), global_get.global->type,
                      global_get);
}

void WasmGCTypeAnalyzer::ProcessRefFunc(const WasmRefFuncOp& ref_func) {
  wasm::ModuleTypeIndex sig_index =
      module_->functions[ref_func.function_index].sig_index;
  RefineTypeKnowledge(graph_.Index(ref_func), wasm::ValueType::Ref(sig_index),
                      ref_func);
}

void WasmGCTypeAnalyzer::ProcessAllocateArray(
    const WasmAllocateArrayOp& allocate_array) {
  wasm::ModuleTypeIndex type_index =
      graph_.Get(allocate_array.rtt()).Cast<RttCanonOp>().type_index;
  RefineTypeKnowledge(graph_.Index(allocate_array),
                      wasm::ValueType::Ref(type_index), allocate_array);
}

void WasmGCTypeAnalyzer::ProcessAllocateStruct(
    const WasmAllocateStructOp& allocate_struct) {
  wasm::ModuleTypeIndex type_index =
      graph_.Get(allocate_struct.rtt()).Cast<RttCanonOp>().type_index;
  RefineTypeKnowledge(graph_.Index(allocate_struct),
                      wasm::ValueType::Ref(type_index), allocate_struct);
}

wasm::ValueType WasmGCTypeAnalyzer::GetTypeForPhiInput(const PhiOp& phi,
                                                       int input_index) {
  OpIndex phi_id = graph_.Index(phi);
  OpIndex input = ResolveAliases(phi.input(input_index));
  // If the input of the phi is in the same block as the phi and appears
  // before the phi, don't use the predecessor value.

  if (current_block_->begin().id() <= input.id() && input.id() < phi_id.id()) {
    // Phi instructions have to be at the beginning of the block, so this can
    // only happen for inputs that are also phis. Furthermore, this is only
    // possible in loop headers of loops and only for the backedge-input.
    DCHECK(graph_.Get(input).Is<PhiOp>());
    DCHECK(current_block_->IsLoop());
    DCHECK_EQ(input_index, 1);
    return types_table_.Get(input);
  }
  return types_table_.GetPredecessorValue(input, input_index);
}

void WasmGCTypeAnalyzer::ProcessPhi(const PhiOp& phi) {
  // The result type of a phi is the union of all its input types.
  // If any of the inputs is the default value ValueType(), there isn't any type
  // knowledge inferrable.
  DCHECK_GT(phi.input_count, 0);
  if (is_first_loop_header_evaluation_) {
    // We don't know anything about the backedge yet, so we only use the
    // forward edge. We will revisit the loop header again once the block with
    // the back edge is evaluated.
    RefineTypeKnowledge(graph_.Index(phi), GetResolvedType((phi.input(0))),
                        phi);
    return;
  }
  wasm::ValueType union_type = GetTypeForPhiInput(phi, 0);
  if (union_type == wasm::ValueType()) return;
  for (int i = 1; i < phi.input_count; ++i) {
    wasm::ValueType input_type = GetTypeForPhiInput(phi, i);
    if (input_type == wasm::ValueType()) return;
    // <bottom> types have to be skipped as an unreachable predecessor doesn't
    // change our type knowledge.
    // TODO(mliedtke): Ideally, we'd skip unreachable predecessors here
    // completely, as we might loosen the known type due to an unreachable
    // predecessor.
    if (input_type.is_uninhabited()) continue;
    if (union_type.is_uninhabited()) {
      union_type = input_type;
    } else {
      union_type = wasm::Union(union_type, input_type, module_, module_).type;
    }
  }
  RefineTypeKnowledge(graph_.Index(phi), union_type, phi);
  if (v8_flags.trace_wasm_typer) {
    for (int i = 0; i < phi.input_count; ++i) {
      OpIndex input = phi.input(i);
      wasm::ValueType type = GetTypeForPhiInput(phi, i);
      TRACE("- phi input %d: #%u(%s) -> %s\n", i, input.id(),
            OpcodeName(graph_.Get(input).opcode), type.name().c_str());
    }
  }
}

void WasmGCTypeAnalyzer::ProcessTypeAnnotation(
    const WasmTypeAnnotationOp& type_annotation) {
  RefineTypeKnowledge(type_annotation.value(), type_annotation.type,
                      type_annotation);
}

void WasmGCTypeAnalyzer::ProcessBranchOnTarget(const BranchOp& branch,
                                               const Block& target) {
  DCHECK_EQ(current_block_, &target);
  const Operation& condition = graph_.Get(branch.condition());
  switch (condition.opcode) {
    case Opcode::kWasmTypeCheck: {
      const WasmTypeCheckOp& check = condition.Cast<WasmTypeCheckOp>();
      if (branch.if_true == &target) {
        // It is known from now on that the type is at least the checked one.
        RefineTypeKnowledge(check.object(), check.config.to, branch);
      } else {
        DCHECK_EQ(branch.if_false, &target);
        if (wasm::IsSubtypeOf(GetResolvedType(check.object()), check.config.to,
                              module_)) {
          // The type check always succeeds, the target is impossible to be
          // reached.
          DCHECK_EQ(target.PredecessorCount(), 1);
          block_is_unreachable_.Add(target.index().id());
          TRACE(
              "[b%uu] Block unreachable as #%u(%s) used in #%u(%s) is always "
              "true\n",
              target.index().id(), branch.condition().id(),
              OpcodeName(condition.opcode), graph_.Index(branch).id(),
              OpcodeName(branch.opcode));
        }
      }
    } break;
    case Opcode::kIsNull: {
      const IsNullOp& is_null = condition.Cast<IsNullOp>();
      if (branch.if_true == &target) {
        if (GetResolvedType(is_null.object()).is_non_nullable()) {
          // The target is impossible to be reached.
          DCHECK_EQ(target.PredecessorCount(), 1);
          block_is_unreachable_.Add(target.index().id());
          TRACE(
              "[b%uu] Block unreachable as #%u(%s) used in #%u(%s) is always "
              "false\n",
              target.index().id(), branch.condition().id(),
              OpcodeName(condition.opcode), graph_.Index(branch).id(),
              OpcodeName(branch.opcode));
          return;
        }
        RefineTypeKnowledge(is_null.object(),
                            wasm::ToNullSentinel({is_null.type, module_}),
                            branch);
      } else {
        DCHECK_EQ(branch.if_false, &target);
        RefineTypeKnowledge(is_null.object(), is_null.type.AsNonNull(), branch);
      }
    } break;
    default:
      break;
  }
}

void WasmGCTypeAnalyzer::ProcessNull(const NullOp& null) {
  wasm::ValueType null_type = wasm::ToNullSentinel({null.type, module_});
  RefineTypeKnowledge(graph_.Index(null), null_type, null);
}

void WasmGCTypeAnalyzer::CreateMergeSnapshot(const Block& block) {
  base::SmallVector<Snapshot, 8> snapshots;
  // Unreachable predecessors should be ignored when merging but we can't remove
  // them from the predecessors as that would mess up the phi inputs. Therefore
  // the reachability of the predecessors is passed as a separate list.
  base::SmallVector<bool, 8> reachable;
  bool all_predecessors_unreachable = true;
  for (const Block* predecessor : block.PredecessorsIterable()) {
    snapshots.push_back(block_to_snapshot_[predecessor->index()].value());
    bool predecessor_reachable = IsReachable(*predecessor);
    reachable.push_back(predecessor_reachable);
    all_predecessors_unreachable &= !predecessor_reachable;
  }
  if (all_predecessors_unreachable) {
    TRACE("[b%u] Block unreachable as all predecessors are unreachable\n",
          block.index().id());
    block_is_unreachable_.Add(block.index().id());
  } else if (v8_flags.trace_wasm_typer) {
    std::stringstream str;
    size_t i = 0;
    for (const Block* predecessor : block.PredecessorsIterable()) {
      if (i != 0) str << ", ";
      str << 'b' << predecessor->index().id() << (reachable[i] ? "" : "u");
      ++i;
    }
    TRACE("[b%u] Predecessors reachability: %s\n", block.index().id(),
          str.str().c_str());
  }
  // The predecessor snapshots need to be reversed to restore the "original"
  // order of predecessors. (This is used to map phi inputs to their
  // corresponding predecessor.)
  std::reverse(snapshots.begin(), snapshots.end());
  std::reverse(reachable.begin(), reachable.end());
  CreateMergeSnapshot(base::VectorOf(snapshots), base::VectorOf(reachable));
}

bool WasmGCTypeAnalyzer::CreateMergeSnapshot(
    base::Vector<const Snapshot> predecessors,
    base::Vector<const bool> reachable) {
  DCHECK_EQ(predecessors.size(), reachable.size());
  // The merging logic is also used to evaluate if two snapshots are
  // "identical", i.e. the known types for all operations are the same.
  bool types_are_equivalent = true;
  types_table_.StartNewSnapshot(
      predecessors, [this, &types_are_equivalent, reachable](
                        TypeSnapshotTable::Key,
                        base::Vector<const wasm::ValueType> predecessors) {
        DCHECK_GT(predecessors.size(), 1);
        size_t i = 0;
        // Initialize the type based on the first reachable predecessor.
        wasm::ValueType first = wasm::kWasmBottom;
        for (; i < reachable.size(); ++i) {
          // Uninhabitated types can only occur in unreachable code e.g. as a
          // result of an always failing cast. Still reachability tracking might
          // in some cases miss that a block becomes unreachable, so we still
          // check for uninhabited in the if below.
          DCHECK_IMPLIES(reachable[i], !predecessors[i].is_uninhabited());
          if (reachable[i] && !predecessors[i].is_uninhabited()) {
            first = predecessors[i];
            ++i;
            break;
          }
        }

        wasm::ValueType res = first;
        for (; i < reachable.size(); ++i) {
          if (!reachable[i]) continue;  // Skip unreachable predecessors.
          wasm::ValueType type = predecessors[i];
          // Uninhabitated types can only occur in unreachable code e.g. as a
          // result of an always failing cast. Still reachability tracking might
          // in some cases miss that a block becomes unreachable, so we still
          // check for uninhabited in the if below.
          DCHECK(!type.is_uninhabited());
          if (type.is_uninhabited()) continue;
          types_are_equivalent &= first == type;
          if (res == wasm::ValueType() || type == wasm::ValueType()) {
            res = wasm::ValueType();
          } else {
            res = wasm::Union(res, type, module_, module_).type;
          }
        }
        return res;
      });
  return !types_are_equivalent;
}

wasm::ValueType WasmGCTypeAnalyzer::RefineTypeKnowledge(
    OpIndex object, wasm::ValueType new_type, const Operation& op) {
  DCHECK_NOT_NULL(current_block_);
  object = ResolveAliases(object);
  wasm::ValueType previous_value = types_table_.Get(object);
  wasm::ValueType intersection_type =
      previous_value == wasm::ValueType()
          ? new_type
          : wasm::Intersection(previous_value, new_type, module_, module_).type;
  if (intersection_type == previous_value) return previous_value;

  TRACE("[b%u%s] #%u(%s): Refine type for object #%u(%s) -> %s%s\n",
        current_block_->index().id(), !IsReachable(*current_block_) ? "u" : "",
        graph_.Index(op).id(), OpcodeName(op.opcode), object.id(),
        OpcodeName(graph_.Get(object).opcode), intersection_type.name().c_str(),
        intersection_type.is_uninhabited() ? " (unreachable!)" : "");

  types_table_.Set(object, intersection_type);
  if (intersection_type.is_uninhabited()) {
    // After this instruction all other instructions in the current block are
    // unreachable.
    block_is_unreachable_.Add(current_block_->index().id());
    // Return bottom to indicate that the operation `op` shall always trap.
    return wasm::kWasmBottom;
  }
  return previous_value;
}

wasm::ValueType WasmGCTypeAnalyzer::RefineTypeKnowledgeNotNull(
    OpIndex object, const Operation& op) {
  object = ResolveAliases(object);
  wasm::ValueType previous_value = types_table_.Get(object);
  if (previous_value.is_non_nullable()) return previous_value;

  wasm::ValueType not_null_type = previous_value.AsNonNull();
  TRACE("[b%u%s] #%u(%s): Refine type for object #%u(%s) -> %s%s\n",
        current_block_->index().id(), !IsReachable(*current_block_) ? "u" : "",
        graph_.Index(op).id(), OpcodeName(op.opcode), object.id(),
        OpcodeName(graph_.Get(object).opcode), not_null_type.name().c_str(),
        not_null_type.is_uninhabited() ? " (unreachable!)" : "");

  types_table_.Set(object, not_null_type);
  if (not_null_type.is_uninhabited()) {
    // After this instruction all other instructions in the current block are
    // unreachable.
    block_is_unreachable_.Add(current_block_->index().id());
    // Return bottom to indicate that the operation `op` shall always trap.
    return wasm::kWasmBottom;
  }
  return previous_value;
}

OpIndex WasmGCTypeAnalyzer::ResolveAliases(OpIndex object) const {
  while (true) {
    const Operation* op = &graph_.Get(object);
    switch (op->opcode) {
      case Opcode::kWasmTypeCast:
        object = op->Cast<WasmTypeCastOp>().object();
        break;
      case Opcode::kAssertNotNull:
        object = op->Cast<AssertNotNullOp>().object();
        break;
      case Opcode::kWasmTypeAnnotation:
        object = op->Cast<WasmTypeAnnotationOp>().value();
        break;
      default:
        return object;
    }
  }
}

bool WasmGCTypeAnalyzer::IsReachable(const Block& block) const {
  return !block_is_unreachable_.Contains(block.index().id());
}

wasm::ValueType WasmGCTypeAnalyzer::GetResolvedType(OpIndex object) const {
  return types_table_.Get(ResolveAliases(object));
}

#undef TRACE

}  // namespace v8::internal::compiler::turboshaft
