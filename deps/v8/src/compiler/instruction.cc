// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction.h"

namespace v8 {
namespace internal {
namespace compiler {

std::ostream& operator<<(std::ostream& os,
                         const PrintableInstructionOperand& printable) {
  const InstructionOperand& op = *printable.op_;
  const RegisterConfiguration* conf = printable.register_configuration_;
  switch (op.kind()) {
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
          return os << "(=" << conf->general_register_name(
                                   unalloc->fixed_register_index()) << ")";
        case UnallocatedOperand::FIXED_DOUBLE_REGISTER:
          return os << "(=" << conf->double_register_name(
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
      return os << "[" << conf->general_register_name(op.index()) << "|R]";
    case InstructionOperand::DOUBLE_REGISTER:
      return os << "[" << conf->double_register_name(op.index()) << "|R]";
    case InstructionOperand::INVALID:
      return os << "(x)";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableMoveOperands& printable) {
  const MoveOperands& mo = *printable.move_operands_;
  PrintableInstructionOperand printable_op = {printable.register_configuration_,
                                              mo.destination()};

  os << printable_op;
  if (!mo.source()->Equals(mo.destination())) {
    printable_op.op_ = mo.source();
    os << " = " << printable_op;
  }
  return os << ";";
}


bool ParallelMove::IsRedundant() const {
  for (int i = 0; i < move_operands_.length(); ++i) {
    if (!move_operands_[i].IsRedundant()) return false;
  }
  return true;
}


Instruction::Instruction(InstructionCode opcode)
    : opcode_(opcode),
      bit_field_(OutputCountField::encode(0) | InputCountField::encode(0) |
                 TempCountField::encode(0) | IsCallField::encode(false) |
                 IsControlField::encode(false)),
      pointer_map_(NULL) {}


Instruction::Instruction(InstructionCode opcode, size_t output_count,
                         InstructionOperand* outputs, size_t input_count,
                         InstructionOperand* inputs, size_t temp_count,
                         InstructionOperand* temps)
    : opcode_(opcode),
      bit_field_(OutputCountField::encode(output_count) |
                 InputCountField::encode(input_count) |
                 TempCountField::encode(temp_count) |
                 IsCallField::encode(false) | IsControlField::encode(false)),
      pointer_map_(NULL) {
  size_t offset = 0;
  for (size_t i = 0; i < output_count; ++i) {
    DCHECK(!outputs[i].IsInvalid());
    operands_[offset++] = outputs[i];
  }
  for (size_t i = 0; i < input_count; ++i) {
    DCHECK(!inputs[i].IsInvalid());
    operands_[offset++] = inputs[i];
  }
  for (size_t i = 0; i < temp_count; ++i) {
    DCHECK(!temps[i].IsInvalid());
    operands_[offset++] = temps[i];
  }
}


bool GapInstruction::IsRedundant() const {
  for (int i = GapInstruction::FIRST_INNER_POSITION;
       i <= GapInstruction::LAST_INNER_POSITION; i++) {
    if (parallel_moves_[i] != NULL && !parallel_moves_[i]->IsRedundant())
      return false;
  }
  return true;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableParallelMove& printable) {
  const ParallelMove& pm = *printable.parallel_move_;
  bool first = true;
  for (ZoneList<MoveOperands>::iterator move = pm.move_operands()->begin();
       move != pm.move_operands()->end(); ++move) {
    if (move->IsEliminated()) continue;
    if (!first) os << " ";
    first = false;
    PrintableMoveOperands pmo = {printable.register_configuration_, move};
    os << pmo;
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


std::ostream& operator<<(std::ostream& os, const PointerMap& pm) {
  os << "{";
  for (ZoneList<InstructionOperand*>::iterator op =
           pm.pointer_operands_.begin();
       op != pm.pointer_operands_.end(); ++op) {
    if (op != pm.pointer_operands_.begin()) os << ";";
    os << *op;
  }
  return os << "}";
}


std::ostream& operator<<(std::ostream& os, const ArchOpcode& ao) {
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


std::ostream& operator<<(std::ostream& os, const AddressingMode& am) {
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


std::ostream& operator<<(std::ostream& os, const FlagsMode& fm) {
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


std::ostream& operator<<(std::ostream& os, const FlagsCondition& fc) {
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
    case kOverflow:
      return os << "overflow";
    case kNotOverflow:
      return os << "not overflow";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableInstruction& printable) {
  const Instruction& instr = *printable.instr_;
  PrintableInstructionOperand printable_op = {printable.register_configuration_,
                                              NULL};
  if (instr.OutputCount() > 1) os << "(";
  for (size_t i = 0; i < instr.OutputCount(); i++) {
    if (i > 0) os << ", ";
    printable_op.op_ = instr.OutputAt(i);
    os << printable_op;
  }

  if (instr.OutputCount() > 1) os << ") = ";
  if (instr.OutputCount() == 1) os << " = ";

  if (instr.IsGapMoves()) {
    const GapInstruction* gap = GapInstruction::cast(&instr);
    os << "gap ";
    for (int i = GapInstruction::FIRST_INNER_POSITION;
         i <= GapInstruction::LAST_INNER_POSITION; i++) {
      os << "(";
      if (gap->parallel_moves_[i] != NULL) {
        PrintableParallelMove ppm = {printable.register_configuration_,
                                     gap->parallel_moves_[i]};
        os << ppm;
      }
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
      printable_op.op_ = instr.InputAt(i);
      os << " " << printable_op;
    }
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, const Constant& constant) {
  switch (constant.type()) {
    case Constant::kInt32:
      return os << constant.ToInt32();
    case Constant::kInt64:
      return os << constant.ToInt64() << "l";
    case Constant::kFloat32:
      return os << constant.ToFloat32() << "f";
    case Constant::kFloat64:
      return os << constant.ToFloat64();
    case Constant::kExternalReference:
      return os << static_cast<const void*>(
                       constant.ToExternalReference().address());
    case Constant::kHeapObject:
      return os << Brief(*constant.ToHeapObject());
    case Constant::kRpoNumber:
      return os << "RPO" << constant.ToRpoNumber().ToInt();
  }
  UNREACHABLE();
  return os;
}


PhiInstruction::PhiInstruction(Zone* zone, int virtual_register,
                               size_t input_count)
    : virtual_register_(virtual_register),
      output_(UnallocatedOperand(UnallocatedOperand::NONE, virtual_register)),
      operands_(input_count, zone),
      inputs_(input_count, zone) {}


void PhiInstruction::SetInput(size_t offset, int virtual_register) {
  DCHECK(inputs_[offset].IsInvalid());
  auto input = UnallocatedOperand(UnallocatedOperand::ANY, virtual_register);
  inputs_[offset] = input;
  operands_[offset] = virtual_register;
}


InstructionBlock::InstructionBlock(Zone* zone, BasicBlock::Id id,
                                   BasicBlock::RpoNumber rpo_number,
                                   BasicBlock::RpoNumber loop_header,
                                   BasicBlock::RpoNumber loop_end,
                                   bool deferred)
    : successors_(zone),
      predecessors_(zone),
      phis_(zone),
      id_(id),
      ao_number_(rpo_number),
      rpo_number_(rpo_number),
      loop_header_(loop_header),
      loop_end_(loop_end),
      code_start_(-1),
      code_end_(-1),
      deferred_(deferred) {}


size_t InstructionBlock::PredecessorIndexOf(
    BasicBlock::RpoNumber rpo_number) const {
  size_t j = 0;
  for (InstructionBlock::Predecessors::const_iterator i = predecessors_.begin();
       i != predecessors_.end(); ++i, ++j) {
    if (*i == rpo_number) break;
  }
  return j;
}


static BasicBlock::RpoNumber GetRpo(BasicBlock* block) {
  if (block == NULL) return BasicBlock::RpoNumber::Invalid();
  return block->GetRpoNumber();
}


static BasicBlock::RpoNumber GetLoopEndRpo(const BasicBlock* block) {
  if (!block->IsLoopHeader()) return BasicBlock::RpoNumber::Invalid();
  return block->loop_end()->GetRpoNumber();
}


static InstructionBlock* InstructionBlockFor(Zone* zone,
                                             const BasicBlock* block) {
  InstructionBlock* instr_block = new (zone) InstructionBlock(
      zone, block->id(), block->GetRpoNumber(), GetRpo(block->loop_header()),
      GetLoopEndRpo(block), block->deferred());
  // Map successors and precessors
  instr_block->successors().reserve(block->SuccessorCount());
  for (BasicBlock* successor : block->successors()) {
    instr_block->successors().push_back(successor->GetRpoNumber());
  }
  instr_block->predecessors().reserve(block->PredecessorCount());
  for (BasicBlock* predecessor : block->predecessors()) {
    instr_block->predecessors().push_back(predecessor->GetRpoNumber());
  }
  return instr_block;
}


InstructionBlocks* InstructionSequence::InstructionBlocksFor(
    Zone* zone, const Schedule* schedule) {
  InstructionBlocks* blocks = zone->NewArray<InstructionBlocks>(1);
  new (blocks) InstructionBlocks(
      static_cast<int>(schedule->rpo_order()->size()), NULL, zone);
  size_t rpo_number = 0;
  for (BasicBlockVector::const_iterator it = schedule->rpo_order()->begin();
       it != schedule->rpo_order()->end(); ++it, ++rpo_number) {
    DCHECK(!(*blocks)[rpo_number]);
    DCHECK((*it)->GetRpoNumber().ToSize() == rpo_number);
    (*blocks)[rpo_number] = InstructionBlockFor(zone, *it);
  }
  ComputeAssemblyOrder(blocks);
  return blocks;
}


void InstructionSequence::ComputeAssemblyOrder(InstructionBlocks* blocks) {
  int ao = 0;
  for (auto const block : *blocks) {
    if (!block->IsDeferred()) {
      block->set_ao_number(BasicBlock::RpoNumber::FromInt(ao++));
    }
  }
  for (auto const block : *blocks) {
    if (block->IsDeferred()) {
      block->set_ao_number(BasicBlock::RpoNumber::FromInt(ao++));
    }
  }
}


InstructionSequence::InstructionSequence(Isolate* isolate,
                                         Zone* instruction_zone,
                                         InstructionBlocks* instruction_blocks)
    : isolate_(isolate),
      zone_(instruction_zone),
      instruction_blocks_(instruction_blocks),
      block_starts_(zone()),
      constants_(ConstantMap::key_compare(),
                 ConstantMap::allocator_type(zone())),
      immediates_(zone()),
      instructions_(zone()),
      next_virtual_register_(0),
      pointer_maps_(zone()),
      doubles_(std::less<int>(), VirtualRegisterSet::allocator_type(zone())),
      references_(std::less<int>(), VirtualRegisterSet::allocator_type(zone())),
      deoptimization_entries_(zone()) {
  block_starts_.reserve(instruction_blocks_->size());
}


int InstructionSequence::NextVirtualRegister() {
  int virtual_register = next_virtual_register_++;
  CHECK_NE(virtual_register, InstructionOperand::kInvalidVirtualRegister);
  return virtual_register;
}


GapInstruction* InstructionSequence::GetBlockStart(
    BasicBlock::RpoNumber rpo) const {
  const InstructionBlock* block = InstructionBlockAt(rpo);
  return GapInstruction::cast(InstructionAt(block->code_start()));
}


void InstructionSequence::StartBlock(BasicBlock::RpoNumber rpo) {
  DCHECK(block_starts_.size() == rpo.ToSize());
  InstructionBlock* block = InstructionBlockAt(rpo);
  int code_start = static_cast<int>(instructions_.size());
  block->set_code_start(code_start);
  block_starts_.push_back(code_start);
}


void InstructionSequence::EndBlock(BasicBlock::RpoNumber rpo) {
  int end = static_cast<int>(instructions_.size());
  InstructionBlock* block = InstructionBlockAt(rpo);
  if (block->code_start() == end) {  // Empty block.  Insert a nop.
    AddInstruction(Instruction::New(zone(), kArchNop));
    end = static_cast<int>(instructions_.size());
  }
  DCHECK(block->code_start() >= 0 && block->code_start() < end);
  block->set_code_end(end);
}


int InstructionSequence::AddInstruction(Instruction* instr) {
  GapInstruction* gap = GapInstruction::New(zone());
  instructions_.push_back(gap);
  int index = static_cast<int>(instructions_.size());
  instructions_.push_back(instr);
  if (instr->NeedsPointerMap()) {
    DCHECK(instr->pointer_map() == NULL);
    PointerMap* pointer_map = new (zone()) PointerMap(zone());
    pointer_map->set_instruction_position(index);
    instr->set_pointer_map(pointer_map);
    pointer_maps_.push_back(pointer_map);
  }
  return index;
}


const InstructionBlock* InstructionSequence::GetInstructionBlock(
    int instruction_index) const {
  DCHECK(instruction_blocks_->size() == block_starts_.size());
  auto begin = block_starts_.begin();
  auto end = std::lower_bound(begin, block_starts_.end(), instruction_index,
                              std::less_equal<int>());
  size_t index = std::distance(begin, end) - 1;
  auto block = instruction_blocks_->at(index);
  DCHECK(block->code_start() <= instruction_index &&
         instruction_index < block->code_end());
  return block;
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


InstructionSequence::StateId InstructionSequence::AddFrameStateDescriptor(
    FrameStateDescriptor* descriptor) {
  int deoptimization_id = static_cast<int>(deoptimization_entries_.size());
  deoptimization_entries_.push_back(descriptor);
  return StateId::FromInt(deoptimization_id);
}

FrameStateDescriptor* InstructionSequence::GetFrameStateDescriptor(
    InstructionSequence::StateId state_id) {
  return deoptimization_entries_[state_id.ToInt()];
}


int InstructionSequence::GetFrameStateDescriptorCount() {
  return static_cast<int>(deoptimization_entries_.size());
}


FrameStateDescriptor::FrameStateDescriptor(
    Zone* zone, const FrameStateCallInfo& state_info, size_t parameters_count,
    size_t locals_count, size_t stack_count, FrameStateDescriptor* outer_state)
    : type_(state_info.type()),
      bailout_id_(state_info.bailout_id()),
      frame_state_combine_(state_info.state_combine()),
      parameters_count_(parameters_count),
      locals_count_(locals_count),
      stack_count_(stack_count),
      types_(zone),
      outer_state_(outer_state),
      jsfunction_(state_info.jsfunction()) {
  types_.resize(GetSize(), kMachNone);
}

size_t FrameStateDescriptor::GetSize(OutputFrameStateCombine combine) const {
  size_t size = parameters_count() + locals_count() + stack_count() +
                (HasContext() ? 1 : 0);
  switch (combine.kind()) {
    case OutputFrameStateCombine::kPushOutput:
      size += combine.GetPushCount();
      break;
    case OutputFrameStateCombine::kPokeAt:
      break;
  }
  return size;
}


size_t FrameStateDescriptor::GetTotalSize() const {
  size_t total_size = 0;
  for (const FrameStateDescriptor* iter = this; iter != NULL;
       iter = iter->outer_state_) {
    total_size += iter->GetSize();
  }
  return total_size;
}


size_t FrameStateDescriptor::GetFrameCount() const {
  size_t count = 0;
  for (const FrameStateDescriptor* iter = this; iter != NULL;
       iter = iter->outer_state_) {
    ++count;
  }
  return count;
}


size_t FrameStateDescriptor::GetJSFrameCount() const {
  size_t count = 0;
  for (const FrameStateDescriptor* iter = this; iter != NULL;
       iter = iter->outer_state_) {
    if (iter->type_ == JS_FRAME) {
      ++count;
    }
  }
  return count;
}


MachineType FrameStateDescriptor::GetType(size_t index) const {
  return types_[index];
}


void FrameStateDescriptor::SetType(size_t index, MachineType type) {
  DCHECK(index < GetSize());
  types_[index] = type;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableInstructionSequence& printable) {
  const InstructionSequence& code = *printable.sequence_;
  for (size_t i = 0; i < code.immediates_.size(); ++i) {
    Constant constant = code.immediates_[i];
    os << "IMM#" << i << ": " << constant << "\n";
  }
  int i = 0;
  for (ConstantMap::const_iterator it = code.constants_.begin();
       it != code.constants_.end(); ++i, ++it) {
    os << "CST#" << i << ": v" << it->first << " = " << it->second << "\n";
  }
  for (int i = 0; i < code.InstructionBlockCount(); i++) {
    BasicBlock::RpoNumber rpo = BasicBlock::RpoNumber::FromInt(i);
    const InstructionBlock* block = code.InstructionBlockAt(rpo);
    CHECK(block->rpo_number() == rpo);

    os << "RPO#" << block->rpo_number();
    os << ": AO#" << block->ao_number();
    os << ": B" << block->id();
    if (block->IsDeferred()) os << " (deferred)";
    if (block->IsLoopHeader()) {
      os << " loop blocks: [" << block->rpo_number() << ", "
         << block->loop_end() << ")";
    }
    os << "  instructions: [" << block->code_start() << ", "
       << block->code_end() << ")\n  predecessors:";

    for (auto pred : block->predecessors()) {
      const InstructionBlock* pred_block = code.InstructionBlockAt(pred);
      os << " B" << pred_block->id();
    }
    os << "\n";

    for (auto phi : block->phis()) {
      PrintableInstructionOperand printable_op = {
          printable.register_configuration_, &phi->output()};
      os << "     phi: " << printable_op << " =";
      for (auto input : phi->inputs()) {
        printable_op.op_ = &input;
        os << " " << printable_op;
      }
      os << "\n";
    }

    ScopedVector<char> buf(32);
    PrintableInstruction printable_instr;
    printable_instr.register_configuration_ = printable.register_configuration_;
    for (int j = block->first_instruction_index();
         j <= block->last_instruction_index(); j++) {
      // TODO(svenpanne) Add some basic formatting to our streams.
      SNPrintF(buf, "%5d", j);
      printable_instr.instr_ = code.InstructionAt(j);
      os << "   " << buf.start() << ": " << printable_instr << "\n";
    }

    for (auto succ : block->successors()) {
      const InstructionBlock* succ_block = code.InstructionBlockAt(succ);
      os << " B" << succ_block->id();
    }
    os << "\n";
  }
  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
