// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/move-optimizer.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

typedef std::pair<InstructionOperand, InstructionOperand> MoveKey;
typedef ZoneMap<MoveKey, unsigned> MoveMap;
typedef ZoneSet<InstructionOperand> OperandSet;


bool GapsCanMoveOver(Instruction* instr) {
  DCHECK(!instr->IsGapMoves());
  return instr->IsSourcePosition() || instr->IsNop();
}


int FindFirstNonEmptySlot(GapInstruction* gap) {
  int i = GapInstruction::FIRST_INNER_POSITION;
  for (; i <= GapInstruction::LAST_INNER_POSITION; i++) {
    auto move = gap->parallel_moves()[i];
    if (move == nullptr) continue;
    auto move_ops = move->move_operands();
    auto op = move_ops->begin();
    for (; op != move_ops->end(); ++op) {
      if (!op->IsRedundant()) break;
      op->Eliminate();
    }
    if (op != move_ops->end()) break;  // Found non-redundant move.
    move_ops->Rewind(0);               // Clear this redundant move.
  }
  return i;
}

}  // namepace


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
  auto move_ops = right->move_operands();
  if (!left->move_operands()->is_empty()) {
    // Modify the right moves in place and collect moves that will be killed by
    // merging the two gaps.
    for (auto op = move_ops->begin(); op != move_ops->end(); ++op) {
      if (op->IsRedundant()) continue;
      auto to_eliminate = left->PrepareInsertAfter(op);
      if (to_eliminate != nullptr) eliminated->push_back(to_eliminate);
    }
    // Eliminate dead moves.  Must happen before insertion of new moves as the
    // contents of eliminated are pointers into a list.
    for (auto to_eliminate : *eliminated) {
      to_eliminate->Eliminate();
    }
    eliminated->clear();
  }
  // Add all possibly modified moves from right side.
  for (auto op = move_ops->begin(); op != move_ops->end(); ++op) {
    if (op->IsRedundant()) continue;
    left->move_operands()->Add(*op, code_zone());
  }
  // Nuke right.
  move_ops->Rewind(0);
}


// Smash all consecutive moves into the left most move slot and accumulate them
// as much as possible across instructions.
void MoveOptimizer::CompressBlock(InstructionBlock* block) {
  auto temp_vector = temp_vector_0();
  DCHECK(temp_vector.empty());
  GapInstruction* prev_gap = nullptr;
  for (int index = block->code_start(); index < block->code_end(); ++index) {
    auto instr = code()->instructions()[index];
    if (!instr->IsGapMoves()) {
      if (GapsCanMoveOver(instr)) continue;
      if (prev_gap != nullptr) to_finalize_.push_back(prev_gap);
      prev_gap = nullptr;
      continue;
    }
    auto gap = GapInstruction::cast(instr);
    int i = FindFirstNonEmptySlot(gap);
    // Nothing to do here.
    if (i == GapInstruction::LAST_INNER_POSITION + 1) {
      if (prev_gap != nullptr) {
        // Slide prev_gap down so we always know where to look for it.
        std::swap(prev_gap->parallel_moves()[0], gap->parallel_moves()[0]);
        prev_gap = gap;
      }
      continue;
    }
    // Move the first non-empty gap to position 0.
    std::swap(gap->parallel_moves()[0], gap->parallel_moves()[i]);
    auto left = gap->parallel_moves()[0];
    // Compress everything into position 0.
    for (++i; i <= GapInstruction::LAST_INNER_POSITION; ++i) {
      auto move = gap->parallel_moves()[i];
      if (move == nullptr) continue;
      CompressMoves(&temp_vector, left, move);
    }
    if (prev_gap != nullptr) {
      // Smash left into prev_gap, killing left.
      auto pred_moves = prev_gap->parallel_moves()[0];
      CompressMoves(&temp_vector, pred_moves, left);
      // Slide prev_gap down so we always know where to look for it.
      std::swap(prev_gap->parallel_moves()[0], gap->parallel_moves()[0]);
    }
    prev_gap = gap;
  }
  if (prev_gap != nullptr) to_finalize_.push_back(prev_gap);
}


GapInstruction* MoveOptimizer::LastGap(InstructionBlock* block) {
  int gap_index = block->last_instruction_index() - 1;
  auto instr = code()->instructions()[gap_index];
  return GapInstruction::cast(instr);
}


void MoveOptimizer::OptimizeMerge(InstructionBlock* block) {
  DCHECK(block->PredecessorCount() > 1);
  // Ensure that the last instruction in all incoming blocks don't contain
  // things that would prevent moving gap moves across them.
  for (auto pred_index : block->predecessors()) {
    auto pred = code()->InstructionBlockAt(pred_index);
    auto last_instr = code()->instructions()[pred->last_instruction_index()];
    DCHECK(!last_instr->IsGapMoves());
    if (last_instr->IsSourcePosition()) continue;
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
    auto gap = LastGap(pred);
    if (gap->parallel_moves()[0] == nullptr ||
        gap->parallel_moves()[0]->move_operands()->is_empty()) {
      return;
    }
    auto move_ops = gap->parallel_moves()[0]->move_operands();
    for (auto op = move_ops->begin(); op != move_ops->end(); ++op) {
      if (op->IsRedundant()) continue;
      auto src = *op->source();
      auto dst = *op->destination();
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
  GapInstruction* gap = nullptr;
  for (int i = block->first_instruction_index();
       i <= block->last_instruction_index(); ++i) {
    auto instr = code()->instructions()[i];
    if (instr->IsGapMoves()) {
      gap = GapInstruction::cast(instr);
      continue;
    }
    if (!GapsCanMoveOver(instr)) break;
  }
  DCHECK(gap != nullptr);
  bool gap_initialized = true;
  if (gap->parallel_moves()[0] == nullptr ||
      gap->parallel_moves()[0]->move_operands()->is_empty()) {
    to_finalize_.push_back(gap);
  } else {
    // Will compress after insertion.
    gap_initialized = false;
    std::swap(gap->parallel_moves()[0], gap->parallel_moves()[1]);
  }
  auto move = gap->GetOrCreateParallelMove(
      static_cast<GapInstruction::InnerPosition>(0), code_zone());
  // Delete relevant entries in predecessors and move everything to block.
  bool first_iteration = true;
  for (auto pred_index : block->predecessors()) {
    auto pred = code()->InstructionBlockAt(pred_index);
    auto gap = LastGap(pred);
    auto move_ops = gap->parallel_moves()[0]->move_operands();
    for (auto op = move_ops->begin(); op != move_ops->end(); ++op) {
      if (op->IsRedundant()) continue;
      MoveKey key = {*op->source(), *op->destination()};
      auto it = move_map.find(key);
      USE(it);
      DCHECK(it != move_map.end());
      if (first_iteration) {
        move->AddMove(op->source(), op->destination(), code_zone());
      }
      op->Eliminate();
    }
    first_iteration = false;
  }
  // Compress.
  if (!gap_initialized) {
    CompressMoves(&temp_vector_0(), gap->parallel_moves()[0],
                  gap->parallel_moves()[1]);
  }
}


// Split multiple loads of the same constant or stack slot off into the second
// slot and keep remaining moves in the first slot.
void MoveOptimizer::FinalizeMoves(GapInstruction* gap) {
  auto loads = temp_vector_0();
  DCHECK(loads.empty());
  auto new_moves = temp_vector_1();
  DCHECK(new_moves.empty());
  auto move_ops = gap->parallel_moves()[0]->move_operands();
  for (auto move = move_ops->begin(); move != move_ops->end(); ++move) {
    if (move->IsRedundant()) {
      move->Eliminate();
      continue;
    }
    if (!(move->source()->IsConstant() || move->source()->IsStackSlot() ||
          move->source()->IsDoubleStackSlot()))
      continue;
    // Search for existing move to this slot.
    MoveOperands* found = nullptr;
    for (auto load : loads) {
      if (load->source()->Equals(move->source())) {
        found = load;
        break;
      }
    }
    // Not found so insert.
    if (found == nullptr) {
      loads.push_back(move);
      // Replace source with copy for later use.
      auto dest = move->destination();
      move->set_destination(
          InstructionOperand::New(code_zone(), dest->kind(), dest->index()));
      continue;
    }
    if ((found->destination()->IsStackSlot() ||
         found->destination()->IsDoubleStackSlot()) &&
        !(move->destination()->IsStackSlot() ||
          move->destination()->IsDoubleStackSlot())) {
      // Found a better source for this load.  Smash it in place to affect other
      // loads that have already been split.
      InstructionOperand::Kind found_kind = found->destination()->kind();
      int found_index = found->destination()->index();
      auto next_dest =
          InstructionOperand::New(code_zone(), found_kind, found_index);
      auto dest = move->destination();
      found->destination()->ConvertTo(dest->kind(), dest->index());
      move->set_destination(next_dest);
    }
    // move from load destination.
    move->set_source(found->destination());
    new_moves.push_back(move);
  }
  loads.clear();
  if (new_moves.empty()) return;
  // Insert all new moves into slot 1.
  auto slot_1 = gap->GetOrCreateParallelMove(
      static_cast<GapInstruction::InnerPosition>(1), code_zone());
  DCHECK(slot_1->move_operands()->is_empty());
  slot_1->move_operands()->AddBlock(MoveOperands(nullptr, nullptr),
                                    static_cast<int>(new_moves.size()),
                                    code_zone());
  auto it = slot_1->move_operands()->begin();
  for (auto new_move : new_moves) {
    std::swap(*new_move, *it);
    ++it;
  }
  DCHECK_EQ(it, slot_1->move_operands()->end());
  new_moves.clear();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
