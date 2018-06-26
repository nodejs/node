// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/register-allocator-verifier.h"

#include "src/bit-vector.h"
#include "src/compiler/instruction.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

size_t OperandCount(const Instruction* instr) {
  return instr->InputCount() + instr->OutputCount() + instr->TempCount();
}

void VerifyEmptyGaps(const Instruction* instr) {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    CHECK_NULL(instr->GetParallelMove(inner_pos));
  }
}

void VerifyAllocatedGaps(const Instruction* instr, const char* caller_info) {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    const ParallelMove* moves = instr->GetParallelMove(inner_pos);
    if (moves == nullptr) continue;
    for (const MoveOperands* move : *moves) {
      if (move->IsRedundant()) continue;
      CHECK_WITH_MSG(
          move->source().IsAllocated() || move->source().IsConstant(),
          caller_info);
      CHECK_WITH_MSG(move->destination().IsAllocated(), caller_info);
    }
  }
}

}  // namespace

RegisterAllocatorVerifier::RegisterAllocatorVerifier(
    Zone* zone, const RegisterConfiguration* config,
    const InstructionSequence* sequence)
    : zone_(zone),
      config_(config),
      sequence_(sequence),
      constraints_(zone),
      assessments_(zone),
      outstanding_assessments_(zone) {
  constraints_.reserve(sequence->instructions().size());
  // TODO(dcarney): model unique constraints.
  // Construct OperandConstraints for all InstructionOperands, eliminating
  // kSameAsFirst along the way.
  for (const Instruction* instr : sequence->instructions()) {
    // All gaps should be totally unallocated at this point.
    VerifyEmptyGaps(instr);
    const size_t operand_count = OperandCount(instr);
    OperandConstraint* op_constraints =
        zone->NewArray<OperandConstraint>(operand_count);
    size_t count = 0;
    for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
      BuildConstraint(instr->InputAt(i), &op_constraints[count]);
      VerifyInput(op_constraints[count]);
    }
    for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
      BuildConstraint(instr->TempAt(i), &op_constraints[count]);
      VerifyTemp(op_constraints[count]);
    }
    for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
      BuildConstraint(instr->OutputAt(i), &op_constraints[count]);
      if (op_constraints[count].type_ == kSameAsFirst) {
        CHECK_LT(0, instr->InputCount());
        op_constraints[count].type_ = op_constraints[0].type_;
        op_constraints[count].value_ = op_constraints[0].value_;
      }
      VerifyOutput(op_constraints[count]);
    }
    InstructionConstraint instr_constraint = {instr, operand_count,
                                              op_constraints};
    constraints()->push_back(instr_constraint);
  }
}

void RegisterAllocatorVerifier::VerifyInput(
    const OperandConstraint& constraint) {
  CHECK_NE(kSameAsFirst, constraint.type_);
  if (constraint.type_ != kImmediate && constraint.type_ != kExplicit) {
    CHECK_NE(InstructionOperand::kInvalidVirtualRegister,
             constraint.virtual_register_);
  }
}

void RegisterAllocatorVerifier::VerifyTemp(
    const OperandConstraint& constraint) {
  CHECK_NE(kSameAsFirst, constraint.type_);
  CHECK_NE(kImmediate, constraint.type_);
  CHECK_NE(kExplicit, constraint.type_);
  CHECK_NE(kConstant, constraint.type_);
}

void RegisterAllocatorVerifier::VerifyOutput(
    const OperandConstraint& constraint) {
  CHECK_NE(kImmediate, constraint.type_);
  CHECK_NE(kExplicit, constraint.type_);
  CHECK_NE(InstructionOperand::kInvalidVirtualRegister,
           constraint.virtual_register_);
}

void RegisterAllocatorVerifier::VerifyAssignment(const char* caller_info) {
  caller_info_ = caller_info;
  CHECK(sequence()->instructions().size() == constraints()->size());
  auto instr_it = sequence()->begin();
  for (const auto& instr_constraint : *constraints()) {
    const Instruction* instr = instr_constraint.instruction_;
    // All gaps should be totally allocated at this point.
    VerifyAllocatedGaps(instr, caller_info_);
    const size_t operand_count = instr_constraint.operand_constaints_size_;
    const OperandConstraint* op_constraints =
        instr_constraint.operand_constraints_;
    CHECK_EQ(instr, *instr_it);
    CHECK(operand_count == OperandCount(instr));
    size_t count = 0;
    for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
      CheckConstraint(instr->InputAt(i), &op_constraints[count]);
    }
    for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
      CheckConstraint(instr->TempAt(i), &op_constraints[count]);
    }
    for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
      CheckConstraint(instr->OutputAt(i), &op_constraints[count]);
    }
    ++instr_it;
  }
}

void RegisterAllocatorVerifier::BuildConstraint(const InstructionOperand* op,
                                                OperandConstraint* constraint) {
  constraint->value_ = kMinInt;
  constraint->virtual_register_ = InstructionOperand::kInvalidVirtualRegister;
  if (op->IsConstant()) {
    constraint->type_ = kConstant;
    constraint->value_ = ConstantOperand::cast(op)->virtual_register();
    constraint->virtual_register_ = constraint->value_;
  } else if (op->IsExplicit()) {
    constraint->type_ = kExplicit;
  } else if (op->IsImmediate()) {
    const ImmediateOperand* imm = ImmediateOperand::cast(op);
    int value = imm->type() == ImmediateOperand::INLINE ? imm->inline_value()
                                                        : imm->indexed_value();
    constraint->type_ = kImmediate;
    constraint->value_ = value;
  } else {
    CHECK(op->IsUnallocated());
    const UnallocatedOperand* unallocated = UnallocatedOperand::cast(op);
    int vreg = unallocated->virtual_register();
    constraint->virtual_register_ = vreg;
    if (unallocated->basic_policy() == UnallocatedOperand::FIXED_SLOT) {
      constraint->type_ = kFixedSlot;
      constraint->value_ = unallocated->fixed_slot_index();
    } else {
      switch (unallocated->extended_policy()) {
        case UnallocatedOperand::REGISTER_OR_SLOT:
        case UnallocatedOperand::NONE:
          if (sequence()->IsFP(vreg)) {
            constraint->type_ = kRegisterOrSlotFP;
          } else {
            constraint->type_ = kRegisterOrSlot;
          }
          break;
        case UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
          DCHECK(!sequence()->IsFP(vreg));
          constraint->type_ = kRegisterOrSlotOrConstant;
          break;
        case UnallocatedOperand::FIXED_REGISTER:
          if (unallocated->HasSecondaryStorage()) {
            constraint->type_ = kRegisterAndSlot;
            constraint->spilled_slot_ = unallocated->GetSecondaryStorage();
          } else {
            constraint->type_ = kFixedRegister;
          }
          constraint->value_ = unallocated->fixed_register_index();
          break;
        case UnallocatedOperand::FIXED_FP_REGISTER:
          constraint->type_ = kFixedFPRegister;
          constraint->value_ = unallocated->fixed_register_index();
          break;
        case UnallocatedOperand::MUST_HAVE_REGISTER:
          if (sequence()->IsFP(vreg)) {
            constraint->type_ = kFPRegister;
          } else {
            constraint->type_ = kRegister;
          }
          break;
        case UnallocatedOperand::MUST_HAVE_SLOT:
          constraint->type_ = kSlot;
          constraint->value_ =
              ElementSizeLog2Of(sequence()->GetRepresentation(vreg));
          break;
        case UnallocatedOperand::SAME_AS_FIRST_INPUT:
          constraint->type_ = kSameAsFirst;
          break;
      }
    }
  }
}

void RegisterAllocatorVerifier::CheckConstraint(
    const InstructionOperand* op, const OperandConstraint* constraint) {
  switch (constraint->type_) {
    case kConstant:
      CHECK_WITH_MSG(op->IsConstant(), caller_info_);
      CHECK_EQ(ConstantOperand::cast(op)->virtual_register(),
               constraint->value_);
      return;
    case kImmediate: {
      CHECK_WITH_MSG(op->IsImmediate(), caller_info_);
      const ImmediateOperand* imm = ImmediateOperand::cast(op);
      int value = imm->type() == ImmediateOperand::INLINE
                      ? imm->inline_value()
                      : imm->indexed_value();
      CHECK_EQ(value, constraint->value_);
      return;
    }
    case kRegister:
      CHECK_WITH_MSG(op->IsRegister(), caller_info_);
      return;
    case kFPRegister:
      CHECK_WITH_MSG(op->IsFPRegister(), caller_info_);
      return;
    case kExplicit:
      CHECK_WITH_MSG(op->IsExplicit(), caller_info_);
      return;
    case kFixedRegister:
    case kRegisterAndSlot:
      CHECK_WITH_MSG(op->IsRegister(), caller_info_);
      CHECK_EQ(LocationOperand::cast(op)->register_code(), constraint->value_);
      return;
    case kFixedFPRegister:
      CHECK_WITH_MSG(op->IsFPRegister(), caller_info_);
      CHECK_EQ(LocationOperand::cast(op)->register_code(), constraint->value_);
      return;
    case kFixedSlot:
      CHECK_WITH_MSG(op->IsStackSlot() || op->IsFPStackSlot(), caller_info_);
      CHECK_EQ(LocationOperand::cast(op)->index(), constraint->value_);
      return;
    case kSlot:
      CHECK_WITH_MSG(op->IsStackSlot() || op->IsFPStackSlot(), caller_info_);
      CHECK_EQ(ElementSizeLog2Of(LocationOperand::cast(op)->representation()),
               constraint->value_);
      return;
    case kRegisterOrSlot:
      CHECK_WITH_MSG(op->IsRegister() || op->IsStackSlot(), caller_info_);
      return;
    case kRegisterOrSlotFP:
      CHECK_WITH_MSG(op->IsFPRegister() || op->IsFPStackSlot(), caller_info_);
      return;
    case kRegisterOrSlotOrConstant:
      CHECK_WITH_MSG(op->IsRegister() || op->IsStackSlot() || op->IsConstant(),
                     caller_info_);
      return;
    case kSameAsFirst:
      CHECK_WITH_MSG(false, caller_info_);
      return;
  }
}

void BlockAssessments::PerformMoves(const Instruction* instruction) {
  const ParallelMove* first =
      instruction->GetParallelMove(Instruction::GapPosition::START);
  PerformParallelMoves(first);
  const ParallelMove* last =
      instruction->GetParallelMove(Instruction::GapPosition::END);
  PerformParallelMoves(last);
}

void BlockAssessments::PerformParallelMoves(const ParallelMove* moves) {
  if (moves == nullptr) return;

  CHECK(map_for_moves_.empty());
  for (MoveOperands* move : *moves) {
    if (move->IsEliminated() || move->IsRedundant()) continue;
    auto it = map_.find(move->source());
    // The RHS of a parallel move should have been already assessed.
    CHECK(it != map_.end());
    // The LHS of a parallel move should not have been assigned in this
    // parallel move.
    CHECK(map_for_moves_.find(move->destination()) == map_for_moves_.end());
    // Copy the assessment to the destination.
    map_for_moves_[move->destination()] = it->second;
  }
  for (auto pair : map_for_moves_) {
    map_[pair.first] = pair.second;
  }
  map_for_moves_.clear();
}

void BlockAssessments::DropRegisters() {
  for (auto iterator = map().begin(), end = map().end(); iterator != end;) {
    auto current = iterator;
    ++iterator;
    InstructionOperand op = current->first;
    if (op.IsAnyRegister()) map().erase(current);
  }
}

void BlockAssessments::Print() const {
  OFStream os(stdout);
  for (const auto pair : map()) {
    const InstructionOperand op = pair.first;
    const Assessment* assessment = pair.second;
    // Use operator<< so we can write the assessment on the same
    // line. Since we need a register configuration, just pick
    // Turbofan for now.
    PrintableInstructionOperand wrapper = {RegisterConfiguration::Default(),
                                           op};
    os << wrapper << " : ";
    if (assessment->kind() == AssessmentKind::Final) {
      os << "v" << FinalAssessment::cast(assessment)->virtual_register();
    } else {
      os << "P";
    }
    os << std::endl;
  }
  os << std::endl;
}

BlockAssessments* RegisterAllocatorVerifier::CreateForBlock(
    const InstructionBlock* block) {
  RpoNumber current_block_id = block->rpo_number();

  BlockAssessments* ret = new (zone()) BlockAssessments(zone());
  if (block->PredecessorCount() == 0) {
    // TODO(mtrofin): the following check should hold, however, in certain
    // unit tests it is invalidated by the last block. Investigate and
    // normalize the CFG.
    // CHECK_EQ(0, current_block_id.ToInt());
    // The phi size test below is because we can, technically, have phi
    // instructions with one argument. Some tests expose that, too.
  } else if (block->PredecessorCount() == 1 && block->phis().size() == 0) {
    const BlockAssessments* prev_block = assessments_[block->predecessors()[0]];
    ret->CopyFrom(prev_block);
  } else {
    for (RpoNumber pred_id : block->predecessors()) {
      // For every operand coming from any of the predecessors, create an
      // Unfinalized assessment.
      auto iterator = assessments_.find(pred_id);
      if (iterator == assessments_.end()) {
        // This block is the head of a loop, and this predecessor is the
        // loopback
        // arc.
        // Validate this is a loop case, otherwise the CFG is malformed.
        CHECK(pred_id >= current_block_id);
        CHECK(block->IsLoopHeader());
        continue;
      }
      const BlockAssessments* pred_assessments = iterator->second;
      CHECK_NOT_NULL(pred_assessments);
      for (auto pair : pred_assessments->map()) {
        InstructionOperand operand = pair.first;
        if (ret->map().find(operand) == ret->map().end()) {
          ret->map().insert(std::make_pair(
              operand, new (zone()) PendingAssessment(zone(), block, operand)));
        }
      }
    }
  }
  return ret;
}

void RegisterAllocatorVerifier::ValidatePendingAssessment(
    RpoNumber block_id, InstructionOperand op,
    const BlockAssessments* current_assessments,
    PendingAssessment* const assessment, int virtual_register) {
  if (assessment->IsAliasOf(virtual_register)) return;

  // When validating a pending assessment, it is possible some of the
  // assessments for the original operand (the one where the assessment was
  // created for first) are also pending. To avoid recursion, we use a work
  // list. To deal with cycles, we keep a set of seen nodes.
  Zone local_zone(zone()->allocator(), ZONE_NAME);
  ZoneQueue<std::pair<const PendingAssessment*, int>> worklist(&local_zone);
  ZoneSet<RpoNumber> seen(&local_zone);
  worklist.push(std::make_pair(assessment, virtual_register));
  seen.insert(block_id);

  while (!worklist.empty()) {
    auto work = worklist.front();
    const PendingAssessment* current_assessment = work.first;
    int current_virtual_register = work.second;
    InstructionOperand current_operand = current_assessment->operand();
    worklist.pop();

    const InstructionBlock* origin = current_assessment->origin();
    CHECK(origin->PredecessorCount() > 1 || origin->phis().size() > 0);

    // Check if the virtual register is a phi first, instead of relying on
    // the incoming assessments. In particular, this handles the case
    // v1 = phi v0 v0, which structurally is identical to v0 having been
    // defined at the top of a diamond, and arriving at the node joining the
    // diamond's branches.
    const PhiInstruction* phi = nullptr;
    for (const PhiInstruction* candidate : origin->phis()) {
      if (candidate->virtual_register() == current_virtual_register) {
        phi = candidate;
        break;
      }
    }

    int op_index = 0;
    for (RpoNumber pred : origin->predecessors()) {
      int expected =
          phi != nullptr ? phi->operands()[op_index] : current_virtual_register;

      ++op_index;
      auto pred_assignment = assessments_.find(pred);
      if (pred_assignment == assessments_.end()) {
        CHECK(origin->IsLoopHeader());
        auto todo_iter = outstanding_assessments_.find(pred);
        DelayedAssessments* set = nullptr;
        if (todo_iter == outstanding_assessments_.end()) {
          set = new (zone()) DelayedAssessments(zone());
          outstanding_assessments_.insert(std::make_pair(pred, set));
        } else {
          set = todo_iter->second;
        }
        set->AddDelayedAssessment(current_operand, expected);
        continue;
      }

      const BlockAssessments* pred_assessments = pred_assignment->second;
      auto found_contribution = pred_assessments->map().find(current_operand);
      CHECK(found_contribution != pred_assessments->map().end());
      Assessment* contribution = found_contribution->second;

      switch (contribution->kind()) {
        case Final:
          CHECK_EQ(FinalAssessment::cast(contribution)->virtual_register(),
                   expected);
          break;
        case Pending: {
          // This happens if we have a diamond feeding into another one, and
          // the inner one never being used - other than for carrying the value.
          const PendingAssessment* next = PendingAssessment::cast(contribution);
          if (seen.find(pred) == seen.end()) {
            worklist.push({next, expected});
            seen.insert(pred);
          }
          // Note that we do not want to finalize pending assessments at the
          // beginning of a block - which is the information we'd have
          // available here. This is because this operand may be reused to
          // define duplicate phis.
          break;
        }
      }
    }
  }
  assessment->AddAlias(virtual_register);
}

void RegisterAllocatorVerifier::ValidateUse(
    RpoNumber block_id, BlockAssessments* current_assessments,
    InstructionOperand op, int virtual_register) {
  auto iterator = current_assessments->map().find(op);
  // We should have seen this operand before.
  CHECK(iterator != current_assessments->map().end());
  Assessment* assessment = iterator->second;

  switch (assessment->kind()) {
    case Final:
      CHECK_EQ(FinalAssessment::cast(assessment)->virtual_register(),
               virtual_register);
      break;
    case Pending: {
      PendingAssessment* pending = PendingAssessment::cast(assessment);
      ValidatePendingAssessment(block_id, op, current_assessments, pending,
                                virtual_register);
      break;
    }
  }
}

void RegisterAllocatorVerifier::VerifyGapMoves() {
  CHECK(assessments_.empty());
  CHECK(outstanding_assessments_.empty());
  const size_t block_count = sequence()->instruction_blocks().size();
  for (size_t block_index = 0; block_index < block_count; ++block_index) {
    const InstructionBlock* block =
        sequence()->instruction_blocks()[block_index];
    BlockAssessments* block_assessments = CreateForBlock(block);

    for (int instr_index = block->code_start(); instr_index < block->code_end();
         ++instr_index) {
      const InstructionConstraint& instr_constraint = constraints_[instr_index];
      const Instruction* instr = instr_constraint.instruction_;
      block_assessments->PerformMoves(instr);

      const OperandConstraint* op_constraints =
          instr_constraint.operand_constraints_;
      size_t count = 0;
      for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
        if (op_constraints[count].type_ == kImmediate ||
            op_constraints[count].type_ == kExplicit) {
          continue;
        }
        int virtual_register = op_constraints[count].virtual_register_;
        InstructionOperand op = *instr->InputAt(i);
        ValidateUse(block->rpo_number(), block_assessments, op,
                    virtual_register);
      }
      for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
        block_assessments->Drop(*instr->TempAt(i));
      }
      if (instr->IsCall()) {
        block_assessments->DropRegisters();
      }
      for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
        int virtual_register = op_constraints[count].virtual_register_;
        block_assessments->AddDefinition(*instr->OutputAt(i), virtual_register);
        if (op_constraints[count].type_ == kRegisterAndSlot) {
          const AllocatedOperand* reg_op =
              AllocatedOperand::cast(instr->OutputAt(i));
          MachineRepresentation rep = reg_op->representation();
          const AllocatedOperand* stack_op = AllocatedOperand::New(
              zone(), LocationOperand::LocationKind::STACK_SLOT, rep,
              op_constraints[i].spilled_slot_);
          block_assessments->AddDefinition(*stack_op, virtual_register);
        }
      }
    }
    // Now commit the assessments for this block. If there are any delayed
    // assessments, ValidatePendingAssessment should see this block, too.
    assessments_[block->rpo_number()] = block_assessments;

    auto todo_iter = outstanding_assessments_.find(block->rpo_number());
    if (todo_iter == outstanding_assessments_.end()) continue;
    DelayedAssessments* todo = todo_iter->second;
    for (auto pair : todo->map()) {
      InstructionOperand op = pair.first;
      int vreg = pair.second;
      auto found_op = block_assessments->map().find(op);
      CHECK(found_op != block_assessments->map().end());
      switch (found_op->second->kind()) {
        case Final:
          CHECK_EQ(FinalAssessment::cast(found_op->second)->virtual_register(),
                   vreg);
          break;
        case Pending:
          ValidatePendingAssessment(block->rpo_number(), op, block_assessments,
                                    PendingAssessment::cast(found_op->second),
                                    vreg);
          break;
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
