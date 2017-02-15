// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction.h"
#include "src/compiler/schedule.h"
#include "src/compiler/state-values-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

const auto GetRegConfig = RegisterConfiguration::Turbofan;

FlagsCondition CommuteFlagsCondition(FlagsCondition condition) {
  switch (condition) {
    case kSignedLessThan:
      return kSignedGreaterThan;
    case kSignedGreaterThanOrEqual:
      return kSignedLessThanOrEqual;
    case kSignedLessThanOrEqual:
      return kSignedGreaterThanOrEqual;
    case kSignedGreaterThan:
      return kSignedLessThan;
    case kUnsignedLessThan:
      return kUnsignedGreaterThan;
    case kUnsignedGreaterThanOrEqual:
      return kUnsignedLessThanOrEqual;
    case kUnsignedLessThanOrEqual:
      return kUnsignedGreaterThanOrEqual;
    case kUnsignedGreaterThan:
      return kUnsignedLessThan;
    case kFloatLessThanOrUnordered:
      return kFloatGreaterThanOrUnordered;
    case kFloatGreaterThanOrEqual:
      return kFloatLessThanOrEqual;
    case kFloatLessThanOrEqual:
      return kFloatGreaterThanOrEqual;
    case kFloatGreaterThanOrUnordered:
      return kFloatLessThanOrUnordered;
    case kFloatLessThan:
      return kFloatGreaterThan;
    case kFloatGreaterThanOrEqualOrUnordered:
      return kFloatLessThanOrEqualOrUnordered;
    case kFloatLessThanOrEqualOrUnordered:
      return kFloatGreaterThanOrEqualOrUnordered;
    case kFloatGreaterThan:
      return kFloatLessThan;
    case kPositiveOrZero:
    case kNegative:
      UNREACHABLE();
      break;
    case kEqual:
    case kNotEqual:
    case kOverflow:
    case kNotOverflow:
    case kUnorderedEqual:
    case kUnorderedNotEqual:
      return condition;
  }
  UNREACHABLE();
  return condition;
}

bool InstructionOperand::InterferesWith(const InstructionOperand& that) const {
  return EqualsCanonicalized(that);
}

void InstructionOperand::Print(const RegisterConfiguration* config) const {
  OFStream os(stdout);
  PrintableInstructionOperand wrapper;
  wrapper.register_configuration_ = config;
  wrapper.op_ = *this;
  os << wrapper << std::endl;
}

void InstructionOperand::Print() const { Print(GetRegConfig()); }

std::ostream& operator<<(std::ostream& os,
                         const PrintableInstructionOperand& printable) {
  const InstructionOperand& op = printable.op_;
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
          return os << "(="
                    << conf->GetGeneralRegisterName(
                           unalloc->fixed_register_index())
                    << ")";
        case UnallocatedOperand::FIXED_FP_REGISTER:
          return os << "(="
                    << conf->GetDoubleRegisterName(
                           unalloc->fixed_register_index())
                    << ")";
        case UnallocatedOperand::MUST_HAVE_REGISTER:
          return os << "(R)";
        case UnallocatedOperand::MUST_HAVE_SLOT:
          return os << "(S)";
        case UnallocatedOperand::SAME_AS_FIRST_INPUT:
          return os << "(1)";
        case UnallocatedOperand::ANY:
          return os << "(-)";
      }
    }
    case InstructionOperand::CONSTANT:
      return os << "[constant:" << ConstantOperand::cast(op).virtual_register()
                << "]";
    case InstructionOperand::IMMEDIATE: {
      ImmediateOperand imm = ImmediateOperand::cast(op);
      switch (imm.type()) {
        case ImmediateOperand::INLINE:
          return os << "#" << imm.inline_value();
        case ImmediateOperand::INDEXED:
          return os << "[immediate:" << imm.indexed_value() << "]";
      }
    }
    case InstructionOperand::EXPLICIT:
    case InstructionOperand::ALLOCATED: {
      LocationOperand allocated = LocationOperand::cast(op);
      if (op.IsStackSlot()) {
        os << "[stack:" << allocated.index();
      } else if (op.IsFPStackSlot()) {
        os << "[fp_stack:" << allocated.index();
      } else if (op.IsRegister()) {
        os << "["
           << GetRegConfig()->GetGeneralRegisterName(allocated.register_code())
           << "|R";
      } else if (op.IsDoubleRegister()) {
        os << "["
           << GetRegConfig()->GetDoubleRegisterName(allocated.register_code())
           << "|R";
      } else if (op.IsFloatRegister()) {
        os << "["
           << GetRegConfig()->GetFloatRegisterName(allocated.register_code())
           << "|R";
      } else {
        DCHECK(op.IsSimd128Register());
        os << "["
           << GetRegConfig()->GetSimd128RegisterName(allocated.register_code())
           << "|R";
      }
      if (allocated.IsExplicit()) {
        os << "|E";
      }
      switch (allocated.representation()) {
        case MachineRepresentation::kNone:
          os << "|-";
          break;
        case MachineRepresentation::kBit:
          os << "|b";
          break;
        case MachineRepresentation::kWord8:
          os << "|w8";
          break;
        case MachineRepresentation::kWord16:
          os << "|w16";
          break;
        case MachineRepresentation::kWord32:
          os << "|w32";
          break;
        case MachineRepresentation::kWord64:
          os << "|w64";
          break;
        case MachineRepresentation::kFloat32:
          os << "|f32";
          break;
        case MachineRepresentation::kFloat64:
          os << "|f64";
          break;
        case MachineRepresentation::kSimd128:
          os << "|s128";
          break;
        case MachineRepresentation::kTaggedSigned:
          os << "|ts";
          break;
        case MachineRepresentation::kTaggedPointer:
          os << "|tp";
          break;
        case MachineRepresentation::kTagged:
          os << "|t";
          break;
      }
      return os << "]";
    }
    case InstructionOperand::INVALID:
      return os << "(x)";
  }
  UNREACHABLE();
  return os;
}

void MoveOperands::Print(const RegisterConfiguration* config) const {
  OFStream os(stdout);
  PrintableInstructionOperand wrapper;
  wrapper.register_configuration_ = config;
  wrapper.op_ = destination();
  os << wrapper << " = ";
  wrapper.op_ = source();
  os << wrapper << std::endl;
}

void MoveOperands::Print() const { Print(GetRegConfig()); }

std::ostream& operator<<(std::ostream& os,
                         const PrintableMoveOperands& printable) {
  const MoveOperands& mo = *printable.move_operands_;
  PrintableInstructionOperand printable_op = {printable.register_configuration_,
                                              mo.destination()};
  os << printable_op;
  if (!mo.source().Equals(mo.destination())) {
    printable_op.op_ = mo.source();
    os << " = " << printable_op;
  }
  return os << ";";
}


bool ParallelMove::IsRedundant() const {
  for (MoveOperands* move : *this) {
    if (!move->IsRedundant()) return false;
  }
  return true;
}


MoveOperands* ParallelMove::PrepareInsertAfter(MoveOperands* move) const {
  MoveOperands* replacement = nullptr;
  MoveOperands* to_eliminate = nullptr;
  for (MoveOperands* curr : *this) {
    if (curr->IsEliminated()) continue;
    if (curr->destination().EqualsCanonicalized(move->source())) {
      DCHECK(!replacement);
      replacement = curr;
      if (to_eliminate != nullptr) break;
    } else if (curr->destination().EqualsCanonicalized(move->destination())) {
      DCHECK(!to_eliminate);
      to_eliminate = curr;
      if (replacement != nullptr) break;
    }
  }
  DCHECK_IMPLIES(replacement == to_eliminate, replacement == nullptr);
  if (replacement != nullptr) move->set_source(replacement->source());
  return to_eliminate;
}


ExplicitOperand::ExplicitOperand(LocationKind kind, MachineRepresentation rep,
                                 int index)
    : LocationOperand(EXPLICIT, kind, rep, index) {
  DCHECK_IMPLIES(kind == REGISTER && !IsFloatingPoint(rep),
                 GetRegConfig()->IsAllocatableGeneralCode(index));
  DCHECK_IMPLIES(kind == REGISTER && rep == MachineRepresentation::kFloat32,
                 GetRegConfig()->IsAllocatableFloatCode(index));
  DCHECK_IMPLIES(kind == REGISTER && (rep == MachineRepresentation::kFloat64),
                 GetRegConfig()->IsAllocatableDoubleCode(index));
}

Instruction::Instruction(InstructionCode opcode)
    : opcode_(opcode),
      bit_field_(OutputCountField::encode(0) | InputCountField::encode(0) |
                 TempCountField::encode(0) | IsCallField::encode(false)),
      reference_map_(nullptr),
      block_(nullptr) {
  parallel_moves_[0] = nullptr;
  parallel_moves_[1] = nullptr;
}

Instruction::Instruction(InstructionCode opcode, size_t output_count,
                         InstructionOperand* outputs, size_t input_count,
                         InstructionOperand* inputs, size_t temp_count,
                         InstructionOperand* temps)
    : opcode_(opcode),
      bit_field_(OutputCountField::encode(output_count) |
                 InputCountField::encode(input_count) |
                 TempCountField::encode(temp_count) |
                 IsCallField::encode(false)),
      reference_map_(nullptr),
      block_(nullptr) {
  parallel_moves_[0] = nullptr;
  parallel_moves_[1] = nullptr;
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


bool Instruction::AreMovesRedundant() const {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    if (parallel_moves_[i] != nullptr && !parallel_moves_[i]->IsRedundant()) {
      return false;
    }
  }
  return true;
}

void Instruction::Print(const RegisterConfiguration* config) const {
  OFStream os(stdout);
  PrintableInstruction wrapper;
  wrapper.instr_ = this;
  wrapper.register_configuration_ = config;
  os << wrapper << std::endl;
}

void Instruction::Print() const { Print(GetRegConfig()); }

std::ostream& operator<<(std::ostream& os,
                         const PrintableParallelMove& printable) {
  const ParallelMove& pm = *printable.parallel_move_;
  bool first = true;
  for (MoveOperands* move : pm) {
    if (move->IsEliminated()) continue;
    if (!first) os << " ";
    first = false;
    PrintableMoveOperands pmo = {printable.register_configuration_, move};
    os << pmo;
  }
  return os;
}


void ReferenceMap::RecordReference(const AllocatedOperand& op) {
  // Do not record arguments as pointers.
  if (op.IsStackSlot() && LocationOperand::cast(op).index() < 0) return;
  DCHECK(!op.IsFPRegister() && !op.IsFPStackSlot());
  reference_operands_.push_back(op);
}


std::ostream& operator<<(std::ostream& os, const ReferenceMap& pm) {
  os << "{";
  bool first = true;
  PrintableInstructionOperand poi = {GetRegConfig(), InstructionOperand()};
  for (const InstructionOperand& op : pm.reference_operands_) {
    if (!first) {
      os << ";";
    } else {
      first = false;
    }
    poi.op_ = op;
    os << poi;
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
    case kFlags_deoptimize:
      return os << "deoptimize";
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
    case kFloatLessThanOrUnordered:
      return os << "less than or unordered (FP)";
    case kFloatGreaterThanOrEqual:
      return os << "greater than or equal (FP)";
    case kFloatLessThanOrEqual:
      return os << "less than or equal (FP)";
    case kFloatGreaterThanOrUnordered:
      return os << "greater than or unordered (FP)";
    case kFloatLessThan:
      return os << "less than (FP)";
    case kFloatGreaterThanOrEqualOrUnordered:
      return os << "greater than, equal or unordered (FP)";
    case kFloatLessThanOrEqualOrUnordered:
      return os << "less than, equal or unordered (FP)";
    case kFloatGreaterThan:
      return os << "greater than (FP)";
    case kUnorderedEqual:
      return os << "unordered equal";
    case kUnorderedNotEqual:
      return os << "unordered not equal";
    case kOverflow:
      return os << "overflow";
    case kNotOverflow:
      return os << "not overflow";
    case kPositiveOrZero:
      return os << "positive or zero";
    case kNegative:
      return os << "negative";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableInstruction& printable) {
  const Instruction& instr = *printable.instr_;
  PrintableInstructionOperand printable_op = {printable.register_configuration_,
                                              InstructionOperand()};
  os << "gap ";
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    os << "(";
    if (instr.parallel_moves()[i] != nullptr) {
      PrintableParallelMove ppm = {printable.register_configuration_,
                                   instr.parallel_moves()[i]};
      os << ppm;
    }
    os << ") ";
  }
  os << "\n          ";

  if (instr.OutputCount() > 1) os << "(";
  for (size_t i = 0; i < instr.OutputCount(); i++) {
    if (i > 0) os << ", ";
    printable_op.op_ = *instr.OutputAt(i);
    os << printable_op;
  }

  if (instr.OutputCount() > 1) os << ") = ";
  if (instr.OutputCount() == 1) os << " = ";

  os << ArchOpcodeField::decode(instr.opcode());
  AddressingMode am = AddressingModeField::decode(instr.opcode());
  if (am != kMode_None) {
    os << " : " << AddressingModeField::decode(instr.opcode());
  }
  FlagsMode fm = FlagsModeField::decode(instr.opcode());
  if (fm != kFlags_none) {
    os << " && " << fm << " if " << FlagsConditionField::decode(instr.opcode());
  }
  if (instr.InputCount() > 0) {
    for (size_t i = 0; i < instr.InputCount(); i++) {
      printable_op.op_ = *instr.InputAt(i);
      os << " " << printable_op;
    }
  }
  return os;
}


Constant::Constant(int32_t v) : type_(kInt32), value_(v) {}

Constant::Constant(RelocatablePtrConstantInfo info) {
  if (info.type() == RelocatablePtrConstantInfo::kInt32) {
    type_ = kInt32;
  } else if (info.type() == RelocatablePtrConstantInfo::kInt64) {
    type_ = kInt64;
  } else {
    UNREACHABLE();
  }
  value_ = info.value();
  rmode_ = info.rmode();
}

Handle<HeapObject> Constant::ToHeapObject() const {
  DCHECK_EQ(kHeapObject, type());
  Handle<HeapObject> value(
      bit_cast<HeapObject**>(static_cast<intptr_t>(value_)));
  return value;
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
      operands_(input_count, InstructionOperand::kInvalidVirtualRegister,
                zone) {}


void PhiInstruction::SetInput(size_t offset, int virtual_register) {
  DCHECK_EQ(InstructionOperand::kInvalidVirtualRegister, operands_[offset]);
  operands_[offset] = virtual_register;
}

void PhiInstruction::RenameInput(size_t offset, int virtual_register) {
  DCHECK_NE(InstructionOperand::kInvalidVirtualRegister, operands_[offset]);
  operands_[offset] = virtual_register;
}

InstructionBlock::InstructionBlock(Zone* zone, RpoNumber rpo_number,
                                   RpoNumber loop_header, RpoNumber loop_end,
                                   bool deferred, bool handler)
    : successors_(zone),
      predecessors_(zone),
      phis_(zone),
      ao_number_(rpo_number),
      rpo_number_(rpo_number),
      loop_header_(loop_header),
      loop_end_(loop_end),
      code_start_(-1),
      code_end_(-1),
      deferred_(deferred),
      handler_(handler),
      needs_frame_(false),
      must_construct_frame_(false),
      must_deconstruct_frame_(false),
      last_deferred_(RpoNumber::Invalid()) {}


size_t InstructionBlock::PredecessorIndexOf(RpoNumber rpo_number) const {
  size_t j = 0;
  for (InstructionBlock::Predecessors::const_iterator i = predecessors_.begin();
       i != predecessors_.end(); ++i, ++j) {
    if (*i == rpo_number) break;
  }
  return j;
}


static RpoNumber GetRpo(const BasicBlock* block) {
  if (block == nullptr) return RpoNumber::Invalid();
  return RpoNumber::FromInt(block->rpo_number());
}


static RpoNumber GetLoopEndRpo(const BasicBlock* block) {
  if (!block->IsLoopHeader()) return RpoNumber::Invalid();
  return RpoNumber::FromInt(block->loop_end()->rpo_number());
}


static InstructionBlock* InstructionBlockFor(Zone* zone,
                                             const BasicBlock* block) {
  bool is_handler =
      !block->empty() && block->front()->opcode() == IrOpcode::kIfException;
  InstructionBlock* instr_block = new (zone)
      InstructionBlock(zone, GetRpo(block), GetRpo(block->loop_header()),
                       GetLoopEndRpo(block), block->deferred(), is_handler);
  // Map successors and precessors
  instr_block->successors().reserve(block->SuccessorCount());
  for (BasicBlock* successor : block->successors()) {
    instr_block->successors().push_back(GetRpo(successor));
  }
  instr_block->predecessors().reserve(block->PredecessorCount());
  for (BasicBlock* predecessor : block->predecessors()) {
    instr_block->predecessors().push_back(GetRpo(predecessor));
  }
  return instr_block;
}

std::ostream& operator<<(std::ostream& os,
                         PrintableInstructionBlock& printable_block) {
  const InstructionBlock* block = printable_block.block_;
  const RegisterConfiguration* config = printable_block.register_configuration_;
  const InstructionSequence* code = printable_block.code_;

  os << "B" << block->rpo_number();
  os << ": AO#" << block->ao_number();
  if (block->IsDeferred()) os << " (deferred)";
  if (!block->needs_frame()) os << " (no frame)";
  if (block->must_construct_frame()) os << " (construct frame)";
  if (block->must_deconstruct_frame()) os << " (deconstruct frame)";
  if (block->IsLoopHeader()) {
    os << " loop blocks: [" << block->rpo_number() << ", " << block->loop_end()
       << ")";
  }
  os << "  instructions: [" << block->code_start() << ", " << block->code_end()
     << ")" << std::endl
     << " predecessors:";

  for (RpoNumber pred : block->predecessors()) {
    os << " B" << pred.ToInt();
  }
  os << std::endl;

  for (const PhiInstruction* phi : block->phis()) {
    PrintableInstructionOperand printable_op = {config, phi->output()};
    os << "     phi: " << printable_op << " =";
    for (int input : phi->operands()) {
      os << " v" << input;
    }
    os << std::endl;
  }

  ScopedVector<char> buf(32);
  PrintableInstruction printable_instr;
  printable_instr.register_configuration_ = config;
  for (int j = block->first_instruction_index();
       j <= block->last_instruction_index(); j++) {
    // TODO(svenpanne) Add some basic formatting to our streams.
    SNPrintF(buf, "%5d", j);
    printable_instr.instr_ = code->InstructionAt(j);
    os << "   " << buf.start() << ": " << printable_instr << std::endl;
  }

  for (RpoNumber succ : block->successors()) {
    os << " B" << succ.ToInt();
  }
  os << std::endl;
  return os;
}

InstructionBlocks* InstructionSequence::InstructionBlocksFor(
    Zone* zone, const Schedule* schedule) {
  InstructionBlocks* blocks = zone->NewArray<InstructionBlocks>(1);
  new (blocks) InstructionBlocks(
      static_cast<int>(schedule->rpo_order()->size()), nullptr, zone);
  size_t rpo_number = 0;
  for (BasicBlockVector::const_iterator it = schedule->rpo_order()->begin();
       it != schedule->rpo_order()->end(); ++it, ++rpo_number) {
    DCHECK(!(*blocks)[rpo_number]);
    DCHECK(GetRpo(*it).ToSize() == rpo_number);
    (*blocks)[rpo_number] = InstructionBlockFor(zone, *it);
  }
  ComputeAssemblyOrder(blocks);
  return blocks;
}

void InstructionSequence::ValidateEdgeSplitForm() const {
  // Validate blocks are in edge-split form: no block with multiple successors
  // has an edge to a block (== a successor) with more than one predecessors.
  for (const InstructionBlock* block : instruction_blocks()) {
    if (block->SuccessorCount() > 1) {
      for (const RpoNumber& successor_id : block->successors()) {
        const InstructionBlock* successor = InstructionBlockAt(successor_id);
        // Expect precisely one predecessor: "block".
        CHECK(successor->PredecessorCount() == 1 &&
              successor->predecessors()[0] == block->rpo_number());
      }
    }
  }
}

void InstructionSequence::ValidateDeferredBlockExitPaths() const {
  // A deferred block with more than one successor must have all its successors
  // deferred.
  for (const InstructionBlock* block : instruction_blocks()) {
    if (!block->IsDeferred() || block->SuccessorCount() <= 1) continue;
    for (RpoNumber successor_id : block->successors()) {
      CHECK(InstructionBlockAt(successor_id)->IsDeferred());
    }
  }
}

void InstructionSequence::ValidateDeferredBlockEntryPaths() const {
  // If a deferred block has multiple predecessors, they have to
  // all be deferred. Otherwise, we can run into a situation where a range
  // that spills only in deferred blocks inserts its spill in the block, but
  // other ranges need moves inserted by ResolveControlFlow in the predecessors,
  // which may clobber the register of this range.
  for (const InstructionBlock* block : instruction_blocks()) {
    if (!block->IsDeferred() || block->PredecessorCount() <= 1) continue;
    for (RpoNumber predecessor_id : block->predecessors()) {
      CHECK(InstructionBlockAt(predecessor_id)->IsDeferred());
    }
  }
}

void InstructionSequence::ValidateSSA() const {
  // TODO(mtrofin): We could use a local zone here instead.
  BitVector definitions(VirtualRegisterCount(), zone());
  for (const Instruction* instruction : *this) {
    for (size_t i = 0; i < instruction->OutputCount(); ++i) {
      const InstructionOperand* output = instruction->OutputAt(i);
      int vreg = (output->IsConstant())
                     ? ConstantOperand::cast(output)->virtual_register()
                     : UnallocatedOperand::cast(output)->virtual_register();
      CHECK(!definitions.Contains(vreg));
      definitions.Add(vreg);
    }
  }
}

void InstructionSequence::ComputeAssemblyOrder(InstructionBlocks* blocks) {
  int ao = 0;
  for (InstructionBlock* const block : *blocks) {
    if (!block->IsDeferred()) {
      block->set_ao_number(RpoNumber::FromInt(ao++));
    }
  }
  for (InstructionBlock* const block : *blocks) {
    if (block->IsDeferred()) {
      block->set_ao_number(RpoNumber::FromInt(ao++));
    }
  }
}

InstructionSequence::InstructionSequence(Isolate* isolate,
                                         Zone* instruction_zone,
                                         InstructionBlocks* instruction_blocks)
    : isolate_(isolate),
      zone_(instruction_zone),
      instruction_blocks_(instruction_blocks),
      source_positions_(zone()),
      constants_(ConstantMap::key_compare(),
                 ConstantMap::allocator_type(zone())),
      immediates_(zone()),
      instructions_(zone()),
      next_virtual_register_(0),
      reference_maps_(zone()),
      representations_(zone()),
      deoptimization_entries_(zone()),
      current_block_(nullptr) {}

int InstructionSequence::NextVirtualRegister() {
  int virtual_register = next_virtual_register_++;
  CHECK_NE(virtual_register, InstructionOperand::kInvalidVirtualRegister);
  return virtual_register;
}


Instruction* InstructionSequence::GetBlockStart(RpoNumber rpo) const {
  const InstructionBlock* block = InstructionBlockAt(rpo);
  return InstructionAt(block->code_start());
}


void InstructionSequence::StartBlock(RpoNumber rpo) {
  DCHECK_NULL(current_block_);
  current_block_ = InstructionBlockAt(rpo);
  int code_start = static_cast<int>(instructions_.size());
  current_block_->set_code_start(code_start);
}


void InstructionSequence::EndBlock(RpoNumber rpo) {
  int end = static_cast<int>(instructions_.size());
  DCHECK_EQ(current_block_->rpo_number(), rpo);
  if (current_block_->code_start() == end) {  // Empty block.  Insert a nop.
    AddInstruction(Instruction::New(zone(), kArchNop));
    end = static_cast<int>(instructions_.size());
  }
  DCHECK(current_block_->code_start() >= 0 &&
         current_block_->code_start() < end);
  current_block_->set_code_end(end);
  current_block_ = nullptr;
}


int InstructionSequence::AddInstruction(Instruction* instr) {
  DCHECK_NOT_NULL(current_block_);
  int index = static_cast<int>(instructions_.size());
  instr->set_block(current_block_);
  instructions_.push_back(instr);
  if (instr->NeedsReferenceMap()) {
    DCHECK(instr->reference_map() == nullptr);
    ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
    reference_map->set_instruction_position(index);
    instr->set_reference_map(reference_map);
    reference_maps_.push_back(reference_map);
  }
  return index;
}


InstructionBlock* InstructionSequence::GetInstructionBlock(
    int instruction_index) const {
  return instructions()[instruction_index]->block();
}


static MachineRepresentation FilterRepresentation(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kBit:
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
      return InstructionSequence::DefaultRepresentation();
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kFloat32:
    case MachineRepresentation::kFloat64:
    case MachineRepresentation::kSimd128:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      return rep;
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return MachineRepresentation::kNone;
}


MachineRepresentation InstructionSequence::GetRepresentation(
    int virtual_register) const {
  DCHECK_LE(0, virtual_register);
  DCHECK_LT(virtual_register, VirtualRegisterCount());
  if (virtual_register >= static_cast<int>(representations_.size())) {
    return DefaultRepresentation();
  }
  return representations_[virtual_register];
}


void InstructionSequence::MarkAsRepresentation(MachineRepresentation rep,
                                               int virtual_register) {
  DCHECK_LE(0, virtual_register);
  DCHECK_LT(virtual_register, VirtualRegisterCount());
  if (virtual_register >= static_cast<int>(representations_.size())) {
    representations_.resize(VirtualRegisterCount(), DefaultRepresentation());
  }
  rep = FilterRepresentation(rep);
  DCHECK_IMPLIES(representations_[virtual_register] != rep,
                 representations_[virtual_register] == DefaultRepresentation());
  representations_[virtual_register] = rep;
}

int InstructionSequence::AddDeoptimizationEntry(
    FrameStateDescriptor* descriptor, DeoptimizeReason reason) {
  int deoptimization_id = static_cast<int>(deoptimization_entries_.size());
  deoptimization_entries_.push_back(DeoptimizationEntry(descriptor, reason));
  return deoptimization_id;
}

DeoptimizationEntry const& InstructionSequence::GetDeoptimizationEntry(
    int state_id) {
  return deoptimization_entries_[state_id];
}


RpoNumber InstructionSequence::InputRpo(Instruction* instr, size_t index) {
  InstructionOperand* operand = instr->InputAt(index);
  Constant constant =
      operand->IsImmediate()
          ? GetImmediate(ImmediateOperand::cast(operand))
          : GetConstant(ConstantOperand::cast(operand)->virtual_register());
  return constant.ToRpoNumber();
}


bool InstructionSequence::GetSourcePosition(const Instruction* instr,
                                            SourcePosition* result) const {
  auto it = source_positions_.find(instr);
  if (it == source_positions_.end()) return false;
  *result = it->second;
  return true;
}


void InstructionSequence::SetSourcePosition(const Instruction* instr,
                                            SourcePosition value) {
  source_positions_.insert(std::make_pair(instr, value));
}

void InstructionSequence::Print(const RegisterConfiguration* config) const {
  OFStream os(stdout);
  PrintableInstructionSequence wrapper;
  wrapper.register_configuration_ = config;
  wrapper.sequence_ = this;
  os << wrapper << std::endl;
}

void InstructionSequence::Print() const { Print(GetRegConfig()); }

void InstructionSequence::PrintBlock(const RegisterConfiguration* config,
                                     int block_id) const {
  OFStream os(stdout);
  RpoNumber rpo = RpoNumber::FromInt(block_id);
  const InstructionBlock* block = InstructionBlockAt(rpo);
  CHECK(block->rpo_number() == rpo);
  PrintableInstructionBlock printable_block = {config, block, this};
  os << printable_block << std::endl;
}

void InstructionSequence::PrintBlock(int block_id) const {
  PrintBlock(GetRegConfig(), block_id);
}

FrameStateDescriptor::FrameStateDescriptor(
    Zone* zone, FrameStateType type, BailoutId bailout_id,
    OutputFrameStateCombine state_combine, size_t parameters_count,
    size_t locals_count, size_t stack_count,
    MaybeHandle<SharedFunctionInfo> shared_info,
    FrameStateDescriptor* outer_state)
    : type_(type),
      bailout_id_(bailout_id),
      frame_state_combine_(state_combine),
      parameters_count_(parameters_count),
      locals_count_(locals_count),
      stack_count_(stack_count),
      values_(zone),
      shared_info_(shared_info),
      outer_state_(outer_state) {}


size_t FrameStateDescriptor::GetSize(OutputFrameStateCombine combine) const {
  size_t size = 1 + parameters_count() + locals_count() + stack_count() +
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
  for (const FrameStateDescriptor* iter = this; iter != nullptr;
       iter = iter->outer_state_) {
    total_size += iter->GetSize();
  }
  return total_size;
}


size_t FrameStateDescriptor::GetFrameCount() const {
  size_t count = 0;
  for (const FrameStateDescriptor* iter = this; iter != nullptr;
       iter = iter->outer_state_) {
    ++count;
  }
  return count;
}


size_t FrameStateDescriptor::GetJSFrameCount() const {
  size_t count = 0;
  for (const FrameStateDescriptor* iter = this; iter != nullptr;
       iter = iter->outer_state_) {
    if (FrameStateFunctionInfo::IsJSFunctionType(iter->type_)) {
      ++count;
    }
  }
  return count;
}


std::ostream& operator<<(std::ostream& os, const RpoNumber& rpo) {
  return os << rpo.ToSize();
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
  PrintableInstructionBlock printable_block = {
      printable.register_configuration_, nullptr, printable.sequence_};
  for (int i = 0; i < code.InstructionBlockCount(); i++) {
    printable_block.block_ = code.InstructionBlockAt(RpoNumber::FromInt(i));
    os << printable_block;
  }
  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
