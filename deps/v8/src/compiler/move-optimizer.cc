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
    if (a.first.EqualsCanonicalized(b.first)) {
      return a.second.CompareCanonicalized(b.second);
    }
    return a.first.CompareCanonicalized(b.first);
  }
};

struct OperandCompare {
  bool operator()(const InstructionOperand& a,
                  const InstructionOperand& b) const {
    return a.CompareCanonicalized(b);
  }
};

typedef ZoneMap<MoveKey, unsigned, MoveKeyCompare> MoveMap;
typedef ZoneSet<InstructionOperand, CompareOperandModuloType> OperandSet;


bool GapsCanMoveOver(Instruction* instr, Zone* zone) {
  if (instr->IsNop()) return true;
  if (instr->ClobbersTemps() || instr->ClobbersRegisters() ||
      instr->ClobbersDoubleRegisters()) {
    return false;
  }
  if (instr->arch_opcode() != ArchOpcode::kArchNop) return false;

  ZoneSet<InstructionOperand, OperandCompare> operands(zone);
  for (size_t i = 0; i < instr->InputCount(); ++i) {
    operands.insert(*instr->InputAt(i));
  }
  for (size_t i = 0; i < instr->OutputCount(); ++i) {
    operands.insert(*instr->OutputAt(i));
  }
  for (size_t i = 0; i < instr->TempCount(); ++i) {
    operands.insert(*instr->TempAt(i));
  }
  for (int i = Instruction::GapPosition::FIRST_GAP_POSITION;
       i <= Instruction::GapPosition::LAST_GAP_POSITION; ++i) {
    ParallelMove* moves = instr->parallel_moves()[i];
    if (moves == nullptr) continue;
    for (MoveOperands* move : *moves) {
      if (operands.count(move->source()) > 0 ||
          operands.count(move->destination()) > 0) {
        return false;
      }
    }
  }
  return true;
}


int FindFirstNonEmptySlot(const Instruction* instr) {
  int i = Instruction::FIRST_GAP_POSITION;
  for (; i <= Instruction::LAST_GAP_POSITION; i++) {
    ParallelMove* moves = instr->parallel_moves()[i];
    if (moves == nullptr) continue;
    for (MoveOperands* move : *moves) {
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
      local_vector_(local_zone) {}


void MoveOptimizer::Run() {
  for (InstructionBlock* block : code()->instruction_blocks()) {
    CompressBlock(block);
  }
  for (InstructionBlock* block : code()->instruction_blocks()) {
    if (block->PredecessorCount() <= 1) continue;
    if (!block->IsDeferred()) {
      bool has_only_deferred = true;
      for (RpoNumber& pred_id : block->predecessors()) {
        if (!code()->InstructionBlockAt(pred_id)->IsDeferred()) {
          has_only_deferred = false;
          break;
        }
      }
      // This would pull down common moves. If the moves occur in deferred
      // blocks, and the closest common successor is not deferred, we lose the
      // optimization of just spilling/filling in deferred blocks, when the
      // current block is not deferred.
      if (has_only_deferred) continue;
    }
    OptimizeMerge(block);
  }
  for (Instruction* gap : to_finalize_) {
    FinalizeMoves(gap);
  }
}


void MoveOptimizer::CompressMoves(ParallelMove* left, ParallelMove* right) {
  if (right == nullptr) return;

  MoveOpVector& eliminated = local_vector();
  DCHECK(eliminated.empty());

  if (!left->empty()) {
    // Modify the right moves in place and collect moves that will be killed by
    // merging the two gaps.
    for (MoveOperands* move : *right) {
      if (move->IsRedundant()) continue;
      MoveOperands* to_eliminate = left->PrepareInsertAfter(move);
      if (to_eliminate != nullptr) eliminated.push_back(to_eliminate);
    }
    // Eliminate dead moves.
    for (MoveOperands* to_eliminate : eliminated) {
      to_eliminate->Eliminate();
    }
    eliminated.clear();
  }
  // Add all possibly modified moves from right side.
  for (MoveOperands* move : *right) {
    if (move->IsRedundant()) continue;
    left->push_back(move);
  }
  // Nuke right.
  right->clear();
  DCHECK(eliminated.empty());
}


// Smash all consecutive moves into the left most move slot and accumulate them
// as much as possible across instructions.
void MoveOptimizer::CompressBlock(InstructionBlock* block) {
  Instruction* prev_instr = nullptr;
  for (int index = block->code_start(); index < block->code_end(); ++index) {
    Instruction* instr = code()->instructions()[index];
    int i = FindFirstNonEmptySlot(instr);
    bool has_moves = i <= Instruction::LAST_GAP_POSITION;

    if (i == Instruction::LAST_GAP_POSITION) {
      std::swap(instr->parallel_moves()[Instruction::FIRST_GAP_POSITION],
                instr->parallel_moves()[Instruction::LAST_GAP_POSITION]);
    } else if (i == Instruction::FIRST_GAP_POSITION) {
      CompressMoves(instr->parallel_moves()[Instruction::FIRST_GAP_POSITION],
                    instr->parallel_moves()[Instruction::LAST_GAP_POSITION]);
    }
    // We either have no moves, or, after swapping or compressing, we have
    // all the moves in the first gap position, and none in the second/end gap
    // position.
    ParallelMove* first =
        instr->parallel_moves()[Instruction::FIRST_GAP_POSITION];
    ParallelMove* last =
        instr->parallel_moves()[Instruction::LAST_GAP_POSITION];
    USE(last);

    DCHECK(!has_moves ||
           (first != nullptr && (last == nullptr || last->empty())));

    if (prev_instr != nullptr) {
      if (has_moves) {
        // Smash first into prev_instr, killing left.
        ParallelMove* pred_moves = prev_instr->parallel_moves()[0];
        CompressMoves(pred_moves, first);
      }
      // Slide prev_instr down so we always know where to look for it.
      std::swap(prev_instr->parallel_moves()[0], instr->parallel_moves()[0]);
    }

    prev_instr = instr->parallel_moves()[0] == nullptr ? nullptr : instr;
    if (GapsCanMoveOver(instr, local_zone())) continue;
    if (prev_instr != nullptr) {
      to_finalize_.push_back(prev_instr);
      prev_instr = nullptr;
    }
  }
  if (prev_instr != nullptr) {
    to_finalize_.push_back(prev_instr);
  }
}


const Instruction* MoveOptimizer::LastInstruction(
    const InstructionBlock* block) const {
  return code()->instructions()[block->last_instruction_index()];
}


void MoveOptimizer::OptimizeMerge(InstructionBlock* block) {
  DCHECK(block->PredecessorCount() > 1);
  // Ensure that the last instruction in all incoming blocks don't contain
  // things that would prevent moving gap moves across them.
  for (RpoNumber& pred_index : block->predecessors()) {
    const InstructionBlock* pred = code()->InstructionBlockAt(pred_index);
    const Instruction* last_instr =
        code()->instructions()[pred->last_instruction_index()];
    if (last_instr->IsCall()) return;
    if (last_instr->TempCount() != 0) return;
    if (last_instr->OutputCount() != 0) return;
    for (size_t i = 0; i < last_instr->InputCount(); ++i) {
      const InstructionOperand* op = last_instr->InputAt(i);
      if (!op->IsConstant() && !op->IsImmediate()) return;
    }
  }
  // TODO(dcarney): pass a ZonePool down for this?
  MoveMap move_map(local_zone());
  size_t correct_counts = 0;
  // Accumulate set of shared moves.
  for (RpoNumber& pred_index : block->predecessors()) {
    const InstructionBlock* pred = code()->InstructionBlockAt(pred_index);
    const Instruction* instr = LastInstruction(pred);
    if (instr->parallel_moves()[0] == nullptr ||
        instr->parallel_moves()[0]->empty()) {
      return;
    }
    for (const MoveOperands* move : *instr->parallel_moves()[0]) {
      if (move->IsRedundant()) continue;
      InstructionOperand src = move->source();
      InstructionOperand dst = move->destination();
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
    if (!GapsCanMoveOver(instr, local_zone()) || !instr->AreMovesRedundant())
      break;
  }
  DCHECK_NOT_NULL(instr);
  bool gap_initialized = true;
  if (instr->parallel_moves()[0] == nullptr ||
      instr->parallel_moves()[0]->empty()) {
    to_finalize_.push_back(instr);
  } else {
    // Will compress after insertion.
    gap_initialized = false;
    std::swap(instr->parallel_moves()[0], instr->parallel_moves()[1]);
  }
  ParallelMove* moves = instr->GetOrCreateParallelMove(
      static_cast<Instruction::GapPosition>(0), code_zone());
  // Delete relevant entries in predecessors and move everything to block.
  bool first_iteration = true;
  for (RpoNumber& pred_index : block->predecessors()) {
    const InstructionBlock* pred = code()->InstructionBlockAt(pred_index);
    for (MoveOperands* move : *LastInstruction(pred)->parallel_moves()[0]) {
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
    CompressMoves(instr->parallel_moves()[0], instr->parallel_moves()[1]);
  }
}


namespace {

bool IsSlot(const InstructionOperand& op) {
  return op.IsStackSlot() || op.IsDoubleStackSlot();
}


bool LoadCompare(const MoveOperands* a, const MoveOperands* b) {
  if (!a->source().EqualsCanonicalized(b->source())) {
    return a->source().CompareCanonicalized(b->source());
  }
  if (IsSlot(a->destination()) && !IsSlot(b->destination())) return false;
  if (!IsSlot(a->destination()) && IsSlot(b->destination())) return true;
  return a->destination().CompareCanonicalized(b->destination());
}

}  // namespace


// Split multiple loads of the same constant or stack slot off into the second
// slot and keep remaining moves in the first slot.
void MoveOptimizer::FinalizeMoves(Instruction* instr) {
  MoveOpVector& loads = local_vector();
  DCHECK(loads.empty());

  // Find all the loads.
  for (MoveOperands* move : *instr->parallel_moves()[0]) {
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
  for (MoveOperands* load : loads) {
    // New group.
    if (group_begin == nullptr ||
        !load->source().EqualsCanonicalized(group_begin->source())) {
      group_begin = load;
      continue;
    }
    // Nothing to be gained from splitting here.
    if (IsSlot(group_begin->destination())) continue;
    // Insert new move into slot 1.
    ParallelMove* slot_1 = instr->GetOrCreateParallelMove(
        static_cast<Instruction::GapPosition>(1), code_zone());
    slot_1->AddMove(group_begin->destination(), load->destination());
    load->Eliminate();
  }
  loads.clear();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
