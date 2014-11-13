// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node-inl.h"
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
  cache = NULL;
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
    os << (instr.IsBlockStart() ? " block-start" : "gap ");
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
  }
  UNREACHABLE();
  return os;
}


InstructionBlock::InstructionBlock(Zone* zone, BasicBlock::Id id,
                                   BasicBlock::RpoNumber ao_number,
                                   BasicBlock::RpoNumber rpo_number,
                                   BasicBlock::RpoNumber loop_header,
                                   BasicBlock::RpoNumber loop_end,
                                   bool deferred)
    : successors_(zone),
      predecessors_(zone),
      phis_(zone),
      id_(id),
      ao_number_(ao_number),
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
      zone, block->id(), block->GetAoNumber(), block->GetRpoNumber(),
      GetRpo(block->loop_header()), GetLoopEndRpo(block), block->deferred());
  // Map successors and precessors
  instr_block->successors().reserve(block->SuccessorCount());
  for (auto it = block->successors_begin(); it != block->successors_end();
       ++it) {
    instr_block->successors().push_back((*it)->GetRpoNumber());
  }
  instr_block->predecessors().reserve(block->PredecessorCount());
  for (auto it = block->predecessors_begin(); it != block->predecessors_end();
       ++it) {
    instr_block->predecessors().push_back((*it)->GetRpoNumber());
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
    DCHECK_EQ(NULL, (*blocks)[rpo_number]);
    DCHECK((*it)->GetRpoNumber().ToSize() == rpo_number);
    (*blocks)[rpo_number] = InstructionBlockFor(zone, *it);
  }
  return blocks;
}


InstructionSequence::InstructionSequence(Zone* instruction_zone,
                                         InstructionBlocks* instruction_blocks)
    : zone_(instruction_zone),
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


BlockStartInstruction* InstructionSequence::GetBlockStart(
    BasicBlock::RpoNumber rpo) {
  InstructionBlock* block = InstructionBlockAt(rpo);
  return BlockStartInstruction::cast(InstructionAt(block->code_start()));
}


void InstructionSequence::StartBlock(BasicBlock::RpoNumber rpo) {
  DCHECK(block_starts_.size() == rpo.ToSize());
  InstructionBlock* block = InstructionBlockAt(rpo);
  int code_start = static_cast<int>(instructions_.size());
  block->set_code_start(code_start);
  block_starts_.push_back(code_start);
  BlockStartInstruction* block_start = BlockStartInstruction::New(zone());
  AddInstruction(block_start);
}


void InstructionSequence::EndBlock(BasicBlock::RpoNumber rpo) {
  int end = static_cast<int>(instructions_.size());
  InstructionBlock* block = InstructionBlockAt(rpo);
  DCHECK(block->code_start() >= 0 && block->code_start() < end);
  block->set_code_end(end);
}


int InstructionSequence::AddInstruction(Instruction* instr) {
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
      os << "     phi: v" << phi->virtual_register() << " =";
      for (auto op_vreg : phi->operands()) {
        os << " v" << op_vreg;
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
