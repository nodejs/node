// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/mips/lithium-mips.h"

#include <sstream>

#if V8_TARGET_ARCH_MIPS

#include "src/crankshaft/hydrogen-osr.h"
#include "src/crankshaft/lithium-inl.h"
#include "src/crankshaft/mips/lithium-codegen-mips.h"

namespace v8 {
namespace internal {

#define DEFINE_COMPILE(type)                            \
  void L##type::CompileToNative(LCodeGen* generator) {  \
    generator->Do##type(this);                          \
  }
LITHIUM_CONCRETE_INSTRUCTION_LIST(DEFINE_COMPILE)
#undef DEFINE_COMPILE

#ifdef DEBUG
void LInstruction::VerifyCall() {
  // Call instructions can use only fixed registers as temporaries and
  // outputs because all registers are blocked by the calling convention.
  // Inputs operands must use a fixed register or use-at-start policy or
  // a non-register policy.
  DCHECK(Output() == NULL ||
         LUnallocated::cast(Output())->HasFixedPolicy() ||
         !LUnallocated::cast(Output())->HasRegisterPolicy());
  for (UseIterator it(this); !it.Done(); it.Advance()) {
    LUnallocated* operand = LUnallocated::cast(it.Current());
    DCHECK(operand->HasFixedPolicy() ||
           operand->IsUsedAtStart());
  }
  for (TempIterator it(this); !it.Done(); it.Advance()) {
    LUnallocated* operand = LUnallocated::cast(it.Current());
    DCHECK(operand->HasFixedPolicy() ||!operand->HasRegisterPolicy());
  }
}
#endif


void LInstruction::PrintTo(StringStream* stream) {
  stream->Add("%s ", this->Mnemonic());

  PrintOutputOperandTo(stream);

  PrintDataTo(stream);

  if (HasEnvironment()) {
    stream->Add(" ");
    environment()->PrintTo(stream);
  }

  if (HasPointerMap()) {
    stream->Add(" ");
    pointer_map()->PrintTo(stream);
  }
}


void LInstruction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  for (int i = 0; i < InputCount(); i++) {
    if (i > 0) stream->Add(" ");
    if (InputAt(i) == NULL) {
      stream->Add("NULL");
    } else {
      InputAt(i)->PrintTo(stream);
    }
  }
}


void LInstruction::PrintOutputOperandTo(StringStream* stream) {
  if (HasResult()) result()->PrintTo(stream);
}


void LLabel::PrintDataTo(StringStream* stream) {
  LGap::PrintDataTo(stream);
  LLabel* rep = replacement();
  if (rep != NULL) {
    stream->Add(" Dead block replaced with B%d", rep->block_id());
  }
}


bool LGap::IsRedundant() const {
  for (int i = 0; i < 4; i++) {
    if (parallel_moves_[i] != NULL && !parallel_moves_[i]->IsRedundant()) {
      return false;
    }
  }

  return true;
}


void LGap::PrintDataTo(StringStream* stream) {
  for (int i = 0; i < 4; i++) {
    stream->Add("(");
    if (parallel_moves_[i] != NULL) {
      parallel_moves_[i]->PrintDataTo(stream);
    }
    stream->Add(") ");
  }
}


const char* LArithmeticD::Mnemonic() const {
  switch (op()) {
    case Token::ADD: return "add-d";
    case Token::SUB: return "sub-d";
    case Token::MUL: return "mul-d";
    case Token::DIV: return "div-d";
    case Token::MOD: return "mod-d";
    default:
      UNREACHABLE();
      return NULL;
  }
}


const char* LArithmeticT::Mnemonic() const {
  switch (op()) {
    case Token::ADD: return "add-t";
    case Token::SUB: return "sub-t";
    case Token::MUL: return "mul-t";
    case Token::MOD: return "mod-t";
    case Token::DIV: return "div-t";
    case Token::BIT_AND: return "bit-and-t";
    case Token::BIT_OR: return "bit-or-t";
    case Token::BIT_XOR: return "bit-xor-t";
    case Token::ROR: return "ror-t";
    case Token::SHL: return "sll-t";
    case Token::SAR: return "sra-t";
    case Token::SHR: return "srl-t";
    default:
      UNREACHABLE();
      return NULL;
  }
}


bool LGoto::HasInterestingComment(LCodeGen* gen) const {
  return !gen->IsNextEmittedBlock(block_id());
}


void LGoto::PrintDataTo(StringStream* stream) {
  stream->Add("B%d", block_id());
}


void LBranch::PrintDataTo(StringStream* stream) {
  stream->Add("B%d | B%d on ", true_block_id(), false_block_id());
  value()->PrintTo(stream);
}


LInstruction* LChunkBuilder::DoDebugBreak(HDebugBreak* instr) {
  return new(zone()) LDebugBreak();
}


void LCompareNumericAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  left()->PrintTo(stream);
  stream->Add(" %s ", Token::String(op()));
  right()->PrintTo(stream);
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsStringAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_string(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsSmiAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_smi(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsUndetectableAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_undetectable(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LStringCompareAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if string_compare(");
  left()->PrintTo(stream);
  right()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_instance_type(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasCachedArrayIndexAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_cached_array_index(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if class_of_test(");
  value()->PrintTo(stream);
  stream->Add(", \"%o\") then B%d else B%d",
              *hydrogen()->class_name(),
              true_block_id(),
              false_block_id());
}


void LTypeofIsAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if typeof ");
  value()->PrintTo(stream);
  stream->Add(" == \"%s\" then B%d else B%d",
              hydrogen()->type_literal()->ToCString().get(),
              true_block_id(), false_block_id());
}


void LStoreCodeEntry::PrintDataTo(StringStream* stream) {
  stream->Add(" = ");
  function()->PrintTo(stream);
  stream->Add(".code_entry = ");
  code_object()->PrintTo(stream);
}


void LInnerAllocatedObject::PrintDataTo(StringStream* stream) {
  stream->Add(" = ");
  base_object()->PrintTo(stream);
  stream->Add(" + ");
  offset()->PrintTo(stream);
}


void LCallFunction::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add(" ");
  function()->PrintTo(stream);
  if (hydrogen()->HasVectorAndSlot()) {
    stream->Add(" (type-feedback-vector ");
    temp_vector()->PrintTo(stream);
    stream->Add(" ");
    temp_slot()->PrintTo(stream);
    stream->Add(")");
  }
}


void LCallJSFunction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  function()->PrintTo(stream);
  stream->Add("#%d / ", arity());
}


void LCallWithDescriptor::PrintDataTo(StringStream* stream) {
  for (int i = 0; i < InputCount(); i++) {
    InputAt(i)->PrintTo(stream);
    stream->Add(" ");
  }
  stream->Add("#%d / ", arity());
}


void LLoadContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add("[%d]", slot_index());
}


void LStoreContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add("[%d] <- ", slot_index());
  value()->PrintTo(stream);
}


void LInvokeFunction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  function()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


void LCallNewArray::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  constructor()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
  ElementsKind kind = hydrogen()->elements_kind();
  stream->Add(" (%s) ", ElementsKindToString(kind));
}


void LAccessArgumentsAt::PrintDataTo(StringStream* stream) {
  arguments()->PrintTo(stream);
  stream->Add(" length ");
  length()->PrintTo(stream);
  stream->Add(" index ");
  index()->PrintTo(stream);
}


void LStoreNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  std::ostringstream os;
  os << hydrogen()->access() << " <- ";
  stream->Add(os.str().c_str());
  value()->PrintTo(stream);
}


void LStoreNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(String::cast(*name())->ToCString().get());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LLoadKeyed::PrintDataTo(StringStream* stream) {
  elements()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  if (hydrogen()->IsDehoisted()) {
    stream->Add(" + %d]", base_offset());
  } else {
    stream->Add("]");
  }
}


void LStoreKeyed::PrintDataTo(StringStream* stream) {
  elements()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  if (hydrogen()->IsDehoisted()) {
    stream->Add(" + %d] <-", base_offset());
  } else {
    stream->Add("] <- ");
  }

  if (value() == NULL) {
    DCHECK(hydrogen()->IsConstantHoleStore() &&
           hydrogen()->value()->representation().IsDouble());
    stream->Add("<the hole(nan)>");
  } else {
    value()->PrintTo(stream);
  }
}


void LStoreKeyedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  stream->Add("] <- ");
  value()->PrintTo(stream);
}


void LTransitionElementsKind::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(" %p -> %p", *original_map(), *transitioned_map());
}


int LPlatformChunk::GetNextSpillIndex(RegisterKind kind) {
  // Skip a slot if for a double-width slot.
  if (kind == DOUBLE_REGISTERS) current_frame_slots_++;
  return current_frame_slots_++;
}


LOperand* LPlatformChunk::GetNextSpillSlot(RegisterKind kind)  {
  int index = GetNextSpillIndex(kind);
  if (kind == DOUBLE_REGISTERS) {
    return LDoubleStackSlot::Create(index, zone());
  } else {
    DCHECK(kind == GENERAL_REGISTERS);
    return LStackSlot::Create(index, zone());
  }
}


LPlatformChunk* LChunkBuilder::Build() {
  DCHECK(is_unused());
  chunk_ = new(zone()) LPlatformChunk(info(), graph());
  LPhase phase("L_Building chunk", chunk_);
  status_ = BUILDING;

  // If compiling for OSR, reserve space for the unoptimized frame,
  // which will be subsumed into this frame.
  if (graph()->has_osr()) {
    for (int i = graph()->osr()->UnoptimizedFrameSlots(); i > 0; i--) {
      chunk_->GetNextSpillIndex(GENERAL_REGISTERS);
    }
  }

  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  for (int i = 0; i < blocks->length(); i++) {
    HBasicBlock* next = NULL;
    if (i < blocks->length() - 1) next = blocks->at(i + 1);
    DoBasicBlock(blocks->at(i), next);
    if (is_aborted()) return NULL;
  }
  status_ = DONE;
  return chunk_;
}


LUnallocated* LChunkBuilder::ToUnallocated(Register reg) {
  return new (zone()) LUnallocated(LUnallocated::FIXED_REGISTER, reg.code());
}


LUnallocated* LChunkBuilder::ToUnallocated(DoubleRegister reg) {
  return new (zone())
      LUnallocated(LUnallocated::FIXED_DOUBLE_REGISTER, reg.code());
}


LOperand* LChunkBuilder::UseFixed(HValue* value, Register fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseFixedDouble(HValue* value, DoubleRegister reg) {
  return Use(value, ToUnallocated(reg));
}


LOperand* LChunkBuilder::UseRegister(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LOperand* LChunkBuilder::UseRegisterAtStart(HValue* value) {
  return Use(value,
             new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER,
                                      LUnallocated::USED_AT_START));
}


LOperand* LChunkBuilder::UseTempRegister(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::WRITABLE_REGISTER));
}


LOperand* LChunkBuilder::Use(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::NONE));
}


LOperand* LChunkBuilder::UseAtStart(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::NONE,
                                     LUnallocated::USED_AT_START));
}


LOperand* LChunkBuilder::UseOrConstant(HValue* value) {
  return value->IsConstant()
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : Use(value);
}


LOperand* LChunkBuilder::UseOrConstantAtStart(HValue* value) {
  return value->IsConstant()
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseAtStart(value);
}


LOperand* LChunkBuilder::UseRegisterOrConstant(HValue* value) {
  return value->IsConstant()
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseRegister(value);
}


LOperand* LChunkBuilder::UseRegisterOrConstantAtStart(HValue* value) {
  return value->IsConstant()
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseRegisterAtStart(value);
}


LOperand* LChunkBuilder::UseConstant(HValue* value) {
  return chunk_->DefineConstantOperand(HConstant::cast(value));
}


LOperand* LChunkBuilder::UseAny(HValue* value) {
  return value->IsConstant()
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      :  Use(value, new(zone()) LUnallocated(LUnallocated::ANY));
}


LOperand* LChunkBuilder::Use(HValue* value, LUnallocated* operand) {
  if (value->EmitAtUses()) {
    HInstruction* instr = HInstruction::cast(value);
    VisitInstruction(instr);
  }
  operand->set_virtual_register(value->id());
  return operand;
}


LInstruction* LChunkBuilder::Define(LTemplateResultInstruction<1>* instr,
                                    LUnallocated* result) {
  result->set_virtual_register(current_instruction_->id());
  instr->set_result(result);
  return instr;
}


LInstruction* LChunkBuilder::DefineAsRegister(
    LTemplateResultInstruction<1>* instr) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LInstruction* LChunkBuilder::DefineAsSpilled(
    LTemplateResultInstruction<1>* instr, int index) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::FIXED_SLOT, index));
}


LInstruction* LChunkBuilder::DefineSameAsFirst(
    LTemplateResultInstruction<1>* instr) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::SAME_AS_FIRST_INPUT));
}


LInstruction* LChunkBuilder::DefineFixed(
    LTemplateResultInstruction<1>* instr, Register reg) {
  return Define(instr, ToUnallocated(reg));
}


LInstruction* LChunkBuilder::DefineFixedDouble(
    LTemplateResultInstruction<1>* instr, DoubleRegister reg) {
  return Define(instr, ToUnallocated(reg));
}


LInstruction* LChunkBuilder::AssignEnvironment(LInstruction* instr) {
  HEnvironment* hydrogen_env = current_block_->last_environment();
  int argument_index_accumulator = 0;
  ZoneList<HValue*> objects_to_materialize(0, zone());
  instr->set_environment(CreateEnvironment(hydrogen_env,
                                           &argument_index_accumulator,
                                           &objects_to_materialize));
  return instr;
}


LInstruction* LChunkBuilder::MarkAsCall(LInstruction* instr,
                                        HInstruction* hinstr,
                                        CanDeoptimize can_deoptimize) {
  info()->MarkAsNonDeferredCalling();
#ifdef DEBUG
  instr->VerifyCall();
#endif
  instr->MarkAsCall();
  instr = AssignPointerMap(instr);

  // If instruction does not have side-effects lazy deoptimization
  // after the call will try to deoptimize to the point before the call.
  // Thus we still need to attach environment to this call even if
  // call sequence can not deoptimize eagerly.
  bool needs_environment =
      (can_deoptimize == CAN_DEOPTIMIZE_EAGERLY) ||
      !hinstr->HasObservableSideEffects();
  if (needs_environment && !instr->HasEnvironment()) {
    instr = AssignEnvironment(instr);
    // We can't really figure out if the environment is needed or not.
    instr->environment()->set_has_been_used();
  }

  return instr;
}


LInstruction* LChunkBuilder::AssignPointerMap(LInstruction* instr) {
  DCHECK(!instr->HasPointerMap());
  instr->set_pointer_map(new(zone()) LPointerMap(zone()));
  return instr;
}


LUnallocated* LChunkBuilder::TempRegister() {
  LUnallocated* operand =
      new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER);
  int vreg = allocator_->GetVirtualRegister();
  if (!allocator_->AllocationOk()) {
    Abort(kOutOfVirtualRegistersWhileTryingToAllocateTempRegister);
    vreg = 0;
  }
  operand->set_virtual_register(vreg);
  return operand;
}


LUnallocated* LChunkBuilder::TempDoubleRegister() {
  LUnallocated* operand =
      new(zone()) LUnallocated(LUnallocated::MUST_HAVE_DOUBLE_REGISTER);
  int vreg = allocator_->GetVirtualRegister();
  if (!allocator_->AllocationOk()) {
    Abort(kOutOfVirtualRegistersWhileTryingToAllocateTempRegister);
    vreg = 0;
  }
  operand->set_virtual_register(vreg);
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(Register reg) {
  LUnallocated* operand = ToUnallocated(reg);
  DCHECK(operand->HasFixedPolicy());
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(DoubleRegister reg) {
  LUnallocated* operand = ToUnallocated(reg);
  DCHECK(operand->HasFixedPolicy());
  return operand;
}


LInstruction* LChunkBuilder::DoBlockEntry(HBlockEntry* instr) {
  return new(zone()) LLabel(instr->block());
}


LInstruction* LChunkBuilder::DoDummyUse(HDummyUse* instr) {
  return DefineAsRegister(new(zone()) LDummyUse(UseAny(instr->value())));
}


LInstruction* LChunkBuilder::DoEnvironmentMarker(HEnvironmentMarker* instr) {
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoDeoptimize(HDeoptimize* instr) {
  return AssignEnvironment(new(zone()) LDeoptimize);
}


LInstruction* LChunkBuilder::DoShift(Token::Value op,
                                     HBitwiseBinaryOperation* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    LOperand* left = UseRegisterAtStart(instr->left());

    HValue* right_value = instr->right();
    LOperand* right = NULL;
    int constant_value = 0;
    bool does_deopt = false;
    if (right_value->IsConstant()) {
      HConstant* constant = HConstant::cast(right_value);
      right = chunk_->DefineConstantOperand(constant);
      constant_value = constant->Integer32Value() & 0x1f;
      // Left shifts can deoptimize if we shift by > 0 and the result cannot be
      // truncated to smi.
      if (instr->representation().IsSmi() && constant_value > 0) {
        does_deopt = !instr->CheckUsesForFlag(HValue::kTruncatingToSmi);
      }
    } else {
      right = UseRegisterAtStart(right_value);
    }

    // Shift operations can only deoptimize if we do a logical shift
    // by 0 and the result cannot be truncated to int32.
    if (op == Token::SHR && constant_value == 0) {
      does_deopt = !instr->CheckFlag(HInstruction::kUint32);
    }

    LInstruction* result =
        DefineAsRegister(new(zone()) LShiftI(op, left, right, does_deopt));
    return does_deopt ? AssignEnvironment(result) : result;
  } else {
    return DoArithmeticT(op, instr);
  }
}


LInstruction* LChunkBuilder::DoArithmeticD(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  DCHECK(instr->representation().IsDouble());
  DCHECK(instr->left()->representation().IsDouble());
  DCHECK(instr->right()->representation().IsDouble());
  if (op == Token::MOD) {
    LOperand* left = UseFixedDouble(instr->left(), f2);
    LOperand* right = UseFixedDouble(instr->right(), f4);
    LArithmeticD* result = new(zone()) LArithmeticD(op, left, right);
    // We call a C function for double modulo. It can't trigger a GC. We need
    // to use fixed result register for the call.
    // TODO(fschneider): Allow any register as input registers.
    return MarkAsCall(DefineFixedDouble(result, f2), instr);
  } else {
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseRegisterAtStart(instr->right());
    LArithmeticD* result = new(zone()) LArithmeticD(op, left, right);
    return DefineAsRegister(result);
  }
}


LInstruction* LChunkBuilder::DoArithmeticT(Token::Value op,
                                           HBinaryOperation* instr) {
  HValue* left = instr->left();
  HValue* right = instr->right();
  DCHECK(left->representation().IsTagged());
  DCHECK(right->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left_operand = UseFixed(left, a1);
  LOperand* right_operand = UseFixed(right, a0);
  LArithmeticT* result =
      new(zone()) LArithmeticT(op, context, left_operand, right_operand);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


void LChunkBuilder::DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block) {
  DCHECK(is_building());
  current_block_ = block;
  next_block_ = next_block;
  if (block->IsStartBlock()) {
    block->UpdateEnvironment(graph_->start_environment());
    argument_count_ = 0;
  } else if (block->predecessors()->length() == 1) {
    // We have a single predecessor => copy environment and outgoing
    // argument count from the predecessor.
    DCHECK(block->phis()->length() == 0);
    HBasicBlock* pred = block->predecessors()->at(0);
    HEnvironment* last_environment = pred->last_environment();
    DCHECK(last_environment != NULL);
    // Only copy the environment, if it is later used again.
    if (pred->end()->SecondSuccessor() == NULL) {
      DCHECK(pred->end()->FirstSuccessor() == block);
    } else {
      if (pred->end()->FirstSuccessor()->block_id() > block->block_id() ||
          pred->end()->SecondSuccessor()->block_id() > block->block_id()) {
        last_environment = last_environment->Copy();
      }
    }
    block->UpdateEnvironment(last_environment);
    DCHECK(pred->argument_count() >= 0);
    argument_count_ = pred->argument_count();
  } else {
    // We are at a state join => process phis.
    HBasicBlock* pred = block->predecessors()->at(0);
    // No need to copy the environment, it cannot be used later.
    HEnvironment* last_environment = pred->last_environment();
    for (int i = 0; i < block->phis()->length(); ++i) {
      HPhi* phi = block->phis()->at(i);
      if (phi->HasMergedIndex()) {
        last_environment->SetValueAt(phi->merged_index(), phi);
      }
    }
    for (int i = 0; i < block->deleted_phis()->length(); ++i) {
      if (block->deleted_phis()->at(i) < last_environment->length()) {
        last_environment->SetValueAt(block->deleted_phis()->at(i),
                                     graph_->GetConstantUndefined());
      }
    }
    block->UpdateEnvironment(last_environment);
    // Pick up the outgoing argument count of one of the predecessors.
    argument_count_ = pred->argument_count();
  }
  HInstruction* current = block->first();
  int start = chunk_->instructions()->length();
  while (current != NULL && !is_aborted()) {
    // Code for constants in registers is generated lazily.
    if (!current->EmitAtUses()) {
      VisitInstruction(current);
    }
    current = current->next();
  }
  int end = chunk_->instructions()->length() - 1;
  if (end >= start) {
    block->set_first_instruction_index(start);
    block->set_last_instruction_index(end);
  }
  block->set_argument_count(argument_count_);
  next_block_ = NULL;
  current_block_ = NULL;
}


void LChunkBuilder::VisitInstruction(HInstruction* current) {
  HInstruction* old_current = current_instruction_;
  current_instruction_ = current;

  LInstruction* instr = NULL;
  if (current->CanReplaceWithDummyUses()) {
    if (current->OperandCount() == 0) {
      instr = DefineAsRegister(new(zone()) LDummy());
    } else {
      DCHECK(!current->OperandAt(0)->IsControlInstruction());
      instr = DefineAsRegister(new(zone())
          LDummyUse(UseAny(current->OperandAt(0))));
    }
    for (int i = 1; i < current->OperandCount(); ++i) {
      if (current->OperandAt(i)->IsControlInstruction()) continue;
      LInstruction* dummy =
          new(zone()) LDummyUse(UseAny(current->OperandAt(i)));
      dummy->set_hydrogen_value(current);
      chunk_->AddInstruction(dummy, current_block_);
    }
  } else {
    HBasicBlock* successor;
    if (current->IsControlInstruction() &&
        HControlInstruction::cast(current)->KnownSuccessorBlock(&successor) &&
        successor != NULL) {
      instr = new(zone()) LGoto(successor);
    } else {
      instr = current->CompileToLithium(this);
    }
  }

  argument_count_ += current->argument_delta();
  DCHECK(argument_count_ >= 0);

  if (instr != NULL) {
    AddInstruction(instr, current);
  }

  current_instruction_ = old_current;
}


void LChunkBuilder::AddInstruction(LInstruction* instr,
                                   HInstruction* hydrogen_val) {
// Associate the hydrogen instruction first, since we may need it for
  // the ClobbersRegisters() or ClobbersDoubleRegisters() calls below.
  instr->set_hydrogen_value(hydrogen_val);

#if DEBUG
  // Make sure that the lithium instruction has either no fixed register
  // constraints in temps or the result OR no uses that are only used at
  // start. If this invariant doesn't hold, the register allocator can decide
  // to insert a split of a range immediately before the instruction due to an
  // already allocated register needing to be used for the instruction's fixed
  // register constraint. In this case, The register allocator won't see an
  // interference between the split child and the use-at-start (it would if
  // the it was just a plain use), so it is free to move the split child into
  // the same register that is used for the use-at-start.
  // See https://code.google.com/p/chromium/issues/detail?id=201590
  if (!(instr->ClobbersRegisters() &&
        instr->ClobbersDoubleRegisters(isolate()))) {
    int fixed = 0;
    int used_at_start = 0;
    for (UseIterator it(instr); !it.Done(); it.Advance()) {
      LUnallocated* operand = LUnallocated::cast(it.Current());
      if (operand->IsUsedAtStart()) ++used_at_start;
    }
    if (instr->Output() != NULL) {
      if (LUnallocated::cast(instr->Output())->HasFixedPolicy()) ++fixed;
    }
    for (TempIterator it(instr); !it.Done(); it.Advance()) {
      LUnallocated* operand = LUnallocated::cast(it.Current());
      if (operand->HasFixedPolicy()) ++fixed;
    }
    DCHECK(fixed == 0 || used_at_start == 0);
  }
#endif

  if (FLAG_stress_pointer_maps && !instr->HasPointerMap()) {
    instr = AssignPointerMap(instr);
  }
  if (FLAG_stress_environments && !instr->HasEnvironment()) {
    instr = AssignEnvironment(instr);
  }
  chunk_->AddInstruction(instr, current_block_);

  if (instr->IsCall() || instr->IsPrologue()) {
    HValue* hydrogen_value_for_lazy_bailout = hydrogen_val;
    if (hydrogen_val->HasObservableSideEffects()) {
      HSimulate* sim = HSimulate::cast(hydrogen_val->next());
      sim->ReplayEnvironment(current_block_->last_environment());
      hydrogen_value_for_lazy_bailout = sim;
    }
    LInstruction* bailout = AssignEnvironment(new(zone()) LLazyBailout());
    bailout->set_hydrogen_value(hydrogen_value_for_lazy_bailout);
    chunk_->AddInstruction(bailout, current_block_);
  }
}


LInstruction* LChunkBuilder::DoPrologue(HPrologue* instr) {
  return new (zone()) LPrologue();
}


LInstruction* LChunkBuilder::DoGoto(HGoto* instr) {
  return new(zone()) LGoto(instr->FirstSuccessor());
}


LInstruction* LChunkBuilder::DoBranch(HBranch* instr) {
  HValue* value = instr->value();
  Representation r = value->representation();
  HType type = value->type();
  ToBooleanStub::Types expected = instr->expected_input_types();
  if (expected.IsEmpty()) expected = ToBooleanStub::Types::Generic();

  bool easy_case = !r.IsTagged() || type.IsBoolean() || type.IsSmi() ||
      type.IsJSArray() || type.IsHeapNumber() || type.IsString();
  LInstruction* branch = new(zone()) LBranch(UseRegister(value));
  if (!easy_case &&
      ((!expected.Contains(ToBooleanStub::SMI) && expected.NeedsMap()) ||
       !expected.IsGeneric())) {
    branch = AssignEnvironment(branch);
  }
  return branch;
}


LInstruction* LChunkBuilder::DoCompareMap(HCompareMap* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  return new(zone()) LCmpMapAndBranch(value, temp);
}


LInstruction* LChunkBuilder::DoArgumentsLength(HArgumentsLength* length) {
  info()->MarkAsRequiresFrame();
  return DefineAsRegister(
      new(zone()) LArgumentsLength(UseRegister(length->value())));
}


LInstruction* LChunkBuilder::DoArgumentsElements(HArgumentsElements* elems) {
  info()->MarkAsRequiresFrame();
  return DefineAsRegister(new(zone()) LArgumentsElements);
}


LInstruction* LChunkBuilder::DoInstanceOf(HInstanceOf* instr) {
  LOperand* left =
      UseFixed(instr->left(), InstanceOfDescriptor::LeftRegister());
  LOperand* right =
      UseFixed(instr->right(), InstanceOfDescriptor::RightRegister());
  LOperand* context = UseFixed(instr->context(), cp);
  LInstanceOf* result = new (zone()) LInstanceOf(context, left, right);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoHasInPrototypeChainAndBranch(
    HHasInPrototypeChainAndBranch* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* prototype = UseRegister(instr->prototype());
  LHasInPrototypeChainAndBranch* result =
      new (zone()) LHasInPrototypeChainAndBranch(object, prototype);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoWrapReceiver(HWrapReceiver* instr) {
  LOperand* receiver = UseRegisterAtStart(instr->receiver());
  LOperand* function = UseRegisterAtStart(instr->function());
  LWrapReceiver* result = new(zone()) LWrapReceiver(receiver, function);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoApplyArguments(HApplyArguments* instr) {
  LOperand* function = UseFixed(instr->function(), a1);
  LOperand* receiver = UseFixed(instr->receiver(), a0);
  LOperand* length = UseFixed(instr->length(), a2);
  LOperand* elements = UseFixed(instr->elements(), a3);
  LApplyArguments* result = new(zone()) LApplyArguments(function,
                                                        receiver,
                                                        length,
                                                        elements);
  return MarkAsCall(DefineFixed(result, v0), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoPushArguments(HPushArguments* instr) {
  int argc = instr->OperandCount();
  for (int i = 0; i < argc; ++i) {
    LOperand* argument = Use(instr->argument(i));
    AddInstruction(new(zone()) LPushArgument(argument), instr);
  }
  return NULL;
}


LInstruction* LChunkBuilder::DoStoreCodeEntry(
    HStoreCodeEntry* store_code_entry) {
  LOperand* function = UseRegister(store_code_entry->function());
  LOperand* code_object = UseTempRegister(store_code_entry->code_object());
  return new(zone()) LStoreCodeEntry(function, code_object);
}


LInstruction* LChunkBuilder::DoInnerAllocatedObject(
    HInnerAllocatedObject* instr) {
  LOperand* base_object = UseRegisterAtStart(instr->base_object());
  LOperand* offset = UseRegisterOrConstantAtStart(instr->offset());
  return DefineAsRegister(
      new(zone()) LInnerAllocatedObject(base_object, offset));
}


LInstruction* LChunkBuilder::DoThisFunction(HThisFunction* instr) {
  return instr->HasNoUses()
      ? NULL
      : DefineAsRegister(new(zone()) LThisFunction);
}


LInstruction* LChunkBuilder::DoContext(HContext* instr) {
  if (instr->HasNoUses()) return NULL;

  if (info()->IsStub()) {
    return DefineFixed(new(zone()) LContext, cp);
  }

  return DefineAsRegister(new(zone()) LContext);
}


LInstruction* LChunkBuilder::DoDeclareGlobals(HDeclareGlobals* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  return MarkAsCall(new(zone()) LDeclareGlobals(context), instr);
}


LInstruction* LChunkBuilder::DoCallJSFunction(
    HCallJSFunction* instr) {
  LOperand* function = UseFixed(instr->function(), a1);

  LCallJSFunction* result = new(zone()) LCallJSFunction(function);

  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoCallWithDescriptor(
    HCallWithDescriptor* instr) {
  CallInterfaceDescriptor descriptor = instr->descriptor();

  LOperand* target = UseRegisterOrConstantAtStart(instr->target());
  ZoneList<LOperand*> ops(instr->OperandCount(), zone());
  // Target
  ops.Add(target, zone());
  // Context
  LOperand* op = UseFixed(instr->OperandAt(1), cp);
  ops.Add(op, zone());
  // Other register parameters
  for (int i = LCallWithDescriptor::kImplicitRegisterParameterCount;
       i < instr->OperandCount(); i++) {
    op =
        UseFixed(instr->OperandAt(i),
                 descriptor.GetRegisterParameter(
                     i - LCallWithDescriptor::kImplicitRegisterParameterCount));
    ops.Add(op, zone());
  }

  LCallWithDescriptor* result = new(zone()) LCallWithDescriptor(
      descriptor, ops, zone());
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoInvokeFunction(HInvokeFunction* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* function = UseFixed(instr->function(), a1);
  LInvokeFunction* result = new(zone()) LInvokeFunction(context, function);
  return MarkAsCall(DefineFixed(result, v0), instr, CANNOT_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoUnaryMathOperation(HUnaryMathOperation* instr) {
  switch (instr->op()) {
    case kMathFloor:
      return DoMathFloor(instr);
    case kMathRound:
      return DoMathRound(instr);
    case kMathFround:
      return DoMathFround(instr);
    case kMathAbs:
      return DoMathAbs(instr);
    case kMathLog:
      return DoMathLog(instr);
    case kMathExp:
      return DoMathExp(instr);
    case kMathSqrt:
      return DoMathSqrt(instr);
    case kMathPowHalf:
      return DoMathPowHalf(instr);
    case kMathClz32:
      return DoMathClz32(instr);
    default:
      UNREACHABLE();
      return NULL;
  }
}


LInstruction* LChunkBuilder::DoMathLog(HUnaryMathOperation* instr) {
  DCHECK(instr->representation().IsDouble());
  DCHECK(instr->value()->representation().IsDouble());
  LOperand* input = UseFixedDouble(instr->value(), f4);
  return MarkAsCall(DefineFixedDouble(new(zone()) LMathLog(input), f4), instr);
}


LInstruction* LChunkBuilder::DoMathClz32(HUnaryMathOperation* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  LMathClz32* result = new(zone()) LMathClz32(input);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoMathExp(HUnaryMathOperation* instr) {
  DCHECK(instr->representation().IsDouble());
  DCHECK(instr->value()->representation().IsDouble());
  LOperand* input = UseRegister(instr->value());
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();
  LOperand* double_temp = TempDoubleRegister();
  LMathExp* result = new(zone()) LMathExp(input, double_temp, temp1, temp2);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoMathPowHalf(HUnaryMathOperation* instr) {
  // Input cannot be the same as the result, see LCodeGen::DoMathPowHalf.
  LOperand* input = UseFixedDouble(instr->value(), f8);
  LOperand* temp = TempDoubleRegister();
  LMathPowHalf* result = new(zone()) LMathPowHalf(input, temp);
  return DefineFixedDouble(result, f4);
}


LInstruction* LChunkBuilder::DoMathFround(HUnaryMathOperation* instr) {
  LOperand* input = UseRegister(instr->value());
  LMathFround* result = new (zone()) LMathFround(input);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoMathAbs(HUnaryMathOperation* instr) {
  Representation r = instr->value()->representation();
  LOperand* context = (r.IsDouble() || r.IsSmiOrInteger32())
      ? NULL
      : UseFixed(instr->context(), cp);
  LOperand* input = UseRegister(instr->value());
  LInstruction* result =
      DefineAsRegister(new(zone()) LMathAbs(context, input));
  if (!r.IsDouble() && !r.IsSmiOrInteger32()) result = AssignPointerMap(result);
  if (!r.IsDouble()) result = AssignEnvironment(result);
  return result;
}


LInstruction* LChunkBuilder::DoMathFloor(HUnaryMathOperation* instr) {
  LOperand* input = UseRegister(instr->value());
  LOperand* temp = TempRegister();
  LMathFloor* result = new(zone()) LMathFloor(input, temp);
  return AssignEnvironment(AssignPointerMap(DefineAsRegister(result)));
}


LInstruction* LChunkBuilder::DoMathSqrt(HUnaryMathOperation* instr) {
  LOperand* input = UseRegister(instr->value());
  LMathSqrt* result = new(zone()) LMathSqrt(input);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoMathRound(HUnaryMathOperation* instr) {
  LOperand* input = UseRegister(instr->value());
  LOperand* temp = TempDoubleRegister();
  LMathRound* result = new(zone()) LMathRound(input, temp);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoCallNewArray(HCallNewArray* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* constructor = UseFixed(instr->constructor(), a1);
  LCallNewArray* result = new(zone()) LCallNewArray(context, constructor);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoCallFunction(HCallFunction* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* function = UseFixed(instr->function(), a1);
  LOperand* slot = NULL;
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    slot = FixedTemp(a3);
    vector = FixedTemp(a2);
  }

  LCallFunction* call =
      new (zone()) LCallFunction(context, function, slot, vector);
  return MarkAsCall(DefineFixed(call, v0), instr);
}


LInstruction* LChunkBuilder::DoCallRuntime(HCallRuntime* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  return MarkAsCall(DefineFixed(new(zone()) LCallRuntime(context), v0), instr);
}


LInstruction* LChunkBuilder::DoRor(HRor* instr) {
  return DoShift(Token::ROR, instr);
}


LInstruction* LChunkBuilder::DoShr(HShr* instr) {
  return DoShift(Token::SHR, instr);
}


LInstruction* LChunkBuilder::DoSar(HSar* instr) {
  return DoShift(Token::SAR, instr);
}


LInstruction* LChunkBuilder::DoShl(HShl* instr) {
  return DoShift(Token::SHL, instr);
}


LInstruction* LChunkBuilder::DoBitwise(HBitwise* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    DCHECK(instr->CheckFlag(HValue::kTruncatingToInt32));

    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right = UseOrConstantAtStart(instr->BetterRightOperand());
    return DefineAsRegister(new(zone()) LBitI(left, right));
  } else {
    return DoArithmeticT(instr->op(), instr);
  }
}


LInstruction* LChunkBuilder::DoDivByPowerOf2I(HDiv* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LInstruction* result = DefineAsRegister(new(zone()) LDivByPowerOf2I(
          dividend, divisor));
  if ((instr->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) ||
      (instr->CheckFlag(HValue::kCanOverflow) && divisor == -1) ||
      (!instr->CheckFlag(HInstruction::kAllUsesTruncatingToInt32) &&
       divisor != 1 && divisor != -1)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoDivByConstI(HDiv* instr) {
  DCHECK(instr->representation().IsInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LInstruction* result = DefineAsRegister(new(zone()) LDivByConstI(
          dividend, divisor));
  if (divisor == 0 ||
      (instr->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) ||
      !instr->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoDivI(HDiv* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  LOperand* divisor = UseRegister(instr->right());
  LOperand* temp = TempRegister();
  LInstruction* result =
      DefineAsRegister(new(zone()) LDivI(dividend, divisor, temp));
  if (instr->CheckFlag(HValue::kCanBeDivByZero) ||
      instr->CheckFlag(HValue::kBailoutOnMinusZero) ||
      (instr->CheckFlag(HValue::kCanOverflow) &&
       !instr->CheckFlag(HValue::kAllUsesTruncatingToInt32)) ||
      (!instr->IsMathFloorOfDiv() &&
       !instr->CheckFlag(HValue::kAllUsesTruncatingToInt32))) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoDiv(HDiv* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    if (instr->RightIsPowerOf2()) {
      return DoDivByPowerOf2I(instr);
    } else if (instr->right()->IsConstant()) {
      return DoDivByConstI(instr);
    } else {
      return DoDivI(instr);
    }
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::DIV, instr);
  } else {
    return DoArithmeticT(Token::DIV, instr);
  }
}


LInstruction* LChunkBuilder::DoFlooringDivByPowerOf2I(HMathFloorOfDiv* instr) {
  LOperand* dividend = UseRegisterAtStart(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LInstruction* result = DefineAsRegister(new(zone()) LFlooringDivByPowerOf2I(
          dividend, divisor));
  if ((instr->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) ||
      (instr->CheckFlag(HValue::kLeftCanBeMinInt) && divisor == -1)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoFlooringDivByConstI(HMathFloorOfDiv* instr) {
  DCHECK(instr->representation().IsInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LOperand* temp =
      ((divisor > 0 && !instr->CheckFlag(HValue::kLeftCanBeNegative)) ||
       (divisor < 0 && !instr->CheckFlag(HValue::kLeftCanBePositive))) ?
      NULL : TempRegister();
  LInstruction* result = DefineAsRegister(
      new(zone()) LFlooringDivByConstI(dividend, divisor, temp));
  if (divisor == 0 ||
      (instr->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoFlooringDivI(HMathFloorOfDiv* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  LOperand* divisor = UseRegister(instr->right());
  LInstruction* result =
      DefineAsRegister(new (zone()) LFlooringDivI(dividend, divisor));
  if (instr->CheckFlag(HValue::kCanBeDivByZero) ||
      instr->CheckFlag(HValue::kBailoutOnMinusZero) ||
      (instr->CheckFlag(HValue::kCanOverflow))) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoMathFloorOfDiv(HMathFloorOfDiv* instr) {
  if (instr->RightIsPowerOf2()) {
    return DoFlooringDivByPowerOf2I(instr);
  } else if (instr->right()->IsConstant()) {
    return DoFlooringDivByConstI(instr);
  } else {
    return DoFlooringDivI(instr);
  }
}


LInstruction* LChunkBuilder::DoModByPowerOf2I(HMod* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegisterAtStart(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LInstruction* result = DefineSameAsFirst(new(zone()) LModByPowerOf2I(
          dividend, divisor));
  if (instr->CheckFlag(HValue::kLeftCanBeNegative) &&
      instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoModByConstI(HMod* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LInstruction* result = DefineAsRegister(new(zone()) LModByConstI(
          dividend, divisor));
  if (divisor == 0 || instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoModI(HMod* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  LOperand* divisor = UseRegister(instr->right());
  LInstruction* result = DefineAsRegister(new(zone()) LModI(
      dividend, divisor));
  if (instr->CheckFlag(HValue::kCanBeDivByZero) ||
      instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoMod(HMod* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    return instr->RightIsPowerOf2() ? DoModByPowerOf2I(instr) : DoModI(instr);
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::MOD, instr);
  } else {
    return DoArithmeticT(Token::MOD, instr);
  }
}


LInstruction* LChunkBuilder::DoMul(HMul* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    HValue* left = instr->BetterLeftOperand();
    HValue* right = instr->BetterRightOperand();
    LOperand* left_op;
    LOperand* right_op;
    bool can_overflow = instr->CheckFlag(HValue::kCanOverflow);
    bool bailout_on_minus_zero = instr->CheckFlag(HValue::kBailoutOnMinusZero);

    int32_t constant_value = 0;
    if (right->IsConstant()) {
      HConstant* constant = HConstant::cast(right);
      constant_value = constant->Integer32Value();
      // Constants -1, 0 and 1 can be optimized if the result can overflow.
      // For other constants, it can be optimized only without overflow.
      if (!can_overflow || ((constant_value >= -1) && (constant_value <= 1))) {
        left_op = UseRegisterAtStart(left);
        right_op = UseConstant(right);
      } else {
        if (bailout_on_minus_zero) {
          left_op = UseRegister(left);
        } else {
          left_op = UseRegisterAtStart(left);
        }
        right_op = UseRegister(right);
      }
    } else {
      if (bailout_on_minus_zero) {
        left_op = UseRegister(left);
      } else {
        left_op = UseRegisterAtStart(left);
      }
      right_op = UseRegister(right);
    }
    LMulI* mul = new(zone()) LMulI(left_op, right_op);
    if (right_op->IsConstantOperand()
            ? ((can_overflow && constant_value == -1) ||
               (bailout_on_minus_zero && constant_value <= 0))
            : (can_overflow || bailout_on_minus_zero)) {
      AssignEnvironment(mul);
    }
    return DefineAsRegister(mul);

  } else if (instr->representation().IsDouble()) {
    if (IsMipsArchVariant(kMips32r2)) {
      if (instr->HasOneUse() && instr->uses().value()->IsAdd()) {
        HAdd* add = HAdd::cast(instr->uses().value());
        if (instr == add->left()) {
          // This mul is the lhs of an add. The add and mul will be folded
          // into a multiply-add.
          return NULL;
        }
        if (instr == add->right() && !add->left()->IsMul()) {
          // This mul is the rhs of an add, where the lhs is not another mul.
          // The add and mul will be folded into a multiply-add.
          return NULL;
        }
      }
    }
    return DoArithmeticD(Token::MUL, instr);
  } else {
    return DoArithmeticT(Token::MUL, instr);
  }
}


LInstruction* LChunkBuilder::DoSub(HSub* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    LSubI* sub = new(zone()) LSubI(left, right);
    LInstruction* result = DefineAsRegister(sub);
    if (instr->CheckFlag(HValue::kCanOverflow)) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::SUB, instr);
  } else {
    return DoArithmeticT(Token::SUB, instr);
  }
}


LInstruction* LChunkBuilder::DoMultiplyAdd(HMul* mul, HValue* addend) {
  LOperand* multiplier_op = UseRegisterAtStart(mul->left());
  LOperand* multiplicand_op = UseRegisterAtStart(mul->right());
  LOperand* addend_op = UseRegisterAtStart(addend);
  return DefineSameAsFirst(new(zone()) LMultiplyAddD(addend_op, multiplier_op,
                                                     multiplicand_op));
}


LInstruction* LChunkBuilder::DoAdd(HAdd* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right = UseOrConstantAtStart(instr->BetterRightOperand());
    LAddI* add = new(zone()) LAddI(left, right);
    LInstruction* result = DefineAsRegister(add);
    if (instr->CheckFlag(HValue::kCanOverflow)) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsExternal()) {
    DCHECK(instr->IsConsistentExternalRepresentation());
    DCHECK(!instr->CheckFlag(HValue::kCanOverflow));
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    LAddI* add = new(zone()) LAddI(left, right);
    LInstruction* result = DefineAsRegister(add);
    return result;
  } else if (instr->representation().IsDouble()) {
    if (IsMipsArchVariant(kMips32r2)) {
      if (instr->left()->IsMul())
        return DoMultiplyAdd(HMul::cast(instr->left()), instr->right());

      if (instr->right()->IsMul()) {
        DCHECK(!instr->left()->IsMul());
        return DoMultiplyAdd(HMul::cast(instr->right()), instr->left());
      }
    }
    return DoArithmeticD(Token::ADD, instr);
  } else {
    return DoArithmeticT(Token::ADD, instr);
  }
}


LInstruction* LChunkBuilder::DoMathMinMax(HMathMinMax* instr) {
  LOperand* left = NULL;
  LOperand* right = NULL;
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    left = UseRegisterAtStart(instr->BetterLeftOperand());
    right = UseOrConstantAtStart(instr->BetterRightOperand());
  } else {
    DCHECK(instr->representation().IsDouble());
    DCHECK(instr->left()->representation().IsDouble());
    DCHECK(instr->right()->representation().IsDouble());
    left = UseRegisterAtStart(instr->left());
    right = UseRegisterAtStart(instr->right());
  }
  return DefineAsRegister(new(zone()) LMathMinMax(left, right));
}


LInstruction* LChunkBuilder::DoPower(HPower* instr) {
  DCHECK(instr->representation().IsDouble());
  // We call a C function for double power. It can't trigger a GC.
  // We need to use fixed result register for the call.
  Representation exponent_type = instr->right()->representation();
  DCHECK(instr->left()->representation().IsDouble());
  LOperand* left = UseFixedDouble(instr->left(), f2);
  LOperand* right =
      exponent_type.IsDouble()
          ? UseFixedDouble(instr->right(), f4)
          : UseFixed(instr->right(), MathPowTaggedDescriptor::exponent());
  LPower* result = new(zone()) LPower(left, right);
  return MarkAsCall(DefineFixedDouble(result, f0),
                    instr,
                    CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoCompareGeneric(HCompareGeneric* instr) {
  DCHECK(instr->left()->representation().IsTagged());
  DCHECK(instr->right()->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left = UseFixed(instr->left(), a1);
  LOperand* right = UseFixed(instr->right(), a0);
  LCmpT* result = new(zone()) LCmpT(context, left, right);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoCompareNumericAndBranch(
    HCompareNumericAndBranch* instr) {
  Representation r = instr->representation();
  if (r.IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(r));
    DCHECK(instr->right()->representation().Equals(r));
    LOperand* left = UseRegisterOrConstantAtStart(instr->left());
    LOperand* right = UseRegisterOrConstantAtStart(instr->right());
    return new(zone()) LCompareNumericAndBranch(left, right);
  } else {
    DCHECK(r.IsDouble());
    DCHECK(instr->left()->representation().IsDouble());
    DCHECK(instr->right()->representation().IsDouble());
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseRegisterAtStart(instr->right());
    return new(zone()) LCompareNumericAndBranch(left, right);
  }
}


LInstruction* LChunkBuilder::DoCompareObjectEqAndBranch(
    HCompareObjectEqAndBranch* instr) {
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterAtStart(instr->right());
  return new(zone()) LCmpObjectEqAndBranch(left, right);
}


LInstruction* LChunkBuilder::DoCompareHoleAndBranch(
    HCompareHoleAndBranch* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LCmpHoleAndBranch(value);
}


LInstruction* LChunkBuilder::DoIsStringAndBranch(HIsStringAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* temp = TempRegister();
  return new(zone()) LIsStringAndBranch(UseRegisterAtStart(instr->value()),
                                        temp);
}


LInstruction* LChunkBuilder::DoIsSmiAndBranch(HIsSmiAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  return new(zone()) LIsSmiAndBranch(Use(instr->value()));
}


LInstruction* LChunkBuilder::DoIsUndetectableAndBranch(
    HIsUndetectableAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  return new(zone()) LIsUndetectableAndBranch(
      UseRegisterAtStart(instr->value()), TempRegister());
}


LInstruction* LChunkBuilder::DoStringCompareAndBranch(
    HStringCompareAndBranch* instr) {
  DCHECK(instr->left()->representation().IsTagged());
  DCHECK(instr->right()->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left = UseFixed(instr->left(), a1);
  LOperand* right = UseFixed(instr->right(), a0);
  LStringCompareAndBranch* result =
      new(zone()) LStringCompareAndBranch(context, left, right);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoHasInstanceTypeAndBranch(
    HHasInstanceTypeAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LHasInstanceTypeAndBranch(value);
}


LInstruction* LChunkBuilder::DoGetCachedArrayIndex(
    HGetCachedArrayIndex* instr)  {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());

  return DefineAsRegister(new(zone()) LGetCachedArrayIndex(value));
}


LInstruction* LChunkBuilder::DoHasCachedArrayIndexAndBranch(
    HHasCachedArrayIndexAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  return new(zone()) LHasCachedArrayIndexAndBranch(
      UseRegisterAtStart(instr->value()));
}


LInstruction* LChunkBuilder::DoClassOfTestAndBranch(
    HClassOfTestAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  return new(zone()) LClassOfTestAndBranch(UseRegister(instr->value()),
                                           TempRegister());
}


LInstruction* LChunkBuilder::DoSeqStringGetChar(HSeqStringGetChar* instr) {
  LOperand* string = UseRegisterAtStart(instr->string());
  LOperand* index = UseRegisterOrConstantAtStart(instr->index());
  return DefineAsRegister(new(zone()) LSeqStringGetChar(string, index));
}


LInstruction* LChunkBuilder::DoSeqStringSetChar(HSeqStringSetChar* instr) {
  LOperand* string = UseRegisterAtStart(instr->string());
  LOperand* index = FLAG_debug_code
      ? UseRegisterAtStart(instr->index())
      : UseRegisterOrConstantAtStart(instr->index());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* context = FLAG_debug_code ? UseFixed(instr->context(), cp) : NULL;
  return new(zone()) LSeqStringSetChar(context, string, index, value);
}


LInstruction* LChunkBuilder::DoBoundsCheck(HBoundsCheck* instr) {
  if (!FLAG_debug_code && instr->skip_check()) return NULL;
  LOperand* index = UseRegisterOrConstantAtStart(instr->index());
  LOperand* length = !index->IsConstantOperand()
  ? UseRegisterOrConstantAtStart(instr->length())
  : UseRegisterAtStart(instr->length());
  LInstruction* result = new(zone()) LBoundsCheck(index, length);
  if (!FLAG_debug_code || !instr->skip_check()) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoBoundsCheckBaseIndexInformation(
    HBoundsCheckBaseIndexInformation* instr) {
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoAbnormalExit(HAbnormalExit* instr) {
  // The control instruction marking the end of a block that completed
  // abruptly (e.g., threw an exception).  There is nothing specific to do.
  return NULL;
}


LInstruction* LChunkBuilder::DoUseConst(HUseConst* instr) {
  return NULL;
}


LInstruction* LChunkBuilder::DoForceRepresentation(HForceRepresentation* bad) {
  // All HForceRepresentation instructions should be eliminated in the
  // representation change phase of Hydrogen.
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoChange(HChange* instr) {
  Representation from = instr->from();
  Representation to = instr->to();
  HValue* val = instr->value();
  if (from.IsSmi()) {
    if (to.IsTagged()) {
      LOperand* value = UseRegister(val);
      return DefineSameAsFirst(new(zone()) LDummyUse(value));
    }
    from = Representation::Tagged();
  }
  if (from.IsTagged()) {
    if (to.IsDouble()) {
      LOperand* value = UseRegister(val);
      LInstruction* result = DefineAsRegister(new(zone()) LNumberUntagD(value));
      if (!val->representation().IsSmi()) result = AssignEnvironment(result);
      return result;
    } else if (to.IsSmi()) {
      LOperand* value = UseRegister(val);
      if (val->type().IsSmi()) {
        return DefineSameAsFirst(new(zone()) LDummyUse(value));
      }
      return AssignEnvironment(DefineSameAsFirst(new(zone()) LCheckSmi(value)));
    } else {
      DCHECK(to.IsInteger32());
      if (val->type().IsSmi() || val->representation().IsSmi()) {
        LOperand* value = UseRegisterAtStart(val);
        return DefineAsRegister(new(zone()) LSmiUntag(value, false));
      } else {
        LOperand* value = UseRegister(val);
        LOperand* temp1 = TempRegister();
        LOperand* temp2 = TempDoubleRegister();
        LInstruction* result =
            DefineSameAsFirst(new(zone()) LTaggedToI(value, temp1, temp2));
        if (!val->representation().IsSmi()) result = AssignEnvironment(result);
        return result;
      }
    }
  } else if (from.IsDouble()) {
    if (to.IsTagged()) {
      info()->MarkAsDeferredCalling();
      LOperand* value = UseRegister(val);
      LOperand* temp1 = TempRegister();
      LOperand* temp2 = TempRegister();
      LUnallocated* result_temp = TempRegister();
      LNumberTagD* result = new(zone()) LNumberTagD(value, temp1, temp2);
      return AssignPointerMap(Define(result, result_temp));
    } else if (to.IsSmi()) {
      LOperand* value = UseRegister(val);
      return AssignEnvironment(
          DefineAsRegister(new(zone()) LDoubleToSmi(value)));
    } else {
      DCHECK(to.IsInteger32());
      LOperand* value = UseRegister(val);
      LInstruction* result = DefineAsRegister(new(zone()) LDoubleToI(value));
      if (!instr->CanTruncateToInt32()) result = AssignEnvironment(result);
      return result;
    }
  } else if (from.IsInteger32()) {
    info()->MarkAsDeferredCalling();
    if (to.IsTagged()) {
      if (!instr->CheckFlag(HValue::kCanOverflow)) {
        LOperand* value = UseRegisterAtStart(val);
        return DefineAsRegister(new(zone()) LSmiTag(value));
      } else if (val->CheckFlag(HInstruction::kUint32)) {
        LOperand* value = UseRegisterAtStart(val);
        LOperand* temp1 = TempRegister();
        LOperand* temp2 = TempRegister();
        LNumberTagU* result = new(zone()) LNumberTagU(value, temp1, temp2);
        return AssignPointerMap(DefineAsRegister(result));
      } else {
        LOperand* value = UseRegisterAtStart(val);
        LOperand* temp1 = TempRegister();
        LOperand* temp2 = TempRegister();
        LNumberTagI* result = new(zone()) LNumberTagI(value, temp1, temp2);
        return AssignPointerMap(DefineAsRegister(result));
      }
    } else if (to.IsSmi()) {
      LOperand* value = UseRegister(val);
      LInstruction* result = DefineAsRegister(new(zone()) LSmiTag(value));
      if (instr->CheckFlag(HValue::kCanOverflow)) {
        result = AssignEnvironment(result);
      }
      return result;
    } else {
      DCHECK(to.IsDouble());
      if (val->CheckFlag(HInstruction::kUint32)) {
        return DefineAsRegister(new(zone()) LUint32ToDouble(UseRegister(val)));
      } else {
        return DefineAsRegister(new(zone()) LInteger32ToDouble(Use(val)));
      }
    }
  }
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckHeapObject(HCheckHeapObject* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LInstruction* result = new(zone()) LCheckNonSmi(value);
  if (!instr->value()->type().IsHeapObject()) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoCheckSmi(HCheckSmi* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckSmi(value));
}


LInstruction* LChunkBuilder::DoCheckArrayBufferNotNeutered(
    HCheckArrayBufferNotNeutered* instr) {
  LOperand* view = UseRegisterAtStart(instr->value());
  LCheckArrayBufferNotNeutered* result =
      new (zone()) LCheckArrayBufferNotNeutered(view);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckInstanceType(HCheckInstanceType* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LInstruction* result = new(zone()) LCheckInstanceType(value);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckValue(HCheckValue* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckValue(value));
}


LInstruction* LChunkBuilder::DoCheckMaps(HCheckMaps* instr) {
  if (instr->IsStabilityCheck()) return new(zone()) LCheckMaps;
  LOperand* value = UseRegisterAtStart(instr->value());
  LInstruction* result = AssignEnvironment(new(zone()) LCheckMaps(value));
  if (instr->HasMigrationTarget()) {
    info()->MarkAsDeferredCalling();
    result = AssignPointerMap(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoClampToUint8(HClampToUint8* instr) {
  HValue* value = instr->value();
  Representation input_rep = value->representation();
  LOperand* reg = UseRegister(value);
  if (input_rep.IsDouble()) {
    // Revisit this decision, here and 8 lines below.
    return DefineAsRegister(new(zone()) LClampDToUint8(reg,
        TempDoubleRegister()));
  } else if (input_rep.IsInteger32()) {
    return DefineAsRegister(new(zone()) LClampIToUint8(reg));
  } else {
    DCHECK(input_rep.IsSmiOrTagged());
    LClampTToUint8* result =
        new(zone()) LClampTToUint8(reg, TempDoubleRegister());
    return AssignEnvironment(DefineAsRegister(result));
  }
}


LInstruction* LChunkBuilder::DoDoubleBits(HDoubleBits* instr) {
  HValue* value = instr->value();
  DCHECK(value->representation().IsDouble());
  return DefineAsRegister(new(zone()) LDoubleBits(UseRegister(value)));
}


LInstruction* LChunkBuilder::DoConstructDouble(HConstructDouble* instr) {
  LOperand* lo = UseRegister(instr->lo());
  LOperand* hi = UseRegister(instr->hi());
  return DefineAsRegister(new(zone()) LConstructDouble(hi, lo));
}


LInstruction* LChunkBuilder::DoReturn(HReturn* instr) {
  LOperand* context = info()->IsStub()
      ? UseFixed(instr->context(), cp)
      : NULL;
  LOperand* parameter_count = UseRegisterOrConstant(instr->parameter_count());
  return new(zone()) LReturn(UseFixed(instr->value(), v0), context,
                             parameter_count);
}


LInstruction* LChunkBuilder::DoConstant(HConstant* instr) {
  Representation r = instr->representation();
  if (r.IsSmi()) {
    return DefineAsRegister(new(zone()) LConstantS);
  } else if (r.IsInteger32()) {
    return DefineAsRegister(new(zone()) LConstantI);
  } else if (r.IsDouble()) {
    return DefineAsRegister(new(zone()) LConstantD);
  } else if (r.IsExternal()) {
    return DefineAsRegister(new(zone()) LConstantE);
  } else if (r.IsTagged()) {
    return DefineAsRegister(new(zone()) LConstantT);
  } else {
    UNREACHABLE();
    return NULL;
  }
}


LInstruction* LChunkBuilder::DoLoadGlobalGeneric(HLoadGlobalGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* global_object =
      UseFixed(instr->global_object(), LoadDescriptor::ReceiverRegister());
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    vector = FixedTemp(LoadWithVectorDescriptor::VectorRegister());
  }
  LLoadGlobalGeneric* result =
      new(zone()) LLoadGlobalGeneric(context, global_object, vector);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoLoadContextSlot(HLoadContextSlot* instr) {
  LOperand* context = UseRegisterAtStart(instr->value());
  LInstruction* result =
      DefineAsRegister(new(zone()) LLoadContextSlot(context));
  if (instr->RequiresHoleCheck() && instr->DeoptimizesOnHole()) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoStoreContextSlot(HStoreContextSlot* instr) {
  LOperand* context;
  LOperand* value;
  if (instr->NeedsWriteBarrier()) {
    context = UseTempRegister(instr->context());
    value = UseTempRegister(instr->value());
  } else {
    context = UseRegister(instr->context());
    value = UseRegister(instr->value());
  }
  LInstruction* result = new(zone()) LStoreContextSlot(context, value);
  if (instr->RequiresHoleCheck() && instr->DeoptimizesOnHole()) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoLoadNamedField(HLoadNamedField* instr) {
  LOperand* obj = UseRegisterAtStart(instr->object());
  return DefineAsRegister(new(zone()) LLoadNamedField(obj));
}


LInstruction* LChunkBuilder::DoLoadNamedGeneric(HLoadNamedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object =
      UseFixed(instr->object(), LoadDescriptor::ReceiverRegister());
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    vector = FixedTemp(LoadWithVectorDescriptor::VectorRegister());
  }

  LInstruction* result =
      DefineFixed(new(zone()) LLoadNamedGeneric(context, object, vector), v0);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoLoadFunctionPrototype(
    HLoadFunctionPrototype* instr) {
  return AssignEnvironment(DefineAsRegister(
      new(zone()) LLoadFunctionPrototype(UseRegister(instr->function()))));
}


LInstruction* LChunkBuilder::DoLoadRoot(HLoadRoot* instr) {
  return DefineAsRegister(new(zone()) LLoadRoot);
}


LInstruction* LChunkBuilder::DoLoadKeyed(HLoadKeyed* instr) {
  DCHECK(instr->key()->representation().IsSmiOrInteger32());
  ElementsKind elements_kind = instr->elements_kind();
  LOperand* key = UseRegisterOrConstantAtStart(instr->key());
  LInstruction* result = NULL;

  if (!instr->is_fixed_typed_array()) {
    LOperand* obj = NULL;
    if (instr->representation().IsDouble()) {
      obj = UseRegister(instr->elements());
    } else {
      DCHECK(instr->representation().IsSmiOrTagged());
      obj = UseRegisterAtStart(instr->elements());
    }
    result = DefineAsRegister(new (zone()) LLoadKeyed(obj, key, nullptr));
  } else {
    DCHECK(
        (instr->representation().IsInteger32() &&
         !IsDoubleOrFloatElementsKind(elements_kind)) ||
        (instr->representation().IsDouble() &&
         IsDoubleOrFloatElementsKind(elements_kind)));
    LOperand* backing_store = UseRegister(instr->elements());
    LOperand* backing_store_owner = UseAny(instr->backing_store_owner());
    result = DefineAsRegister(
        new (zone()) LLoadKeyed(backing_store, key, backing_store_owner));
  }

  bool needs_environment;
  if (instr->is_fixed_typed_array()) {
    // see LCodeGen::DoLoadKeyedExternalArray
    needs_environment = elements_kind == UINT32_ELEMENTS &&
                        !instr->CheckFlag(HInstruction::kUint32);
  } else {
    // see LCodeGen::DoLoadKeyedFixedDoubleArray and
    // LCodeGen::DoLoadKeyedFixedArray
    needs_environment =
        instr->RequiresHoleCheck() ||
        (instr->hole_mode() == CONVERT_HOLE_TO_UNDEFINED && info()->IsStub());
  }

  if (needs_environment) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoLoadKeyedGeneric(HLoadKeyedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object =
      UseFixed(instr->object(), LoadDescriptor::ReceiverRegister());
  LOperand* key = UseFixed(instr->key(), LoadDescriptor::NameRegister());
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    vector = FixedTemp(LoadWithVectorDescriptor::VectorRegister());
  }

  LInstruction* result =
      DefineFixed(new(zone()) LLoadKeyedGeneric(context, object, key, vector),
                  v0);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoStoreKeyed(HStoreKeyed* instr) {
  if (!instr->is_fixed_typed_array()) {
    DCHECK(instr->elements()->representation().IsTagged());
    bool needs_write_barrier = instr->NeedsWriteBarrier();
    LOperand* object = NULL;
    LOperand* val = NULL;
    LOperand* key = NULL;

    if (instr->value()->representation().IsDouble()) {
      object = UseRegisterAtStart(instr->elements());
      key = UseRegisterOrConstantAtStart(instr->key());
      val = UseRegister(instr->value());
    } else {
      DCHECK(instr->value()->representation().IsSmiOrTagged());
      if (needs_write_barrier) {
        object = UseTempRegister(instr->elements());
        val = UseTempRegister(instr->value());
        key = UseTempRegister(instr->key());
      } else {
        object = UseRegisterAtStart(instr->elements());
        val = UseRegisterAtStart(instr->value());
        key = UseRegisterOrConstantAtStart(instr->key());
      }
    }

    return new (zone()) LStoreKeyed(object, key, val, nullptr);
  }

  DCHECK(
      (instr->value()->representation().IsInteger32() &&
       !IsDoubleOrFloatElementsKind(instr->elements_kind())) ||
      (instr->value()->representation().IsDouble() &&
       IsDoubleOrFloatElementsKind(instr->elements_kind())));
  DCHECK(instr->elements()->representation().IsExternal());
  LOperand* val = UseRegister(instr->value());
  LOperand* key = UseRegisterOrConstantAtStart(instr->key());
  LOperand* backing_store = UseRegister(instr->elements());
  LOperand* backing_store_owner = UseAny(instr->backing_store_owner());
  return new (zone()) LStoreKeyed(backing_store, key, val, backing_store_owner);
}


LInstruction* LChunkBuilder::DoStoreKeyedGeneric(HStoreKeyedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* obj =
      UseFixed(instr->object(), StoreDescriptor::ReceiverRegister());
  LOperand* key = UseFixed(instr->key(), StoreDescriptor::NameRegister());
  LOperand* val = UseFixed(instr->value(), StoreDescriptor::ValueRegister());

  DCHECK(instr->object()->representation().IsTagged());
  DCHECK(instr->key()->representation().IsTagged());
  DCHECK(instr->value()->representation().IsTagged());

  LOperand* slot = NULL;
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    slot = FixedTemp(VectorStoreICDescriptor::SlotRegister());
    vector = FixedTemp(VectorStoreICDescriptor::VectorRegister());
  }

  LStoreKeyedGeneric* result =
      new (zone()) LStoreKeyedGeneric(context, obj, key, val, slot, vector);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoTransitionElementsKind(
    HTransitionElementsKind* instr) {
  if (IsSimpleMapChangeTransition(instr->from_kind(), instr->to_kind())) {
    LOperand* object = UseRegister(instr->object());
    LOperand* new_map_reg = TempRegister();
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, NULL, new_map_reg);
    return result;
  } else {
    LOperand* object = UseFixed(instr->object(), a0);
    LOperand* context = UseFixed(instr->context(), cp);
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, context, NULL);
    return MarkAsCall(result, instr);
  }
}


LInstruction* LChunkBuilder::DoTrapAllocationMemento(
    HTrapAllocationMemento* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* temp = TempRegister();
  LTrapAllocationMemento* result =
      new(zone()) LTrapAllocationMemento(object, temp);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoMaybeGrowElements(HMaybeGrowElements* instr) {
  info()->MarkAsDeferredCalling();
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object = Use(instr->object());
  LOperand* elements = Use(instr->elements());
  LOperand* key = UseRegisterOrConstant(instr->key());
  LOperand* current_capacity = UseRegisterOrConstant(instr->current_capacity());

  LMaybeGrowElements* result = new (zone())
      LMaybeGrowElements(context, object, elements, key, current_capacity);
  DefineFixed(result, v0);
  return AssignPointerMap(AssignEnvironment(result));
}


LInstruction* LChunkBuilder::DoStoreNamedField(HStoreNamedField* instr) {
  bool is_in_object = instr->access().IsInobject();
  bool needs_write_barrier = instr->NeedsWriteBarrier();
  bool needs_write_barrier_for_map = instr->has_transition() &&
      instr->NeedsWriteBarrierForMap();

  LOperand* obj;
  if (needs_write_barrier) {
    obj = is_in_object
        ? UseRegister(instr->object())
        : UseTempRegister(instr->object());
  } else {
    obj = needs_write_barrier_for_map
        ? UseRegister(instr->object())
        : UseRegisterAtStart(instr->object());
  }

  LOperand* val;
  if (needs_write_barrier) {
    val = UseTempRegister(instr->value());
  } else if (instr->field_representation().IsDouble()) {
    val = UseRegisterAtStart(instr->value());
  } else {
    val = UseRegister(instr->value());
  }

  // We need a temporary register for write barrier of the map field.
  LOperand* temp = needs_write_barrier_for_map ? TempRegister() : NULL;

  return new(zone()) LStoreNamedField(obj, val, temp);
}


LInstruction* LChunkBuilder::DoStoreNamedGeneric(HStoreNamedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* obj =
      UseFixed(instr->object(), StoreDescriptor::ReceiverRegister());
  LOperand* val = UseFixed(instr->value(), StoreDescriptor::ValueRegister());
  LOperand* slot = NULL;
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    slot = FixedTemp(VectorStoreICDescriptor::SlotRegister());
    vector = FixedTemp(VectorStoreICDescriptor::VectorRegister());
  }

  LStoreNamedGeneric* result =
      new (zone()) LStoreNamedGeneric(context, obj, val, slot, vector);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoStringAdd(HStringAdd* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left = UseFixed(instr->left(), a1);
  LOperand* right = UseFixed(instr->right(), a0);
  return MarkAsCall(
      DefineFixed(new(zone()) LStringAdd(context, left, right), v0),
      instr);
}


LInstruction* LChunkBuilder::DoStringCharCodeAt(HStringCharCodeAt* instr) {
  LOperand* string = UseTempRegister(instr->string());
  LOperand* index = UseTempRegister(instr->index());
  LOperand* context = UseAny(instr->context());
  LStringCharCodeAt* result =
      new(zone()) LStringCharCodeAt(context, string, index);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoStringCharFromCode(HStringCharFromCode* instr) {
  LOperand* char_code = UseRegister(instr->value());
  LOperand* context = UseAny(instr->context());
  LStringCharFromCode* result =
      new(zone()) LStringCharFromCode(context, char_code);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoAllocate(HAllocate* instr) {
  info()->MarkAsDeferredCalling();
  LOperand* context = UseAny(instr->context());
  LOperand* size = UseRegisterOrConstant(instr->size());
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();
  LAllocate* result = new(zone()) LAllocate(context, size, temp1, temp2);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoOsrEntry(HOsrEntry* instr) {
  DCHECK(argument_count_ == 0);
  allocator_->MarkAsOsrEntry();
  current_block_->last_environment()->set_ast_id(instr->ast_id());
  return AssignEnvironment(new(zone()) LOsrEntry);
}


LInstruction* LChunkBuilder::DoParameter(HParameter* instr) {
  LParameter* result = new(zone()) LParameter;
  if (instr->kind() == HParameter::STACK_PARAMETER) {
    int spill_index = chunk()->GetParameterStackSlot(instr->index());
    return DefineAsSpilled(result, spill_index);
  } else {
    DCHECK(info()->IsStub());
    CallInterfaceDescriptor descriptor = graph()->descriptor();
    int index = static_cast<int>(instr->index());
    Register reg = descriptor.GetRegisterParameter(index);
    return DefineFixed(result, reg);
  }
}


LInstruction* LChunkBuilder::DoUnknownOSRValue(HUnknownOSRValue* instr) {
  // Use an index that corresponds to the location in the unoptimized frame,
  // which the optimized frame will subsume.
  int env_index = instr->index();
  int spill_index = 0;
  if (instr->environment()->is_parameter_index(env_index)) {
    spill_index = chunk()->GetParameterStackSlot(env_index);
  } else {
    spill_index = env_index - instr->environment()->first_local_index();
    if (spill_index > LUnallocated::kMaxFixedSlotIndex) {
      Retry(kTooManySpillSlotsNeededForOSR);
      spill_index = 0;
    }
    spill_index += StandardFrameConstants::kFixedSlotCount;
  }
  return DefineAsSpilled(new(zone()) LUnknownOSRValue, spill_index);
}


LInstruction* LChunkBuilder::DoArgumentsObject(HArgumentsObject* instr) {
  // There are no real uses of the arguments object.
  // arguments.length and element access are supported directly on
  // stack arguments, and any real arguments object use causes a bailout.
  // So this value is never used.
  return NULL;
}


LInstruction* LChunkBuilder::DoCapturedObject(HCapturedObject* instr) {
  instr->ReplayEnvironment(current_block_->last_environment());

  // There are no real uses of a captured object.
  return NULL;
}


LInstruction* LChunkBuilder::DoAccessArgumentsAt(HAccessArgumentsAt* instr) {
  info()->MarkAsRequiresFrame();
  LOperand* args = UseRegister(instr->arguments());
  LOperand* length = UseRegisterOrConstantAtStart(instr->length());
  LOperand* index = UseRegisterOrConstantAtStart(instr->index());
  return DefineAsRegister(new(zone()) LAccessArgumentsAt(args, length, index));
}


LInstruction* LChunkBuilder::DoToFastProperties(HToFastProperties* instr) {
  LOperand* object = UseFixed(instr->value(), a0);
  LToFastProperties* result = new(zone()) LToFastProperties(object);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoTypeof(HTypeof* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* value = UseFixed(instr->value(), a3);
  LTypeof* result = new (zone()) LTypeof(context, value);
  return MarkAsCall(DefineFixed(result, v0), instr);
}


LInstruction* LChunkBuilder::DoTypeofIsAndBranch(HTypeofIsAndBranch* instr) {
  return new(zone()) LTypeofIsAndBranch(UseTempRegister(instr->value()));
}


LInstruction* LChunkBuilder::DoSimulate(HSimulate* instr) {
  instr->ReplayEnvironment(current_block_->last_environment());
  return NULL;
}


LInstruction* LChunkBuilder::DoStackCheck(HStackCheck* instr) {
  if (instr->is_function_entry()) {
    LOperand* context = UseFixed(instr->context(), cp);
    return MarkAsCall(new(zone()) LStackCheck(context), instr);
  } else {
    DCHECK(instr->is_backwards_branch());
    LOperand* context = UseAny(instr->context());
    return AssignEnvironment(
        AssignPointerMap(new(zone()) LStackCheck(context)));
  }
}


LInstruction* LChunkBuilder::DoEnterInlined(HEnterInlined* instr) {
  HEnvironment* outer = current_block_->last_environment();
  outer->set_ast_id(instr->ReturnId());
  HConstant* undefined = graph()->GetConstantUndefined();
  HEnvironment* inner = outer->CopyForInlining(instr->closure(),
                                               instr->arguments_count(),
                                               instr->function(),
                                               undefined,
                                               instr->inlining_kind());
  // Only replay binding of arguments object if it wasn't removed from graph.
  if (instr->arguments_var() != NULL && instr->arguments_object()->IsLinked()) {
    inner->Bind(instr->arguments_var(), instr->arguments_object());
  }
  inner->BindContext(instr->closure_context());
  inner->set_entry(instr);
  current_block_->UpdateEnvironment(inner);
  chunk_->AddInlinedFunction(instr->shared());
  return NULL;
}


LInstruction* LChunkBuilder::DoLeaveInlined(HLeaveInlined* instr) {
  LInstruction* pop = NULL;

  HEnvironment* env = current_block_->last_environment();

  if (env->entry()->arguments_pushed()) {
    int argument_count = env->arguments_environment()->parameter_count();
    pop = new(zone()) LDrop(argument_count);
    DCHECK(instr->argument_delta() == -argument_count);
  }

  HEnvironment* outer = current_block_->last_environment()->
      DiscardInlined(false);
  current_block_->UpdateEnvironment(outer);

  return pop;
}


LInstruction* LChunkBuilder::DoForInPrepareMap(HForInPrepareMap* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object = UseFixed(instr->enumerable(), a0);
  LForInPrepareMap* result = new(zone()) LForInPrepareMap(context, object);
  return MarkAsCall(DefineFixed(result, v0), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoForInCacheArray(HForInCacheArray* instr) {
  LOperand* map = UseRegister(instr->map());
  return AssignEnvironment(DefineAsRegister(new(zone()) LForInCacheArray(map)));
}


LInstruction* LChunkBuilder::DoCheckMapValue(HCheckMapValue* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* map = UseRegisterAtStart(instr->map());
  return AssignEnvironment(new(zone()) LCheckMapValue(value, map));
}


LInstruction* LChunkBuilder::DoLoadFieldByIndex(HLoadFieldByIndex* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* index = UseTempRegister(instr->index());
  LLoadFieldByIndex* load = new(zone()) LLoadFieldByIndex(object, index);
  LInstruction* result = DefineSameAsFirst(load);
  return AssignPointerMap(result);
}



LInstruction* LChunkBuilder::DoStoreFrameContext(HStoreFrameContext* instr) {
  LOperand* context = UseRegisterAtStart(instr->context());
  return new(zone()) LStoreFrameContext(context);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS
