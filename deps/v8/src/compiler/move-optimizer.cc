// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/move-optimizer.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

struct MoveKey {
  InstructionOperand source;
  InstructionOperand destination;
};

struct MoveKeyCompare {
  bool operator()(const MoveKey& a, const MoveKey& b) const {
    if (a.source.EqualsCanonicalized(b.source)) {
      return a.destination.CompareCanonicalized(b.destination);
    }
    return a.source.CompareCanonicalized(b.source);
  }
};

typedef ZoneMap<MoveKey, unsigned, MoveKeyCompare> MoveMap;

class OperandSet {
 public:
  explicit OperandSet(ZoneVector<InstructionOperand>* buffer)
      : set_(buffer), fp_reps_(0) {
    buffer->clear();
  }

  void InsertOp(const InstructionOperand& op) {
    set_->push_back(op);

    if (!kSimpleFPAliasing && op.IsFPRegister())
      fp_reps_ |= RepresentationBit(LocationOperand::cast(op).representation());
  }

  bool Contains(const InstructionOperand& op) const {
    for (const InstructionOperand& elem : *set_) {
      if (elem.EqualsCanonicalized(op)) return true;
    }
    return false;
  }

  bool ContainsOpOrAlias(const InstructionOperand& op) const {
    if (Contains(op)) return true;

    if (!kSimpleFPAliasing && op.IsFPRegister()) {
      // Platforms where FP registers have complex aliasing need extra checks.
      const LocationOperand& loc = LocationOperand::cast(op);
      MachineRepresentation rep = loc.representation();
      // If haven't encountered mixed rep FP registers, skip the extra checks.
      if (!HasMixedFPReps(fp_reps_ | RepresentationBit(rep))) return false;

      // Check register against aliasing registers of other FP representations.
      MachineRepresentation other_rep1, other_rep2;
      switch (rep) {
        case MachineRepresentation::kFloat32:
          other_rep1 = MachineRepresentation::kFloat64;
          other_rep2 = MachineRepresentation::kSimd128;
          break;
        case MachineRepresentation::kFloat64:
          other_rep1 = MachineRepresentation::kFloat32;
          other_rep2 = MachineRepresentation::kSimd128;
          break;
        case MachineRepresentation::kSimd128:
          other_rep1 = MachineRepresentation::kFloat32;
          other_rep2 = MachineRepresentation::kFloat64;
          break;
        default:
          UNREACHABLE();
          break;
      }
      const RegisterConfiguration* config = RegisterConfiguration::Default();
      int base = -1;
      int aliases =
          config->GetAliases(rep, loc.register_code(), other_rep1, &base);
      DCHECK(aliases > 0 || (aliases == 0 && base == -1));
      while (aliases--) {
        if (Contains(AllocatedOperand(LocationOperand::REGISTER, other_rep1,
                                      base + aliases))) {
          return true;
        }
      }
      aliases = config->GetAliases(rep, loc.register_code(), other_rep2, &base);
      DCHECK(aliases > 0 || (aliases == 0 && base == -1));
      while (aliases--) {
        if (Contains(AllocatedOperand(LocationOperand::REGISTER, other_rep2,
                                      base + aliases))) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  static bool HasMixedFPReps(int reps) {
    return reps && !base::bits::IsPowerOfTwo(reps);
  }

  ZoneVector<InstructionOperand>* set_;
  int fp_reps_;
};

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
      local_vector_(local_zone),
      operand_buffer1(local_zone),
      operand_buffer2(local_zone) {}

void MoveOptimizer::Run() {
  for (Instruction* instruction : code()->instructions()) {
    CompressGaps(instruction);
  }
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
  for (Instruction* gap : code()->instructions()) {
    FinalizeMoves(gap);
  }
}

void MoveOptimizer::RemoveClobberedDestinations(Instruction* instruction) {
  if (instruction->IsCall()) return;
  ParallelMove* moves = instruction->parallel_moves()[0];
  if (moves == nullptr) return;

  DCHECK(instruction->parallel_moves()[1] == nullptr ||
         instruction->parallel_moves()[1]->empty());

  OperandSet outputs(&operand_buffer1);
  OperandSet inputs(&operand_buffer2);

  // Outputs and temps are treated together as potentially clobbering a
  // destination operand.
  for (size_t i = 0; i < instruction->OutputCount(); ++i) {
    outputs.InsertOp(*instruction->OutputAt(i));
  }
  for (size_t i = 0; i < instruction->TempCount(); ++i) {
    outputs.InsertOp(*instruction->TempAt(i));
  }

  // Input operands block elisions.
  for (size_t i = 0; i < instruction->InputCount(); ++i) {
    inputs.InsertOp(*instruction->InputAt(i));
  }

  // Elide moves made redundant by the instruction.
  for (MoveOperands* move : *moves) {
    if (outputs.ContainsOpOrAlias(move->destination()) &&
        !inputs.ContainsOpOrAlias(move->destination())) {
      move->Eliminate();
    }
  }

  // The ret instruction makes any assignment before it unnecessary, except for
  // the one for its input.
  if (instruction->IsRet() || instruction->IsTailCall()) {
    for (MoveOperands* move : *moves) {
      if (!inputs.ContainsOpOrAlias(move->destination())) {
        move->Eliminate();
      }
    }
  }
}

void MoveOptimizer::MigrateMoves(Instruction* to, Instruction* from) {
  if (from->IsCall()) return;

  ParallelMove* from_moves = from->parallel_moves()[0];
  if (from_moves == nullptr || from_moves->empty()) return;

  OperandSet dst_cant_be(&operand_buffer1);
  OperandSet src_cant_be(&operand_buffer2);

  // If an operand is an input to the instruction, we cannot move assignments
  // where it appears on the LHS.
  for (size_t i = 0; i < from->InputCount(); ++i) {
    dst_cant_be.InsertOp(*from->InputAt(i));
  }
  // If an operand is output to the instruction, we cannot move assignments
  // where it appears on the RHS, because we would lose its value before the
  // instruction.
  // Same for temp operands.
  // The output can't appear on the LHS because we performed
  // RemoveClobberedDestinations for the "from" instruction.
  for (size_t i = 0; i < from->OutputCount(); ++i) {
    src_cant_be.InsertOp(*from->OutputAt(i));
  }
  for (size_t i = 0; i < from->TempCount(); ++i) {
    src_cant_be.InsertOp(*from->TempAt(i));
  }
  for (MoveOperands* move : *from_moves) {
    if (move->IsRedundant()) continue;
    // Assume dest has a value "V". If we have a "dest = y" move, then we can't
    // move "z = dest", because z would become y rather than "V".
    // We assume CompressMoves has happened before this, which means we don't
    // have more than one assignment to dest.
    src_cant_be.InsertOp(move->destination());
  }

  ZoneSet<MoveKey, MoveKeyCompare> move_candidates(local_zone());
  // We start with all the moves that don't have conflicting source or
  // destination operands are eligible for being moved down.
  for (MoveOperands* move : *from_moves) {
    if (move->IsRedundant()) continue;
    if (!dst_cant_be.ContainsOpOrAlias(move->destination())) {
      MoveKey key = {move->source(), move->destination()};
      move_candidates.insert(key);
    }
  }
  if (move_candidates.empty()) return;

  // Stabilize the candidate set.
  bool changed = false;
  do {
    changed = false;
    for (auto iter = move_candidates.begin(); iter != move_candidates.end();) {
      auto current = iter;
      ++iter;
      InstructionOperand src = current->source;
      if (src_cant_be.ContainsOpOrAlias(src)) {
        src_cant_be.InsertOp(current->destination);
        move_candidates.erase(current);
        changed = true;
      }
    }
  } while (changed);

  ParallelMove to_move(local_zone());
  for (MoveOperands* move : *from_moves) {
    if (move->IsRedundant()) continue;
    MoveKey key = {move->source(), move->destination()};
    if (move_candidates.find(key) != move_candidates.end()) {
      to_move.AddMove(move->source(), move->destination(), code_zone());
      move->Eliminate();
    }
  }
  if (to_move.empty()) return;

  ParallelMove* dest =
      to->GetOrCreateParallelMove(Instruction::GapPosition::START, code_zone());

  CompressMoves(&to_move, dest);
  DCHECK(dest->empty());
  for (MoveOperands* m : to_move) {
    dest->push_back(m);
  }
}

void MoveOptimizer::CompressMoves(ParallelMove* left, MoveOpVector* right) {
  if (right == nullptr) return;

  MoveOpVector& eliminated = local_vector();
  DCHECK(eliminated.empty());

  if (!left->empty()) {
    // Modify the right moves in place and collect moves that will be killed by
    // merging the two gaps.
    for (MoveOperands* move : *right) {
      if (move->IsRedundant()) continue;
      left->PrepareInsertAfter(move, &eliminated);
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

void MoveOptimizer::CompressGaps(Instruction* instruction) {
  int i = FindFirstNonEmptySlot(instruction);
  bool has_moves = i <= Instruction::LAST_GAP_POSITION;
  USE(has_moves);

  if (i == Instruction::LAST_GAP_POSITION) {
    std::swap(instruction->parallel_moves()[Instruction::FIRST_GAP_POSITION],
              instruction->parallel_moves()[Instruction::LAST_GAP_POSITION]);
  } else if (i == Instruction::FIRST_GAP_POSITION) {
    CompressMoves(
        instruction->parallel_moves()[Instruction::FIRST_GAP_POSITION],
        instruction->parallel_moves()[Instruction::LAST_GAP_POSITION]);
  }
  // We either have no moves, or, after swapping or compressing, we have
  // all the moves in the first gap position, and none in the second/end gap
  // position.
  ParallelMove* first =
      instruction->parallel_moves()[Instruction::FIRST_GAP_POSITION];
  ParallelMove* last =
      instruction->parallel_moves()[Instruction::LAST_GAP_POSITION];
  USE(first);
  USE(last);

  DCHECK(!has_moves ||
         (first != nullptr && (last == nullptr || last->empty())));
}

void MoveOptimizer::CompressBlock(InstructionBlock* block) {
  int first_instr_index = block->first_instruction_index();
  int last_instr_index = block->last_instruction_index();

  // Start by removing gap assignments where the output of the subsequent
  // instruction appears on LHS, as long as they are not needed by its input.
  Instruction* prev_instr = code()->instructions()[first_instr_index];
  RemoveClobberedDestinations(prev_instr);

  for (int index = first_instr_index + 1; index <= last_instr_index; ++index) {
    Instruction* instr = code()->instructions()[index];
    // Migrate to the gap of prev_instr eligible moves from instr.
    MigrateMoves(instr, prev_instr);
    // Remove gap assignments clobbered by instr's output.
    RemoveClobberedDestinations(instr);
    prev_instr = instr;
  }
}


const Instruction* MoveOptimizer::LastInstruction(
    const InstructionBlock* block) const {
  return code()->instructions()[block->last_instruction_index()];
}


void MoveOptimizer::OptimizeMerge(InstructionBlock* block) {
  DCHECK_LT(1, block->PredecessorCount());
  // Ensure that the last instruction in all incoming blocks don't contain
  // things that would prevent moving gap moves across them.
  for (RpoNumber& pred_index : block->predecessors()) {
    const InstructionBlock* pred = code()->InstructionBlockAt(pred_index);

    // If the predecessor has more than one successor, we shouldn't attempt to
    // move down to this block (one of the successors) any of the gap moves,
    // because their effect may be necessary to the other successors.
    if (pred->SuccessorCount() > 1) return;

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
  // TODO(dcarney): pass a ZoneStats down for this?
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
  if (move_map.empty() || correct_counts == 0) return;

  // Find insertion point.
  Instruction* instr = code()->instructions()[block->first_instruction_index()];

  if (correct_counts != move_map.size()) {
    // Moves that are unique to each predecessor won't be pushed to the common
    // successor.
    OperandSet conflicting_srcs(&operand_buffer1);
    for (auto iter = move_map.begin(), end = move_map.end(); iter != end;) {
      auto current = iter;
      ++iter;
      if (current->second != block->PredecessorCount()) {
        InstructionOperand dest = current->first.destination;
        // Not all the moves in all the gaps are the same. Maybe some are. If
        // there are such moves, we could move them, but the destination of the
        // moves staying behind can't appear as a source of a common move,
        // because the move staying behind will clobber this destination.
        conflicting_srcs.InsertOp(dest);
        move_map.erase(current);
      }
    }

    bool changed = false;
    do {
      // If a common move can't be pushed to the common successor, then its
      // destination also can't appear as source to any move being pushed.
      changed = false;
      for (auto iter = move_map.begin(), end = move_map.end(); iter != end;) {
        auto current = iter;
        ++iter;
        DCHECK_EQ(block->PredecessorCount(), current->second);
        if (conflicting_srcs.ContainsOpOrAlias(current->first.source)) {
          conflicting_srcs.InsertOp(current->first.destination);
          move_map.erase(current);
          changed = true;
        }
      }
    } while (changed);
  }

  if (move_map.empty()) return;

  DCHECK_NOT_NULL(instr);
  bool gap_initialized = true;
  if (instr->parallel_moves()[0] != nullptr &&
      !instr->parallel_moves()[0]->empty()) {
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
      if (it != move_map.end()) {
        if (first_iteration) {
          moves->AddMove(move->source(), move->destination());
        }
        move->Eliminate();
      }
    }
    first_iteration = false;
  }
  // Compress.
  if (!gap_initialized) {
    CompressMoves(instr->parallel_moves()[0], instr->parallel_moves()[1]);
  }
  CompressBlock(block);
}


namespace {

bool IsSlot(const InstructionOperand& op) {
  return op.IsStackSlot() || op.IsFPStackSlot();
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

  ParallelMove* parallel_moves = instr->parallel_moves()[0];
  if (parallel_moves == nullptr) return;
  // Find all the loads.
  for (MoveOperands* move : *parallel_moves) {
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
