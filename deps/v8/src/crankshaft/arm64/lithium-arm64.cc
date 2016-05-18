// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/arm64/lithium-arm64.h"

#include <sstream>

#include "src/crankshaft/arm64/lithium-codegen-arm64.h"
#include "src/crankshaft/hydrogen-osr.h"
#include "src/crankshaft/lithium-inl.h"

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


void LLabel::PrintDataTo(StringStream* stream) {
  LGap::PrintDataTo(stream);
  LLabel* rep = replacement();
  if (rep != NULL) {
    stream->Add(" Dead block replaced with B%d", rep->block_id());
  }
}


void LAccessArgumentsAt::PrintDataTo(StringStream* stream) {
  arguments()->PrintTo(stream);
  stream->Add(" length ");
  length()->PrintTo(stream);
  stream->Add(" index ");
  index()->PrintTo(stream);
}


void LBranch::PrintDataTo(StringStream* stream) {
  stream->Add("B%d | B%d on ", true_block_id(), false_block_id());
  value()->PrintTo(stream);
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


void LCallNewArray::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  constructor()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
  ElementsKind kind = hydrogen()->elements_kind();
  stream->Add(" (%s) ", ElementsKindToString(kind));
}


void LClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if class_of_test(");
  value()->PrintTo(stream);
  stream->Add(", \"%o\") then B%d else B%d",
              *hydrogen()->class_name(),
              true_block_id(),
              false_block_id());
}


void LCompareNumericAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  left()->PrintTo(stream);
  stream->Add(" %s ", Token::String(op()));
  right()->PrintTo(stream);
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LHasCachedArrayIndexAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_cached_array_index(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


bool LGoto::HasInterestingComment(LCodeGen* gen) const {
  return !gen->IsNextEmittedBlock(block_id());
}


void LGoto::PrintDataTo(StringStream* stream) {
  stream->Add("B%d", block_id());
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


void LInvokeFunction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  function()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


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


void LHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_instance_type(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
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


void LTypeofIsAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if typeof ");
  value()->PrintTo(stream);
  stream->Add(" == \"%s\" then B%d else B%d",
              hydrogen()->type_literal()->ToCString().get(),
              true_block_id(), false_block_id());
}


void LIsUndetectableAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_undetectable(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


bool LGap::IsRedundant() const {
  for (int i = 0; i < 4; i++) {
    if ((parallel_moves_[i] != NULL) && !parallel_moves_[i]->IsRedundant()) {
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


void LLoadContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add("[%d]", slot_index());
}


void LStoreCodeEntry::PrintDataTo(StringStream* stream) {
  stream->Add(" = ");
  function()->PrintTo(stream);
  stream->Add(".code_entry = ");
  code_object()->PrintTo(stream);
}


void LStoreContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add("[%d] <- ", slot_index());
  value()->PrintTo(stream);
}


void LStoreKeyedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  stream->Add("] <- ");
  value()->PrintTo(stream);
}


void LStoreNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  std::ostringstream os;
  os << hydrogen()->access();
  stream->Add(os.str().c_str());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStoreNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(String::cast(*name())->ToCString().get());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStringCompareAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if string_compare(");
  left()->PrintTo(stream);
  right()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LTransitionElementsKind::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add("%p -> %p", *original_map(), *transitioned_map());
}


template<int T>
void LUnaryMathOperation<T>::PrintDataTo(StringStream* stream) {
  value()->PrintTo(stream);
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
    case Token::SHL: return "shl-t";
    case Token::SAR: return "sar-t";
    case Token::SHR: return "shr-t";
    default:
      UNREACHABLE();
      return NULL;
  }
}


LUnallocated* LChunkBuilder::ToUnallocated(Register reg) {
  return new (zone()) LUnallocated(LUnallocated::FIXED_REGISTER, reg.code());
}


LUnallocated* LChunkBuilder::ToUnallocated(DoubleRegister reg) {
  return new (zone())
      LUnallocated(LUnallocated::FIXED_DOUBLE_REGISTER, reg.code());
}


LOperand* LChunkBuilder::Use(HValue* value, LUnallocated* operand) {
  if (value->EmitAtUses()) {
    HInstruction* instr = HInstruction::cast(value);
    VisitInstruction(instr);
  }
  operand->set_virtual_register(value->id());
  return operand;
}


LOperand* LChunkBuilder::UseFixed(HValue* value, Register fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseFixedDouble(HValue* value,
                                        DoubleRegister fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseRegister(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LOperand* LChunkBuilder::UseRegisterAndClobber(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::WRITABLE_REGISTER));
}


LOperand* LChunkBuilder::UseRegisterAtStart(HValue* value) {
  return Use(value,
             new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER,
                                      LUnallocated::USED_AT_START));
}


LOperand* LChunkBuilder::UseRegisterOrConstant(HValue* value) {
  return value->IsConstant() ? UseConstant(value) : UseRegister(value);
}


LOperand* LChunkBuilder::UseRegisterOrConstantAtStart(HValue* value) {
  return value->IsConstant() ? UseConstant(value) : UseRegisterAtStart(value);
}


LConstantOperand* LChunkBuilder::UseConstant(HValue* value) {
  return chunk_->DefineConstantOperand(HConstant::cast(value));
}


LOperand* LChunkBuilder::UseAny(HValue* value) {
  return value->IsConstant()
      ? UseConstant(value)
      : Use(value, new(zone()) LUnallocated(LUnallocated::ANY));
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

int LPlatformChunk::GetNextSpillIndex() { return current_frame_slots_++; }

LOperand* LPlatformChunk::GetNextSpillSlot(RegisterKind kind) {
  int index = GetNextSpillIndex();
  if (kind == DOUBLE_REGISTERS) {
    return LDoubleStackSlot::Create(index, zone());
  } else {
    DCHECK(kind == GENERAL_REGISTERS);
    return LStackSlot::Create(index, zone());
  }
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


LPlatformChunk* LChunkBuilder::Build() {
  DCHECK(is_unused());
  chunk_ = new(zone()) LPlatformChunk(info_, graph_);
  LPhase phase("L_Building chunk", chunk_);
  status_ = BUILDING;

  // If compiling for OSR, reserve space for the unoptimized frame,
  // which will be subsumed into this frame.
  if (graph()->has_osr()) {
    // TODO(all): GetNextSpillIndex just increments a field. It has no other
    // side effects, so we should get rid of this loop.
    for (int i = graph()->osr()->UnoptimizedFrameSlots(); i > 0; i--) {
      chunk_->GetNextSpillIndex();
    }
  }

  const ZoneList<HBasicBlock*>* blocks = graph_->blocks();
  for (int i = 0; i < blocks->length(); i++) {
    DoBasicBlock(blocks->at(i));
    if (is_aborted()) return NULL;
  }
  status_ = DONE;
  return chunk_;
}


void LChunkBuilder::DoBasicBlock(HBasicBlock* block) {
  DCHECK(is_building());
  current_block_ = block;

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
      if ((pred->end()->FirstSuccessor()->block_id() > block->block_id()) ||
          (pred->end()->SecondSuccessor()->block_id() > block->block_id())) {
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

  // Translate hydrogen instructions to lithium ones for the current block.
  HInstruction* current = block->first();
  int start = chunk_->instructions()->length();
  while ((current != NULL) && !is_aborted()) {
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
  // register constraint. In this case, the register allocator won't see an
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


LInstruction* LChunkBuilder::AssignEnvironment(LInstruction* instr) {
  HEnvironment* hydrogen_env = current_block_->last_environment();
  int argument_index_accumulator = 0;
  ZoneList<HValue*> objects_to_materialize(0, zone());
  instr->set_environment(CreateEnvironment(hydrogen_env,
                                           &argument_index_accumulator,
                                           &objects_to_materialize));
  return instr;
}


LInstruction* LChunkBuilder::DoPrologue(HPrologue* instr) {
  return new (zone()) LPrologue();
}


LInstruction* LChunkBuilder::DoAbnormalExit(HAbnormalExit* instr) {
  // The control instruction marking the end of a block that completed
  // abruptly (e.g., threw an exception). There is nothing specific to do.
  return NULL;
}


LInstruction* LChunkBuilder::DoArithmeticD(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  DCHECK(instr->representation().IsDouble());
  DCHECK(instr->left()->representation().IsDouble());
  DCHECK(instr->right()->representation().IsDouble());

  if (op == Token::MOD) {
    LOperand* left = UseFixedDouble(instr->left(), d0);
    LOperand* right = UseFixedDouble(instr->right(), d1);
    LArithmeticD* result = new(zone()) LArithmeticD(Token::MOD, left, right);
    return MarkAsCall(DefineFixedDouble(result, d0), instr);
  } else {
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseRegisterAtStart(instr->right());
    LArithmeticD* result = new(zone()) LArithmeticD(op, left, right);
    return DefineAsRegister(result);
  }
}


LInstruction* LChunkBuilder::DoArithmeticT(Token::Value op,
                                           HBinaryOperation* instr) {
  DCHECK((op == Token::ADD) || (op == Token::SUB) || (op == Token::MUL) ||
         (op == Token::DIV) || (op == Token::MOD) || (op == Token::SHR) ||
         (op == Token::SHL) || (op == Token::SAR) || (op == Token::ROR) ||
         (op == Token::BIT_OR) || (op == Token::BIT_AND) ||
         (op == Token::BIT_XOR));
  HValue* left = instr->left();
  HValue* right = instr->right();

  // TODO(jbramley): Once we've implemented smi support for all arithmetic
  // operations, these assertions should check IsTagged().
  DCHECK(instr->representation().IsSmiOrTagged());
  DCHECK(left->representation().IsSmiOrTagged());
  DCHECK(right->representation().IsSmiOrTagged());

  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left_operand = UseFixed(left, x1);
  LOperand* right_operand = UseFixed(right, x0);
  LArithmeticT* result =
      new(zone()) LArithmeticT(op, context, left_operand, right_operand);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoBoundsCheckBaseIndexInformation(
    HBoundsCheckBaseIndexInformation* instr) {
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoAccessArgumentsAt(HAccessArgumentsAt* instr) {
  info()->MarkAsRequiresFrame();
  LOperand* args = NULL;
  LOperand* length = NULL;
  LOperand* index = NULL;

  if (instr->length()->IsConstant() && instr->index()->IsConstant()) {
    args = UseRegisterAtStart(instr->arguments());
    length = UseConstant(instr->length());
    index = UseConstant(instr->index());
  } else {
    args = UseRegister(instr->arguments());
    length = UseRegisterAtStart(instr->length());
    index = UseRegisterOrConstantAtStart(instr->index());
  }

  return DefineAsRegister(new(zone()) LAccessArgumentsAt(args, length, index));
}


LInstruction* LChunkBuilder::DoAdd(HAdd* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));

    LInstruction* shifted_operation = TryDoOpWithShiftedRightOperand(instr);
    if (shifted_operation != NULL) {
      return shifted_operation;
    }

    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right =
        UseRegisterOrConstantAtStart(instr->BetterRightOperand());
    LInstruction* result = instr->representation().IsSmi() ?
        DefineAsRegister(new(zone()) LAddS(left, right)) :
        DefineAsRegister(new(zone()) LAddI(left, right));
    if (instr->CheckFlag(HValue::kCanOverflow)) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsExternal()) {
    DCHECK(instr->IsConsistentExternalRepresentation());
    DCHECK(!instr->CheckFlag(HValue::kCanOverflow));
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseRegisterOrConstantAtStart(instr->right());
    return DefineAsRegister(new(zone()) LAddE(left, right));
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::ADD, instr);
  } else {
    DCHECK(instr->representation().IsTagged());
    return DoArithmeticT(Token::ADD, instr);
  }
}


LInstruction* LChunkBuilder::DoAllocate(HAllocate* instr) {
  info()->MarkAsDeferredCalling();
  LOperand* context = UseAny(instr->context());
  LOperand* size = UseRegisterOrConstant(instr->size());
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();
  LOperand* temp3 = instr->MustPrefillWithFiller() ? TempRegister() : NULL;
  LAllocate* result = new(zone()) LAllocate(context, size, temp1, temp2, temp3);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoApplyArguments(HApplyArguments* instr) {
  LOperand* function = UseFixed(instr->function(), x1);
  LOperand* receiver = UseFixed(instr->receiver(), x0);
  LOperand* length = UseFixed(instr->length(), x2);
  LOperand* elements = UseFixed(instr->elements(), x3);
  LApplyArguments* result = new(zone()) LApplyArguments(function,
                                                        receiver,
                                                        length,
                                                        elements);
  return MarkAsCall(DefineFixed(result, x0), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoArgumentsElements(HArgumentsElements* instr) {
  info()->MarkAsRequiresFrame();
  LOperand* temp = instr->from_inlined() ? NULL : TempRegister();
  return DefineAsRegister(new(zone()) LArgumentsElements(temp));
}


LInstruction* LChunkBuilder::DoArgumentsLength(HArgumentsLength* instr) {
  info()->MarkAsRequiresFrame();
  LOperand* value = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LArgumentsLength(value));
}


LInstruction* LChunkBuilder::DoArgumentsObject(HArgumentsObject* instr) {
  // There are no real uses of the arguments object.
  // arguments.length and element access are supported directly on
  // stack arguments, and any real arguments object use causes a bailout.
  // So this value is never used.
  return NULL;
}


LInstruction* LChunkBuilder::DoBitwise(HBitwise* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    DCHECK(instr->CheckFlag(HValue::kTruncatingToInt32));

    LInstruction* shifted_operation = TryDoOpWithShiftedRightOperand(instr);
    if (shifted_operation != NULL) {
      return shifted_operation;
    }

    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right =
        UseRegisterOrConstantAtStart(instr->BetterRightOperand());
    return instr->representation().IsSmi() ?
        DefineAsRegister(new(zone()) LBitS(left, right)) :
        DefineAsRegister(new(zone()) LBitI(left, right));
  } else {
    return DoArithmeticT(instr->op(), instr);
  }
}


LInstruction* LChunkBuilder::DoBlockEntry(HBlockEntry* instr) {
  // V8 expects a label to be generated for each basic block.
  // This is used in some places like LAllocator::IsBlockBoundary
  // in lithium-allocator.cc
  return new(zone()) LLabel(instr->block());
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


LInstruction* LChunkBuilder::DoBranch(HBranch* instr) {
  HValue* value = instr->value();
  Representation r = value->representation();
  HType type = value->type();

  if (r.IsInteger32() || r.IsSmi() || r.IsDouble()) {
    // These representations have simple checks that cannot deoptimize.
    return new(zone()) LBranch(UseRegister(value), NULL, NULL);
  } else {
    DCHECK(r.IsTagged());
    if (type.IsBoolean() || type.IsSmi() || type.IsJSArray() ||
        type.IsHeapNumber()) {
      // These types have simple checks that cannot deoptimize.
      return new(zone()) LBranch(UseRegister(value), NULL, NULL);
    }

    if (type.IsString()) {
      // This type cannot deoptimize, but needs a scratch register.
      return new(zone()) LBranch(UseRegister(value), TempRegister(), NULL);
    }

    ToBooleanStub::Types expected = instr->expected_input_types();
    bool needs_temps = expected.NeedsMap() || expected.IsEmpty();
    LOperand* temp1 = needs_temps ? TempRegister() : NULL;
    LOperand* temp2 = needs_temps ? TempRegister() : NULL;

    if (expected.IsGeneric() || expected.IsEmpty()) {
      // The generic case cannot deoptimize because it already supports every
      // possible input type.
      DCHECK(needs_temps);
      return new(zone()) LBranch(UseRegister(value), temp1, temp2);
    } else {
      return AssignEnvironment(
          new(zone()) LBranch(UseRegister(value), temp1, temp2));
    }
  }
}


LInstruction* LChunkBuilder::DoCallJSFunction(
    HCallJSFunction* instr) {
  LOperand* function = UseFixed(instr->function(), x1);

  LCallJSFunction* result = new(zone()) LCallJSFunction(function);

  return MarkAsCall(DefineFixed(result, x0), instr);
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

  LCallWithDescriptor* result = new(zone()) LCallWithDescriptor(descriptor,
                                                                ops,
                                                                zone());
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoCallFunction(HCallFunction* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* function = UseFixed(instr->function(), x1);
  LOperand* slot = NULL;
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    slot = FixedTemp(x3);
    vector = FixedTemp(x2);
  }

  LCallFunction* call =
      new (zone()) LCallFunction(context, function, slot, vector);
  return MarkAsCall(DefineFixed(call, x0), instr);
}


LInstruction* LChunkBuilder::DoCallNewArray(HCallNewArray* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  // The call to ArrayConstructCode will expect the constructor to be in x1.
  LOperand* constructor = UseFixed(instr->constructor(), x1);
  LCallNewArray* result = new(zone()) LCallNewArray(context, constructor);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoCallRuntime(HCallRuntime* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  return MarkAsCall(DefineFixed(new(zone()) LCallRuntime(context), x0), instr);
}


LInstruction* LChunkBuilder::DoCapturedObject(HCapturedObject* instr) {
  instr->ReplayEnvironment(current_block_->last_environment());

  // There are no real uses of a captured object.
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
      LOperand* temp = TempRegister();
      LInstruction* result =
          DefineAsRegister(new(zone()) LNumberUntagD(value, temp));
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
        LOperand* temp2 = instr->CanTruncateToInt32()
            ? NULL : TempDoubleRegister();
        LInstruction* result =
            DefineAsRegister(new(zone()) LTaggedToI(value, temp1, temp2));
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
      LNumberTagD* result = new(zone()) LNumberTagD(value, temp1, temp2);
      return AssignPointerMap(DefineAsRegister(result));
    } else {
      DCHECK(to.IsSmi() || to.IsInteger32());
      if (instr->CanTruncateToInt32()) {
        LOperand* value = UseRegister(val);
        return DefineAsRegister(new(zone()) LTruncateDoubleToIntOrSmi(value));
      } else {
        LOperand* value = UseRegister(val);
        LDoubleToIntOrSmi* result = new(zone()) LDoubleToIntOrSmi(value);
        return AssignEnvironment(DefineAsRegister(result));
      }
    }
  } else if (from.IsInteger32()) {
    info()->MarkAsDeferredCalling();
    if (to.IsTagged()) {
      if (val->CheckFlag(HInstruction::kUint32)) {
        LOperand* value = UseRegister(val);
        LNumberTagU* result =
            new(zone()) LNumberTagU(value, TempRegister(), TempRegister());
        return AssignPointerMap(DefineAsRegister(result));
      } else {
        STATIC_ASSERT((kMinInt == Smi::kMinValue) &&
                      (kMaxInt == Smi::kMaxValue));
        LOperand* value = UseRegisterAtStart(val);
        return DefineAsRegister(new(zone()) LSmiTag(value));
      }
    } else if (to.IsSmi()) {
      LOperand* value = UseRegisterAtStart(val);
      LInstruction* result = DefineAsRegister(new(zone()) LSmiTag(value));
      if (instr->CheckFlag(HValue::kCanOverflow)) {
        result = AssignEnvironment(result);
      }
      return result;
    } else {
      DCHECK(to.IsDouble());
      if (val->CheckFlag(HInstruction::kUint32)) {
        return DefineAsRegister(
            new(zone()) LUint32ToDouble(UseRegisterAtStart(val)));
      } else {
        return DefineAsRegister(
            new(zone()) LInteger32ToDouble(UseRegisterAtStart(val)));
      }
    }
  }
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckValue(HCheckValue* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckValue(value));
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
  LOperand* temp = TempRegister();
  LInstruction* result = new(zone()) LCheckInstanceType(value, temp);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckMaps(HCheckMaps* instr) {
  if (instr->IsStabilityCheck()) return new(zone()) LCheckMaps;
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  LInstruction* result = AssignEnvironment(new(zone()) LCheckMaps(value, temp));
  if (instr->HasMigrationTarget()) {
    info()->MarkAsDeferredCalling();
    result = AssignPointerMap(result);
  }
  return result;
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


LInstruction* LChunkBuilder::DoClampToUint8(HClampToUint8* instr) {
  HValue* value = instr->value();
  Representation input_rep = value->representation();
  LOperand* reg = UseRegister(value);
  if (input_rep.IsDouble()) {
    return DefineAsRegister(new(zone()) LClampDToUint8(reg));
  } else if (input_rep.IsInteger32()) {
    return DefineAsRegister(new(zone()) LClampIToUint8(reg));
  } else {
    DCHECK(input_rep.IsSmiOrTagged());
    return AssignEnvironment(
        DefineAsRegister(new(zone()) LClampTToUint8(reg,
                                                    TempDoubleRegister())));
  }
}


LInstruction* LChunkBuilder::DoClassOfTestAndBranch(
    HClassOfTestAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LClassOfTestAndBranch(value,
                                           TempRegister(),
                                           TempRegister());
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
    if (instr->left()->IsConstant() && instr->right()->IsConstant()) {
      LOperand* left = UseConstant(instr->left());
      LOperand* right = UseConstant(instr->right());
      return new(zone()) LCompareNumericAndBranch(left, right);
    }
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseRegisterAtStart(instr->right());
    return new(zone()) LCompareNumericAndBranch(left, right);
  }
}


LInstruction* LChunkBuilder::DoCompareGeneric(HCompareGeneric* instr) {
  DCHECK(instr->left()->representation().IsTagged());
  DCHECK(instr->right()->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left = UseFixed(instr->left(), x1);
  LOperand* right = UseFixed(instr->right(), x0);
  LCmpT* result = new(zone()) LCmpT(context, left, right);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoCompareHoleAndBranch(
    HCompareHoleAndBranch* instr) {
  LOperand* value = UseRegister(instr->value());
  if (instr->representation().IsTagged()) {
    return new(zone()) LCmpHoleAndBranchT(value);
  } else {
    LOperand* temp = TempRegister();
    return new(zone()) LCmpHoleAndBranchD(value, temp);
  }
}


LInstruction* LChunkBuilder::DoCompareObjectEqAndBranch(
    HCompareObjectEqAndBranch* instr) {
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterAtStart(instr->right());
  return new(zone()) LCmpObjectEqAndBranch(left, right);
}


LInstruction* LChunkBuilder::DoCompareMap(HCompareMap* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  return new(zone()) LCmpMapAndBranch(value, temp);
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


LInstruction* LChunkBuilder::DoContext(HContext* instr) {
  if (instr->HasNoUses()) return NULL;

  if (info()->IsStub()) {
    return DefineFixed(new(zone()) LContext, cp);
  }

  return DefineAsRegister(new(zone()) LContext);
}


LInstruction* LChunkBuilder::DoDebugBreak(HDebugBreak* instr) {
  return new(zone()) LDebugBreak();
}


LInstruction* LChunkBuilder::DoDeclareGlobals(HDeclareGlobals* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  return MarkAsCall(new(zone()) LDeclareGlobals(context), instr);
}


LInstruction* LChunkBuilder::DoDeoptimize(HDeoptimize* instr) {
  return AssignEnvironment(new(zone()) LDeoptimize);
}


LInstruction* LChunkBuilder::DoDivByPowerOf2I(HDiv* instr) {
  DCHECK(instr->representation().IsInteger32());
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
  LOperand* temp = instr->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)
      ? NULL : TempRegister();
  LInstruction* result = DefineAsRegister(new(zone()) LDivByConstI(
          dividend, divisor, temp));
  if (divisor == 0 ||
      (instr->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) ||
      !instr->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoDivI(HBinaryOperation* instr) {
  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  LOperand* divisor = UseRegister(instr->right());
  LOperand* temp = instr->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)
      ? NULL : TempRegister();
  LInstruction* result =
      DefineAsRegister(new(zone()) LDivI(dividend, divisor, temp));
  if (!instr->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
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


LInstruction* LChunkBuilder::DoDummyUse(HDummyUse* instr) {
  return DefineAsRegister(new(zone()) LDummyUse(UseAny(instr->value())));
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
  if ((instr->arguments_var() != NULL) &&
      instr->arguments_object()->IsLinked()) {
    inner->Bind(instr->arguments_var(), instr->arguments_object());
  }
  inner->BindContext(instr->closure_context());
  inner->set_entry(instr);
  current_block_->UpdateEnvironment(inner);
  chunk_->AddInlinedFunction(instr->shared());
  return NULL;
}


LInstruction* LChunkBuilder::DoEnvironmentMarker(HEnvironmentMarker* instr) {
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoForceRepresentation(
    HForceRepresentation* instr) {
  // All HForceRepresentation instructions should be eliminated in the
  // representation change phase of Hydrogen.
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoGetCachedArrayIndex(
    HGetCachedArrayIndex* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LGetCachedArrayIndex(value));
}


LInstruction* LChunkBuilder::DoGoto(HGoto* instr) {
  return new(zone()) LGoto(instr->FirstSuccessor());
}


LInstruction* LChunkBuilder::DoHasCachedArrayIndexAndBranch(
    HHasCachedArrayIndexAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  return new(zone()) LHasCachedArrayIndexAndBranch(
      UseRegisterAtStart(instr->value()), TempRegister());
}


LInstruction* LChunkBuilder::DoHasInstanceTypeAndBranch(
    HHasInstanceTypeAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LHasInstanceTypeAndBranch(value, TempRegister());
}


LInstruction* LChunkBuilder::DoInnerAllocatedObject(
    HInnerAllocatedObject* instr) {
  LOperand* base_object = UseRegisterAtStart(instr->base_object());
  LOperand* offset = UseRegisterOrConstantAtStart(instr->offset());
  return DefineAsRegister(
      new(zone()) LInnerAllocatedObject(base_object, offset));
}


LInstruction* LChunkBuilder::DoInstanceOf(HInstanceOf* instr) {
  LOperand* left =
      UseFixed(instr->left(), InstanceOfDescriptor::LeftRegister());
  LOperand* right =
      UseFixed(instr->right(), InstanceOfDescriptor::RightRegister());
  LOperand* context = UseFixed(instr->context(), cp);
  LInstanceOf* result = new (zone()) LInstanceOf(context, left, right);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoHasInPrototypeChainAndBranch(
    HHasInPrototypeChainAndBranch* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* prototype = UseRegister(instr->prototype());
  LOperand* scratch1 = TempRegister();
  LOperand* scratch2 = TempRegister();
  LHasInPrototypeChainAndBranch* result = new (zone())
      LHasInPrototypeChainAndBranch(object, prototype, scratch1, scratch2);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoInvokeFunction(HInvokeFunction* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  // The function is required (by MacroAssembler::InvokeFunction) to be in x1.
  LOperand* function = UseFixed(instr->function(), x1);
  LInvokeFunction* result = new(zone()) LInvokeFunction(context, function);
  return MarkAsCall(DefineFixed(result, x0), instr, CANNOT_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoIsStringAndBranch(HIsStringAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  return new(zone()) LIsStringAndBranch(value, temp);
}


LInstruction* LChunkBuilder::DoIsSmiAndBranch(HIsSmiAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  return new(zone()) LIsSmiAndBranch(UseRegisterAtStart(instr->value()));
}


LInstruction* LChunkBuilder::DoIsUndetectableAndBranch(
    HIsUndetectableAndBranch* instr) {
  DCHECK(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LIsUndetectableAndBranch(value, TempRegister());
}


LInstruction* LChunkBuilder::DoLeaveInlined(HLeaveInlined* instr) {
  LInstruction* pop = NULL;
  HEnvironment* env = current_block_->last_environment();

  if (env->entry()->arguments_pushed()) {
    int argument_count = env->arguments_environment()->parameter_count();
    pop = new(zone()) LDrop(argument_count);
    DCHECK(instr->argument_delta() == -argument_count);
  }

  HEnvironment* outer =
      current_block_->last_environment()->DiscardInlined(false);
  current_block_->UpdateEnvironment(outer);

  return pop;
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


LInstruction* LChunkBuilder::DoLoadFunctionPrototype(
    HLoadFunctionPrototype* instr) {
  LOperand* function = UseRegister(instr->function());
  LOperand* temp = TempRegister();
  return AssignEnvironment(DefineAsRegister(
      new(zone()) LLoadFunctionPrototype(function, temp)));
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
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoLoadKeyed(HLoadKeyed* instr) {
  DCHECK(instr->key()->representation().IsSmiOrInteger32());
  ElementsKind elements_kind = instr->elements_kind();
  LOperand* elements = UseRegister(instr->elements());
  LOperand* key = UseRegisterOrConstant(instr->key());

  if (!instr->is_fixed_typed_array()) {
    if (instr->representation().IsDouble()) {
      LOperand* temp = (!instr->key()->IsConstant() ||
                        instr->RequiresHoleCheck())
             ? TempRegister()
             : NULL;
      LInstruction* result = DefineAsRegister(
          new (zone()) LLoadKeyedFixedDouble(elements, key, temp));
      if (instr->RequiresHoleCheck()) {
        result = AssignEnvironment(result);
      }
      return result;
    } else {
      DCHECK(instr->representation().IsSmiOrTagged() ||
             instr->representation().IsInteger32());
      LOperand* temp = instr->key()->IsConstant() ? NULL : TempRegister();
      LInstruction* result =
          DefineAsRegister(new (zone()) LLoadKeyedFixed(elements, key, temp));
      if (instr->RequiresHoleCheck() ||
          (instr->hole_mode() == CONVERT_HOLE_TO_UNDEFINED &&
           info()->IsStub())) {
        result = AssignEnvironment(result);
      }
      return result;
    }
  } else {
    DCHECK((instr->representation().IsInteger32() &&
            !IsDoubleOrFloatElementsKind(instr->elements_kind())) ||
           (instr->representation().IsDouble() &&
            IsDoubleOrFloatElementsKind(instr->elements_kind())));

    LOperand* temp = instr->key()->IsConstant() ? NULL : TempRegister();
    LOperand* backing_store_owner = UseAny(instr->backing_store_owner());
    LInstruction* result = DefineAsRegister(new (zone()) LLoadKeyedExternal(
        elements, key, backing_store_owner, temp));
    if (elements_kind == UINT32_ELEMENTS &&
        !instr->CheckFlag(HInstruction::kUint32)) {
      result = AssignEnvironment(result);
    }
    return result;
  }
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
                  x0);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoLoadNamedField(HLoadNamedField* instr) {
  LOperand* object = UseRegisterAtStart(instr->object());
  return DefineAsRegister(new(zone()) LLoadNamedField(object));
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
      DefineFixed(new(zone()) LLoadNamedGeneric(context, object, vector), x0);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoLoadRoot(HLoadRoot* instr) {
  return DefineAsRegister(new(zone()) LLoadRoot);
}


LInstruction* LChunkBuilder::DoFlooringDivByPowerOf2I(HMathFloorOfDiv* instr) {
  DCHECK(instr->representation().IsInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
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
  LOperand* dividend = UseRegister(instr->left());
  LOperand* divisor = UseRegister(instr->right());
  LOperand* remainder = TempRegister();
  LInstruction* result =
      DefineAsRegister(new(zone()) LFlooringDivI(dividend, divisor, remainder));
  return AssignEnvironment(result);
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


LInstruction* LChunkBuilder::DoMathMinMax(HMathMinMax* instr) {
  LOperand* left = NULL;
  LOperand* right = NULL;
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));
    left = UseRegisterAtStart(instr->BetterLeftOperand());
    right = UseRegisterOrConstantAtStart(instr->BetterRightOperand());
  } else {
    DCHECK(instr->representation().IsDouble());
    DCHECK(instr->left()->representation().IsDouble());
    DCHECK(instr->right()->representation().IsDouble());
    left = UseRegisterAtStart(instr->left());
    right = UseRegisterAtStart(instr->right());
  }
  return DefineAsRegister(new(zone()) LMathMinMax(left, right));
}


LInstruction* LChunkBuilder::DoModByPowerOf2I(HMod* instr) {
  DCHECK(instr->representation().IsInteger32());
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
  DCHECK(instr->representation().IsInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));
  LOperand* dividend = UseRegister(instr->left());
  int32_t divisor = instr->right()->GetInteger32Constant();
  LOperand* temp = TempRegister();
  LInstruction* result = DefineAsRegister(new(zone()) LModByConstI(
          dividend, divisor, temp));
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
  LInstruction* result = DefineAsRegister(new(zone()) LModI(dividend, divisor));
  if (instr->CheckFlag(HValue::kCanBeDivByZero) ||
      instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoMod(HMod* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    if (instr->RightIsPowerOf2()) {
      return DoModByPowerOf2I(instr);
    } else if (instr->right()->IsConstant()) {
      return DoModByConstI(instr);
    } else {
      return DoModI(instr);
    }
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

    bool can_overflow = instr->CheckFlag(HValue::kCanOverflow);
    bool bailout_on_minus_zero = instr->CheckFlag(HValue::kBailoutOnMinusZero);

    HValue* least_const = instr->BetterLeftOperand();
    HValue* most_const = instr->BetterRightOperand();

    // LMulConstI can handle a subset of constants:
    //  With support for overflow detection:
    //    -1, 0, 1, 2
    //    2^n, -(2^n)
    //  Without support for overflow detection:
    //    2^n + 1, -(2^n - 1)
    if (most_const->IsConstant()) {
      int32_t constant = HConstant::cast(most_const)->Integer32Value();
      bool small_constant = (constant >= -1) && (constant <= 2);
      bool end_range_constant = (constant <= -kMaxInt) || (constant == kMaxInt);
      int32_t constant_abs = Abs(constant);

      if (!end_range_constant &&
          (small_constant || (base::bits::IsPowerOfTwo32(constant_abs)) ||
           (!can_overflow && (base::bits::IsPowerOfTwo32(constant_abs + 1) ||
                              base::bits::IsPowerOfTwo32(constant_abs - 1))))) {
        LConstantOperand* right = UseConstant(most_const);
        bool need_register =
            base::bits::IsPowerOfTwo32(constant_abs) && !small_constant;
        LOperand* left = need_register ? UseRegister(least_const)
                                       : UseRegisterAtStart(least_const);
        LInstruction* result =
            DefineAsRegister(new(zone()) LMulConstIS(left, right));
        if ((bailout_on_minus_zero && constant <= 0) ||
            (can_overflow && constant != 1 &&
             base::bits::IsPowerOfTwo32(constant_abs))) {
          result = AssignEnvironment(result);
        }
        return result;
      }
    }

    // LMulI/S can handle all cases, but it requires that a register is
    // allocated for the second operand.
    LOperand* left = UseRegisterAtStart(least_const);
    LOperand* right = UseRegisterAtStart(most_const);
    LInstruction* result = instr->representation().IsSmi()
        ? DefineAsRegister(new(zone()) LMulS(left, right))
        : DefineAsRegister(new(zone()) LMulI(left, right));
    if ((bailout_on_minus_zero && least_const != most_const) || can_overflow) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::MUL, instr);
  } else {
    return DoArithmeticT(Token::MUL, instr);
  }
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
    int spill_index = chunk_->GetParameterStackSlot(instr->index());
    return DefineAsSpilled(result, spill_index);
  } else {
    DCHECK(info()->IsStub());
    CallInterfaceDescriptor descriptor = graph()->descriptor();
    int index = static_cast<int>(instr->index());
    Register reg = descriptor.GetRegisterParameter(index);
    return DefineFixed(result, reg);
  }
}


LInstruction* LChunkBuilder::DoPower(HPower* instr) {
  DCHECK(instr->representation().IsDouble());
  // We call a C function for double power. It can't trigger a GC.
  // We need to use fixed result register for the call.
  Representation exponent_type = instr->right()->representation();
  DCHECK(instr->left()->representation().IsDouble());
  LOperand* left = UseFixedDouble(instr->left(), d0);
  LOperand* right;
  if (exponent_type.IsInteger32()) {
    right = UseFixed(instr->right(), MathPowIntegerDescriptor::exponent());
  } else if (exponent_type.IsDouble()) {
    right = UseFixedDouble(instr->right(), d1);
  } else {
    right = UseFixed(instr->right(), MathPowTaggedDescriptor::exponent());
  }
  LPower* result = new(zone()) LPower(left, right);
  return MarkAsCall(DefineFixedDouble(result, d0),
                    instr,
                    CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoPushArguments(HPushArguments* instr) {
  int argc = instr->OperandCount();
  AddInstruction(new(zone()) LPreparePushArguments(argc), instr);

  LPushArguments* push_args = new(zone()) LPushArguments(zone());

  for (int i = 0; i < argc; ++i) {
    if (push_args->ShouldSplitPush()) {
      AddInstruction(push_args, instr);
      push_args = new(zone()) LPushArguments(zone());
    }
    push_args->AddArgument(UseRegister(instr->argument(i)));
  }

  return push_args;
}


LInstruction* LChunkBuilder::DoDoubleBits(HDoubleBits* instr) {
  HValue* value = instr->value();
  DCHECK(value->representation().IsDouble());
  return DefineAsRegister(new(zone()) LDoubleBits(UseRegister(value)));
}


LInstruction* LChunkBuilder::DoConstructDouble(HConstructDouble* instr) {
  LOperand* lo = UseRegisterAndClobber(instr->lo());
  LOperand* hi = UseRegister(instr->hi());
  return DefineAsRegister(new(zone()) LConstructDouble(hi, lo));
}


LInstruction* LChunkBuilder::DoReturn(HReturn* instr) {
  LOperand* context = info()->IsStub()
      ? UseFixed(instr->context(), cp)
      : NULL;
  LOperand* parameter_count = UseRegisterOrConstant(instr->parameter_count());
  return new(zone()) LReturn(UseFixed(instr->value(), x0), context,
                             parameter_count);
}


LInstruction* LChunkBuilder::DoSeqStringGetChar(HSeqStringGetChar* instr) {
  LOperand* string = UseRegisterAtStart(instr->string());
  LOperand* index = UseRegisterOrConstantAtStart(instr->index());
  LOperand* temp = TempRegister();
  LSeqStringGetChar* result =
      new(zone()) LSeqStringGetChar(string, index, temp);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoSeqStringSetChar(HSeqStringSetChar* instr) {
  LOperand* string = UseRegister(instr->string());
  LOperand* index = FLAG_debug_code
      ? UseRegister(instr->index())
      : UseRegisterOrConstant(instr->index());
  LOperand* value = UseRegister(instr->value());
  LOperand* context = FLAG_debug_code ? UseFixed(instr->context(), cp) : NULL;
  LOperand* temp = TempRegister();
  LSeqStringSetChar* result =
      new(zone()) LSeqStringSetChar(context, string, index, value, temp);
  return DefineAsRegister(result);
}


HBitwiseBinaryOperation* LChunkBuilder::CanTransformToShiftedOp(HValue* val,
                                                                HValue** left) {
  if (!val->representation().IsInteger32()) return NULL;
  if (!(val->IsBitwise() || val->IsAdd() || val->IsSub())) return NULL;

  HBinaryOperation* hinstr = HBinaryOperation::cast(val);
  HValue* hleft = hinstr->left();
  HValue* hright = hinstr->right();
  DCHECK(hleft->representation().Equals(hinstr->representation()));
  DCHECK(hright->representation().Equals(hinstr->representation()));

  if (hleft == hright) return NULL;

  if ((hright->IsConstant() &&
       LikelyFitsImmField(hinstr, HConstant::cast(hright)->Integer32Value())) ||
      (hinstr->IsCommutative() && hleft->IsConstant() &&
       LikelyFitsImmField(hinstr, HConstant::cast(hleft)->Integer32Value()))) {
    // The constant operand will likely fit in the immediate field. We are
    // better off with
    //     lsl x8, x9, #imm
    //     add x0, x8, #imm2
    // than with
    //     mov x16, #imm2
    //     add x0, x16, x9 LSL #imm
    return NULL;
  }

  HBitwiseBinaryOperation* shift = NULL;
  // TODO(aleram): We will miss situations where a shift operation is used by
  // different instructions both as a left and right operands.
  if (hright->IsBitwiseBinaryShift() &&
      HBitwiseBinaryOperation::cast(hright)->right()->IsConstant()) {
    shift = HBitwiseBinaryOperation::cast(hright);
    if (left != NULL) {
      *left = hleft;
    }
  } else if (hinstr->IsCommutative() &&
             hleft->IsBitwiseBinaryShift() &&
             HBitwiseBinaryOperation::cast(hleft)->right()->IsConstant()) {
    shift = HBitwiseBinaryOperation::cast(hleft);
    if (left != NULL) {
      *left = hright;
    }
  } else {
    return NULL;
  }

  if ((JSShiftAmountFromHConstant(shift->right()) == 0) && shift->IsShr()) {
    // Shifts right by zero can deoptimize.
    return NULL;
  }

  return shift;
}


bool LChunkBuilder::ShiftCanBeOptimizedAway(HBitwiseBinaryOperation* shift) {
  if (!shift->representation().IsInteger32()) {
    return false;
  }
  for (HUseIterator it(shift->uses()); !it.Done(); it.Advance()) {
    if (shift != CanTransformToShiftedOp(it.value())) {
      return false;
    }
  }
  return true;
}


LInstruction* LChunkBuilder::TryDoOpWithShiftedRightOperand(
    HBinaryOperation* instr) {
  HValue* left;
  HBitwiseBinaryOperation* shift = CanTransformToShiftedOp(instr, &left);

  if ((shift != NULL) && ShiftCanBeOptimizedAway(shift)) {
    return DoShiftedBinaryOp(instr, left, shift);
  }
  return NULL;
}


LInstruction* LChunkBuilder::DoShiftedBinaryOp(
    HBinaryOperation* hinstr, HValue* hleft, HBitwiseBinaryOperation* hshift) {
  DCHECK(hshift->IsBitwiseBinaryShift());
  DCHECK(!hshift->IsShr() || (JSShiftAmountFromHConstant(hshift->right()) > 0));

  LTemplateResultInstruction<1>* res;
  LOperand* left = UseRegisterAtStart(hleft);
  LOperand* right = UseRegisterAtStart(hshift->left());
  LOperand* shift_amount = UseConstant(hshift->right());
  Shift shift_op;
  switch (hshift->opcode()) {
    case HValue::kShl: shift_op = LSL; break;
    case HValue::kShr: shift_op = LSR; break;
    case HValue::kSar: shift_op = ASR; break;
    default: UNREACHABLE(); shift_op = NO_SHIFT;
  }

  if (hinstr->IsBitwise()) {
    res = new(zone()) LBitI(left, right, shift_op, shift_amount);
  } else if (hinstr->IsAdd()) {
    res = new(zone()) LAddI(left, right, shift_op, shift_amount);
  } else {
    DCHECK(hinstr->IsSub());
    res = new(zone()) LSubI(left, right, shift_op, shift_amount);
  }
  if (hinstr->CheckFlag(HValue::kCanOverflow)) {
    AssignEnvironment(res);
  }
  return DefineAsRegister(res);
}


LInstruction* LChunkBuilder::DoShift(Token::Value op,
                                     HBitwiseBinaryOperation* instr) {
  if (instr->representation().IsTagged()) {
    return DoArithmeticT(op, instr);
  }

  DCHECK(instr->representation().IsSmiOrInteger32());
  DCHECK(instr->left()->representation().Equals(instr->representation()));
  DCHECK(instr->right()->representation().Equals(instr->representation()));

  if (ShiftCanBeOptimizedAway(instr)) {
    return NULL;
  }

  LOperand* left = instr->representation().IsSmi()
      ? UseRegister(instr->left())
      : UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterOrConstantAtStart(instr->right());

  // The only shift that can deoptimize is `left >>> 0`, where left is negative.
  // In these cases, the result is a uint32 that is too large for an int32.
  bool right_can_be_zero = !instr->right()->IsConstant() ||
                           (JSShiftAmountFromHConstant(instr->right()) == 0);
  bool can_deopt = false;
  if ((op == Token::SHR) && right_can_be_zero) {
    can_deopt = !instr->CheckFlag(HInstruction::kUint32);
  }

  LInstruction* result;
  if (instr->representation().IsInteger32()) {
    result = DefineAsRegister(new (zone()) LShiftI(op, left, right, can_deopt));
  } else {
    DCHECK(instr->representation().IsSmi());
    result = DefineAsRegister(new (zone()) LShiftS(op, left, right, can_deopt));
  }

  return can_deopt ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoRor(HRor* instr) {
  return DoShift(Token::ROR, instr);
}


LInstruction* LChunkBuilder::DoSar(HSar* instr) {
  return DoShift(Token::SAR, instr);
}


LInstruction* LChunkBuilder::DoShl(HShl* instr) {
  return DoShift(Token::SHL, instr);
}


LInstruction* LChunkBuilder::DoShr(HShr* instr) {
  return DoShift(Token::SHR, instr);
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


LInstruction* LChunkBuilder::DoStoreCodeEntry(HStoreCodeEntry* instr) {
  LOperand* function = UseRegister(instr->function());
  LOperand* code_object = UseRegisterAtStart(instr->code_object());
  LOperand* temp = TempRegister();
  return new(zone()) LStoreCodeEntry(function, code_object, temp);
}


LInstruction* LChunkBuilder::DoStoreContextSlot(HStoreContextSlot* instr) {
  LOperand* temp = TempRegister();
  LOperand* context;
  LOperand* value;
  if (instr->NeedsWriteBarrier()) {
    // TODO(all): Replace these constraints when RecordWriteStub has been
    // rewritten.
    context = UseRegisterAndClobber(instr->context());
    value = UseRegisterAndClobber(instr->value());
  } else {
    context = UseRegister(instr->context());
    value = UseRegister(instr->value());
  }
  LInstruction* result = new(zone()) LStoreContextSlot(context, value, temp);
  if (instr->RequiresHoleCheck() && instr->DeoptimizesOnHole()) {
    result = AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoStoreKeyed(HStoreKeyed* instr) {
  LOperand* key = UseRegisterOrConstant(instr->key());
  LOperand* temp = NULL;
  LOperand* elements = NULL;
  LOperand* val = NULL;

  if (!instr->is_fixed_typed_array() &&
      instr->value()->representation().IsTagged() &&
      instr->NeedsWriteBarrier()) {
    // RecordWrite() will clobber all registers.
    elements = UseRegisterAndClobber(instr->elements());
    val = UseRegisterAndClobber(instr->value());
    temp = TempRegister();
  } else {
    elements = UseRegister(instr->elements());
    val = UseRegister(instr->value());
    temp = instr->key()->IsConstant() ? NULL : TempRegister();
  }

  if (instr->is_fixed_typed_array()) {
    DCHECK((instr->value()->representation().IsInteger32() &&
            !IsDoubleOrFloatElementsKind(instr->elements_kind())) ||
           (instr->value()->representation().IsDouble() &&
            IsDoubleOrFloatElementsKind(instr->elements_kind())));
    DCHECK(instr->elements()->representation().IsExternal());
    LOperand* backing_store_owner = UseAny(instr->backing_store_owner());
    return new (zone())
        LStoreKeyedExternal(elements, key, val, backing_store_owner, temp);

  } else if (instr->value()->representation().IsDouble()) {
    DCHECK(instr->elements()->representation().IsTagged());
    return new(zone()) LStoreKeyedFixedDouble(elements, key, val, temp);

  } else {
    DCHECK(instr->elements()->representation().IsTagged());
    DCHECK(instr->value()->representation().IsSmiOrTagged() ||
           instr->value()->representation().IsInteger32());
    return new(zone()) LStoreKeyedFixed(elements, key, val, temp);
  }
}


LInstruction* LChunkBuilder::DoStoreKeyedGeneric(HStoreKeyedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object =
      UseFixed(instr->object(), StoreDescriptor::ReceiverRegister());
  LOperand* key = UseFixed(instr->key(), StoreDescriptor::NameRegister());
  LOperand* value = UseFixed(instr->value(), StoreDescriptor::ValueRegister());

  DCHECK(instr->object()->representation().IsTagged());
  DCHECK(instr->key()->representation().IsTagged());
  DCHECK(instr->value()->representation().IsTagged());

  LOperand* slot = NULL;
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    slot = FixedTemp(VectorStoreICDescriptor::SlotRegister());
    vector = FixedTemp(VectorStoreICDescriptor::VectorRegister());
  }

  LStoreKeyedGeneric* result = new (zone())
      LStoreKeyedGeneric(context, object, key, value, slot, vector);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoStoreNamedField(HStoreNamedField* instr) {
  // TODO(jbramley): It might be beneficial to allow value to be a constant in
  // some cases. x64 makes use of this with FLAG_track_fields, for example.

  LOperand* object = UseRegister(instr->object());
  LOperand* value;
  LOperand* temp0 = NULL;
  LOperand* temp1 = NULL;

  if (instr->access().IsExternalMemory() ||
      (!FLAG_unbox_double_fields && instr->field_representation().IsDouble())) {
    value = UseRegister(instr->value());
  } else if (instr->NeedsWriteBarrier()) {
    value = UseRegisterAndClobber(instr->value());
    temp0 = TempRegister();
    temp1 = TempRegister();
  } else if (instr->NeedsWriteBarrierForMap()) {
    value = UseRegister(instr->value());
    temp0 = TempRegister();
    temp1 = TempRegister();
  } else {
    value = UseRegister(instr->value());
    temp0 = TempRegister();
  }

  return new(zone()) LStoreNamedField(object, value, temp0, temp1);
}


LInstruction* LChunkBuilder::DoStoreNamedGeneric(HStoreNamedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object =
      UseFixed(instr->object(), StoreDescriptor::ReceiverRegister());
  LOperand* value = UseFixed(instr->value(), StoreDescriptor::ValueRegister());

  LOperand* slot = NULL;
  LOperand* vector = NULL;
  if (instr->HasVectorAndSlot()) {
    slot = FixedTemp(VectorStoreICDescriptor::SlotRegister());
    vector = FixedTemp(VectorStoreICDescriptor::VectorRegister());
  }

  LStoreNamedGeneric* result =
      new (zone()) LStoreNamedGeneric(context, object, value, slot, vector);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoStringAdd(HStringAdd* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left = UseFixed(instr->left(), x1);
  LOperand* right = UseFixed(instr->right(), x0);

  LStringAdd* result = new(zone()) LStringAdd(context, left, right);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoStringCharCodeAt(HStringCharCodeAt* instr) {
  LOperand* string = UseRegisterAndClobber(instr->string());
  LOperand* index = UseRegisterAndClobber(instr->index());
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


LInstruction* LChunkBuilder::DoStringCompareAndBranch(
    HStringCompareAndBranch* instr) {
  DCHECK(instr->left()->representation().IsTagged());
  DCHECK(instr->right()->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* left = UseFixed(instr->left(), x1);
  LOperand* right = UseFixed(instr->right(), x0);
  LStringCompareAndBranch* result =
      new(zone()) LStringCompareAndBranch(context, left, right);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoSub(HSub* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    DCHECK(instr->left()->representation().Equals(instr->representation()));
    DCHECK(instr->right()->representation().Equals(instr->representation()));

    LInstruction* shifted_operation = TryDoOpWithShiftedRightOperand(instr);
    if (shifted_operation != NULL) {
      return shifted_operation;
    }

    LOperand *left;
    if (instr->left()->IsConstant() &&
        (HConstant::cast(instr->left())->Integer32Value() == 0)) {
      left = UseConstant(instr->left());
    } else {
      left = UseRegisterAtStart(instr->left());
    }
    LOperand* right = UseRegisterOrConstantAtStart(instr->right());
    LInstruction* result = instr->representation().IsSmi() ?
        DefineAsRegister(new(zone()) LSubS(left, right)) :
        DefineAsRegister(new(zone()) LSubI(left, right));
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


LInstruction* LChunkBuilder::DoThisFunction(HThisFunction* instr) {
  if (instr->HasNoUses()) {
    return NULL;
  } else {
    return DefineAsRegister(new(zone()) LThisFunction);
  }
}


LInstruction* LChunkBuilder::DoToFastProperties(HToFastProperties* instr) {
  LOperand* object = UseFixed(instr->value(), x0);
  LToFastProperties* result = new(zone()) LToFastProperties(object);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoTransitionElementsKind(
    HTransitionElementsKind* instr) {
  if (IsSimpleMapChangeTransition(instr->from_kind(), instr->to_kind())) {
    LOperand* object = UseRegister(instr->object());
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, NULL,
                                            TempRegister(), TempRegister());
    return result;
  } else {
    LOperand* object = UseFixed(instr->object(), x0);
    LOperand* context = UseFixed(instr->context(), cp);
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, context, NULL, NULL);
    return MarkAsCall(result, instr);
  }
}


LInstruction* LChunkBuilder::DoTrapAllocationMemento(
    HTrapAllocationMemento* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();
  LTrapAllocationMemento* result =
      new(zone()) LTrapAllocationMemento(object, temp1, temp2);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoMaybeGrowElements(HMaybeGrowElements* instr) {
  info()->MarkAsDeferredCalling();
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* object = UseRegister(instr->object());
  LOperand* elements = UseRegister(instr->elements());
  LOperand* key = UseRegisterOrConstant(instr->key());
  LOperand* current_capacity = UseRegisterOrConstant(instr->current_capacity());

  LMaybeGrowElements* result = new (zone())
      LMaybeGrowElements(context, object, elements, key, current_capacity);
  DefineFixed(result, x0);
  return AssignPointerMap(AssignEnvironment(result));
}


LInstruction* LChunkBuilder::DoTypeof(HTypeof* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  LOperand* value = UseFixed(instr->value(), x3);
  LTypeof* result = new (zone()) LTypeof(context, value);
  return MarkAsCall(DefineFixed(result, x0), instr);
}


LInstruction* LChunkBuilder::DoTypeofIsAndBranch(HTypeofIsAndBranch* instr) {
  // We only need temp registers in some cases, but we can't dereference the
  // instr->type_literal() handle to test that here.
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();

  return new(zone()) LTypeofIsAndBranch(
      UseRegister(instr->value()), temp1, temp2);
}


LInstruction* LChunkBuilder::DoUnaryMathOperation(HUnaryMathOperation* instr) {
  switch (instr->op()) {
    case kMathAbs: {
      Representation r = instr->representation();
      if (r.IsTagged()) {
        // The tagged case might need to allocate a HeapNumber for the result,
        // so it is handled by a separate LInstruction.
        LOperand* context = UseFixed(instr->context(), cp);
        LOperand* input = UseRegister(instr->value());
        LOperand* temp1 = TempRegister();
        LOperand* temp2 = TempRegister();
        LOperand* temp3 = TempRegister();
        LInstruction* result = DefineAsRegister(
            new(zone()) LMathAbsTagged(context, input, temp1, temp2, temp3));
        return AssignEnvironment(AssignPointerMap(result));
      } else {
        LOperand* input = UseRegisterAtStart(instr->value());
        LInstruction* result = DefineAsRegister(new(zone()) LMathAbs(input));
        if (!r.IsDouble()) result = AssignEnvironment(result);
        return result;
      }
    }
    case kMathExp: {
      DCHECK(instr->representation().IsDouble());
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseRegister(instr->value());
      LOperand* double_temp1 = TempDoubleRegister();
      LOperand* temp1 = TempRegister();
      LOperand* temp2 = TempRegister();
      LOperand* temp3 = TempRegister();
      LMathExp* result = new(zone()) LMathExp(input, double_temp1,
                                              temp1, temp2, temp3);
      return DefineAsRegister(result);
    }
    case kMathFloor: {
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseRegisterAtStart(instr->value());
      if (instr->representation().IsInteger32()) {
        LMathFloorI* result = new(zone()) LMathFloorI(input);
        return AssignEnvironment(AssignPointerMap(DefineAsRegister(result)));
      } else {
        DCHECK(instr->representation().IsDouble());
        LMathFloorD* result = new(zone()) LMathFloorD(input);
        return DefineAsRegister(result);
      }
    }
    case kMathLog: {
      DCHECK(instr->representation().IsDouble());
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseFixedDouble(instr->value(), d0);
      LMathLog* result = new(zone()) LMathLog(input);
      return MarkAsCall(DefineFixedDouble(result, d0), instr);
    }
    case kMathPowHalf: {
      DCHECK(instr->representation().IsDouble());
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseRegister(instr->value());
      return DefineAsRegister(new(zone()) LMathPowHalf(input));
    }
    case kMathRound: {
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseRegister(instr->value());
      if (instr->representation().IsInteger32()) {
        LOperand* temp = TempDoubleRegister();
        LMathRoundI* result = new(zone()) LMathRoundI(input, temp);
        return AssignEnvironment(DefineAsRegister(result));
      } else {
        DCHECK(instr->representation().IsDouble());
        LMathRoundD* result = new(zone()) LMathRoundD(input);
        return DefineAsRegister(result);
      }
    }
    case kMathFround: {
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseRegister(instr->value());
      LMathFround* result = new (zone()) LMathFround(input);
      return DefineAsRegister(result);
    }
    case kMathSqrt: {
      DCHECK(instr->representation().IsDouble());
      DCHECK(instr->value()->representation().IsDouble());
      LOperand* input = UseRegisterAtStart(instr->value());
      return DefineAsRegister(new(zone()) LMathSqrt(input));
    }
    case kMathClz32: {
      DCHECK(instr->representation().IsInteger32());
      DCHECK(instr->value()->representation().IsInteger32());
      LOperand* input = UseRegisterAtStart(instr->value());
      return DefineAsRegister(new(zone()) LMathClz32(input));
    }
    default:
      UNREACHABLE();
      return NULL;
  }
}


LInstruction* LChunkBuilder::DoUnknownOSRValue(HUnknownOSRValue* instr) {
  // Use an index that corresponds to the location in the unoptimized frame,
  // which the optimized frame will subsume.
  int env_index = instr->index();
  int spill_index = 0;
  if (instr->environment()->is_parameter_index(env_index)) {
    spill_index = chunk_->GetParameterStackSlot(env_index);
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


LInstruction* LChunkBuilder::DoUseConst(HUseConst* instr) {
  return NULL;
}


LInstruction* LChunkBuilder::DoForInPrepareMap(HForInPrepareMap* instr) {
  LOperand* context = UseFixed(instr->context(), cp);
  // Assign object to a fixed register different from those already used in
  // LForInPrepareMap.
  LOperand* object = UseFixed(instr->enumerable(), x0);
  LForInPrepareMap* result = new(zone()) LForInPrepareMap(context, object);
  return MarkAsCall(DefineFixed(result, x0), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoForInCacheArray(HForInCacheArray* instr) {
  LOperand* map = UseRegister(instr->map());
  return AssignEnvironment(DefineAsRegister(new(zone()) LForInCacheArray(map)));
}


LInstruction* LChunkBuilder::DoCheckMapValue(HCheckMapValue* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* map = UseRegister(instr->map());
  LOperand* temp = TempRegister();
  return AssignEnvironment(new(zone()) LCheckMapValue(value, map, temp));
}


LInstruction* LChunkBuilder::DoLoadFieldByIndex(HLoadFieldByIndex* instr) {
  LOperand* object = UseRegisterAtStart(instr->object());
  LOperand* index = UseRegisterAndClobber(instr->index());
  LLoadFieldByIndex* load = new(zone()) LLoadFieldByIndex(object, index);
  LInstruction* result = DefineSameAsFirst(load);
  return AssignPointerMap(result);
}


LInstruction* LChunkBuilder::DoWrapReceiver(HWrapReceiver* instr) {
  LOperand* receiver = UseRegister(instr->receiver());
  LOperand* function = UseRegister(instr->function());
  LWrapReceiver* result = new(zone()) LWrapReceiver(receiver, function);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoStoreFrameContext(HStoreFrameContext* instr) {
  LOperand* context = UseRegisterAtStart(instr->context());
  return new(zone()) LStoreFrameContext(context);
}


}  // namespace internal
}  // namespace v8
