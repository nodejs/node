// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if defined(V8_TARGET_ARCH_X64)

#include "lithium-allocator-inl.h"
#include "x64/lithium-x64.h"
#include "x64/lithium-codegen-x64.h"

namespace v8 {
namespace internal {

#define DEFINE_COMPILE(type)                            \
  void L##type::CompileToNative(LCodeGen* generator) {  \
    generator->Do##type(this);                          \
  }
LITHIUM_CONCRETE_INSTRUCTION_LIST(DEFINE_COMPILE)
#undef DEFINE_COMPILE

LOsrEntry::LOsrEntry() {
  for (int i = 0; i < Register::kNumAllocatableRegisters; ++i) {
    register_spills_[i] = NULL;
  }
  for (int i = 0; i < DoubleRegister::kNumAllocatableRegisters; ++i) {
    double_register_spills_[i] = NULL;
  }
}


void LOsrEntry::MarkSpilledRegister(int allocation_index,
                                    LOperand* spill_operand) {
  ASSERT(spill_operand->IsStackSlot());
  ASSERT(register_spills_[allocation_index] == NULL);
  register_spills_[allocation_index] = spill_operand;
}


void LOsrEntry::MarkSpilledDoubleRegister(int allocation_index,
                                          LOperand* spill_operand) {
  ASSERT(spill_operand->IsDoubleStackSlot());
  ASSERT(double_register_spills_[allocation_index] == NULL);
  double_register_spills_[allocation_index] = spill_operand;
}


#ifdef DEBUG
void LInstruction::VerifyCall() {
  // Call instructions can use only fixed registers as temporaries and
  // outputs because all registers are blocked by the calling convention.
  // Inputs operands must use a fixed register or use-at-start policy or
  // a non-register policy.
  ASSERT(Output() == NULL ||
         LUnallocated::cast(Output())->HasFixedPolicy() ||
         !LUnallocated::cast(Output())->HasRegisterPolicy());
  for (UseIterator it(this); !it.Done(); it.Advance()) {
    LUnallocated* operand = LUnallocated::cast(it.Current());
    ASSERT(operand->HasFixedPolicy() ||
           operand->IsUsedAtStart());
  }
  for (TempIterator it(this); !it.Done(); it.Advance()) {
    LUnallocated* operand = LUnallocated::cast(it.Current());
    ASSERT(operand->HasFixedPolicy() ||!operand->HasRegisterPolicy());
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


template<int R, int I, int T>
void LTemplateInstruction<R, I, T>::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  for (int i = 0; i < inputs_.length(); i++) {
    if (i > 0) stream->Add(" ");
    inputs_[i]->PrintTo(stream);
  }
}


template<int R, int I, int T>
void LTemplateInstruction<R, I, T>::PrintOutputOperandTo(StringStream* stream) {
  for (int i = 0; i < results_.length(); i++) {
    if (i > 0) stream->Add(" ");
    results_[i]->PrintTo(stream);
  }
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
    case Token::SHL: return "sal-t";
    case Token::SAR: return "sar-t";
    case Token::SHR: return "shr-t";
    default:
      UNREACHABLE();
      return NULL;
  }
}


void LGoto::PrintDataTo(StringStream* stream) {
  stream->Add("B%d", block_id());
}


void LBranch::PrintDataTo(StringStream* stream) {
  stream->Add("B%d | B%d on ", true_block_id(), false_block_id());
  InputAt(0)->PrintTo(stream);
}


void LCmpIDAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  InputAt(0)->PrintTo(stream);
  stream->Add(" %s ", Token::String(op()));
  InputAt(1)->PrintTo(stream);
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsNilAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  InputAt(0)->PrintTo(stream);
  stream->Add(kind() == kStrictEquality ? " === " : " == ");
  stream->Add(nil() == kNullValue ? "null" : "undefined");
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsObjectAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_object(");
  InputAt(0)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsStringAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_string(");
  InputAt(0)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsSmiAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_smi(");
  InputAt(0)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsUndetectableAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_undetectable(");
  InputAt(0)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LStringCompareAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if string_compare(");
  InputAt(0)->PrintTo(stream);
  InputAt(1)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_instance_type(");
  InputAt(0)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasCachedArrayIndexAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_cached_array_index(");
  InputAt(0)->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if class_of_test(");
  InputAt(0)->PrintTo(stream);
  stream->Add(", \"%o\") then B%d else B%d",
              *hydrogen()->class_name(),
              true_block_id(),
              false_block_id());
}


void LTypeofIsAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if typeof ");
  InputAt(0)->PrintTo(stream);
  stream->Add(" == \"%s\" then B%d else B%d",
              *hydrogen()->type_literal()->ToCString(),
              true_block_id(), false_block_id());
}


void LCallConstantFunction::PrintDataTo(StringStream* stream) {
  stream->Add("#%d / ", arity());
}


void LUnaryMathOperation::PrintDataTo(StringStream* stream) {
  stream->Add("/%s ", hydrogen()->OpName());
  InputAt(0)->PrintTo(stream);
}


void LLoadContextSlot::PrintDataTo(StringStream* stream) {
  InputAt(0)->PrintTo(stream);
  stream->Add("[%d]", slot_index());
}


void LStoreContextSlot::PrintDataTo(StringStream* stream) {
  InputAt(0)->PrintTo(stream);
  stream->Add("[%d] <- ", slot_index());
  InputAt(1)->PrintTo(stream);
}


void LInvokeFunction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  InputAt(0)->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


void LCallKeyed::PrintDataTo(StringStream* stream) {
  stream->Add("[rcx] #%d / ", arity());
}


void LCallNamed::PrintDataTo(StringStream* stream) {
  SmartArrayPointer<char> name_string = name()->ToCString();
  stream->Add("%s #%d / ", *name_string, arity());
}


void LCallGlobal::PrintDataTo(StringStream* stream) {
  SmartArrayPointer<char> name_string = name()->ToCString();
  stream->Add("%s #%d / ", *name_string, arity());
}


void LCallKnownGlobal::PrintDataTo(StringStream* stream) {
  stream->Add("#%d / ", arity());
}


void LCallNew::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  InputAt(0)->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


void LAccessArgumentsAt::PrintDataTo(StringStream* stream) {
  arguments()->PrintTo(stream);

  stream->Add(" length ");
  length()->PrintTo(stream);

  stream->Add(" index ");
  index()->PrintTo(stream);
}


int LChunk::GetNextSpillIndex(bool is_double) {
  return spill_slot_count_++;
}


LOperand* LChunk::GetNextSpillSlot(bool is_double) {
  // All stack slots are Double stack slots on x64.
  // Alternatively, at some point, start using half-size
  // stack slots for int32 values.
  int index = GetNextSpillIndex(is_double);
  if (is_double) {
    return LDoubleStackSlot::Create(index);
  } else {
    return LStackSlot::Create(index);
  }
}


void LChunk::MarkEmptyBlocks() {
  HPhase phase("L_Mark empty blocks", this);
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    int first = block->first_instruction_index();
    int last = block->last_instruction_index();
    LInstruction* first_instr = instructions()->at(first);
    LInstruction* last_instr = instructions()->at(last);

    LLabel* label = LLabel::cast(first_instr);
    if (last_instr->IsGoto()) {
      LGoto* goto_instr = LGoto::cast(last_instr);
      if (label->IsRedundant() &&
          !label->is_loop_header()) {
        bool can_eliminate = true;
        for (int i = first + 1; i < last && can_eliminate; ++i) {
          LInstruction* cur = instructions()->at(i);
          if (cur->IsGap()) {
            LGap* gap = LGap::cast(cur);
            if (!gap->IsRedundant()) {
              can_eliminate = false;
            }
          } else {
            can_eliminate = false;
          }
        }

        if (can_eliminate) {
          label->set_replacement(GetLabel(goto_instr->block_id()));
        }
      }
    }
  }
}


void LStoreNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStoreNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStoreKeyedFastElement::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  stream->Add("] <- ");
  value()->PrintTo(stream);
}


void LStoreKeyedFastDoubleElement::PrintDataTo(StringStream* stream) {
  elements()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  stream->Add("] <- ");
  value()->PrintTo(stream);
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


void LChunk::AddInstruction(LInstruction* instr, HBasicBlock* block) {
  LInstructionGap* gap = new(graph_->zone()) LInstructionGap(block);
  int index = -1;
  if (instr->IsControl()) {
    instructions_.Add(gap);
    index = instructions_.length();
    instructions_.Add(instr);
  } else {
    index = instructions_.length();
    instructions_.Add(instr);
    instructions_.Add(gap);
  }
  if (instr->HasPointerMap()) {
    pointer_maps_.Add(instr->pointer_map());
    instr->pointer_map()->set_lithium_position(index);
  }
}


LConstantOperand* LChunk::DefineConstantOperand(HConstant* constant) {
  return LConstantOperand::Create(constant->id());
}


int LChunk::GetParameterStackSlot(int index) const {
  // The receiver is at index 0, the first parameter at index 1, so we
  // shift all parameter indexes down by the number of parameters, and
  // make sure they end up negative so they are distinguishable from
  // spill slots.
  int result = index - info()->scope()->num_parameters() - 1;
  ASSERT(result < 0);
  return result;
}

// A parameter relative to ebp in the arguments stub.
int LChunk::ParameterAt(int index) {
  ASSERT(-1 <= index);  // -1 is the receiver.
  return (1 + info()->scope()->num_parameters() - index) *
      kPointerSize;
}


LGap* LChunk::GetGapAt(int index) const {
  return LGap::cast(instructions_[index]);
}


bool LChunk::IsGapAt(int index) const {
  return instructions_[index]->IsGap();
}


int LChunk::NearestGapPos(int index) const {
  while (!IsGapAt(index)) index--;
  return index;
}


void LChunk::AddGapMove(int index, LOperand* from, LOperand* to) {
  GetGapAt(index)->GetOrCreateParallelMove(LGap::START)->AddMove(from, to);
}


Handle<Object> LChunk::LookupLiteral(LConstantOperand* operand) const {
  return HConstant::cast(graph_->LookupValue(operand->index()))->handle();
}


Representation LChunk::LookupLiteralRepresentation(
    LConstantOperand* operand) const {
  return graph_->LookupValue(operand->index())->representation();
}


LChunk* LChunkBuilder::Build() {
  ASSERT(is_unused());
  chunk_ = new(zone()) LChunk(info(), graph());
  HPhase phase("L_Building chunk", chunk_);
  status_ = BUILDING;
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


void LChunkBuilder::Abort(const char* format, ...) {
  if (FLAG_trace_bailout) {
    SmartArrayPointer<char> name(
        info()->shared_info()->DebugName()->ToCString());
    PrintF("Aborting LChunk building in @\"%s\": ", *name);
    va_list arguments;
    va_start(arguments, format);
    OS::VPrint(format, arguments);
    va_end(arguments);
    PrintF("\n");
  }
  status_ = ABORTED;
}


LUnallocated* LChunkBuilder::ToUnallocated(Register reg) {
  return new(zone()) LUnallocated(LUnallocated::FIXED_REGISTER,
                                  Register::ToAllocationIndex(reg));
}


LUnallocated* LChunkBuilder::ToUnallocated(XMMRegister reg) {
  return new(zone()) LUnallocated(LUnallocated::FIXED_DOUBLE_REGISTER,
                                  XMMRegister::ToAllocationIndex(reg));
}


LOperand* LChunkBuilder::UseFixed(HValue* value, Register fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseFixedDouble(HValue* value, XMMRegister reg) {
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


template<int I, int T>
LInstruction* LChunkBuilder::Define(LTemplateInstruction<1, I, T>* instr,
                                    LUnallocated* result) {
  result->set_virtual_register(current_instruction_->id());
  instr->set_result(result);
  return instr;
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineAsRegister(
    LTemplateInstruction<1, I, T>* instr) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineAsSpilled(
    LTemplateInstruction<1, I, T>* instr,
    int index) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::FIXED_SLOT, index));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineSameAsFirst(
    LTemplateInstruction<1, I, T>* instr) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::SAME_AS_FIRST_INPUT));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineFixed(LTemplateInstruction<1, I, T>* instr,
                                         Register reg) {
  return Define(instr, ToUnallocated(reg));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineFixedDouble(
    LTemplateInstruction<1, I, T>* instr,
    XMMRegister reg) {
  return Define(instr, ToUnallocated(reg));
}


LInstruction* LChunkBuilder::AssignEnvironment(LInstruction* instr) {
  HEnvironment* hydrogen_env = current_block_->last_environment();
  int argument_index_accumulator = 0;
  instr->set_environment(CreateEnvironment(hydrogen_env,
                                           &argument_index_accumulator));
  return instr;
}


LInstruction* LChunkBuilder::SetInstructionPendingDeoptimizationEnvironment(
    LInstruction* instr, int ast_id) {
  ASSERT(instruction_pending_deoptimization_environment_ == NULL);
  ASSERT(pending_deoptimization_ast_id_ == AstNode::kNoNumber);
  instruction_pending_deoptimization_environment_ = instr;
  pending_deoptimization_ast_id_ = ast_id;
  return instr;
}


void LChunkBuilder::ClearInstructionPendingDeoptimizationEnvironment() {
  instruction_pending_deoptimization_environment_ = NULL;
  pending_deoptimization_ast_id_ = AstNode::kNoNumber;
}


LInstruction* LChunkBuilder::MarkAsCall(LInstruction* instr,
                                        HInstruction* hinstr,
                                        CanDeoptimize can_deoptimize) {
#ifdef DEBUG
  instr->VerifyCall();
#endif
  instr->MarkAsCall();
  instr = AssignPointerMap(instr);

  if (hinstr->HasObservableSideEffects()) {
    ASSERT(hinstr->next()->IsSimulate());
    HSimulate* sim = HSimulate::cast(hinstr->next());
    instr = SetInstructionPendingDeoptimizationEnvironment(
        instr, sim->ast_id());
  }

  // If instruction does not have side-effects lazy deoptimization
  // after the call will try to deoptimize to the point before the call.
  // Thus we still need to attach environment to this call even if
  // call sequence can not deoptimize eagerly.
  bool needs_environment =
      (can_deoptimize == CAN_DEOPTIMIZE_EAGERLY) ||
      !hinstr->HasObservableSideEffects();
  if (needs_environment && !instr->HasEnvironment()) {
    instr = AssignEnvironment(instr);
  }

  return instr;
}


LInstruction* LChunkBuilder::MarkAsSaveDoubles(LInstruction* instr) {
  instr->MarkAsSaveDoubles();
  return instr;
}


LInstruction* LChunkBuilder::AssignPointerMap(LInstruction* instr) {
  ASSERT(!instr->HasPointerMap());
  instr->set_pointer_map(new(zone()) LPointerMap(position_));
  return instr;
}


LUnallocated* LChunkBuilder::TempRegister() {
  LUnallocated* operand =
      new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER);
  operand->set_virtual_register(allocator_->GetVirtualRegister());
  if (!allocator_->AllocationOk()) Abort("Not enough virtual registers.");
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(Register reg) {
  LUnallocated* operand = ToUnallocated(reg);
  ASSERT(operand->HasFixedPolicy());
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(XMMRegister reg) {
  LUnallocated* operand = ToUnallocated(reg);
  ASSERT(operand->HasFixedPolicy());
  return operand;
}


LInstruction* LChunkBuilder::DoBlockEntry(HBlockEntry* instr) {
  return new(zone()) LLabel(instr->block());
}


LInstruction* LChunkBuilder::DoSoftDeoptimize(HSoftDeoptimize* instr) {
  return AssignEnvironment(new(zone()) LDeoptimize);
}


LInstruction* LChunkBuilder::DoDeoptimize(HDeoptimize* instr) {
  return AssignEnvironment(new(zone()) LDeoptimize);
}


LInstruction* LChunkBuilder::DoShift(Token::Value op,
                                     HBitwiseBinaryOperation* instr) {
  if (instr->representation().IsTagged()) {
    ASSERT(instr->left()->representation().IsTagged());
    ASSERT(instr->right()->representation().IsTagged());

    LOperand* left = UseFixed(instr->left(), rdx);
    LOperand* right = UseFixed(instr->right(), rax);
    LArithmeticT* result = new(zone()) LArithmeticT(op, left, right);
    return MarkAsCall(DefineFixed(result, rax), instr);
  }

  ASSERT(instr->representation().IsInteger32());
  ASSERT(instr->left()->representation().IsInteger32());
  ASSERT(instr->right()->representation().IsInteger32());
  LOperand* left = UseRegisterAtStart(instr->left());

  HValue* right_value = instr->right();
  LOperand* right = NULL;
  int constant_value = 0;
  if (right_value->IsConstant()) {
    HConstant* constant = HConstant::cast(right_value);
    right = chunk_->DefineConstantOperand(constant);
    constant_value = constant->Integer32Value() & 0x1f;
  } else {
    right = UseFixed(right_value, rcx);
  }

  // Shift operations can only deoptimize if we do a logical shift by 0 and
  // the result cannot be truncated to int32.
  bool may_deopt = (op == Token::SHR && constant_value == 0);
  bool does_deopt = false;
  if (may_deopt) {
    for (HUseIterator it(instr->uses()); !it.Done(); it.Advance()) {
      if (!it.value()->CheckFlag(HValue::kTruncatingToInt32)) {
        does_deopt = true;
        break;
      }
    }
  }

  LInstruction* result =
      DefineSameAsFirst(new(zone()) LShiftI(op, left, right, does_deopt));
  return does_deopt ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoArithmeticD(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->left()->representation().IsDouble());
  ASSERT(instr->right()->representation().IsDouble());
  ASSERT(op != Token::MOD);
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterAtStart(instr->right());
  LArithmeticD* result = new(zone()) LArithmeticD(op, left, right);
  return DefineSameAsFirst(result);
}


LInstruction* LChunkBuilder::DoArithmeticT(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  ASSERT(op == Token::ADD ||
         op == Token::DIV ||
         op == Token::MOD ||
         op == Token::MUL ||
         op == Token::SUB);
  HValue* left = instr->left();
  HValue* right = instr->right();
  ASSERT(left->representation().IsTagged());
  ASSERT(right->representation().IsTagged());
  LOperand* left_operand = UseFixed(left, rdx);
  LOperand* right_operand = UseFixed(right, rax);
  LArithmeticT* result =
      new(zone()) LArithmeticT(op, left_operand, right_operand);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


void LChunkBuilder::DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block) {
  ASSERT(is_building());
  current_block_ = block;
  next_block_ = next_block;
  if (block->IsStartBlock()) {
    block->UpdateEnvironment(graph_->start_environment());
    argument_count_ = 0;
  } else if (block->predecessors()->length() == 1) {
    // We have a single predecessor => copy environment and outgoing
    // argument count from the predecessor.
    ASSERT(block->phis()->length() == 0);
    HBasicBlock* pred = block->predecessors()->at(0);
    HEnvironment* last_environment = pred->last_environment();
    ASSERT(last_environment != NULL);
    // Only copy the environment, if it is later used again.
    if (pred->end()->SecondSuccessor() == NULL) {
      ASSERT(pred->end()->FirstSuccessor() == block);
    } else {
      if (pred->end()->FirstSuccessor()->block_id() > block->block_id() ||
          pred->end()->SecondSuccessor()->block_id() > block->block_id()) {
        last_environment = last_environment->Copy();
      }
    }
    block->UpdateEnvironment(last_environment);
    ASSERT(pred->argument_count() >= 0);
    argument_count_ = pred->argument_count();
  } else {
    // We are at a state join => process phis.
    HBasicBlock* pred = block->predecessors()->at(0);
    // No need to copy the environment, it cannot be used later.
    HEnvironment* last_environment = pred->last_environment();
    for (int i = 0; i < block->phis()->length(); ++i) {
      HPhi* phi = block->phis()->at(i);
      last_environment->SetValueAt(phi->merged_index(), phi);
    }
    for (int i = 0; i < block->deleted_phis()->length(); ++i) {
      last_environment->SetValueAt(block->deleted_phis()->at(i),
                                   graph_->GetConstantUndefined());
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
  if (current->has_position()) position_ = current->position();
  LInstruction* instr = current->CompileToLithium(this);

  if (instr != NULL) {
    if (FLAG_stress_pointer_maps && !instr->HasPointerMap()) {
      instr = AssignPointerMap(instr);
    }
    if (FLAG_stress_environments && !instr->HasEnvironment()) {
      instr = AssignEnvironment(instr);
    }
    instr->set_hydrogen_value(current);
    chunk_->AddInstruction(instr, current_block_);
  }
  current_instruction_ = old_current;
}


LEnvironment* LChunkBuilder::CreateEnvironment(
    HEnvironment* hydrogen_env,
    int* argument_index_accumulator) {
  if (hydrogen_env == NULL) return NULL;

  LEnvironment* outer =
      CreateEnvironment(hydrogen_env->outer(), argument_index_accumulator);
  int ast_id = hydrogen_env->ast_id();
  ASSERT(ast_id != AstNode::kNoNumber ||
         hydrogen_env->frame_type() != JS_FUNCTION);
  int value_count = hydrogen_env->length();
  LEnvironment* result = new(zone()) LEnvironment(
      hydrogen_env->closure(),
      hydrogen_env->frame_type(),
      ast_id,
      hydrogen_env->parameter_count(),
      argument_count_,
      value_count,
      outer);
  int argument_index = *argument_index_accumulator;
  for (int i = 0; i < value_count; ++i) {
    if (hydrogen_env->is_special_index(i)) continue;

    HValue* value = hydrogen_env->values()->at(i);
    LOperand* op = NULL;
    if (value->IsArgumentsObject()) {
      op = NULL;
    } else if (value->IsPushArgument()) {
      op = new(zone()) LArgument(argument_index++);
    } else {
      op = UseAny(value);
    }
    result->AddValue(op, value->representation());
  }

  if (hydrogen_env->frame_type() == JS_FUNCTION) {
    *argument_index_accumulator = argument_index;
  }

  return result;
}


LInstruction* LChunkBuilder::DoGoto(HGoto* instr) {
  return new(zone()) LGoto(instr->FirstSuccessor()->block_id());
}


LInstruction* LChunkBuilder::DoBranch(HBranch* instr) {
  HValue* value = instr->value();
  if (value->EmitAtUses()) {
    ASSERT(value->IsConstant());
    ASSERT(!value->representation().IsDouble());
    HBasicBlock* successor = HConstant::cast(value)->ToBoolean()
        ? instr->FirstSuccessor()
        : instr->SecondSuccessor();
    return new(zone()) LGoto(successor->block_id());
  }

  LBranch* result = new(zone()) LBranch(UseRegister(value));
  // Tagged values that are not known smis or booleans require a
  // deoptimization environment.
  Representation rep = value->representation();
  HType type = value->type();
  if (rep.IsTagged() && !type.IsSmi() && !type.IsBoolean()) {
    return AssignEnvironment(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoCompareMap(HCompareMap* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LCmpMapAndBranch(value);
}


LInstruction* LChunkBuilder::DoArgumentsLength(HArgumentsLength* length) {
  return DefineAsRegister(new(zone()) LArgumentsLength(Use(length->value())));
}


LInstruction* LChunkBuilder::DoArgumentsElements(HArgumentsElements* elems) {
  return DefineAsRegister(new(zone()) LArgumentsElements);
}


LInstruction* LChunkBuilder::DoInstanceOf(HInstanceOf* instr) {
  LOperand* left = UseFixed(instr->left(), rax);
  LOperand* right = UseFixed(instr->right(), rdx);
  LInstanceOf* result = new(zone()) LInstanceOf(left, right);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoInstanceOfKnownGlobal(
    HInstanceOfKnownGlobal* instr) {
  LInstanceOfKnownGlobal* result =
      new(zone()) LInstanceOfKnownGlobal(UseFixed(instr->left(), rax),
                                         FixedTemp(rdi));
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoWrapReceiver(HWrapReceiver* instr) {
  LOperand* receiver = UseRegister(instr->receiver());
  LOperand* function = UseRegisterAtStart(instr->function());
  LWrapReceiver* result = new(zone()) LWrapReceiver(receiver, function);
  return AssignEnvironment(DefineSameAsFirst(result));
}


LInstruction* LChunkBuilder::DoApplyArguments(HApplyArguments* instr) {
  LOperand* function = UseFixed(instr->function(), rdi);
  LOperand* receiver = UseFixed(instr->receiver(), rax);
  LOperand* length = UseFixed(instr->length(), rbx);
  LOperand* elements = UseFixed(instr->elements(), rcx);
  LApplyArguments* result = new(zone()) LApplyArguments(function,
                                                receiver,
                                                length,
                                                elements);
  return MarkAsCall(DefineFixed(result, rax), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoPushArgument(HPushArgument* instr) {
  ++argument_count_;
  LOperand* argument = UseOrConstant(instr->argument());
  return new(zone()) LPushArgument(argument);
}


LInstruction* LChunkBuilder::DoThisFunction(HThisFunction* instr) {
  return instr->HasNoUses()
      ? NULL
      : DefineAsRegister(new(zone()) LThisFunction);
}


LInstruction* LChunkBuilder::DoContext(HContext* instr) {
  return instr->HasNoUses() ? NULL : DefineAsRegister(new(zone()) LContext);
}


LInstruction* LChunkBuilder::DoOuterContext(HOuterContext* instr) {
  LOperand* context = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LOuterContext(context));
}


LInstruction* LChunkBuilder::DoDeclareGlobals(HDeclareGlobals* instr) {
  return MarkAsCall(new(zone()) LDeclareGlobals, instr);
}


LInstruction* LChunkBuilder::DoGlobalObject(HGlobalObject* instr) {
  return DefineAsRegister(new(zone()) LGlobalObject);
}


LInstruction* LChunkBuilder::DoGlobalReceiver(HGlobalReceiver* instr) {
  LOperand* global_object = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LGlobalReceiver(global_object));
}


LInstruction* LChunkBuilder::DoCallConstantFunction(
    HCallConstantFunction* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new(zone()) LCallConstantFunction, rax), instr);
}


LInstruction* LChunkBuilder::DoInvokeFunction(HInvokeFunction* instr) {
  LOperand* function = UseFixed(instr->function(), rdi);
  argument_count_ -= instr->argument_count();
  LInvokeFunction* result = new(zone()) LInvokeFunction(function);
  return MarkAsCall(DefineFixed(result, rax), instr, CANNOT_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoUnaryMathOperation(HUnaryMathOperation* instr) {
  BuiltinFunctionId op = instr->op();
  if (op == kMathLog || op == kMathSin || op == kMathCos || op == kMathTan) {
    LOperand* input = UseFixedDouble(instr->value(), xmm1);
    LUnaryMathOperation* result = new(zone()) LUnaryMathOperation(input);
    return MarkAsCall(DefineFixedDouble(result, xmm1), instr);
  } else {
    LOperand* input = UseRegisterAtStart(instr->value());
    LUnaryMathOperation* result = new(zone()) LUnaryMathOperation(input);
    switch (op) {
      case kMathAbs:
        return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
      case kMathFloor:
        return AssignEnvironment(DefineAsRegister(result));
      case kMathRound:
        return AssignEnvironment(DefineAsRegister(result));
      case kMathSqrt:
        return DefineSameAsFirst(result);
      case kMathPowHalf:
        return DefineSameAsFirst(result);
      default:
        UNREACHABLE();
        return NULL;
    }
  }
}


LInstruction* LChunkBuilder::DoCallKeyed(HCallKeyed* instr) {
  ASSERT(instr->key()->representation().IsTagged());
  LOperand* key = UseFixed(instr->key(), rcx);
  argument_count_ -= instr->argument_count();
  LCallKeyed* result = new(zone()) LCallKeyed(key);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoCallNamed(HCallNamed* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new(zone()) LCallNamed, rax), instr);
}


LInstruction* LChunkBuilder::DoCallGlobal(HCallGlobal* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new(zone()) LCallGlobal, rax), instr);
}


LInstruction* LChunkBuilder::DoCallKnownGlobal(HCallKnownGlobal* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new(zone()) LCallKnownGlobal, rax), instr);
}


LInstruction* LChunkBuilder::DoCallNew(HCallNew* instr) {
  LOperand* constructor = UseFixed(instr->constructor(), rdi);
  argument_count_ -= instr->argument_count();
  LCallNew* result = new(zone()) LCallNew(constructor);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoCallFunction(HCallFunction* instr) {
  LOperand* function = UseFixed(instr->function(), rdi);
  argument_count_ -= instr->argument_count();
  LCallFunction* result = new(zone()) LCallFunction(function);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoCallRuntime(HCallRuntime* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new(zone()) LCallRuntime, rax), instr);
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
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());

    LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
    LOperand* right = UseOrConstantAtStart(instr->MostConstantOperand());
    return DefineSameAsFirst(new(zone()) LBitI(left, right));
  } else {
    ASSERT(instr->representation().IsTagged());
    ASSERT(instr->left()->representation().IsTagged());
    ASSERT(instr->right()->representation().IsTagged());

    LOperand* left = UseFixed(instr->left(), rdx);
    LOperand* right = UseFixed(instr->right(), rax);
    LArithmeticT* result = new(zone()) LArithmeticT(instr->op(), left, right);
    return MarkAsCall(DefineFixed(result, rax), instr);
  }
}


LInstruction* LChunkBuilder::DoBitNot(HBitNot* instr) {
  ASSERT(instr->value()->representation().IsInteger32());
  ASSERT(instr->representation().IsInteger32());
  LOperand* input = UseRegisterAtStart(instr->value());
  LBitNotI* result = new(zone()) LBitNotI(input);
  return DefineSameAsFirst(result);
}


LInstruction* LChunkBuilder::DoDiv(HDiv* instr) {
  if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::DIV, instr);
  } else if (instr->representation().IsInteger32()) {
    // The temporary operand is necessary to ensure that right is not allocated
    // into rdx.
    LOperand* temp = FixedTemp(rdx);
    LOperand* dividend = UseFixed(instr->left(), rax);
    LOperand* divisor = UseRegister(instr->right());
    LDivI* result = new(zone()) LDivI(dividend, divisor, temp);
    return AssignEnvironment(DefineFixed(result, rax));
  } else {
    ASSERT(instr->representation().IsTagged());
    return DoArithmeticT(Token::DIV, instr);
  }
}


LInstruction* LChunkBuilder::DoMod(HMod* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());

    LInstruction* result;
    if (instr->HasPowerOf2Divisor()) {
      ASSERT(!instr->CheckFlag(HValue::kCanBeDivByZero));
      LOperand* value = UseRegisterAtStart(instr->left());
      LModI* mod =
          new(zone()) LModI(value, UseOrConstant(instr->right()), NULL);
      result = DefineSameAsFirst(mod);
    } else {
      // The temporary operand is necessary to ensure that right is not
      // allocated into edx.
      LOperand* temp = FixedTemp(rdx);
      LOperand* value = UseFixed(instr->left(), rax);
      LOperand* divisor = UseRegister(instr->right());
      LModI* mod = new(zone()) LModI(value, divisor, temp);
      result = DefineFixed(mod, rdx);
    }

    return (instr->CheckFlag(HValue::kBailoutOnMinusZero) ||
            instr->CheckFlag(HValue::kCanBeDivByZero))
        ? AssignEnvironment(result)
        : result;
  } else if (instr->representation().IsTagged()) {
    return DoArithmeticT(Token::MOD, instr);
  } else {
    ASSERT(instr->representation().IsDouble());
    // We call a C function for double modulo. It can't trigger a GC.
    // We need to use fixed result register for the call.
    // TODO(fschneider): Allow any register as input registers.
    LOperand* left = UseFixedDouble(instr->left(), xmm2);
    LOperand* right = UseFixedDouble(instr->right(), xmm1);
    LArithmeticD* result = new(zone()) LArithmeticD(Token::MOD, left, right);
    return MarkAsCall(DefineFixedDouble(result, xmm1), instr);
  }
}


LInstruction* LChunkBuilder::DoMul(HMul* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
    LOperand* right = UseOrConstant(instr->MostConstantOperand());
    LMulI* mul = new(zone()) LMulI(left, right);
    if (instr->CheckFlag(HValue::kCanOverflow) ||
        instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
      AssignEnvironment(mul);
    }
    return DefineSameAsFirst(mul);
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::MUL, instr);
  } else {
    ASSERT(instr->representation().IsTagged());
    return DoArithmeticT(Token::MUL, instr);
  }
}


LInstruction* LChunkBuilder::DoSub(HSub* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    LSubI* sub = new(zone()) LSubI(left, right);
    LInstruction* result = DefineSameAsFirst(sub);
    if (instr->CheckFlag(HValue::kCanOverflow)) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::SUB, instr);
  } else {
    ASSERT(instr->representation().IsTagged());
    return DoArithmeticT(Token::SUB, instr);
  }
}


LInstruction* LChunkBuilder::DoAdd(HAdd* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
    LOperand* right = UseOrConstantAtStart(instr->MostConstantOperand());
    LAddI* add = new(zone()) LAddI(left, right);
    LInstruction* result = DefineSameAsFirst(add);
    if (instr->CheckFlag(HValue::kCanOverflow)) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::ADD, instr);
  } else {
    ASSERT(instr->representation().IsTagged());
    return DoArithmeticT(Token::ADD, instr);
  }
  return NULL;
}


LInstruction* LChunkBuilder::DoPower(HPower* instr) {
  ASSERT(instr->representation().IsDouble());
  // We call a C function for double power. It can't trigger a GC.
  // We need to use fixed result register for the call.
  Representation exponent_type = instr->right()->representation();
  ASSERT(instr->left()->representation().IsDouble());
  LOperand* left = UseFixedDouble(instr->left(), xmm2);
  LOperand* right = exponent_type.IsDouble() ?
      UseFixedDouble(instr->right(), xmm1) :
#ifdef _WIN64
      UseFixed(instr->right(), rdx);
#else
      UseFixed(instr->right(), rdi);
#endif
  LPower* result = new(zone()) LPower(left, right);
  return MarkAsCall(DefineFixedDouble(result, xmm3), instr,
                    CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoRandom(HRandom* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->global_object()->representation().IsTagged());
#ifdef _WIN64
  LOperand* global_object = UseFixed(instr->global_object(), rcx);
#else
  LOperand* global_object = UseFixed(instr->global_object(), rdi);
#endif
  LRandom* result = new(zone()) LRandom(global_object);
  return MarkAsCall(DefineFixedDouble(result, xmm1), instr);
}


LInstruction* LChunkBuilder::DoCompareGeneric(HCompareGeneric* instr) {
  ASSERT(instr->left()->representation().IsTagged());
  ASSERT(instr->right()->representation().IsTagged());
  LOperand* left = UseFixed(instr->left(), rdx);
  LOperand* right = UseFixed(instr->right(), rax);
  LCmpT* result = new(zone()) LCmpT(left, right);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoCompareIDAndBranch(
    HCompareIDAndBranch* instr) {
  Representation r = instr->GetInputRepresentation();
  if (r.IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterOrConstantAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    return new(zone()) LCmpIDAndBranch(left, right);
  } else {
    ASSERT(r.IsDouble());
    ASSERT(instr->left()->representation().IsDouble());
    ASSERT(instr->right()->representation().IsDouble());
    LOperand* left;
    LOperand* right;
    if (instr->left()->IsConstant() && instr->right()->IsConstant()) {
      left = UseRegisterOrConstantAtStart(instr->left());
      right = UseRegisterOrConstantAtStart(instr->right());
    } else {
      left = UseRegisterAtStart(instr->left());
      right = UseRegisterAtStart(instr->right());
    }
    return new(zone()) LCmpIDAndBranch(left, right);
  }
}


LInstruction* LChunkBuilder::DoCompareObjectEqAndBranch(
    HCompareObjectEqAndBranch* instr) {
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterAtStart(instr->right());
  return new(zone()) LCmpObjectEqAndBranch(left, right);
}


LInstruction* LChunkBuilder::DoCompareConstantEqAndBranch(
    HCompareConstantEqAndBranch* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LCmpConstantEqAndBranch(value);
}


LInstruction* LChunkBuilder::DoIsNilAndBranch(HIsNilAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* temp = instr->kind() == kStrictEquality ? NULL : TempRegister();
  return new(zone()) LIsNilAndBranch(UseRegisterAtStart(instr->value()), temp);
}


LInstruction* LChunkBuilder::DoIsObjectAndBranch(HIsObjectAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LIsObjectAndBranch(UseRegisterAtStart(instr->value()));
}


LInstruction* LChunkBuilder::DoIsStringAndBranch(HIsStringAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  return new(zone()) LIsStringAndBranch(value, temp);
}


LInstruction* LChunkBuilder::DoIsSmiAndBranch(HIsSmiAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LIsSmiAndBranch(Use(instr->value()));
}


LInstruction* LChunkBuilder::DoIsUndetectableAndBranch(
    HIsUndetectableAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  return new(zone()) LIsUndetectableAndBranch(value, temp);
}


LInstruction* LChunkBuilder::DoStringCompareAndBranch(
    HStringCompareAndBranch* instr) {

  ASSERT(instr->left()->representation().IsTagged());
  ASSERT(instr->right()->representation().IsTagged());
  LOperand* left = UseFixed(instr->left(), rdx);
  LOperand* right = UseFixed(instr->right(), rax);
  LStringCompareAndBranch* result =
      new(zone()) LStringCompareAndBranch(left, right);

  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoHasInstanceTypeAndBranch(
    HHasInstanceTypeAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LHasInstanceTypeAndBranch(value);
}


LInstruction* LChunkBuilder::DoGetCachedArrayIndex(
    HGetCachedArrayIndex* instr)  {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());

  return DefineAsRegister(new(zone()) LGetCachedArrayIndex(value));
}


LInstruction* LChunkBuilder::DoHasCachedArrayIndexAndBranch(
    HHasCachedArrayIndexAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LHasCachedArrayIndexAndBranch(value);
}


LInstruction* LChunkBuilder::DoClassOfTestAndBranch(
    HClassOfTestAndBranch* instr) {
  LOperand* value = UseRegister(instr->value());
  return new(zone()) LClassOfTestAndBranch(value,
                                           TempRegister(),
                                           TempRegister());
}


LInstruction* LChunkBuilder::DoJSArrayLength(HJSArrayLength* instr) {
  LOperand* array = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LJSArrayLength(array));
}


LInstruction* LChunkBuilder::DoFixedArrayBaseLength(
    HFixedArrayBaseLength* instr) {
  LOperand* array = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LFixedArrayBaseLength(array));
}


LInstruction* LChunkBuilder::DoElementsKind(HElementsKind* instr) {
  LOperand* object = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LElementsKind(object));
}


LInstruction* LChunkBuilder::DoValueOf(HValueOf* instr) {
  LOperand* object = UseRegister(instr->value());
  LValueOf* result = new(zone()) LValueOf(object);
  return DefineSameAsFirst(result);
}


LInstruction* LChunkBuilder::DoDateField(HDateField* instr) {
  LOperand* object = UseFixed(instr->value(), rax);
  LDateField* result = new LDateField(object, instr->index());
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoBoundsCheck(HBoundsCheck* instr) {
  LOperand* value = UseRegisterOrConstantAtStart(instr->index());
  LOperand* length = Use(instr->length());
  return AssignEnvironment(new(zone()) LBoundsCheck(value, length));
}


LInstruction* LChunkBuilder::DoAbnormalExit(HAbnormalExit* instr) {
  // The control instruction marking the end of a block that completed
  // abruptly (e.g., threw an exception).  There is nothing specific to do.
  return NULL;
}


LInstruction* LChunkBuilder::DoThrow(HThrow* instr) {
  LOperand* value = UseFixed(instr->value(), rax);
  return MarkAsCall(new(zone()) LThrow(value), instr);
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
  if (from.IsTagged()) {
    if (to.IsDouble()) {
      LOperand* value = UseRegister(instr->value());
      LNumberUntagD* res = new(zone()) LNumberUntagD(value);
      return AssignEnvironment(DefineAsRegister(res));
    } else {
      ASSERT(to.IsInteger32());
      LOperand* value = UseRegister(instr->value());
      bool needs_check = !instr->value()->type().IsSmi();
      if (needs_check) {
        bool truncating = instr->CanTruncateToInt32();
        LOperand* xmm_temp = truncating ? NULL : FixedTemp(xmm1);
        LTaggedToI* res = new(zone()) LTaggedToI(value, xmm_temp);
        return AssignEnvironment(DefineSameAsFirst(res));
      } else {
        return DefineSameAsFirst(new(zone()) LSmiUntag(value, needs_check));
      }
    }
  } else if (from.IsDouble()) {
    if (to.IsTagged()) {
      LOperand* value = UseRegister(instr->value());
      LOperand* temp = TempRegister();

      // Make sure that temp and result_temp are different registers.
      LUnallocated* result_temp = TempRegister();
      LNumberTagD* result = new(zone()) LNumberTagD(value, temp);
      return AssignPointerMap(Define(result, result_temp));
    } else {
      ASSERT(to.IsInteger32());
      LOperand* value = UseRegister(instr->value());
      return AssignEnvironment(DefineAsRegister(new(zone()) LDoubleToI(value)));
    }
  } else if (from.IsInteger32()) {
    if (to.IsTagged()) {
      HValue* val = instr->value();
      LOperand* value = UseRegister(val);
      if (val->HasRange() && val->range()->IsInSmiRange()) {
        return DefineSameAsFirst(new(zone()) LSmiTag(value));
      } else {
        LNumberTagI* result = new(zone()) LNumberTagI(value);
        return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
      }
    } else {
      ASSERT(to.IsDouble());
      LOperand* value = Use(instr->value());
      return DefineAsRegister(new(zone()) LInteger32ToDouble(value));
    }
  }
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckNonSmi(HCheckNonSmi* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckNonSmi(value));
}


LInstruction* LChunkBuilder::DoCheckInstanceType(HCheckInstanceType* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LCheckInstanceType* result = new(zone()) LCheckInstanceType(value);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckPrototypeMaps(HCheckPrototypeMaps* instr) {
  LOperand* temp = TempRegister();
  LCheckPrototypeMaps* result = new(zone()) LCheckPrototypeMaps(temp);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckSmi(HCheckSmi* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckSmi(value));
}


LInstruction* LChunkBuilder::DoCheckFunction(HCheckFunction* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckFunction(value));
}


LInstruction* LChunkBuilder::DoCheckMap(HCheckMap* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LCheckMap* result = new(zone()) LCheckMap(value);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoClampToUint8(HClampToUint8* instr) {
  HValue* value = instr->value();
  Representation input_rep = value->representation();
  LOperand* reg = UseRegister(value);
  if (input_rep.IsDouble()) {
    return DefineAsRegister(new(zone()) LClampDToUint8(reg,
                                               TempRegister()));
  } else if (input_rep.IsInteger32()) {
    return DefineSameAsFirst(new(zone()) LClampIToUint8(reg));
  } else {
    ASSERT(input_rep.IsTagged());
    // Register allocator doesn't (yet) support allocation of double
    // temps. Reserve xmm1 explicitly.
    LClampTToUint8* result = new(zone()) LClampTToUint8(reg,
                                                        TempRegister(),
                                                        FixedTemp(xmm1));
    return AssignEnvironment(DefineSameAsFirst(result));
  }
}


LInstruction* LChunkBuilder::DoReturn(HReturn* instr) {
  return new(zone()) LReturn(UseFixed(instr->value(), rax));
}


LInstruction* LChunkBuilder::DoConstant(HConstant* instr) {
  Representation r = instr->representation();
  if (r.IsInteger32()) {
    return DefineAsRegister(new(zone()) LConstantI);
  } else if (r.IsDouble()) {
    LOperand* temp = TempRegister();
    return DefineAsRegister(new(zone()) LConstantD(temp));
  } else if (r.IsTagged()) {
    return DefineAsRegister(new(zone()) LConstantT);
  } else {
    UNREACHABLE();
    return NULL;
  }
}


LInstruction* LChunkBuilder::DoLoadGlobalCell(HLoadGlobalCell* instr) {
  LLoadGlobalCell* result = new(zone()) LLoadGlobalCell;
  return instr->RequiresHoleCheck()
      ? AssignEnvironment(DefineAsRegister(result))
      : DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoLoadGlobalGeneric(HLoadGlobalGeneric* instr) {
  LOperand* global_object = UseFixed(instr->global_object(), rax);
  LLoadGlobalGeneric* result = new(zone()) LLoadGlobalGeneric(global_object);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoStoreGlobalCell(HStoreGlobalCell* instr) {
  LOperand* value = UseRegister(instr->value());
  // Use a temp to avoid reloading the cell value address in the case where
  // we perform a hole check.
  return instr->RequiresHoleCheck()
      ? AssignEnvironment(new(zone()) LStoreGlobalCell(value, TempRegister()))
      : new(zone()) LStoreGlobalCell(value, NULL);
}


LInstruction* LChunkBuilder::DoStoreGlobalGeneric(HStoreGlobalGeneric* instr) {
  LOperand* global_object = UseFixed(instr->global_object(), rdx);
  LOperand* value = UseFixed(instr->value(), rax);
  LStoreGlobalGeneric* result =  new(zone()) LStoreGlobalGeneric(global_object,
                                                                 value);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoLoadContextSlot(HLoadContextSlot* instr) {
  LOperand* context = UseRegisterAtStart(instr->value());
  LInstruction* result =
      DefineAsRegister(new(zone()) LLoadContextSlot(context));
  return instr->RequiresHoleCheck() ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoStoreContextSlot(HStoreContextSlot* instr) {
  LOperand* context;
  LOperand* value;
  LOperand* temp;
  if (instr->NeedsWriteBarrier()) {
    context = UseTempRegister(instr->context());
    value = UseTempRegister(instr->value());
    temp = TempRegister();
  } else {
    context = UseRegister(instr->context());
    value = UseRegister(instr->value());
    temp = NULL;
  }
  LInstruction* result = new(zone()) LStoreContextSlot(context, value, temp);
  return instr->RequiresHoleCheck() ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoLoadNamedField(HLoadNamedField* instr) {
  ASSERT(instr->representation().IsTagged());
  LOperand* obj = UseRegisterAtStart(instr->object());
  return DefineAsRegister(new(zone()) LLoadNamedField(obj));
}


LInstruction* LChunkBuilder::DoLoadNamedFieldPolymorphic(
    HLoadNamedFieldPolymorphic* instr) {
  ASSERT(instr->representation().IsTagged());
  if (instr->need_generic()) {
    LOperand* obj = UseFixed(instr->object(), rax);
    LLoadNamedFieldPolymorphic* result =
        new(zone()) LLoadNamedFieldPolymorphic(obj);
    return MarkAsCall(DefineFixed(result, rax), instr);
  } else {
    LOperand* obj = UseRegisterAtStart(instr->object());
    LLoadNamedFieldPolymorphic* result =
        new(zone()) LLoadNamedFieldPolymorphic(obj);
    return AssignEnvironment(DefineAsRegister(result));
  }
}


LInstruction* LChunkBuilder::DoLoadNamedGeneric(HLoadNamedGeneric* instr) {
  LOperand* object = UseFixed(instr->object(), rax);
  LLoadNamedGeneric* result = new(zone()) LLoadNamedGeneric(object);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoLoadFunctionPrototype(
    HLoadFunctionPrototype* instr) {
  return AssignEnvironment(DefineAsRegister(
      new(zone()) LLoadFunctionPrototype(UseRegister(instr->function()))));
}


LInstruction* LChunkBuilder::DoLoadElements(HLoadElements* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LLoadElements(input));
}


LInstruction* LChunkBuilder::DoLoadExternalArrayPointer(
    HLoadExternalArrayPointer* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LLoadExternalArrayPointer(input));
}


LInstruction* LChunkBuilder::DoLoadKeyedFastElement(
    HLoadKeyedFastElement* instr) {
  ASSERT(instr->representation().IsTagged());
  ASSERT(instr->key()->representation().IsInteger32());
  LOperand* obj = UseRegisterAtStart(instr->object());
  LOperand* key = UseRegisterOrConstantAtStart(instr->key());
  LLoadKeyedFastElement* result = new(zone()) LLoadKeyedFastElement(obj, key);
  if (instr->RequiresHoleCheck()) AssignEnvironment(result);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoLoadKeyedFastDoubleElement(
    HLoadKeyedFastDoubleElement* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->key()->representation().IsInteger32());
  LOperand* elements = UseRegisterAtStart(instr->elements());
  LOperand* key = UseRegisterOrConstantAtStart(instr->key());
  LLoadKeyedFastDoubleElement* result =
      new(zone()) LLoadKeyedFastDoubleElement(elements, key);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoLoadKeyedSpecializedArrayElement(
    HLoadKeyedSpecializedArrayElement* instr) {
  ElementsKind elements_kind = instr->elements_kind();
  ASSERT(
      (instr->representation().IsInteger32() &&
       (elements_kind != EXTERNAL_FLOAT_ELEMENTS) &&
       (elements_kind != EXTERNAL_DOUBLE_ELEMENTS)) ||
      (instr->representation().IsDouble() &&
       ((elements_kind == EXTERNAL_FLOAT_ELEMENTS) ||
       (elements_kind == EXTERNAL_DOUBLE_ELEMENTS))));
  ASSERT(instr->key()->representation().IsInteger32());
  LOperand* external_pointer = UseRegister(instr->external_pointer());
  LOperand* key = UseRegisterOrConstant(instr->key());
  LLoadKeyedSpecializedArrayElement* result =
      new(zone()) LLoadKeyedSpecializedArrayElement(external_pointer, key);
  LInstruction* load_instr = DefineAsRegister(result);
  // An unsigned int array load might overflow and cause a deopt, make sure it
  // has an environment.
  return (elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) ?
      AssignEnvironment(load_instr) : load_instr;
}


LInstruction* LChunkBuilder::DoLoadKeyedGeneric(HLoadKeyedGeneric* instr) {
  LOperand* object = UseFixed(instr->object(), rdx);
  LOperand* key = UseFixed(instr->key(), rax);

  LLoadKeyedGeneric* result = new(zone()) LLoadKeyedGeneric(object, key);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoStoreKeyedFastElement(
    HStoreKeyedFastElement* instr) {
  bool needs_write_barrier = instr->NeedsWriteBarrier();
  ASSERT(instr->value()->representation().IsTagged());
  ASSERT(instr->object()->representation().IsTagged());
  ASSERT(instr->key()->representation().IsInteger32());

  LOperand* obj = UseTempRegister(instr->object());
  LOperand* val = needs_write_barrier
      ? UseTempRegister(instr->value())
      : UseRegisterAtStart(instr->value());
  LOperand* key = needs_write_barrier
      ? UseTempRegister(instr->key())
      : UseRegisterOrConstantAtStart(instr->key());
  return new(zone()) LStoreKeyedFastElement(obj, key, val);
}


LInstruction* LChunkBuilder::DoStoreKeyedFastDoubleElement(
    HStoreKeyedFastDoubleElement* instr) {
  ASSERT(instr->value()->representation().IsDouble());
  ASSERT(instr->elements()->representation().IsTagged());
  ASSERT(instr->key()->representation().IsInteger32());

  LOperand* elements = UseRegisterAtStart(instr->elements());
  LOperand* val = UseTempRegister(instr->value());
  LOperand* key = UseRegisterOrConstantAtStart(instr->key());

  return new(zone()) LStoreKeyedFastDoubleElement(elements, key, val);
}


LInstruction* LChunkBuilder::DoStoreKeyedSpecializedArrayElement(
    HStoreKeyedSpecializedArrayElement* instr) {
  ElementsKind elements_kind = instr->elements_kind();
  ASSERT(
      (instr->value()->representation().IsInteger32() &&
       (elements_kind != EXTERNAL_FLOAT_ELEMENTS) &&
       (elements_kind != EXTERNAL_DOUBLE_ELEMENTS)) ||
      (instr->value()->representation().IsDouble() &&
       ((elements_kind == EXTERNAL_FLOAT_ELEMENTS) ||
       (elements_kind == EXTERNAL_DOUBLE_ELEMENTS))));
  ASSERT(instr->external_pointer()->representation().IsExternal());
  ASSERT(instr->key()->representation().IsInteger32());

  LOperand* external_pointer = UseRegister(instr->external_pointer());
  bool val_is_temp_register =
      elements_kind == EXTERNAL_PIXEL_ELEMENTS ||
      elements_kind == EXTERNAL_FLOAT_ELEMENTS;
  LOperand* val = val_is_temp_register
      ? UseTempRegister(instr->value())
      : UseRegister(instr->value());
  LOperand* key = UseRegisterOrConstant(instr->key());

  return new(zone()) LStoreKeyedSpecializedArrayElement(external_pointer,
                                                        key,
                                                        val);
}


LInstruction* LChunkBuilder::DoStoreKeyedGeneric(HStoreKeyedGeneric* instr) {
  LOperand* object = UseFixed(instr->object(), rdx);
  LOperand* key = UseFixed(instr->key(), rcx);
  LOperand* value = UseFixed(instr->value(), rax);

  ASSERT(instr->object()->representation().IsTagged());
  ASSERT(instr->key()->representation().IsTagged());
  ASSERT(instr->value()->representation().IsTagged());

  LStoreKeyedGeneric* result =
      new(zone()) LStoreKeyedGeneric(object, key, value);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoTransitionElementsKind(
    HTransitionElementsKind* instr) {
  if (instr->original_map()->elements_kind() == FAST_SMI_ONLY_ELEMENTS &&
      instr->transitioned_map()->elements_kind() == FAST_ELEMENTS) {
    LOperand* object = UseRegister(instr->object());
    LOperand* new_map_reg = TempRegister();
    LOperand* temp_reg = TempRegister();
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, new_map_reg, temp_reg);
    return DefineSameAsFirst(result);
  } else {
    LOperand* object = UseFixed(instr->object(), rax);
    LOperand* fixed_object_reg = FixedTemp(rdx);
    LOperand* new_map_reg = FixedTemp(rbx);
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object,
                                            new_map_reg,
                                            fixed_object_reg);
    return MarkAsCall(DefineFixed(result, rax), instr);
  }
}


LInstruction* LChunkBuilder::DoStoreNamedField(HStoreNamedField* instr) {
  bool needs_write_barrier = instr->NeedsWriteBarrier();

  LOperand* obj = needs_write_barrier
      ? UseTempRegister(instr->object())
      : UseRegisterAtStart(instr->object());

  LOperand* val = needs_write_barrier
      ? UseTempRegister(instr->value())
      : UseRegister(instr->value());

  // We only need a scratch register if we have a write barrier or we
  // have a store into the properties array (not in-object-property).
  LOperand* temp = (!instr->is_in_object() || needs_write_barrier)
      ? TempRegister() : NULL;

  return new(zone()) LStoreNamedField(obj, val, temp);
}


LInstruction* LChunkBuilder::DoStoreNamedGeneric(HStoreNamedGeneric* instr) {
  LOperand* object = UseFixed(instr->object(), rdx);
  LOperand* value = UseFixed(instr->value(), rax);

  LStoreNamedGeneric* result = new(zone()) LStoreNamedGeneric(object, value);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoStringAdd(HStringAdd* instr) {
  LOperand* left = UseOrConstantAtStart(instr->left());
  LOperand* right = UseOrConstantAtStart(instr->right());
  return MarkAsCall(DefineFixed(new(zone()) LStringAdd(left, right), rax),
                    instr);
}


LInstruction* LChunkBuilder::DoStringCharCodeAt(HStringCharCodeAt* instr) {
  LOperand* string = UseTempRegister(instr->string());
  LOperand* index = UseTempRegister(instr->index());
  LStringCharCodeAt* result = new(zone()) LStringCharCodeAt(string, index);
  return AssignEnvironment(AssignPointerMap(DefineAsRegister(result)));
}


LInstruction* LChunkBuilder::DoStringCharFromCode(HStringCharFromCode* instr) {
  LOperand* char_code = UseRegister(instr->value());
  LStringCharFromCode* result = new(zone()) LStringCharFromCode(char_code);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoStringLength(HStringLength* instr) {
  LOperand* string = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LStringLength(string));
}


LInstruction* LChunkBuilder::DoAllocateObject(HAllocateObject* instr) {
  LAllocateObject* result = new LAllocateObject(TempRegister());
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoFastLiteral(HFastLiteral* instr) {
  return MarkAsCall(DefineFixed(new(zone()) LFastLiteral, rax), instr);
}


LInstruction* LChunkBuilder::DoArrayLiteral(HArrayLiteral* instr) {
  return MarkAsCall(DefineFixed(new(zone()) LArrayLiteral, rax), instr);
}


LInstruction* LChunkBuilder::DoObjectLiteral(HObjectLiteral* instr) {
  return MarkAsCall(DefineFixed(new(zone()) LObjectLiteral, rax), instr);
}


LInstruction* LChunkBuilder::DoRegExpLiteral(HRegExpLiteral* instr) {
  return MarkAsCall(DefineFixed(new(zone()) LRegExpLiteral, rax), instr);
}


LInstruction* LChunkBuilder::DoFunctionLiteral(HFunctionLiteral* instr) {
  return MarkAsCall(DefineFixed(new(zone()) LFunctionLiteral, rax), instr);
}


LInstruction* LChunkBuilder::DoDeleteProperty(HDeleteProperty* instr) {
  LOperand* object = UseAtStart(instr->object());
  LOperand* key = UseOrConstantAtStart(instr->key());
  LDeleteProperty* result = new(zone()) LDeleteProperty(object, key);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoOsrEntry(HOsrEntry* instr) {
  allocator_->MarkAsOsrEntry();
  current_block_->last_environment()->set_ast_id(instr->ast_id());
  return AssignEnvironment(new(zone()) LOsrEntry);
}


LInstruction* LChunkBuilder::DoParameter(HParameter* instr) {
  int spill_index = chunk()->GetParameterStackSlot(instr->index());
  return DefineAsSpilled(new(zone()) LParameter, spill_index);
}


LInstruction* LChunkBuilder::DoUnknownOSRValue(HUnknownOSRValue* instr) {
  int spill_index = chunk()->GetNextSpillIndex(false);  // Not double-width.
  if (spill_index > LUnallocated::kMaxFixedIndex) {
    Abort("Too many spill slots needed for OSR");
    spill_index = 0;
  }
  return DefineAsSpilled(new(zone()) LUnknownOSRValue, spill_index);
}


LInstruction* LChunkBuilder::DoCallStub(HCallStub* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new(zone()) LCallStub, rax), instr);
}


LInstruction* LChunkBuilder::DoArgumentsObject(HArgumentsObject* instr) {
  // There are no real uses of the arguments object.
  // arguments.length and element access are supported directly on
  // stack arguments, and any real arguments object use causes a bailout.
  // So this value is never used.
  return NULL;
}


LInstruction* LChunkBuilder::DoAccessArgumentsAt(HAccessArgumentsAt* instr) {
  LOperand* arguments = UseRegister(instr->arguments());
  LOperand* length = UseTempRegister(instr->length());
  LOperand* index = Use(instr->index());
  LAccessArgumentsAt* result =
      new(zone()) LAccessArgumentsAt(arguments, length, index);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoToFastProperties(HToFastProperties* instr) {
  LOperand* object = UseFixed(instr->value(), rax);
  LToFastProperties* result = new(zone()) LToFastProperties(object);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoTypeof(HTypeof* instr) {
  LTypeof* result = new(zone()) LTypeof(UseAtStart(instr->value()));
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoTypeofIsAndBranch(HTypeofIsAndBranch* instr) {
  return new(zone()) LTypeofIsAndBranch(UseTempRegister(instr->value()));
}


LInstruction* LChunkBuilder::DoIsConstructCallAndBranch(
    HIsConstructCallAndBranch* instr) {
  return new(zone()) LIsConstructCallAndBranch(TempRegister());
}


LInstruction* LChunkBuilder::DoSimulate(HSimulate* instr) {
  HEnvironment* env = current_block_->last_environment();
  ASSERT(env != NULL);

  env->set_ast_id(instr->ast_id());

  env->Drop(instr->pop_count());
  for (int i = 0; i < instr->values()->length(); ++i) {
    HValue* value = instr->values()->at(i);
    if (instr->HasAssignedIndexAt(i)) {
      env->Bind(instr->GetAssignedIndexAt(i), value);
    } else {
      env->Push(value);
    }
  }

  // If there is an instruction pending deoptimization environment create a
  // lazy bailout instruction to capture the environment.
  if (pending_deoptimization_ast_id_ == instr->ast_id()) {
    LLazyBailout* lazy_bailout = new(zone()) LLazyBailout;
    LInstruction* result = AssignEnvironment(lazy_bailout);
    instruction_pending_deoptimization_environment_->
        set_deoptimization_environment(result->environment());
    ClearInstructionPendingDeoptimizationEnvironment();
    return result;
  }

  return NULL;
}


LInstruction* LChunkBuilder::DoStackCheck(HStackCheck* instr) {
  if (instr->is_function_entry()) {
    return MarkAsCall(new(zone()) LStackCheck, instr);
  } else {
    ASSERT(instr->is_backwards_branch());
    return AssignEnvironment(AssignPointerMap(new(zone()) LStackCheck));
  }
}


LInstruction* LChunkBuilder::DoEnterInlined(HEnterInlined* instr) {
  HEnvironment* outer = current_block_->last_environment();
  HConstant* undefined = graph()->GetConstantUndefined();
  HEnvironment* inner = outer->CopyForInlining(instr->closure(),
                                               instr->arguments_count(),
                                               instr->function(),
                                               undefined,
                                               instr->call_kind(),
                                               instr->is_construct());
  if (instr->arguments() != NULL) {
    inner->Bind(instr->arguments(), graph()->GetArgumentsObject());
  }
  current_block_->UpdateEnvironment(inner);
  chunk_->AddInlinedClosure(instr->closure());
  return NULL;
}


LInstruction* LChunkBuilder::DoLeaveInlined(HLeaveInlined* instr) {
  HEnvironment* outer = current_block_->last_environment()->
      DiscardInlined(false);
  current_block_->UpdateEnvironment(outer);
  return NULL;
}


LInstruction* LChunkBuilder::DoIn(HIn* instr) {
  LOperand* key = UseOrConstantAtStart(instr->key());
  LOperand* object = UseOrConstantAtStart(instr->object());
  LIn* result = new(zone()) LIn(key, object);
  return MarkAsCall(DefineFixed(result, rax), instr);
}


LInstruction* LChunkBuilder::DoForInPrepareMap(HForInPrepareMap* instr) {
  LOperand* object = UseFixed(instr->enumerable(), rax);
  LForInPrepareMap* result = new(zone()) LForInPrepareMap(object);
  return MarkAsCall(DefineFixed(result, rax), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoForInCacheArray(HForInCacheArray* instr) {
  LOperand* map = UseRegister(instr->map());
  return AssignEnvironment(DefineAsRegister(
      new(zone()) LForInCacheArray(map)));
}


LInstruction* LChunkBuilder::DoCheckMapValue(HCheckMapValue* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* map = UseRegisterAtStart(instr->map());
  return AssignEnvironment(new(zone()) LCheckMapValue(value, map));
}


LInstruction* LChunkBuilder::DoLoadFieldByIndex(HLoadFieldByIndex* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* index = UseTempRegister(instr->index());
  return DefineSameAsFirst(new(zone()) LLoadFieldByIndex(object, index));
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
