// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-gc-type-reducer.h"

namespace v8::internal::compiler::turboshaft {

void WasmGCTypeAnalyzer::Run() {
  for (uint32_t block_index = 0; block_index < graph_.block_count();
       block_index++) {
    const Block& block = graph_.Get(BlockIndex{block_index});
    StartNewSnapshotFor(block);
    ProcessOperations(block);
    // Finish snapshot.
    block_to_snapshot_[block.index()] = MaybeSnapshot(types_table_.Seal());
  }
}

void WasmGCTypeAnalyzer::StartNewSnapshotFor(const Block& block) {
  // Start new snapshot based on predecessor information.
  if (block.HasPredecessors() == 0) {
    // The first block just starts with an empty snapshot.
    DCHECK_EQ(block.index().id(), 0);
    types_table_.StartNewSnapshot();
  } else if (block.IsLoop()) {
    // TODO(mliedtke): Once we want to propagate type information on LoopPhis,
    // we will need to revisit loops to also evaluate the backedge.
    Snapshot forward_edge_snap =
        block_to_snapshot_
            [block.LastPredecessor()->NeighboringPredecessor()->index()]
                .value();
    types_table_.StartNewSnapshot(forward_edge_snap);
  } else if (block.IsBranchTarget()) {
    DCHECK_EQ(block.PredecessorCount(), 1);
    types_table_.StartNewSnapshot(
        block_to_snapshot_[block.LastPredecessor()->index()].value());
    const BranchOp* branch =
        block.Predecessors()[0]->LastOperation(graph_).TryCast<BranchOp>();
    if (branch != nullptr) {
      ProcessBranchOnTarget(*branch, block);
    }
  } else {
    DCHECK_EQ(block.kind(), Block::Kind::kMerge);
    CreateMergeSnapshot(block);
  }
}

void WasmGCTypeAnalyzer::ProcessOperations(const Block& block) {
  for (OpIndex op_idx : graph_.OperationIndices(block)) {
    Operation& op = graph_.Get(op_idx);
    // TODO(mliedtke): We need a typeguard mechanism. Otherwise, how are we
    // going to figure out the type of an ArrayNew that already got lowered to
    // some __ Allocate?
    switch (op.opcode) {
      case Opcode::kWasmTypeCast:
        ProcessTypeCast(op.Cast<WasmTypeCastOp>());
        break;
      case Opcode::kAssertNotNull:
        ProcessAssertNotNull(op.Cast<AssertNotNullOp>());
        break;
      case Opcode::kBranch:
        // Handling branch conditions implying special values is handled on the
        // beginning of the successor block.
      default:
        // TODO(mliedtke): Make sure that we handle all relevant operations
        // above.
        break;
    }
  }
}

void WasmGCTypeAnalyzer::ProcessTypeCast(const WasmTypeCastOp& type_cast) {
  OpIndex object = type_cast.object();
  wasm::ValueType target_type = type_cast.config.to;
  // TODO(mliedtke): The cast also produces a result that is the same object as
  // the input but that is not known, so we also need to refine the cast's
  // result type to elide potential useless casts in chains like
  // (ref.cast eq (ref.cast $MyStruct (local.get 0))).
  wasm::ValueType known_input_type = RefineTypeKnowledge(object, target_type);
  input_type_map_[graph_.Index(type_cast)] = known_input_type;
}

void WasmGCTypeAnalyzer::ProcessAssertNotNull(
    const AssertNotNullOp& assert_not_null) {
  OpIndex object = assert_not_null.object();
  wasm::ValueType new_type = assert_not_null.type.AsNonNull();
  wasm::ValueType known_input_type = RefineTypeKnowledge(object, new_type);
  input_type_map_[graph_.Index(assert_not_null)] = known_input_type;
  // AssertNotNull also returns the input.
  RefineTypeKnowledge(graph_.Index(assert_not_null), new_type);
}

void WasmGCTypeAnalyzer::ProcessBranchOnTarget(const BranchOp& branch,
                                               const Block& target) {
  const Operation& condition = graph_.Get(branch.condition());
  switch (condition.opcode) {
    case Opcode::kWasmTypeCheck: {
      if (branch.if_true == &target) {
        // It is known from now on that the type is at least the checked one.
        const WasmTypeCheckOp& check = condition.Cast<WasmTypeCheckOp>();
        wasm::ValueType known_input_type =
            RefineTypeKnowledge(check.object(), check.config.to);
        input_type_map_[branch.condition()] = known_input_type;
      }
    } break;
    default:
      break;
  }
}

void WasmGCTypeAnalyzer::CreateMergeSnapshot(const Block& block) {
  DCHECK(!block_to_snapshot_[block.index()].has_value());
  base::SmallVector<Snapshot, 8> snapshots;
  NeighboringPredecessorIterable iterable = block.PredecessorsIterable();
  std::transform(iterable.begin(), iterable.end(),
                 std::back_insert_iterator(snapshots), [this](Block* pred) {
                   return block_to_snapshot_[pred->index()].value();
                 });
  types_table_.StartNewSnapshot(
      base::VectorOf(snapshots),
      [this](TypeSnapshotTable::Key,
             base::Vector<const wasm::ValueType> predecessors) {
        DCHECK_GT(predecessors.size(), 1);
        wasm::ValueType res = predecessors[0];
        if (res == wasm::ValueType()) return wasm::ValueType();
        for (auto iter = predecessors.begin() + 1; iter != predecessors.end();
             ++iter) {
          if (*iter == wasm::ValueType()) return wasm::ValueType();
          res = wasm::Union(res, *iter, module_, module_).type;
        }
        return res;
      });
}

wasm::ValueType WasmGCTypeAnalyzer::RefineTypeKnowledge(
    OpIndex object, wasm::ValueType new_type) {
  wasm::ValueType previous_value = types_table_.Get(object);
  wasm::ValueType intersection_type =
      previous_value == wasm::ValueType()
          ? new_type
          : wasm::Intersection(previous_value, new_type, module_, module_).type;
  types_table_.Set(object, intersection_type);
  return previous_value;
}

}  // namespace v8::internal::compiler::turboshaft
