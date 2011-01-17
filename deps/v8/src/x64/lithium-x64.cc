// Copyright 2011 the V8 project authors. All rights reserved.
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


void LInstruction::PrintTo(StringStream* stream) {
  stream->Add("%s ", this->Mnemonic());
  if (HasResult()) {
    PrintOutputOperandTo(stream);
  }

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
  for (int i = 0; i < I; i++) {
    stream->Add(i == 0 ? "= " : " ");
    inputs_.at(i)->PrintTo(stream);
  }
}


template<int R, int I, int T>
void LTemplateInstruction<R, I, T>::PrintOutputOperandTo(StringStream* stream) {
  if (this->HasResult()) {
    this->result()->PrintTo(stream);
    stream->Add(" ");
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
  input()->PrintTo(stream);
}


void LCmpIDAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  left()->PrintTo(stream);
  stream->Add(" %s ", Token::String(op()));
  right()->PrintTo(stream);
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsNullAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  input()->PrintTo(stream);
  stream->Add(is_strict() ? " === null" : " == null");
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsObjectAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_object(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsSmiAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_smi(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_instance_type(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasCachedArrayIndexAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_cached_array_index(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if class_of_test(");
  input()->PrintTo(stream);
  stream->Add(", \"%o\") then B%d else B%d",
              *hydrogen()->class_name(),
              true_block_id(),
              false_block_id());
}


void LTypeofIs::PrintDataTo(StringStream* stream) {
  input()->PrintTo(stream);
  stream->Add(" == \"%s\"", *hydrogen()->type_literal()->ToCString());
}


void LTypeofIsAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if typeof ");
  input()->PrintTo(stream);
  stream->Add(" == \"%s\" then B%d else B%d",
              *hydrogen()->type_literal()->ToCString(),
              true_block_id(), false_block_id());
}


void LCallConstantFunction::PrintDataTo(StringStream* stream) {
  stream->Add("#%d / ", arity());
}


void LUnaryMathOperation::PrintDataTo(StringStream* stream) {
  stream->Add("/%s ", hydrogen()->OpName());
  input()->PrintTo(stream);
}


void LLoadContextSlot::PrintDataTo(StringStream* stream) {
  stream->Add("(%d, %d)", context_chain_length(), slot_index());
}


void LCallKeyed::PrintDataTo(StringStream* stream) {
  stream->Add("[rcx] #%d / ", arity());
}


void LCallNamed::PrintDataTo(StringStream* stream) {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s #%d / ", *name_string, arity());
}


void LCallGlobal::PrintDataTo(StringStream* stream) {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s #%d / ", *name_string, arity());
}


void LCallKnownGlobal::PrintDataTo(StringStream* stream) {
  stream->Add("#%d / ", arity());
}


void LCallNew::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  input()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


void LClassOfTest::PrintDataTo(StringStream* stream) {
  stream->Add("= class_of_test(");
  input()->PrintTo(stream);
  stream->Add(", \"%o\")", *hydrogen()->class_name());
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


LOperand* LChunk::GetNextSpillSlot(bool is_double)  {
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
  HPhase phase("Mark empty blocks", this);
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    int first = block->first_instruction_index();
    int last = block->last_instruction_index();
    LInstruction* first_instr = instructions()->at(first);
    LInstruction* last_instr = instructions()->at(last);

    LLabel* label = LLabel::cast(first_instr);
    if (last_instr->IsGoto()) {
      LGoto* goto_instr = LGoto::cast(last_instr);
      if (!goto_instr->include_stack_check() &&
          label->IsRedundant() &&
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


void LStoreNamed::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStoreKeyed::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  stream->Add("] <- ");
  value()->PrintTo(stream);
}


int LChunk::AddInstruction(LInstruction* instr, HBasicBlock* block) {
  LGap* gap = new LGap(block);
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
  return index;
}


LConstantOperand* LChunk::DefineConstantOperand(HConstant* constant) {
  return LConstantOperand::Create(constant->id());
}


int LChunk::GetParameterStackSlot(int index) const {
  // The receiver is at index 0, the first parameter at index 1, so we
  // shift all parameter indexes down by the number of parameters, and
  // make sure they end up negative so they are distinguishable from
  // spill slots.
  int result = index - graph()->info()->scope()->num_parameters() - 1;
  ASSERT(result < 0);
  return result;
}

// A parameter relative to ebp in the arguments stub.
int LChunk::ParameterAt(int index) {
  ASSERT(-1 <= index);  // -1 is the receiver.
  return (1 + graph()->info()->scope()->num_parameters() - index) *
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
  chunk_ = new LChunk(graph());
  HPhase phase("Building chunk", chunk_);
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
    SmartPointer<char> debug_name = graph()->debug_name()->ToCString();
    PrintF("Aborting LChunk building in @\"%s\": ", *debug_name);
    va_list arguments;
    va_start(arguments, format);
    OS::VPrint(format, arguments);
    va_end(arguments);
    PrintF("\n");
  }
  status_ = ABORTED;
}


LRegister* LChunkBuilder::ToOperand(Register reg) {
  return LRegister::Create(Register::ToAllocationIndex(reg));
}


LUnallocated* LChunkBuilder::ToUnallocated(Register reg) {
  return new LUnallocated(LUnallocated::FIXED_REGISTER,
                          Register::ToAllocationIndex(reg));
}


LUnallocated* LChunkBuilder::ToUnallocated(XMMRegister reg) {
  return new LUnallocated(LUnallocated::FIXED_DOUBLE_REGISTER,
                          XMMRegister::ToAllocationIndex(reg));
}


LOperand* LChunkBuilder::UseFixed(HValue* value, Register fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseFixedDouble(HValue* value, XMMRegister reg) {
  return Use(value, ToUnallocated(reg));
}


LOperand* LChunkBuilder::UseRegister(HValue* value) {
  return Use(value, new LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LOperand* LChunkBuilder::UseRegisterAtStart(HValue* value) {
  return Use(value,
             new LUnallocated(LUnallocated::MUST_HAVE_REGISTER,
                              LUnallocated::USED_AT_START));
}


LOperand* LChunkBuilder::UseTempRegister(HValue* value) {
  return Use(value, new LUnallocated(LUnallocated::WRITABLE_REGISTER));
}


LOperand* LChunkBuilder::Use(HValue* value) {
  return Use(value, new LUnallocated(LUnallocated::NONE));
}


LOperand* LChunkBuilder::UseAtStart(HValue* value) {
  return Use(value, new LUnallocated(LUnallocated::NONE,
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


LOperand* LChunkBuilder::Use(HValue* value, LUnallocated* operand) {
  if (value->EmitAtUses()) {
    HInstruction* instr = HInstruction::cast(value);
    VisitInstruction(instr);
  }
  allocator_->RecordUse(value, operand);
  return operand;
}


template<int I, int T>
LInstruction* LChunkBuilder::Define(LTemplateInstruction<1, I, T>* instr,
                                    LUnallocated* result) {
  allocator_->RecordDefinition(current_instruction_, result);
  instr->set_result(result);
  return instr;
}


template<int I, int T>
LInstruction* LChunkBuilder::Define(LTemplateInstruction<1, I, T>* instr) {
  return Define(instr, new LUnallocated(LUnallocated::NONE));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineAsRegister(
    LTemplateInstruction<1, I, T>* instr) {
  return Define(instr, new LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineAsSpilled(
    LTemplateInstruction<1, I, T>* instr,
    int index) {
  return Define(instr, new LUnallocated(LUnallocated::FIXED_SLOT, index));
}


template<int I, int T>
LInstruction* LChunkBuilder::DefineSameAsFirst(
    LTemplateInstruction<1, I, T>* instr) {
  return Define(instr, new LUnallocated(LUnallocated::SAME_AS_FIRST_INPUT));
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
  instr->set_environment(CreateEnvironment(hydrogen_env));
  return instr;
}


LInstruction* LChunkBuilder::SetInstructionPendingDeoptimizationEnvironment(
    LInstruction* instr, int ast_id) {
  ASSERT(instructions_pending_deoptimization_environment_ == NULL);
  ASSERT(pending_deoptimization_ast_id_ == AstNode::kNoNumber);
  instructions_pending_deoptimization_environment_ = instr;
  pending_deoptimization_ast_id_ = ast_id;
  return instr;
}


void LChunkBuilder::ClearInstructionPendingDeoptimizationEnvironment() {
  instructions_pending_deoptimization_environment_ = NULL;
  pending_deoptimization_ast_id_ = AstNode::kNoNumber;
}


LInstruction* LChunkBuilder::MarkAsCall(LInstruction* instr,
                                        HInstruction* hinstr,
                                        CanDeoptimize can_deoptimize) {
  allocator_->MarkAsCall();
  instr = AssignPointerMap(instr);

  if (hinstr->HasSideEffects()) {
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
      (can_deoptimize == CAN_DEOPTIMIZE_EAGERLY) || !hinstr->HasSideEffects();
  if (needs_environment && !instr->HasEnvironment()) {
    instr = AssignEnvironment(instr);
  }

  return instr;
}


LInstruction* LChunkBuilder::MarkAsSaveDoubles(LInstruction* instr) {
  allocator_->MarkAsSaveDoubles();
  return instr;
}


LInstruction* LChunkBuilder::AssignPointerMap(LInstruction* instr) {
  ASSERT(!instr->HasPointerMap());
  instr->set_pointer_map(new LPointerMap(position_));
  return instr;
}


LUnallocated* LChunkBuilder::TempRegister() {
  LUnallocated* operand = new LUnallocated(LUnallocated::MUST_HAVE_REGISTER);
  allocator_->RecordTemporary(operand);
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(Register reg) {
  LUnallocated* operand = ToUnallocated(reg);
  allocator_->RecordTemporary(operand);
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(XMMRegister reg) {
  LUnallocated* operand = ToUnallocated(reg);
  allocator_->RecordTemporary(operand);
  return operand;
}


LInstruction* LChunkBuilder::DoBlockEntry(HBlockEntry* instr) {
  return new LLabel(instr->block());
}


LInstruction* LChunkBuilder::DoDeoptimize(HDeoptimize* instr) {
  return AssignEnvironment(new LDeoptimize);
}


LInstruction* LChunkBuilder::DoBit(Token::Value op,
                                   HBitwiseBinaryOperation* instr) {
  Abort("Unimplemented: %s", "DoBit");
  return NULL;
}


LInstruction* LChunkBuilder::DoArithmeticD(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  Abort("Unimplemented: %s", "DoArithmeticD");
  return NULL;
}


LInstruction* LChunkBuilder::DoArithmeticT(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  Abort("Unimplemented: %s", "DoArithmeticT");
  return NULL;
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
  allocator_->BeginInstruction();
  if (current->has_position()) position_ = current->position();
  LInstruction* instr = current->CompileToLithium(this);

  if (instr != NULL) {
    if (FLAG_stress_pointer_maps && !instr->HasPointerMap()) {
      instr = AssignPointerMap(instr);
    }
    if (FLAG_stress_environments && !instr->HasEnvironment()) {
      instr = AssignEnvironment(instr);
    }
    if (current->IsBranch()) {
      instr->set_hydrogen_value(HBranch::cast(current)->value());
    } else {
      instr->set_hydrogen_value(current);
    }

    int index = chunk_->AddInstruction(instr, current_block_);
    allocator_->SummarizeInstruction(index);
  } else {
    // This instruction should be omitted.
    allocator_->OmitInstruction();
  }
  current_instruction_ = old_current;
}


LEnvironment* LChunkBuilder::CreateEnvironment(HEnvironment* hydrogen_env) {
  if (hydrogen_env == NULL) return NULL;

  LEnvironment* outer = CreateEnvironment(hydrogen_env->outer());
  int ast_id = hydrogen_env->ast_id();
  ASSERT(ast_id != AstNode::kNoNumber);
  int value_count = hydrogen_env->length();
  LEnvironment* result = new LEnvironment(hydrogen_env->closure(),
                                          ast_id,
                                          hydrogen_env->parameter_count(),
                                          argument_count_,
                                          value_count,
                                          outer);
  int argument_index = 0;
  for (int i = 0; i < value_count; ++i) {
    HValue* value = hydrogen_env->values()->at(i);
    LOperand* op = NULL;
    if (value->IsArgumentsObject()) {
      op = NULL;
    } else if (value->IsPushArgument()) {
      op = new LArgument(argument_index++);
    } else {
      op = UseOrConstant(value);
      if (op->IsUnallocated()) {
        LUnallocated* unalloc = LUnallocated::cast(op);
        unalloc->set_policy(LUnallocated::ANY);
      }
    }
    result->AddValue(op, value->representation());
  }

  return result;
}


LInstruction* LChunkBuilder::DoGoto(HGoto* instr) {
  LGoto* result = new LGoto(instr->FirstSuccessor()->block_id(),
                            instr->include_stack_check());
  return (instr->include_stack_check())
      ? AssignPointerMap(result)
      : result;
}


LInstruction* LChunkBuilder::DoBranch(HBranch* instr) {
  Abort("Unimplemented: %s", "DoBranch");
  return NULL;
}


LInstruction* LChunkBuilder::DoCompareMapAndBranch(
    HCompareMapAndBranch* instr) {
  Abort("Unimplemented: %s", "DoCompareMapAndBranch");
  return NULL;
}


LInstruction* LChunkBuilder::DoArgumentsLength(HArgumentsLength* length) {
  Abort("Unimplemented: %s", "DoArgumentsLength");
  return NULL;
}


LInstruction* LChunkBuilder::DoArgumentsElements(HArgumentsElements* elems) {
  Abort("Unimplemented: %s", "DoArgumentsElements");
  return NULL;
}


LInstruction* LChunkBuilder::DoInstanceOf(HInstanceOf* instr) {
  Abort("Unimplemented: %s", "DoInstanceOf");
  return NULL;
}


LInstruction* LChunkBuilder::DoInstanceOfKnownGlobal(
    HInstanceOfKnownGlobal* instr) {
  Abort("Unimplemented: %s", "DoInstanceOfKnownGlobal");
  return NULL;
}


LInstruction* LChunkBuilder::DoApplyArguments(HApplyArguments* instr) {
  Abort("Unimplemented: %s", "DoApplyArguments");
  return NULL;
}


LInstruction* LChunkBuilder::DoPushArgument(HPushArgument* instr) {
  Abort("Unimplemented: %s", "DoPushArgument");
  return NULL;
}


LInstruction* LChunkBuilder::DoGlobalObject(HGlobalObject* instr) {
  Abort("Unimplemented: %s", "DoGlobalObject");
  return NULL;
}


LInstruction* LChunkBuilder::DoGlobalReceiver(HGlobalReceiver* instr) {
  Abort("Unimplemented: %s", "DoGlobalReceiver");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallConstantFunction(
    HCallConstantFunction* instr) {
  Abort("Unimplemented: %s", "DoCallConstantFunction");
  return NULL;
}


LInstruction* LChunkBuilder::DoUnaryMathOperation(HUnaryMathOperation* instr) {
  Abort("Unimplemented: %s", "DoUnaryMathOperation");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallKeyed(HCallKeyed* instr) {
  Abort("Unimplemented: %s", "DoCallKeyed");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallNamed(HCallNamed* instr) {
  Abort("Unimplemented: %s", "DoCallNamed");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallGlobal(HCallGlobal* instr) {
  Abort("Unimplemented: %s", "DoCallGlobal");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallKnownGlobal(HCallKnownGlobal* instr) {
  Abort("Unimplemented: %s", "DoCallKnownGlobal");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallNew(HCallNew* instr) {
  Abort("Unimplemented: %s", "DoCallNew");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallFunction(HCallFunction* instr) {
  Abort("Unimplemented: %s", "DoCallFunction");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallRuntime(HCallRuntime* instr) {
  Abort("Unimplemented: %s", "DoCallRuntime");
  return NULL;
}


LInstruction* LChunkBuilder::DoShr(HShr* instr) {
  Abort("Unimplemented: %s", "DoShr");
  return NULL;
}


LInstruction* LChunkBuilder::DoSar(HSar* instr) {
  Abort("Unimplemented: %s", "DoSar");
  return NULL;
}


LInstruction* LChunkBuilder::DoShl(HShl* instr) {
  Abort("Unimplemented: %s", "DoShl");
  return NULL;
}


LInstruction* LChunkBuilder::DoBitAnd(HBitAnd* instr) {
  Abort("Unimplemented: %s", "DoBitAnd");
  return NULL;
}


LInstruction* LChunkBuilder::DoBitNot(HBitNot* instr) {
  Abort("Unimplemented: %s", "DoBitNot");
  return NULL;
}


LInstruction* LChunkBuilder::DoBitOr(HBitOr* instr) {
  Abort("Unimplemented: %s", "DoBitOr");
  return NULL;
}


LInstruction* LChunkBuilder::DoBitXor(HBitXor* instr) {
  Abort("Unimplemented: %s", "DoBitXor");
  return NULL;
}


LInstruction* LChunkBuilder::DoDiv(HDiv* instr) {
  Abort("Unimplemented: %s", "DoDiv");
  return NULL;
}


LInstruction* LChunkBuilder::DoMod(HMod* instr) {
  Abort("Unimplemented: %s", "DoMod");
  return NULL;
}


LInstruction* LChunkBuilder::DoMul(HMul* instr) {
  Abort("Unimplemented: %s", "DoMul");
  return NULL;
}


LInstruction* LChunkBuilder::DoSub(HSub* instr) {
  Abort("Unimplemented: %s", "DoSub");
  return NULL;
}


LInstruction* LChunkBuilder::DoAdd(HAdd* instr) {
  Abort("Unimplemented: %s", "DoAdd");
  return NULL;
}


LInstruction* LChunkBuilder::DoPower(HPower* instr) {
  Abort("Unimplemented: %s", "DoPower");
  return NULL;
}


LInstruction* LChunkBuilder::DoCompare(HCompare* instr) {
  Abort("Unimplemented: %s", "DoCompare");
  return NULL;
}


LInstruction* LChunkBuilder::DoCompareJSObjectEq(
    HCompareJSObjectEq* instr) {
  Abort("Unimplemented: %s", "DoCompareJSObjectEq");
  return NULL;
}


LInstruction* LChunkBuilder::DoIsNull(HIsNull* instr) {
  Abort("Unimplemented: %s", "DoIsNull");
  return NULL;
}


LInstruction* LChunkBuilder::DoIsObject(HIsObject* instr) {
  Abort("Unimplemented: %s", "DoIsObject");
  return NULL;
}


LInstruction* LChunkBuilder::DoIsSmi(HIsSmi* instr) {
  Abort("Unimplemented: %s", "DoIsSmi");
  return NULL;
}


LInstruction* LChunkBuilder::DoHasInstanceType(HHasInstanceType* instr) {
  Abort("Unimplemented: %s", "DoHasInstanceType");
  return NULL;
}


LInstruction* LChunkBuilder::DoHasCachedArrayIndex(
    HHasCachedArrayIndex* instr) {
  Abort("Unimplemented: %s", "DoHasCachedArrayIndex");
  return NULL;
}


LInstruction* LChunkBuilder::DoClassOfTest(HClassOfTest* instr) {
  Abort("Unimplemented: %s", "DoClassOfTest");
  return NULL;
}


LInstruction* LChunkBuilder::DoJSArrayLength(HJSArrayLength* instr) {
  Abort("Unimplemented: %s", "DoJSArrayLength");
  return NULL;
}


LInstruction* LChunkBuilder::DoFixedArrayLength(HFixedArrayLength* instr) {
  Abort("Unimplemented: %s", "DoFixedArrayLength");
  return NULL;
}


LInstruction* LChunkBuilder::DoValueOf(HValueOf* instr) {
  Abort("Unimplemented: %s", "DoValueOf");
  return NULL;
}


LInstruction* LChunkBuilder::DoBoundsCheck(HBoundsCheck* instr) {
  Abort("Unimplemented: %s", "DoBoundsCheck");
  return NULL;
}


LInstruction* LChunkBuilder::DoThrow(HThrow* instr) {
  Abort("Unimplemented: %s", "DoThrow");
  return NULL;
}


LInstruction* LChunkBuilder::DoChange(HChange* instr) {
  Abort("Unimplemented: %s", "DoChange");
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckNonSmi(HCheckNonSmi* instr) {
  Abort("Unimplemented: %s", "DoCheckNonSmi");
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckInstanceType(HCheckInstanceType* instr) {
  Abort("Unimplemented: %s", "DoCheckInstanceType");
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckPrototypeMaps(HCheckPrototypeMaps* instr) {
  Abort("Unimplemented: %s", "DoCheckPrototypeMaps");
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckSmi(HCheckSmi* instr) {
  Abort("Unimplemented: %s", "DoCheckSmi");
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckFunction(HCheckFunction* instr) {
  Abort("Unimplemented: %s", "DoCheckFunction");
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckMap(HCheckMap* instr) {
  Abort("Unimplemented: %s", "DoCheckMap");
  return NULL;
}


LInstruction* LChunkBuilder::DoReturn(HReturn* instr) {
  return new LReturn(UseFixed(instr->value(), rax));
}


LInstruction* LChunkBuilder::DoConstant(HConstant* instr) {
  Representation r = instr->representation();
  if (r.IsInteger32()) {
    int32_t value = instr->Integer32Value();
    return DefineAsRegister(new LConstantI(value));
  } else if (r.IsDouble()) {
    double value = instr->DoubleValue();
    return DefineAsRegister(new LConstantD(value));
  } else if (r.IsTagged()) {
    return DefineAsRegister(new LConstantT(instr->handle()));
  } else {
    UNREACHABLE();
    return NULL;
  }
}


LInstruction* LChunkBuilder::DoLoadGlobal(HLoadGlobal* instr) {
  Abort("Unimplemented: %s", "DoLoadGlobal");
  return NULL;
}


LInstruction* LChunkBuilder::DoStoreGlobal(HStoreGlobal* instr) {
  Abort("Unimplemented: %s", "DoStoreGlobal");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadContextSlot(HLoadContextSlot* instr) {
  Abort("Unimplemented: %s", "DoLoadContextSlot");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadNamedField(HLoadNamedField* instr) {
  Abort("Unimplemented: %s", "DoLoadNamedField");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadNamedGeneric(HLoadNamedGeneric* instr) {
  Abort("Unimplemented: %s", "DoLoadNamedGeneric");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadFunctionPrototype(
    HLoadFunctionPrototype* instr) {
  Abort("Unimplemented: %s", "DoLoadFunctionPrototype");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadElements(HLoadElements* instr) {
  Abort("Unimplemented: %s", "DoLoadElements");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadKeyedFastElement(
    HLoadKeyedFastElement* instr) {
  Abort("Unimplemented: %s", "DoLoadKeyedFastElement");
  return NULL;
}


LInstruction* LChunkBuilder::DoLoadKeyedGeneric(HLoadKeyedGeneric* instr) {
  Abort("Unimplemented: %s", "DoLoadKeyedGeneric");
  return NULL;
}


LInstruction* LChunkBuilder::DoStoreKeyedFastElement(
    HStoreKeyedFastElement* instr) {
  Abort("Unimplemented: %s", "DoStoreKeyedFastElement");
  return NULL;
}


LInstruction* LChunkBuilder::DoStoreKeyedGeneric(HStoreKeyedGeneric* instr) {
  Abort("Unimplemented: %s", "DoStoreKeyedGeneric");
  return NULL;
}


LInstruction* LChunkBuilder::DoStoreNamedField(HStoreNamedField* instr) {
  Abort("Unimplemented: %s", "DoStoreNamedField");
  return NULL;
}


LInstruction* LChunkBuilder::DoStoreNamedGeneric(HStoreNamedGeneric* instr) {
  Abort("Unimplemented: %s", "DoStoreNamedGeneric");
  return NULL;
}


LInstruction* LChunkBuilder::DoArrayLiteral(HArrayLiteral* instr) {
  Abort("Unimplemented: %s", "DoArrayLiteral");
  return NULL;
}


LInstruction* LChunkBuilder::DoObjectLiteral(HObjectLiteral* instr) {
  Abort("Unimplemented: %s", "DoObjectLiteral");
  return NULL;
}


LInstruction* LChunkBuilder::DoRegExpLiteral(HRegExpLiteral* instr) {
  Abort("Unimplemented: %s", "DoRegExpLiteral");
  return NULL;
}


LInstruction* LChunkBuilder::DoFunctionLiteral(HFunctionLiteral* instr) {
  Abort("Unimplemented: %s", "DoFunctionLiteral");
  return NULL;
}


LInstruction* LChunkBuilder::DoDeleteProperty(HDeleteProperty* instr) {
  Abort("Unimplemented: %s", "DoDeleteProperty");
  return NULL;
}


LInstruction* LChunkBuilder::DoOsrEntry(HOsrEntry* instr) {
  Abort("Unimplemented: %s", "DoOsrEntry");
  return NULL;
}


LInstruction* LChunkBuilder::DoParameter(HParameter* instr) {
  int spill_index = chunk()->GetParameterStackSlot(instr->index());
  return DefineAsSpilled(new LParameter, spill_index);
}


LInstruction* LChunkBuilder::DoUnknownOSRValue(HUnknownOSRValue* instr) {
  Abort("Unimplemented: %s", "DoUnknownOSRValue");
  return NULL;
}


LInstruction* LChunkBuilder::DoCallStub(HCallStub* instr) {
  Abort("Unimplemented: %s", "DoCallStub");
  return NULL;
}


LInstruction* LChunkBuilder::DoArgumentsObject(HArgumentsObject* instr) {
  Abort("Unimplemented: %s", "DoArgumentsObject");
  return NULL;
}


LInstruction* LChunkBuilder::DoAccessArgumentsAt(HAccessArgumentsAt* instr) {
  Abort("Unimplemented: %s", "DoAccessArgumentsAt");
  return NULL;
}


LInstruction* LChunkBuilder::DoTypeof(HTypeof* instr) {
  Abort("Unimplemented: %s", "DoTypeof");
  return NULL;
}


LInstruction* LChunkBuilder::DoTypeofIs(HTypeofIs* instr) {
  Abort("Unimplemented: %s", "DoTypeofIs");
  return NULL;
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
  ASSERT(env->length() == instr->environment_length());

  // If there is an instruction pending deoptimization environment create a
  // lazy bailout instruction to capture the environment.
  if (pending_deoptimization_ast_id_ == instr->ast_id()) {
    LLazyBailout* lazy_bailout = new LLazyBailout;
    LInstruction* result = AssignEnvironment(lazy_bailout);
    instructions_pending_deoptimization_environment_->
        set_deoptimization_environment(result->environment());
    ClearInstructionPendingDeoptimizationEnvironment();
    return result;
  }

  return NULL;
}


LInstruction* LChunkBuilder::DoStackCheck(HStackCheck* instr) {
  return MarkAsCall(new LStackCheck, instr);
}


LInstruction* LChunkBuilder::DoEnterInlined(HEnterInlined* instr) {
  Abort("Unimplemented: %s", "DoEnterInlined");
  return NULL;
}


LInstruction* LChunkBuilder::DoLeaveInlined(HLeaveInlined* instr) {
  Abort("Unimplemented: %s", "DoLeaveInlined");
  return NULL;
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
