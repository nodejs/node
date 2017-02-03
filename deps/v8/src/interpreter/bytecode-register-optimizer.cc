// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register-optimizer.h"

namespace v8 {
namespace internal {
namespace interpreter {

const uint32_t BytecodeRegisterOptimizer::kInvalidEquivalenceId;

// A class for tracking the state of a register. This class tracks
// which equivalence set a register is a member of and also whether a
// register is materialized in the bytecode stream.
class BytecodeRegisterOptimizer::RegisterInfo final : public ZoneObject {
 public:
  RegisterInfo(Register reg, uint32_t equivalence_id, bool materialized,
               bool allocated)
      : register_(reg),
        equivalence_id_(equivalence_id),
        materialized_(materialized),
        allocated_(allocated),
        next_(this),
        prev_(this) {}

  void AddToEquivalenceSetOf(RegisterInfo* info);
  void MoveToNewEquivalenceSet(uint32_t equivalence_id, bool materialized);
  bool IsOnlyMemberOfEquivalenceSet() const;
  bool IsOnlyMaterializedMemberOfEquivalenceSet() const;
  bool IsInSameEquivalenceSet(RegisterInfo* info) const;

  // Get a member of this register's equivalence set that is
  // materialized. The materialized equivalent will be this register
  // if it is materialized. Returns nullptr if no materialized
  // equivalent exists.
  RegisterInfo* GetMaterializedEquivalent();

  // Get a member of this register's equivalence set that is
  // materialized and not register |reg|. The materialized equivalent
  // will be this register if it is materialized. Returns nullptr if
  // no materialized equivalent exists.
  RegisterInfo* GetMaterializedEquivalentOtherThan(Register reg);

  // Get a member of this register's equivalence set that is intended
  // to be materialized in place of this register (which is currently
  // materialized). The best candidate is deemed to be the register
  // with the lowest index as this permits temporary registers to be
  // removed from the bytecode stream. Returns nullptr if no candidate
  // exists.
  RegisterInfo* GetEquivalentToMaterialize();

  // Marks all temporary registers of the equivalence set as unmaterialized.
  void MarkTemporariesAsUnmaterialized(Register temporary_base);

  // Get an equivalent register. Returns this if none exists.
  RegisterInfo* GetEquivalent();

  Register register_value() const { return register_; }
  bool materialized() const { return materialized_; }
  void set_materialized(bool materialized) { materialized_ = materialized; }
  bool allocated() const { return allocated_; }
  void set_allocated(bool allocated) { allocated_ = allocated; }
  void set_equivalence_id(uint32_t equivalence_id) {
    equivalence_id_ = equivalence_id;
  }
  uint32_t equivalence_id() const { return equivalence_id_; }

 private:
  Register register_;
  uint32_t equivalence_id_;
  bool materialized_;
  bool allocated_;

  // Equivalence set pointers.
  RegisterInfo* next_;
  RegisterInfo* prev_;

  DISALLOW_COPY_AND_ASSIGN(RegisterInfo);
};

void BytecodeRegisterOptimizer::RegisterInfo::AddToEquivalenceSetOf(
    RegisterInfo* info) {
  DCHECK_NE(kInvalidEquivalenceId, info->equivalence_id());
  // Fix old list
  next_->prev_ = prev_;
  prev_->next_ = next_;
  // Add to new list.
  next_ = info->next_;
  prev_ = info;
  prev_->next_ = this;
  next_->prev_ = this;
  set_equivalence_id(info->equivalence_id());
  set_materialized(false);
}

void BytecodeRegisterOptimizer::RegisterInfo::MoveToNewEquivalenceSet(
    uint32_t equivalence_id, bool materialized) {
  next_->prev_ = prev_;
  prev_->next_ = next_;
  next_ = prev_ = this;
  equivalence_id_ = equivalence_id;
  materialized_ = materialized;
}

bool BytecodeRegisterOptimizer::RegisterInfo::IsOnlyMemberOfEquivalenceSet()
    const {
  return this->next_ == this;
}

bool BytecodeRegisterOptimizer::RegisterInfo::
    IsOnlyMaterializedMemberOfEquivalenceSet() const {
  DCHECK(materialized());

  const RegisterInfo* visitor = this->next_;
  while (visitor != this) {
    if (visitor->materialized()) {
      return false;
    }
    visitor = visitor->next_;
  }
  return true;
}

bool BytecodeRegisterOptimizer::RegisterInfo::IsInSameEquivalenceSet(
    RegisterInfo* info) const {
  return equivalence_id() == info->equivalence_id();
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetMaterializedEquivalent() {
  RegisterInfo* visitor = this;
  do {
    if (visitor->materialized()) {
      return visitor;
    }
    visitor = visitor->next_;
  } while (visitor != this);

  return nullptr;
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetMaterializedEquivalentOtherThan(
    Register reg) {
  RegisterInfo* visitor = this;
  do {
    if (visitor->materialized() && visitor->register_value() != reg) {
      return visitor;
    }
    visitor = visitor->next_;
  } while (visitor != this);

  return nullptr;
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetEquivalentToMaterialize() {
  DCHECK(this->materialized());
  RegisterInfo* visitor = this->next_;
  RegisterInfo* best_info = nullptr;
  while (visitor != this) {
    if (visitor->materialized()) {
      return nullptr;
    }
    if (visitor->allocated() &&
        (best_info == nullptr ||
         visitor->register_value() < best_info->register_value())) {
      best_info = visitor;
    }
    visitor = visitor->next_;
  }
  return best_info;
}

void BytecodeRegisterOptimizer::RegisterInfo::MarkTemporariesAsUnmaterialized(
    Register temporary_base) {
  DCHECK(this->register_value() < temporary_base);
  DCHECK(this->materialized());
  RegisterInfo* visitor = this->next_;
  while (visitor != this) {
    if (visitor->register_value() >= temporary_base) {
      visitor->set_materialized(false);
    }
    visitor = visitor->next_;
  }
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetEquivalent() {
  return next_;
}

BytecodeRegisterOptimizer::BytecodeRegisterOptimizer(
    Zone* zone, BytecodeRegisterAllocator* register_allocator,
    int fixed_registers_count, int parameter_count,
    BytecodePipelineStage* next_stage)
    : accumulator_(Register::virtual_accumulator()),
      temporary_base_(fixed_registers_count),
      max_register_index_(fixed_registers_count - 1),
      register_info_table_(zone),
      equivalence_id_(0),
      next_stage_(next_stage),
      flush_required_(false),
      zone_(zone) {
  register_allocator->set_observer(this);

  // Calculate offset so register index values can be mapped into
  // a vector of register metadata.
  if (parameter_count != 0) {
    register_info_table_offset_ =
        -Register::FromParameterIndex(0, parameter_count).index();
  } else {
    // TODO(oth): This path shouldn't be necessary in bytecode generated
    // from Javascript, but a set of tests do not include the JS receiver.
    register_info_table_offset_ = -accumulator_.index();
  }

  // Initialize register map for parameters, locals, and the
  // accumulator.
  register_info_table_.resize(register_info_table_offset_ +
                              static_cast<size_t>(temporary_base_.index()));
  for (size_t i = 0; i < register_info_table_.size(); ++i) {
    register_info_table_[i] = new (zone) RegisterInfo(
        RegisterFromRegisterInfoTableIndex(i), NextEquivalenceId(), true, true);
    DCHECK_EQ(register_info_table_[i]->register_value().index(),
              RegisterFromRegisterInfoTableIndex(i).index());
  }
  accumulator_info_ = GetRegisterInfo(accumulator_);
  DCHECK(accumulator_info_->register_value() == accumulator_);
}

// override
Handle<BytecodeArray> BytecodeRegisterOptimizer::ToBytecodeArray(
    Isolate* isolate, int register_count, int parameter_count,
    Handle<FixedArray> handler_table) {
  FlushState();
  return next_stage_->ToBytecodeArray(isolate, max_register_index_ + 1,
                                      parameter_count, handler_table);
}

// override
void BytecodeRegisterOptimizer::Write(BytecodeNode* node) {
  // Jumps are handled by WriteJump.
  DCHECK(!Bytecodes::IsJump(node->bytecode()));
  //
  // Transfers with observable registers as the destination will be
  // immediately materialized so the source position information will
  // be ordered correctly.
  //
  // Transfers without observable destination registers will initially
  // be emitted as Nop's with the source position. They may, or may
  // not, be materialized by the optimizer. However, the source
  // position is not lost and being attached to a Nop is fine as the
  // destination register is not observable in the debugger.
  //
  switch (node->bytecode()) {
    case Bytecode::kLdar: {
      DoLdar(node);
      return;
    }
    case Bytecode::kStar: {
      DoStar(node);
      return;
    }
    case Bytecode::kMov: {
      DoMov(node);
      return;
    }
    default:
      break;
  }

  if (node->bytecode() == Bytecode::kDebugger ||
      node->bytecode() == Bytecode::kSuspendGenerator) {
    // All state must be flushed before emitting
    // - a call to the debugger (as it can manipulate locals and parameters),
    // - a generator suspend (as this involves saving all registers).
    FlushState();
  }

  PrepareOperands(node);
  next_stage_->Write(node);
}

// override
void BytecodeRegisterOptimizer::WriteJump(BytecodeNode* node,
                                          BytecodeLabel* label) {
  FlushState();
  next_stage_->WriteJump(node, label);
}

// override
void BytecodeRegisterOptimizer::BindLabel(BytecodeLabel* label) {
  FlushState();
  next_stage_->BindLabel(label);
}

// override
void BytecodeRegisterOptimizer::BindLabel(const BytecodeLabel& target,
                                          BytecodeLabel* label) {
  // There is no need to flush here, it will have been flushed when |target|
  // was bound.
  next_stage_->BindLabel(target, label);
}

void BytecodeRegisterOptimizer::FlushState() {
  if (!flush_required_) {
    return;
  }

  // Materialize all live registers and break equivalences.
  size_t count = register_info_table_.size();
  for (size_t i = 0; i < count; ++i) {
    RegisterInfo* reg_info = register_info_table_[i];
    if (reg_info->materialized()) {
      // Walk equivalents of materialized registers, materializing
      // each equivalent register as necessary and placing in their
      // own equivalence set.
      RegisterInfo* equivalent;
      while ((equivalent = reg_info->GetEquivalent()) != reg_info) {
        if (equivalent->allocated() && !equivalent->materialized()) {
          OutputRegisterTransfer(reg_info, equivalent);
        }
        equivalent->MoveToNewEquivalenceSet(NextEquivalenceId(), true);
      }
    }
  }

  flush_required_ = false;
}

void BytecodeRegisterOptimizer::OutputRegisterTransfer(
    RegisterInfo* input_info, RegisterInfo* output_info,
    BytecodeSourceInfo* source_info) {
  Register input = input_info->register_value();
  Register output = output_info->register_value();
  DCHECK_NE(input.index(), output.index());

  if (input == accumulator_) {
    uint32_t operand = static_cast<uint32_t>(output.ToOperand());
    BytecodeNode node(Bytecode::kStar, operand, source_info);
    next_stage_->Write(&node);
  } else if (output == accumulator_) {
    uint32_t operand = static_cast<uint32_t>(input.ToOperand());
    BytecodeNode node(Bytecode::kLdar, operand, source_info);
    next_stage_->Write(&node);
  } else {
    uint32_t operand0 = static_cast<uint32_t>(input.ToOperand());
    uint32_t operand1 = static_cast<uint32_t>(output.ToOperand());
    BytecodeNode node(Bytecode::kMov, operand0, operand1, source_info);
    next_stage_->Write(&node);
  }
  if (output != accumulator_) {
    max_register_index_ = std::max(max_register_index_, output.index());
  }
  output_info->set_materialized(true);
}

void BytecodeRegisterOptimizer::CreateMaterializedEquivalent(
    RegisterInfo* info) {
  DCHECK(info->materialized());
  RegisterInfo* unmaterialized = info->GetEquivalentToMaterialize();
  if (unmaterialized) {
    OutputRegisterTransfer(info, unmaterialized);
  }
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::GetMaterializedEquivalent(RegisterInfo* info) {
  return info->materialized() ? info : info->GetMaterializedEquivalent();
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::GetMaterializedEquivalentNotAccumulator(
    RegisterInfo* info) {
  if (info->materialized()) {
    return info;
  }

  RegisterInfo* result = info->GetMaterializedEquivalentOtherThan(accumulator_);
  if (result == nullptr) {
    Materialize(info);
    result = info;
  }
  DCHECK(result->register_value() != accumulator_);
  return result;
}

void BytecodeRegisterOptimizer::Materialize(RegisterInfo* info) {
  if (!info->materialized()) {
    RegisterInfo* materialized = info->GetMaterializedEquivalent();
    OutputRegisterTransfer(materialized, info);
  }
}

void BytecodeRegisterOptimizer::AddToEquivalenceSet(
    RegisterInfo* set_member, RegisterInfo* non_set_member) {
  non_set_member->AddToEquivalenceSetOf(set_member);
  // Flushing is only required when two or more registers are placed
  // in the same equivalence set.
  flush_required_ = true;
}

void BytecodeRegisterOptimizer::RegisterTransfer(
    RegisterInfo* input_info, RegisterInfo* output_info,
    BytecodeSourceInfo* source_info) {
  // Materialize an alternate in the equivalence set that
  // |output_info| is leaving.
  if (output_info->materialized()) {
    CreateMaterializedEquivalent(output_info);
  }

  // Add |output_info| to new equivalence set.
  if (!output_info->IsInSameEquivalenceSet(input_info)) {
    AddToEquivalenceSet(input_info, output_info);
  }

  bool output_is_observable =
      RegisterIsObservable(output_info->register_value());
  if (output_is_observable) {
    // Force store to be emitted when register is observable.
    output_info->set_materialized(false);
    RegisterInfo* materialized_info = input_info->GetMaterializedEquivalent();
    OutputRegisterTransfer(materialized_info, output_info, source_info);
  } else if (source_info->is_valid()) {
    // Emit a placeholder nop to maintain source position info.
    EmitNopForSourceInfo(source_info);
  }

  bool input_is_observable = RegisterIsObservable(input_info->register_value());
  if (input_is_observable) {
    // If input is observable by the debugger, mark all other temporaries
    // registers as unmaterialized so that this register is used in preference.
    input_info->MarkTemporariesAsUnmaterialized(temporary_base_);
  }
}

void BytecodeRegisterOptimizer::EmitNopForSourceInfo(
    BytecodeSourceInfo* source_info) const {
  DCHECK(source_info->is_valid());
  BytecodeNode nop(Bytecode::kNop, source_info);
  next_stage_->Write(&nop);
}

void BytecodeRegisterOptimizer::DoLdar(BytecodeNode* node) {
  Register input = GetRegisterInputOperand(
      0, node->bytecode(), node->operands(), node->operand_count());
  RegisterInfo* input_info = GetRegisterInfo(input);
  RegisterTransfer(input_info, accumulator_info_, node->source_info_ptr());
}

void BytecodeRegisterOptimizer::DoMov(BytecodeNode* node) {
  Register input = GetRegisterInputOperand(
      0, node->bytecode(), node->operands(), node->operand_count());
  RegisterInfo* input_info = GetRegisterInfo(input);
  Register output = GetRegisterOutputOperand(
      1, node->bytecode(), node->operands(), node->operand_count());
  RegisterInfo* output_info = GetRegisterInfo(output);
  RegisterTransfer(input_info, output_info, node->source_info_ptr());
}

void BytecodeRegisterOptimizer::DoStar(BytecodeNode* node) {
  Register output = GetRegisterOutputOperand(
      0, node->bytecode(), node->operands(), node->operand_count());
  RegisterInfo* output_info = GetRegisterInfo(output);
  RegisterTransfer(accumulator_info_, output_info, node->source_info_ptr());
}

void BytecodeRegisterOptimizer::PrepareRegisterOutputOperand(
    RegisterInfo* reg_info) {
  if (reg_info->materialized()) {
    CreateMaterializedEquivalent(reg_info);
  }
  max_register_index_ =
      std::max(max_register_index_, reg_info->register_value().index());
  reg_info->MoveToNewEquivalenceSet(NextEquivalenceId(), true);
}

void BytecodeRegisterOptimizer::PrepareRegisterRangeOutputOperand(
    Register start, int count) {
  for (int i = 0; i < count; ++i) {
    Register reg(start.index() + i);
    RegisterInfo* reg_info = GetRegisterInfo(reg);
    PrepareRegisterOutputOperand(reg_info);
  }
}

Register BytecodeRegisterOptimizer::GetEquivalentRegisterForInputOperand(
    Register reg) {
  // For a temporary register, RegInfo state may need be created. For
  // locals and parameters, the RegInfo state is created in the
  // BytecodeRegisterOptimizer constructor.
  RegisterInfo* reg_info = GetRegisterInfo(reg);
  if (reg_info->materialized()) {
    return reg;
  } else {
    RegisterInfo* equivalent_info =
        GetMaterializedEquivalentNotAccumulator(reg_info);
    return equivalent_info->register_value();
  }
}

void BytecodeRegisterOptimizer::PrepareRegisterInputOperand(
    BytecodeNode* const node, Register reg, int operand_index) {
  Register equivalent = GetEquivalentRegisterForInputOperand(reg);
  node->UpdateOperand(operand_index,
                      static_cast<uint32_t>(equivalent.ToOperand()));
}

void BytecodeRegisterOptimizer::PrepareRegisterRangeInputOperand(Register start,
                                                                 int count) {
  for (int i = 0; i < count; ++i) {
    Register current(start.index() + i);
    RegisterInfo* input_info = GetRegisterInfo(current);
    Materialize(input_info);
  }
}

void BytecodeRegisterOptimizer::PrepareRegisterOperands(
    BytecodeNode* const node) {
  //
  // For each input operand, get a materialized equivalent if it is
  // just a single register, otherwise materialize register range.
  // Update operand_scale if necessary.
  //
  // For each output register about to be clobbered, materialize an
  // equivalent if it exists. Put each register in it's own equivalence set.
  //
  const uint32_t* operands = node->operands();
  int operand_count = node->operand_count();
  const OperandType* operand_types =
      Bytecodes::GetOperandTypes(node->bytecode());
  for (int i = 0; i < operand_count; ++i) {
    int count;
    if (operand_types[i] == OperandType::kRegList) {
      DCHECK_LT(i, operand_count - 1);
      DCHECK(operand_types[i + 1] == OperandType::kRegCount);
      count = static_cast<int>(operands[i + 1]);
    } else {
      count = Bytecodes::GetNumberOfRegistersRepresentedBy(operand_types[i]);
    }

    if (count == 0) {
      continue;
    }

    Register reg = Register::FromOperand(static_cast<int32_t>(operands[i]));
    if (Bytecodes::IsRegisterInputOperandType(operand_types[i])) {
      if (count == 1) {
        PrepareRegisterInputOperand(node, reg, i);
      } else if (count > 1) {
        PrepareRegisterRangeInputOperand(reg, count);
      }
    } else if (Bytecodes::IsRegisterOutputOperandType(operand_types[i])) {
      PrepareRegisterRangeOutputOperand(reg, count);
    }
  }
}

void BytecodeRegisterOptimizer::PrepareAccumulator(BytecodeNode* const node) {
  // Materialize the accumulator if it is read by the bytecode. The
  // accumulator is special and no other register can be materialized
  // in it's place.
  if (Bytecodes::ReadsAccumulator(node->bytecode()) &&
      !accumulator_info_->materialized()) {
    Materialize(accumulator_info_);
  }

  // Materialize an equivalent to the accumulator if it will be
  // clobbered when the bytecode is dispatched.
  if (Bytecodes::WritesAccumulator(node->bytecode())) {
    PrepareRegisterOutputOperand(accumulator_info_);
  }
}

void BytecodeRegisterOptimizer::PrepareOperands(BytecodeNode* const node) {
  PrepareAccumulator(node);
  PrepareRegisterOperands(node);
}

// static
Register BytecodeRegisterOptimizer::GetRegisterInputOperand(
    int index, Bytecode bytecode, const uint32_t* operands, int operand_count) {
  DCHECK_LT(index, operand_count);
  DCHECK(Bytecodes::IsRegisterInputOperandType(
      Bytecodes::GetOperandType(bytecode, index)));
  return OperandToRegister(operands[index]);
}

// static
Register BytecodeRegisterOptimizer::GetRegisterOutputOperand(
    int index, Bytecode bytecode, const uint32_t* operands, int operand_count) {
  DCHECK_LT(index, operand_count);
  DCHECK(Bytecodes::IsRegisterOutputOperandType(
      Bytecodes::GetOperandType(bytecode, index)));
  return OperandToRegister(operands[index]);
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::GetRegisterInfo(Register reg) {
  size_t index = GetRegisterInfoTableIndex(reg);
  DCHECK_LT(index, register_info_table_.size());
  return register_info_table_[index];
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::GetOrCreateRegisterInfo(Register reg) {
  size_t index = GetRegisterInfoTableIndex(reg);
  return index < register_info_table_.size() ? register_info_table_[index]
                                             : NewRegisterInfo(reg);
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::NewRegisterInfo(Register reg) {
  size_t index = GetRegisterInfoTableIndex(reg);
  DCHECK_GE(index, register_info_table_.size());
  GrowRegisterMap(reg);
  return register_info_table_[index];
}

void BytecodeRegisterOptimizer::GrowRegisterMap(Register reg) {
  DCHECK(RegisterIsTemporary(reg));
  size_t index = GetRegisterInfoTableIndex(reg);
  if (index >= register_info_table_.size()) {
    size_t new_size = index + 1;
    size_t old_size = register_info_table_.size();
    register_info_table_.resize(new_size);
    for (size_t i = old_size; i < new_size; ++i) {
      register_info_table_[i] =
          new (zone()) RegisterInfo(RegisterFromRegisterInfoTableIndex(i),
                                    NextEquivalenceId(), false, false);
    }
  }
}

void BytecodeRegisterOptimizer::RegisterAllocateEvent(Register reg) {
  GetOrCreateRegisterInfo(reg)->set_allocated(true);
}

void BytecodeRegisterOptimizer::RegisterListAllocateEvent(
    RegisterList reg_list) {
  if (reg_list.register_count() != 0) {
    int first_index = reg_list.first_register().index();
    GrowRegisterMap(Register(first_index + reg_list.register_count() - 1));
    for (int i = 0; i < reg_list.register_count(); i++) {
      GetRegisterInfo(Register(first_index + i))->set_allocated(true);
    }
  }
}

void BytecodeRegisterOptimizer::RegisterListFreeEvent(RegisterList reg_list) {
  int first_index = reg_list.first_register().index();
  for (int i = 0; i < reg_list.register_count(); i++) {
    GetRegisterInfo(Register(first_index + i))->set_allocated(false);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
