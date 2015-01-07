// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction.h"
#include "src/compiler/register-allocator-verifier.h"

namespace v8 {
namespace internal {
namespace compiler {

static size_t OperandCount(const Instruction* instr) {
  return instr->InputCount() + instr->OutputCount() + instr->TempCount();
}


static void VerifyGapEmpty(const GapInstruction* gap) {
  for (int i = GapInstruction::FIRST_INNER_POSITION;
       i <= GapInstruction::LAST_INNER_POSITION; i++) {
    GapInstruction::InnerPosition inner_pos =
        static_cast<GapInstruction::InnerPosition>(i);
    CHECK_EQ(NULL, gap->GetParallelMove(inner_pos));
  }
}


void RegisterAllocatorVerifier::VerifyInput(
    const OperandConstraint& constraint) {
  CHECK_NE(kSameAsFirst, constraint.type_);
  if (constraint.type_ != kImmediate) {
    CHECK_NE(UnallocatedOperand::kInvalidVirtualRegister,
             constraint.virtual_register_);
  }
}


void RegisterAllocatorVerifier::VerifyTemp(
    const OperandConstraint& constraint) {
  CHECK_NE(kSameAsFirst, constraint.type_);
  CHECK_NE(kImmediate, constraint.type_);
  CHECK_NE(kConstant, constraint.type_);
  CHECK_EQ(UnallocatedOperand::kInvalidVirtualRegister,
           constraint.virtual_register_);
}


void RegisterAllocatorVerifier::VerifyOutput(
    const OperandConstraint& constraint) {
  CHECK_NE(kImmediate, constraint.type_);
  CHECK_NE(UnallocatedOperand::kInvalidVirtualRegister,
           constraint.virtual_register_);
}


RegisterAllocatorVerifier::RegisterAllocatorVerifier(
    Zone* zone, const RegisterConfiguration* config,
    const InstructionSequence* sequence)
    : zone_(zone), config_(config), sequence_(sequence), constraints_(zone) {
  constraints_.reserve(sequence->instructions().size());
  // TODO(dcarney): model unique constraints.
  // Construct OperandConstraints for all InstructionOperands, eliminating
  // kSameAsFirst along the way.
  for (const auto* instr : sequence->instructions()) {
    const size_t operand_count = OperandCount(instr);
    auto* op_constraints =
        zone->NewArray<OperandConstraint>(static_cast<int>(operand_count));
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
        CHECK(instr->InputCount() > 0);
        op_constraints[count].type_ = op_constraints[0].type_;
        op_constraints[count].value_ = op_constraints[0].value_;
      }
      VerifyOutput(op_constraints[count]);
    }
    // All gaps should be totally unallocated at this point.
    if (instr->IsGapMoves()) {
      CHECK(operand_count == 0);
      VerifyGapEmpty(GapInstruction::cast(instr));
    }
    InstructionConstraint instr_constraint = {instr, operand_count,
                                              op_constraints};
    constraints()->push_back(instr_constraint);
  }
}


void RegisterAllocatorVerifier::VerifyAssignment() {
  CHECK(sequence()->instructions().size() == constraints()->size());
  auto instr_it = sequence()->begin();
  for (const auto& instr_constraint : *constraints()) {
    const auto* instr = instr_constraint.instruction_;
    const size_t operand_count = instr_constraint.operand_constaints_size_;
    const auto* op_constraints = instr_constraint.operand_constraints_;
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
  constraint->virtual_register_ = UnallocatedOperand::kInvalidVirtualRegister;
  if (op->IsConstant()) {
    constraint->type_ = kConstant;
    constraint->value_ = ConstantOperand::cast(op)->index();
    constraint->virtual_register_ = constraint->value_;
  } else if (op->IsImmediate()) {
    constraint->type_ = kImmediate;
    constraint->value_ = ImmediateOperand::cast(op)->index();
  } else {
    CHECK(op->IsUnallocated());
    const auto* unallocated = UnallocatedOperand::cast(op);
    int vreg = unallocated->virtual_register();
    constraint->virtual_register_ = vreg;
    if (unallocated->basic_policy() == UnallocatedOperand::FIXED_SLOT) {
      constraint->type_ = kFixedSlot;
      constraint->value_ = unallocated->fixed_slot_index();
    } else {
      switch (unallocated->extended_policy()) {
        case UnallocatedOperand::ANY:
          CHECK(false);
          break;
        case UnallocatedOperand::NONE:
          if (sequence()->IsDouble(vreg)) {
            constraint->type_ = kNoneDouble;
          } else {
            constraint->type_ = kNone;
          }
          break;
        case UnallocatedOperand::FIXED_REGISTER:
          constraint->type_ = kFixedRegister;
          constraint->value_ = unallocated->fixed_register_index();
          break;
        case UnallocatedOperand::FIXED_DOUBLE_REGISTER:
          constraint->type_ = kFixedDoubleRegister;
          constraint->value_ = unallocated->fixed_register_index();
          break;
        case UnallocatedOperand::MUST_HAVE_REGISTER:
          if (sequence()->IsDouble(vreg)) {
            constraint->type_ = kDoubleRegister;
          } else {
            constraint->type_ = kRegister;
          }
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
      CHECK(op->IsConstant());
      CHECK_EQ(op->index(), constraint->value_);
      return;
    case kImmediate:
      CHECK(op->IsImmediate());
      CHECK_EQ(op->index(), constraint->value_);
      return;
    case kRegister:
      CHECK(op->IsRegister());
      return;
    case kFixedRegister:
      CHECK(op->IsRegister());
      CHECK_EQ(op->index(), constraint->value_);
      return;
    case kDoubleRegister:
      CHECK(op->IsDoubleRegister());
      return;
    case kFixedDoubleRegister:
      CHECK(op->IsDoubleRegister());
      CHECK_EQ(op->index(), constraint->value_);
      return;
    case kFixedSlot:
      CHECK(op->IsStackSlot());
      CHECK_EQ(op->index(), constraint->value_);
      return;
    case kNone:
      CHECK(op->IsRegister() || op->IsStackSlot());
      return;
    case kNoneDouble:
      CHECK(op->IsDoubleRegister() || op->IsDoubleStackSlot());
      return;
    case kSameAsFirst:
      CHECK(false);
      return;
  }
}


class RegisterAllocatorVerifier::OutgoingMapping : public ZoneObject {
 public:
  struct OperandLess {
    bool operator()(const InstructionOperand* a,
                    const InstructionOperand* b) const {
      if (a->kind() == b->kind()) return a->index() < b->index();
      return a->kind() < b->kind();
    }
  };

  typedef std::map<
      const InstructionOperand*, int, OperandLess,
      zone_allocator<std::pair<const InstructionOperand*, const int>>>
      LocationMap;

  explicit OutgoingMapping(Zone* zone)
      : locations_(LocationMap::key_compare(),
                   LocationMap::allocator_type(zone)),
        predecessor_intersection_(LocationMap::key_compare(),
                                  LocationMap::allocator_type(zone)) {}

  LocationMap* locations() { return &locations_; }

  void RunPhis(const InstructionSequence* sequence,
               const InstructionBlock* block, size_t phi_index) {
    // This operation is only valid in edge split form.
    size_t predecessor_index = block->predecessors()[phi_index].ToSize();
    CHECK(sequence->instruction_blocks()[predecessor_index]->SuccessorCount() ==
          1);
    for (const auto* phi : block->phis()) {
      auto input = phi->inputs()[phi_index];
      CHECK(locations()->find(input) != locations()->end());
      auto it = locations()->find(phi->output());
      CHECK(it != locations()->end());
      if (input->IsConstant()) {
        CHECK_EQ(it->second, input->index());
      } else {
        CHECK_EQ(it->second, phi->operands()[phi_index]);
      }
      it->second = phi->virtual_register();
    }
  }

  void RunGapInstruction(Zone* zone, const GapInstruction* gap) {
    for (int i = GapInstruction::FIRST_INNER_POSITION;
         i <= GapInstruction::LAST_INNER_POSITION; i++) {
      GapInstruction::InnerPosition inner_pos =
          static_cast<GapInstruction::InnerPosition>(i);
      const ParallelMove* move = gap->GetParallelMove(inner_pos);
      if (move == nullptr) continue;
      RunParallelMoves(zone, move);
    }
  }

  void RunParallelMoves(Zone* zone, const ParallelMove* move) {
    // Compute outgoing mappings.
    LocationMap to_insert((LocationMap::key_compare()),
                          LocationMap::allocator_type(zone));
    auto* moves = move->move_operands();
    for (auto i = moves->begin(); i != moves->end(); ++i) {
      if (i->IsEliminated()) continue;
      auto cur = locations()->find(i->source());
      CHECK(cur != locations()->end());
      to_insert.insert(std::make_pair(i->destination(), cur->second));
    }
    // Drop current mappings.
    for (auto i = moves->begin(); i != moves->end(); ++i) {
      if (i->IsEliminated()) continue;
      auto cur = locations()->find(i->destination());
      if (cur != locations()->end()) locations()->erase(cur);
    }
    // Insert new values.
    locations()->insert(to_insert.begin(), to_insert.end());
  }

  void Map(const InstructionOperand* op, int virtual_register) {
    locations()->insert(std::make_pair(op, virtual_register));
  }

  void Drop(const InstructionOperand* op) {
    auto it = locations()->find(op);
    if (it != locations()->end()) locations()->erase(it);
  }

  void DropRegisters(const RegisterConfiguration* config) {
    for (int i = 0; i < config->num_general_registers(); ++i) {
      InstructionOperand op(InstructionOperand::REGISTER, i);
      Drop(&op);
    }
    for (int i = 0; i < config->num_double_registers(); ++i) {
      InstructionOperand op(InstructionOperand::DOUBLE_REGISTER, i);
      Drop(&op);
    }
  }

  void InitializeFromFirstPredecessor(const InstructionSequence* sequence,
                                      const OutgoingMappings* outgoing_mappings,
                                      const InstructionBlock* block) {
    if (block->predecessors().empty()) return;
    size_t predecessor_index = block->predecessors()[0].ToSize();
    CHECK(predecessor_index < block->rpo_number().ToSize());
    auto* incoming = outgoing_mappings->at(predecessor_index);
    if (block->PredecessorCount() > 1) {
      // Update incoming map with phis. The remaining phis will be checked later
      // as their mappings are not guaranteed to exist yet.
      incoming->RunPhis(sequence, block, 0);
    }
    // Now initialize outgoing mapping for this block with incoming mapping.
    CHECK(locations_.empty());
    locations_ = incoming->locations_;
  }

  void InitializeFromIntersection() { locations_ = predecessor_intersection_; }

  void InitializeIntersection(const OutgoingMapping* incoming) {
    CHECK(predecessor_intersection_.empty());
    predecessor_intersection_ = incoming->locations_;
  }

  void Intersect(const OutgoingMapping* other) {
    if (predecessor_intersection_.empty()) return;
    auto it = predecessor_intersection_.begin();
    OperandLess less;
    for (const auto& o : other->locations_) {
      while (less(it->first, o.first)) {
        ++it;
        if (it == predecessor_intersection_.end()) return;
      }
      if (it->first->Equals(o.first)) {
        if (o.second != it->second) {
          predecessor_intersection_.erase(it++);
        } else {
          ++it;
        }
        if (it == predecessor_intersection_.end()) return;
      }
    }
  }

 private:
  LocationMap locations_;
  LocationMap predecessor_intersection_;

  DISALLOW_COPY_AND_ASSIGN(OutgoingMapping);
};


// Verify that all gap moves move the operands for a virtual register into the
// correct location for every instruction.
void RegisterAllocatorVerifier::VerifyGapMoves() {
  typedef ZoneVector<OutgoingMapping*> OutgoingMappings;
  OutgoingMappings outgoing_mappings(
      static_cast<int>(sequence()->instruction_blocks().size()), nullptr,
      zone());
  // Construct all mappings, ignoring back edges and multiple entries.
  ConstructOutgoingMappings(&outgoing_mappings, true);
  // Run all remaining phis and compute the intersection of all predecessor
  // mappings.
  for (const auto* block : sequence()->instruction_blocks()) {
    if (block->PredecessorCount() == 0) continue;
    const size_t block_index = block->rpo_number().ToSize();
    auto* mapping = outgoing_mappings[block_index];
    bool initialized = false;
    // Walk predecessors in reverse to ensure Intersect is correctly working.
    // If it did nothing, the second pass would do exactly what the first pass
    // did.
    for (size_t phi_input = block->PredecessorCount() - 1; true; --phi_input) {
      const size_t pred_block_index = block->predecessors()[phi_input].ToSize();
      auto* incoming = outgoing_mappings[pred_block_index];
      if (phi_input != 0) incoming->RunPhis(sequence(), block, phi_input);
      if (!initialized) {
        mapping->InitializeIntersection(incoming);
        initialized = true;
      } else {
        mapping->Intersect(incoming);
      }
      if (phi_input == 0) break;
    }
  }
  // Construct all mappings again, this time using the instersection mapping
  // above as the incoming mapping instead of the result from the first
  // predecessor.
  ConstructOutgoingMappings(&outgoing_mappings, false);
}


void RegisterAllocatorVerifier::ConstructOutgoingMappings(
    OutgoingMappings* outgoing_mappings, bool initial_pass) {
  // Compute the locations of all virtual registers leaving every block, using
  // only the first predecessor as source for the input mapping.
  for (const auto* block : sequence()->instruction_blocks()) {
    const size_t block_index = block->rpo_number().ToSize();
    auto* current = outgoing_mappings->at(block_index);
    CHECK(initial_pass == (current == nullptr));
    // Initialize current.
    if (!initial_pass) {
      // Skip check second time around for blocks without multiple predecessors
      // as we have already executed this in the initial run.
      if (block->PredecessorCount() <= 1) continue;
      current->InitializeFromIntersection();
    } else {
      current = new (zone()) OutgoingMapping(zone());
      outgoing_mappings->at(block_index) = current;
      // Copy outgoing values from predecessor block.
      current->InitializeFromFirstPredecessor(sequence(), outgoing_mappings,
                                              block);
    }
    // Update current with gaps and operands for all instructions in block.
    for (int instr_index = block->code_start(); instr_index < block->code_end();
         ++instr_index) {
      const auto& instr_constraint = constraints_[instr_index];
      const auto* instr = instr_constraint.instruction_;
      const auto* op_constraints = instr_constraint.operand_constraints_;
      size_t count = 0;
      for (size_t i = 0; i < instr->InputCount(); ++i, ++count) {
        if (op_constraints[count].type_ == kImmediate) continue;
        auto it = current->locations()->find(instr->InputAt(i));
        int virtual_register = op_constraints[count].virtual_register_;
        CHECK(it != current->locations()->end());
        CHECK_EQ(it->second, virtual_register);
      }
      for (size_t i = 0; i < instr->TempCount(); ++i, ++count) {
        current->Drop(instr->TempAt(i));
      }
      if (instr->IsCall()) {
        current->DropRegisters(config());
      }
      for (size_t i = 0; i < instr->OutputCount(); ++i, ++count) {
        current->Drop(instr->OutputAt(i));
        int virtual_register = op_constraints[count].virtual_register_;
        current->Map(instr->OutputAt(i), virtual_register);
      }
      if (instr->IsGapMoves()) {
        const auto* gap = GapInstruction::cast(instr);
        current->RunGapInstruction(zone(), gap);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
