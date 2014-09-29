// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction.h"

#include "src/compiler/common-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

OStream& operator<<(OStream& os, const InstructionOperand& op) {
  switch (op.kind()) {
    case InstructionOperand::INVALID:
      return os << "(0)";
    case InstructionOperand::UNALLOCATED: {
      const UnallocatedOperand* unalloc = UnallocatedOperand::cast(&op);
      os << "v" << unalloc->virtual_register();
      if (unalloc->basic_policy() == UnallocatedOperand::FIXED_SLOT) {
        return os << "(=" << unalloc->fixed_slot_index() << "S)";
      }
      switch (unalloc->extended_policy()) {
        case UnallocatedOperand::NONE:
          return os;
        case UnallocatedOperand::FIXED_REGISTER:
          return os << "(=" << Register::AllocationIndexToString(
                                   unalloc->fixed_register_index()) << ")";
        case UnallocatedOperand::FIXED_DOUBLE_REGISTER:
          return os << "(=" << DoubleRegister::AllocationIndexToString(
                                   unalloc->fixed_register_index()) << ")";
        case UnallocatedOperand::MUST_HAVE_REGISTER:
          return os << "(R)";
        case UnallocatedOperand::SAME_AS_FIRST_INPUT:
          return os << "(1)";
        case UnallocatedOperand::ANY:
          return os << "(-)";
      }
    }
    case InstructionOperand::CONSTANT:
      return os << "[constant:" << op.index() << "]";
    case InstructionOperand::IMMEDIATE:
      return os << "[immediate:" << op.index() << "]";
    case InstructionOperand::STACK_SLOT:
      return os << "[stack:" << op.index() << "]";
    case InstructionOperand::DOUBLE_STACK_SLOT:
      return os << "[double_stack:" << op.index() << "]";
    case InstructionOperand::REGISTER:
      return os << "[" << Register::AllocationIndexToString(op.index())
                << "|R]";
    case InstructionOperand::DOUBLE_REGISTER:
      return os << "[" << DoubleRegister::AllocationIndexToString(op.index())
                << "|R]";
  }
  UNREACHABLE();
  return os;
}


template <InstructionOperand::Kind kOperandKind, int kNumCachedOperands>
SubKindOperand<kOperandKind, kNumCachedOperands>*
    SubKindOperand<kOperandKind, kNumCachedOperands>::cache = NULL;


template <InstructionOperand::Kind kOperandKind, int kNumCachedOperands>
void SubKindOperand<kOperandKind, kNumCachedOperands>::SetUpCache() {
  if (cache) return;
  cache = new SubKindOperand[kNumCachedOperands];
  for (int i = 0; i < kNumCachedOperands; i++) {
    cache[i].ConvertTo(kOperandKind, i);
  }
}


template <InstructionOperand::Kind kOperandKind, int kNumCachedOperands>
void SubKindOperand<kOperandKind, kNumCachedOperands>::TearDownCache() {
  delete[] cache;
}


void InstructionOperand::SetUpCaches() {
#define INSTRUCTION_OPERAND_SETUP(name, type, number) \
  name##Operand::SetUpCache();
  INSTRUCTION_OPERAND_LIST(INSTRUCTION_OPERAND_SETUP)
#undef INSTRUCTION_OPERAND_SETUP
}


void InstructionOperand::TearDownCaches() {
#define INSTRUCTION_OPERAND_TEARDOWN(name, type, number) \
  name##Operand::TearDownCache();
  INSTRUCTION_OPERAND_LIST(INSTRUCTION_OPERAND_TEARDOWN)
#undef INSTRUCTION_OPERAND_TEARDOWN
}


OStream& operator<<(OStream& os, const MoveOperands& mo) {
  os << *mo.destination();
  if (!mo.source()->Equals(mo.destination())) os << " = " << *mo.source();
  return os << ";";
}


bool ParallelMove::IsRedundant() const {
  for (int i = 0; i < move_operands_.length(); ++i) {
    if (!move_operands_[i].IsRedundant()) return false;
  }
  return true;
}


OStream& operator<<(OStream& os, const ParallelMove& pm) {
  bool first = true;
  for (ZoneList<MoveOperands>::iterator move = pm.move_operands()->begin();
       move != pm.move_operands()->end(); ++move) {
    if (move->IsEliminated()) continue;
    if (!first) os << " ";
    first = false;
    os << *move;
  }
  return os;
}


void PointerMap::RecordPointer(InstructionOperand* op, Zone* zone) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  DCHECK(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  pointer_operands_.Add(op, zone);
}


void PointerMap::RemovePointer(InstructionOperand* op) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  DCHECK(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  for (int i = 0; i < pointer_operands_.length(); ++i) {
    if (pointer_operands_[i]->Equals(op)) {
      pointer_operands_.Remove(i);
      --i;
    }
  }
}


void PointerMap::RecordUntagged(InstructionOperand* op, Zone* zone) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  DCHECK(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  untagged_operands_.Add(op, zone);
}


OStream& operator<<(OStream& os, const PointerMap& pm) {
  os << "{";
  for (ZoneList<InstructionOperand*>::iterator op =
           pm.pointer_operands_.begin();
       op != pm.pointer_operands_.end(); ++op) {
    if (op != pm.pointer_operands_.begin()) os << ";";
    os << *op;
  }
  return os << "}";
}


OStream& operator<<(OStream& os, const ArchOpcode& ao) {
  switch (ao) {
#define CASE(Name) \
  case k##Name:    \
    return os << #Name;
    ARCH_OPCODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return os;
}


OStream& operator<<(OStream& os, const AddressingMode& am) {
  switch (am) {
    case kMode_None:
      return os;
#define CASE(Name)   \
  case kMode_##Name: \
    return os << #Name;
      TARGET_ADDRESSING_MODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return os;
}


OStream& operator<<(OStream& os, const FlagsMode& fm) {
  switch (fm) {
    case kFlags_none:
      return os;
    case kFlags_branch:
      return os << "branch";
    case kFlags_set:
      return os << "set";
  }
  UNREACHABLE();
  return os;
}


OStream& operator<<(OStream& os, const FlagsCondition& fc) {
  switch (fc) {
    case kEqual:
      return os << "equal";
    case kNotEqual:
      return os << "not equal";
    case kSignedLessThan:
      return os << "signed less than";
    case kSignedGreaterThanOrEqual:
      return os << "signed greater than or equal";
    case kSignedLessThanOrEqual:
      return os << "signed less than or equal";
    case kSignedGreaterThan:
      return os << "signed greater than";
    case kUnsignedLessThan:
      return os << "unsigned less than";
    case kUnsignedGreaterThanOrEqual:
      return os << "unsigned greater than or equal";
    case kUnsignedLessThanOrEqual:
      return os << "unsigned less than or equal";
    case kUnsignedGreaterThan:
      return os << "unsigned greater than";
    case kUnorderedEqual:
      return os << "unordered equal";
    case kUnorderedNotEqual:
      return os << "unordered not equal";
    case kUnorderedLessThan:
      return os << "unordered less than";
    case kUnorderedGreaterThanOrEqual:
      return os << "unordered greater than or equal";
    case kUnorderedLessThanOrEqual:
      return os << "unordered less than or equal";
    case kUnorderedGreaterThan:
      return os << "unordered greater than";
    case kOverflow:
      return os << "overflow";
    case kNotOverflow:
      return os << "not overflow";
  }
  UNREACHABLE();
  return os;
}


OStream& operator<<(OStream& os, const Instruction& instr) {
  if (instr.OutputCount() > 1) os << "(";
  for (size_t i = 0; i < instr.OutputCount(); i++) {
    if (i > 0) os << ", ";
    os << *instr.OutputAt(i);
  }

  if (instr.OutputCount() > 1) os << ") = ";
  if (instr.OutputCount() == 1) os << " = ";

  if (instr.IsGapMoves()) {
    const GapInstruction* gap = GapInstruction::cast(&instr);
    os << (instr.IsBlockStart() ? " block-start" : "gap ");
    for (int i = GapInstruction::FIRST_INNER_POSITION;
         i <= GapInstruction::LAST_INNER_POSITION; i++) {
      os << "(";
      if (gap->parallel_moves_[i] != NULL) os << *gap->parallel_moves_[i];
      os << ") ";
    }
  } else if (instr.IsSourcePosition()) {
    const SourcePositionInstruction* pos =
        SourcePositionInstruction::cast(&instr);
    os << "position (" << pos->source_position().raw() << ")";
  } else {
    os << ArchOpcodeField::decode(instr.opcode());
    AddressingMode am = AddressingModeField::decode(instr.opcode());
    if (am != kMode_None) {
      os << " : " << AddressingModeField::decode(instr.opcode());
    }
    FlagsMode fm = FlagsModeField::decode(instr.opcode());
    if (fm != kFlags_none) {
      os << " && " << fm << " if "
         << FlagsConditionField::decode(instr.opcode());
    }
  }
  if (instr.InputCount() > 0) {
    for (size_t i = 0; i < instr.InputCount(); i++) {
      os << " " << *instr.InputAt(i);
    }
  }
  return os << "\n";
}


OStream& operator<<(OStream& os, const Constant& constant) {
  switch (constant.type()) {
    case Constant::kInt32:
      return os << constant.ToInt32();
    case Constant::kInt64:
      return os << constant.ToInt64() << "l";
    case Constant::kFloat64:
      return os << constant.ToFloat64();
    case Constant::kExternalReference:
      return os << constant.ToExternalReference().address();
    case Constant::kHeapObject:
      return os << Brief(*constant.ToHeapObject());
  }
  UNREACHABLE();
  return os;
}


Label* InstructionSequence::GetLabel(BasicBlock* block) {
  return GetBlockStart(block)->label();
}


BlockStartInstruction* InstructionSequence::GetBlockStart(BasicBlock* block) {
  return BlockStartInstruction::cast(InstructionAt(block->code_start_));
}


void InstructionSequence::StartBlock(BasicBlock* block) {
  block->code_start_ = static_cast<int>(instructions_.size());
  BlockStartInstruction* block_start =
      BlockStartInstruction::New(zone(), block);
  AddInstruction(block_start, block);
}


void InstructionSequence::EndBlock(BasicBlock* block) {
  int end = static_cast<int>(instructions_.size());
  DCHECK(block->code_start_ >= 0 && block->code_start_ < end);
  block->code_end_ = end;
}


int InstructionSequence::AddInstruction(Instruction* instr, BasicBlock* block) {
  // TODO(titzer): the order of these gaps is a holdover from Lithium.
  GapInstruction* gap = GapInstruction::New(zone());
  if (instr->IsControl()) instructions_.push_back(gap);
  int index = static_cast<int>(instructions_.size());
  instructions_.push_back(instr);
  if (!instr->IsControl()) instructions_.push_back(gap);
  if (instr->NeedsPointerMap()) {
    DCHECK(instr->pointer_map() == NULL);
    PointerMap* pointer_map = new (zone()) PointerMap(zone());
    pointer_map->set_instruction_position(index);
    instr->set_pointer_map(pointer_map);
    pointer_maps_.push_back(pointer_map);
  }
  return index;
}


BasicBlock* InstructionSequence::GetBasicBlock(int instruction_index) {
  // TODO(turbofan): Optimize this.
  for (;;) {
    DCHECK_LE(0, instruction_index);
    Instruction* instruction = InstructionAt(instruction_index--);
    if (instruction->IsBlockStart()) {
      return BlockStartInstruction::cast(instruction)->block();
    }
  }
}


bool InstructionSequence::IsReference(int virtual_register) const {
  return references_.find(virtual_register) != references_.end();
}


bool InstructionSequence::IsDouble(int virtual_register) const {
  return doubles_.find(virtual_register) != doubles_.end();
}


void InstructionSequence::MarkAsReference(int virtual_register) {
  references_.insert(virtual_register);
}


void InstructionSequence::MarkAsDouble(int virtual_register) {
  doubles_.insert(virtual_register);
}


void InstructionSequence::AddGapMove(int index, InstructionOperand* from,
                                     InstructionOperand* to) {
  GapAt(index)->GetOrCreateParallelMove(GapInstruction::START, zone())->AddMove(
      from, to, zone());
}


int InstructionSequence::AddDeoptimizationEntry(
    FrameStateDescriptor* descriptor) {
  int deoptimization_id = static_cast<int>(deoptimization_entries_.size());
  deoptimization_entries_.push_back(descriptor);
  return deoptimization_id;
}

FrameStateDescriptor* InstructionSequence::GetDeoptimizationEntry(
    int deoptimization_id) {
  return deoptimization_entries_[deoptimization_id];
}


int InstructionSequence::GetDeoptimizationEntryCount() {
  return static_cast<int>(deoptimization_entries_.size());
}


OStream& operator<<(OStream& os, const InstructionSequence& code) {
  for (size_t i = 0; i < code.immediates_.size(); ++i) {
    Constant constant = code.immediates_[i];
    os << "IMM#" << i << ": " << constant << "\n";
  }
  int i = 0;
  for (ConstantMap::const_iterator it = code.constants_.begin();
       it != code.constants_.end(); ++i, ++it) {
    os << "CST#" << i << ": v" << it->first << " = " << it->second << "\n";
  }
  for (int i = 0; i < code.BasicBlockCount(); i++) {
    BasicBlock* block = code.BlockAt(i);

    int bid = block->id();
    os << "RPO#" << block->rpo_number_ << ": B" << bid;
    CHECK(block->rpo_number_ == i);
    if (block->IsLoopHeader()) {
      os << " loop blocks: [" << block->rpo_number_ << ", " << block->loop_end_
         << ")";
    }
    os << "  instructions: [" << block->code_start_ << ", " << block->code_end_
       << ")\n  predecessors:";

    BasicBlock::Predecessors predecessors = block->predecessors();
    for (BasicBlock::Predecessors::iterator iter = predecessors.begin();
         iter != predecessors.end(); ++iter) {
      os << " B" << (*iter)->id();
    }
    os << "\n";

    for (BasicBlock::const_iterator j = block->begin(); j != block->end();
         ++j) {
      Node* phi = *j;
      if (phi->opcode() != IrOpcode::kPhi) continue;
      os << "     phi: v" << phi->id() << " =";
      Node::Inputs inputs = phi->inputs();
      for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
           ++iter) {
        os << " v" << (*iter)->id();
      }
      os << "\n";
    }

    ScopedVector<char> buf(32);
    for (int j = block->first_instruction_index();
         j <= block->last_instruction_index(); j++) {
      // TODO(svenpanne) Add some basic formatting to our streams.
      SNPrintF(buf, "%5d", j);
      os << "   " << buf.start() << ": " << *code.InstructionAt(j);
    }

    os << "  " << block->control_;

    if (block->control_input_ != NULL) {
      os << " v" << block->control_input_->id();
    }

    BasicBlock::Successors successors = block->successors();
    for (BasicBlock::Successors::iterator iter = successors.begin();
         iter != successors.end(); ++iter) {
      os << " B" << (*iter)->id();
    }
    os << "\n";
  }
  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
