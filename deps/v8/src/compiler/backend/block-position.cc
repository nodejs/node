// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/block-position.h"

namespace v8::internal::compiler {

// Get best unvisited successor.
std::pair<const InstructionBlock*, uint64_t>
BlockPositionNumberer::GetBestSuccessor(
    const InstructionBlock* block, const std::set<RpoNumber>& visited) const {
  const InstructionBlock* best_succ = nullptr;
  uint64_t best_execution_count = 0;

  if (block->SuccessorCount() == 1) {
    RpoNumber index = block->successors()[0];
    if (!visited.contains(index)) {
      best_succ = InstructionBlockAt(index);
      best_execution_count = block->pgo_execution_count();
    }
    return {best_succ, best_execution_count};
  }

  for (const RpoNumber index : block->successors()) {
    if (visited.contains(index)) continue;
    InstructionBlock* succ = InstructionBlockAt(index);
    // In split form, when block has multiple successors, the successor won't
    // have multiple predecessors.
    uint64_t succ_count = succ->pgo_execution_count();
    // We dont miss succ with zero count.
    if (succ_count > best_execution_count || best_succ == nullptr) {
      best_execution_count = succ_count;
      best_succ = succ;
    }
  }
  return {best_succ, best_execution_count};
}

const InstructionBlock* BlockPositionNumberer::GetBestSuccessorFromPlacedBlocks(
    const std::set<RpoNumber>& visited) const {
  const InstructionBlock* best_succ = nullptr;
  uint64_t best_execution_count = 0;
  for (const RpoNumber index : visited) {
    const InstructionBlock* block = InstructionBlockAt(index);
    auto [succ, succ_count] = GetBestSuccessor(block, visited);
    if (succ == nullptr) continue;
    //  We dont miss succ with zero count.
    if (succ_count > best_execution_count || best_succ == nullptr) {
      best_execution_count = succ_count;
      best_succ = succ;
    }
  }
  return best_succ;
}

ZoneVector<int32_t> BlockPositionNumberer::ComputeTopdownPosition(
    const InstructionBlock* entry) {
  ZoneVector<int32_t> block_permutation(code_->instruction_blocks().size(),
                                        zone());
  size_t index = 0;
  const InstructionBlock* start = entry;
  std::set<RpoNumber> visited;
  while (start) {
    // Get blocks from start's successors.
    const InstructionBlock* next = start;
    while (next) {
      CHECK(index < block_permutation.size());
      block_permutation[index++] = next->rpo_number().ToInt();
      visited.insert(next->rpo_number());
      auto result = GetBestSuccessor(next, visited);
      next = result.first;
    }

    // Find new start from connected blocks.
    start = GetBestSuccessorFromPlacedBlocks(visited);
  }

  CHECK(visited.size() == block_permutation.size());
  return block_permutation;
}

const InstructionBlocks& BlockPositionNumberer::instruction_blocks() const {
  return code_->instruction_blocks();
}

InstructionBlock* BlockPositionNumberer::InstructionBlockAt(
    RpoNumber rpo_number) const {
  return code_->InstructionBlockAt(rpo_number);
}

ZoneVector<int32_t> BlockPositionNumberer::ComputeProfileGuidedBlockPosition() {
  const InstructionBlock* entry = instruction_blocks().at(0);
  return ComputeTopdownPosition(entry);
}

}  // namespace v8::internal::compiler
