// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/move-optimizer.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

typedef std::pair<InstructionOperand, InstructionOperand> MoveKey;

struct MoveKeyCompare {
  bool operator()(const MoveKey& a, const MoveKey& b) const {
    if (a.first.EqualsModuloType(b.first)) {
      return a.second.CompareModuloType(b.second);
    }
    return a.first.CompareModuloType(b.first);
  }
};

typedef ZoneMap<MoveKey, unsigned, MoveKeyCompare> MoveMap;
typedef ZoneSet<InstructionOperand, CompareOperandModuloType> OperandSet;


bool GapsCanMoveOver(Instruction* instr) { return instr->IsNop(); }


int FindFirstNonEmptySlot(Instruction* instr) {
  int i = Instruction::FIRST_GAP_POSITION;
  for (; i <= Instruction::LAST_GAP_POSITION; i++) {
    auto moves = instr->parallel_moves()[i];
    if (moves == nullptr) continue;
    for (auto move : *moves) {
      if (!move->IsRedundant()) return i;
      move->Eliminate();
    }
    moves->clear();  // Clear this redundant move.
  }
  return i;
}

}  // namespace


MoveOptimizer::MoveOptimizer(Zone* local_zone, InstructionSequence* code)
    : local_zone_(local_zone),
      code_(code),
      to_finalize_(local_zone),
      temp_vector_0_(local_zone),
      temp_vector_1_(local_zone) {}


void MoveOptimizer::Run() {
  for (auto* block : code()->instruction_blocks()) {
    CompressBlock(block);
  }
  for (auto block : code()->instruction_blocks()) {
    if (block->PredecessorCount() <= 1) continue;
    OptimizeMerge(block);
  }
  for (auto gap : to_finalize_) {
    FinalizeMoves(gap);
  }
}


void MoveOptimizer::CompressMoves(MoveOpVector* eliminated, ParallelMove* left,
                                  ParallelMove* right) {
  DCHECK(eliminated->empty());
  if (!left->empty()) {
    // Modify the right moves in place and collect moves that will be killed by
    // merging the two gaps.
    for (auto move : *right) {
      if (move->IsRedundant()) continue;
      auto to_eliminate = left->PrepareInsertAfter(move);
      if (to_eliminate != nullptr) eliminated->push_back(to_eliminate);
    }
    // Eliminate dead moves.
    for (auto to_eliminate : *eliminated) {
      to_eliminate->Eliminate();
    }
    eliminated->clear();
  }
  // Add all possibly modified moves from right side.
  for (auto move : *right) {
    if (move->IsRedundant()) continue;
    left->push_back(move);
  }
  // Nuke right.
  right->clear();
}


// Smash all consecutive moves into the left most move slot and accumulate them
// as much as possible across instructions.
void MoveOptimizer::CompressBlock(InstructionBlock* block) {
  auto temp_vector = temp_vector_0();
  DCHECK(temp_vector.empty());
  Instruction* prev_instr = nullptr;
  for (int index = block->code_start(); index < block->code_end(); ++index) {
    auto instr = code()->instructions()[index];
    int i = FindFirstNonEmptySlot(instr);
    if (i <= Instruction::LAST_GAP_POSITION) {
      // Move the first non-empty gap to position 0.
      std::swap(instr->parallel_moves()[0], instr->parallel_moves()[i]);
      auto left = instr->parallel_moves()[0];
      // Compress everything into position 0.
      for (++i; i <= Instruction::LAST_GAP_POSITION; ++i) {
        auto move = instr->parallel_moves()[i];
        if (move == nullptr) continue;
        CompressMoves(&temp_vector, left, move);
      }
      if (prev_instr != nullptr) {
        // Smash left into prev_instr, killing left.
        auto pred_moves = prev_instr->parallel_moves()[0];
        CompressMoves(&temp_vector, pred_moves, left);
      }
    }
    if (prev_instr != nullptr) {
      // Slide prev_instr down so we always know where to look for it.
      std::swap(prev_instr->parallel_moves()[0], instr->parallel_moves()[0]);
    }
    prev_instr = instr->parallel_moves()[0] == nullptr ? nullptr : instr;
    if (GapsCanMoveOver(instr)) continue;
    if (prev_instr != nullptr) {
      to_finalize_.push_back(prev_instr);
      prev_instr = nullptr;
    }
  }
  if (prev_instr != nullptr) {
    to_finalize_.push_back(prev_instr);
  }
}


Instruction* MoveOptimizer::LastInstruction(InstructionBlock* block) {
  return code()->instructions()[block->last_instruction_index()];
}


void MoveOptimizer::OptimizeMerge(InstructionBlock* block) {
  DCHECK(block->PredecessorCount() > 1);
  // Ensure that the last instruction in all incoming blocks don't contain
  // things that would prevent moving gap moves across them.
  for (auto pred_index : block->predecessors()) {
    auto pred = code()->InstructionBlockAt(pred_index);
    auto last_instr = code()->instructions()[pred->last_instruction_index()];
    if (last_instr->IsCall()) return;
    if (last_instr->TempCount() != 0) return;
    if (last_instr->OutputCount() != 0) return;
    for (size_t i = 0; i < last_instr->InputCount(); ++i) {
      auto op = last_instr->InputAt(i);
      if (!op->IsConstant() && !op->IsImmediate()) return;
    }
  }
  // TODO(dcarney): pass a ZonePool down for this?
  MoveMap move_map(local_zone());
  size_t correct_counts = 0;
  // Accumulate set of shared moves.
  for (auto pred_index : block->predecessors()) {
    auto pred = code()->InstructionBlockAt(pred_index);
    auto instr = LastInstruction(pred);
    if (instr->parallel_moves()[0] == nullptr ||
        instr->parallel_moves()[0]->empty()) {
      return;
    }
    for (auto move : *instr->parallel_moves()[0]) {
      if (move->IsRedundant()) continue;
      auto src = move->source();
      auto dst = move->destination();
      MoveKey key = {src, dst};
      auto res = move_map.insert(std::make_pair(key, 1));
      if (!res.second) {
        res.first->second++;
        if (res.first->second == block->PredecessorCount()) {
          correct_counts++;
        }
      }
    }
  }
  if (move_map.empty() || correct_counts != move_map.size()) return;
  // Find insertion point.
  Instruction* instr = nullptr;
  for (int i = block->first_instruction_index();
       i <= block->last_instruction_index(); ++i) {
    instr = code()->instructions()[i];
    if (!GapsCanMoveOver(instr) || !instr->AreMovesRedundant()) break;
  }
  DCHECK(instr != nullptr);
  bool gap_initialized = true;
  if (instr->parallel_moves()[0] == nullptr ||
      instr->parallel_moves()[0]->empty()) {
    to_finalize_.push_back(instr);
  } else {
    // Will compress after insertion.
    gap_initialized = false;
    std::swap(instr->parallel_moves()[0], instr->parallel_moves()[1]);
  }
  auto moves = instr->GetOrCreateParallelMove(
      static_cast<Instruction::GapPosition>(0), code_zone());
  // Delete relevant entries in predecessors and move everything to block.
  bool first_iteration = true;
  for (auto pred_index : block->predecessors()) {
    auto pred = code()->InstructionBlockAt(pred_index);
    for (auto move : *LastInstruction(pred)->parallel_moves()[0]) {
      if (move->IsRedundant()) continue;
      MoveKey key = {move->source(), move->destination()};
      auto it = move_map.find(key);
      USE(it);
      DCHECK(it != move_map.end());
      if (first_iteration) {
        moves->AddMove(move->source(), move->destination());
      }
      move->Eliminate();
    }
    first_iteration = false;
  }
  // Compress.
  if (!gap_initialized) {
    CompressMoves(&temp_vector_0(), instr->parallel_moves()[0],
                  instr->parallel_moves()[1]);
  }
}


namespace {

bool IsSlot(const InstructionOperand& op) {
  return op.IsStackSlot() || op.IsDoubleStackSlot();
}


bool LoadCompare(const MoveOperands* a, const MoveOperands* b) {
  if (!a->source().EqualsModuloType(b->source())) {
    return a->source().CompareModuloType(b->source());
  }
  if (IsSlot(a->destination()) && !IsSlot(b->destination())) return false;
  if (!IsSlot(a->destination()) && IsSlot(b->destination())) return true;
  return a->destination().CompareModuloType(b->destination());
}

}  // namespace


// Split multiple loads of the same constant or stack slot off into the second
// slot and keep remaining moves in the first slot.
void MoveOptimizer::FinalizeMoves(Instruction* instr) {
  auto loads = temp_vector_0();
  DCHECK(loads.empty());
  // Find all the loads.
  for (auto move : *instr->parallel_moves()[0]) {
    if (move->IsRedundant()) continue;
    if (move->source().IsConstant() || IsSlot(move->source())) {
      loads.push_back(move);
    }
  }
  if (loads.empty()) return;
  // Group the loads by source, moving the preferred destination to the
  // beginning of the group.
  std::sort(loads.begin(), loads.end(), LoadCompare);
  MoveOperands* group_begin = nullptr;
  for (auto load : loads) {
    // New group.
    if (group_begin == nullptr ||
        !load->source().EqualsModuloType(group_begin->source())) {
      group_begin = load;
      continue;
    }
    // Nothing to be gained from splitting here.
    if (IsSlot(group_begin->destination())) continue;
    // Insert new move into slot 1.
    auto slot_1 = instr->GetOrCreateParallelMove(
        static_cast<Instruction::GapPosition>(1), code_zone());
    slot_1->AddMove(group_begin->destination(), load->destination());
    load->Eliminate();
  }
  loads.clear();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
