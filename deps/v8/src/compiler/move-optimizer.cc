// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/move-optimizer.h"

namespace v8 {
namespace internal {
namespace compiler {

MoveOptimizer::MoveOptimizer(Zone* local_zone, InstructionSequence* code)
    : local_zone_(local_zone),
      code_(code),
      temp_vector_0_(local_zone),
      temp_vector_1_(local_zone) {}


void MoveOptimizer::Run() {
  // First smash all consecutive moves into the left most move slot.
  for (auto* block : code()->instruction_blocks()) {
    GapInstruction* prev_gap = nullptr;
    for (int index = block->code_start(); index < block->code_end(); ++index) {
      auto instr = code()->instructions()[index];
      if (!instr->IsGapMoves()) {
        if (instr->IsSourcePosition() || instr->IsNop()) continue;
        FinalizeMoves(&temp_vector_0_, &temp_vector_1_, prev_gap);
        prev_gap = nullptr;
        continue;
      }
      auto gap = GapInstruction::cast(instr);
      // Find first non-empty slot.
      int i = GapInstruction::FIRST_INNER_POSITION;
      for (; i <= GapInstruction::LAST_INNER_POSITION; i++) {
        auto move = gap->parallel_moves()[i];
        if (move == nullptr) continue;
        auto move_ops = move->move_operands();
        auto op = move_ops->begin();
        for (; op != move_ops->end(); ++op) {
          if (!op->IsRedundant()) break;
        }
        if (op == move_ops->end()) {
          move_ops->Rewind(0);  // Clear this redundant move.
        } else {
          break;  // Found index of first non-redundant move.
        }
      }
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
        CompressMoves(&temp_vector_0_, left, move);
      }
      if (prev_gap != nullptr) {
        // Smash left into prev_gap, killing left.
        auto pred_moves = prev_gap->parallel_moves()[0];
        CompressMoves(&temp_vector_0_, pred_moves, left);
        std::swap(prev_gap->parallel_moves()[0], gap->parallel_moves()[0]);
      }
      prev_gap = gap;
    }
    FinalizeMoves(&temp_vector_0_, &temp_vector_1_, prev_gap);
  }
}


static MoveOperands* PrepareInsertAfter(ParallelMove* left, MoveOperands* move,
                                        Zone* zone) {
  auto move_ops = left->move_operands();
  MoveOperands* replacement = nullptr;
  MoveOperands* to_eliminate = nullptr;
  for (auto curr = move_ops->begin(); curr != move_ops->end(); ++curr) {
    if (curr->IsEliminated()) continue;
    if (curr->destination()->Equals(move->source())) {
      DCHECK_EQ(nullptr, replacement);
      replacement = curr;
      if (to_eliminate != nullptr) break;
    } else if (curr->destination()->Equals(move->destination())) {
      DCHECK_EQ(nullptr, to_eliminate);
      to_eliminate = curr;
      if (replacement != nullptr) break;
    }
  }
  DCHECK(!(replacement == to_eliminate && replacement != nullptr));
  if (replacement != nullptr) {
    auto new_source = new (zone) InstructionOperand(
        replacement->source()->kind(), replacement->source()->index());
    move->set_source(new_source);
  }
  return to_eliminate;
}


void MoveOptimizer::CompressMoves(MoveOpVector* eliminated, ParallelMove* left,
                                  ParallelMove* right) {
  DCHECK(eliminated->empty());
  auto move_ops = right->move_operands();
  // Modify the right moves in place and collect moves that will be killed by
  // merging the two gaps.
  for (auto op = move_ops->begin(); op != move_ops->end(); ++op) {
    if (op->IsRedundant()) continue;
    MoveOperands* to_eliminate = PrepareInsertAfter(left, op, code_zone());
    if (to_eliminate != nullptr) {
      eliminated->push_back(to_eliminate);
    }
  }
  // Eliminate dead moves.  Must happen before insertion of new moves as the
  // contents of eliminated are pointers into a list.
  for (auto to_eliminate : *eliminated) {
    to_eliminate->Eliminate();
  }
  eliminated->clear();
  // Add all possibly modified moves from right side.
  for (auto op = move_ops->begin(); op != move_ops->end(); ++op) {
    if (op->IsRedundant()) continue;
    left->move_operands()->Add(*op, code_zone());
  }
  // Nuke right.
  move_ops->Rewind(0);
}


void MoveOptimizer::FinalizeMoves(MoveOpVector* loads, MoveOpVector* new_moves,
                                  GapInstruction* gap) {
  DCHECK(loads->empty());
  DCHECK(new_moves->empty());
  if (gap == nullptr) return;
  // Split multiple loads of the same constant or stack slot off into the second
  // slot and keep remaining moves in the first slot.
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
    for (auto load : *loads) {
      if (load->source()->Equals(move->source())) {
        found = load;
        break;
      }
    }
    // Not found so insert.
    if (found == nullptr) {
      loads->push_back(move);
      // Replace source with copy for later use.
      auto dest = move->destination();
      move->set_destination(new (code_zone())
                            InstructionOperand(dest->kind(), dest->index()));
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
          new (code_zone()) InstructionOperand(found_kind, found_index);
      auto dest = move->destination();
      found->destination()->ConvertTo(dest->kind(), dest->index());
      move->set_destination(next_dest);
    }
    // move from load destination.
    move->set_source(found->destination());
    new_moves->push_back(move);
  }
  loads->clear();
  if (new_moves->empty()) return;
  // Insert all new moves into slot 1.
  auto slot_1 = gap->GetOrCreateParallelMove(
      static_cast<GapInstruction::InnerPosition>(1), code_zone());
  DCHECK(slot_1->move_operands()->is_empty());
  slot_1->move_operands()->AddBlock(MoveOperands(nullptr, nullptr),
                                    static_cast<int>(new_moves->size()),
                                    code_zone());
  auto it = slot_1->move_operands()->begin();
  for (auto new_move : *new_moves) {
    std::swap(*new_move, *it);
    ++it;
  }
  DCHECK_EQ(it, slot_1->move_operands()->end());
  new_moves->clear();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
