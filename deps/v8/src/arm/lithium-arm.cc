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

#include "arm/lithium-arm.h"
#include "arm/lithium-codegen-arm.h"

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


void LInstruction::PrintTo(StringStream* stream) const {
  stream->Add("%s ", this->Mnemonic());
  if (HasResult()) {
    result()->PrintTo(stream);
    stream->Add(" ");
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


void LLabel::PrintDataTo(StringStream* stream) const {
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


void LGap::PrintDataTo(StringStream* stream) const {
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



void LBinaryOperation::PrintDataTo(StringStream* stream) const {
  stream->Add("= ");
  left()->PrintTo(stream);
  stream->Add(" ");
  right()->PrintTo(stream);
}


void LGoto::PrintDataTo(StringStream* stream) const {
  stream->Add("B%d", block_id());
}


void LBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("B%d | B%d on ", true_block_id(), false_block_id());
  input()->PrintTo(stream);
}


void LCmpIDAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if ");
  left()->PrintTo(stream);
  stream->Add(" %s ", Token::String(op()));
  right()->PrintTo(stream);
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsNullAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if ");
  input()->PrintTo(stream);
  stream->Add(is_strict() ? " === null" : " == null");
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsObjectAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if is_object(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsSmiAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if is_smi(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if has_instance_type(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasCachedArrayIndexAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if has_cached_array_index(");
  input()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LClassOfTestAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if class_of_test(");
  input()->PrintTo(stream);
  stream->Add(", \"%o\") then B%d else B%d",
              *hydrogen()->class_name(),
              true_block_id(),
              false_block_id());
}


void LTypeofIs::PrintDataTo(StringStream* stream) const {
  input()->PrintTo(stream);
  stream->Add(" == \"%s\"", *hydrogen()->type_literal()->ToCString());
}


void LTypeofIsAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("if typeof ");
  input()->PrintTo(stream);
  stream->Add(" == \"%s\" then B%d else B%d",
              *hydrogen()->type_literal()->ToCString(),
              true_block_id(), false_block_id());
}


void LCallConstantFunction::PrintDataTo(StringStream* stream) const {
  stream->Add("#%d / ", arity());
}


void LUnaryMathOperation::PrintDataTo(StringStream* stream) const {
  stream->Add("/%s ", hydrogen()->OpName());
  input()->PrintTo(stream);
}


void LLoadContextSlot::PrintDataTo(StringStream* stream) {
  stream->Add("(%d, %d)", context_chain_length(), slot_index());
}


void LCallKeyed::PrintDataTo(StringStream* stream) const {
  stream->Add("[r2] #%d / ", arity());
}


void LCallNamed::PrintDataTo(StringStream* stream) const {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s #%d / ", *name_string, arity());
}


void LCallGlobal::PrintDataTo(StringStream* stream) const {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s #%d / ", *name_string, arity());
}


void LCallKnownGlobal::PrintDataTo(StringStream* stream) const {
  stream->Add("#%d / ", arity());
}


void LCallNew::PrintDataTo(StringStream* stream) const {
  LUnaryOperation::PrintDataTo(stream);
  stream->Add(" #%d / ", arity());
}


void LClassOfTest::PrintDataTo(StringStream* stream) const {
  stream->Add("= class_of_test(");
  input()->PrintTo(stream);
  stream->Add(", \"%o\")", *hydrogen()->class_name());
}


void LUnaryOperation::PrintDataTo(StringStream* stream) const {
  stream->Add("= ");
  input()->PrintTo(stream);
}


void LAccessArgumentsAt::PrintDataTo(StringStream* stream) const {
  arguments()->PrintTo(stream);

  stream->Add(" length ");
  length()->PrintTo(stream);

  stream->Add(" index ");
  index()->PrintTo(stream);
}


LChunk::LChunk(HGraph* graph)
    : spill_slot_count_(0),
      graph_(graph),
      instructions_(32),
      pointer_maps_(8),
      inlined_closures_(1) {
}


void LChunk::Verify() const {
  // TODO(twuerthinger): Implement verification for chunk.
}


int LChunk::GetNextSpillIndex(bool is_double) {
  // Skip a slot if for a double-width slot.
  if (is_double) spill_slot_count_++;
  return spill_slot_count_++;
}


LOperand* LChunk::GetNextSpillSlot(bool is_double)  {
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


void LStoreNamed::PrintDataTo(StringStream* stream) const {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStoreKeyed::PrintDataTo(StringStream* stream) const {
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


LUnallocated* LChunkBuilder::ToUnallocated(DoubleRegister reg) {
  return new LUnallocated(LUnallocated::FIXED_DOUBLE_REGISTER,
                          DoubleRegister::ToAllocationIndex(reg));
}


LOperand* LChunkBuilder::UseFixed(HValue* value, Register fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseFixedDouble(HValue* value, DoubleRegister reg) {
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


LInstruction* LChunkBuilder::Define(LInstruction* instr) {
  return Define(instr, new LUnallocated(LUnallocated::NONE));
}


LInstruction* LChunkBuilder::DefineAsRegister(LInstruction* instr) {
  return Define(instr, new LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LInstruction* LChunkBuilder::DefineAsSpilled(LInstruction* instr, int index) {
  return Define(instr, new LUnallocated(LUnallocated::FIXED_SLOT, index));
}


LInstruction* LChunkBuilder::DefineSameAsFirst(LInstruction* instr) {
  return Define(instr, new LUnallocated(LUnallocated::SAME_AS_FIRST_INPUT));
}


LInstruction* LChunkBuilder::DefineFixed(LInstruction* instr, Register reg) {
  return Define(instr, ToUnallocated(reg));
}


LInstruction* LChunkBuilder::DefineFixedDouble(LInstruction* instr,
                                               DoubleRegister reg) {
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


LInstruction* LChunkBuilder::AssignPointerMap(LInstruction* instr) {
  ASSERT(!instr->HasPointerMap());
  instr->set_pointer_map(new LPointerMap(position_));
  return instr;
}


LInstruction* LChunkBuilder::Define(LInstruction* instr, LUnallocated* result) {
  allocator_->RecordDefinition(current_instruction_, result);
  instr->set_result(result);
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


LOperand* LChunkBuilder::FixedTemp(DoubleRegister reg) {
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
  ASSERT(instr->representation().IsInteger32());
  ASSERT(instr->left()->representation().IsInteger32());
  ASSERT(instr->right()->representation().IsInteger32());

  LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
  LOperand* right = UseOrConstantAtStart(instr->MostConstantOperand());
  return DefineSameAsFirst(new LBitI(op, left, right));
}


LInstruction* LChunkBuilder::DoShift(Token::Value op,
                                     HBitwiseBinaryOperation* instr) {
  ASSERT(instr->representation().IsInteger32());
  ASSERT(instr->OperandAt(0)->representation().IsInteger32());
  ASSERT(instr->OperandAt(1)->representation().IsInteger32());
  LOperand* left = UseRegisterAtStart(instr->OperandAt(0));

  HValue* right_value = instr->OperandAt(1);
  LOperand* right = NULL;
  int constant_value = 0;
  if (right_value->IsConstant()) {
    HConstant* constant = HConstant::cast(right_value);
    right = chunk_->DefineConstantOperand(constant);
    constant_value = constant->Integer32Value() & 0x1f;
  } else {
    right = UseRegister(right_value);
  }

  // Shift operations can only deoptimize if we do a logical shift
  // by 0 and the result cannot be truncated to int32.
  bool can_deopt = (op == Token::SHR && constant_value == 0);
  if (can_deopt) {
    bool can_truncate = true;
    for (int i = 0; i < instr->uses()->length(); i++) {
      if (!instr->uses()->at(i)->CheckFlag(HValue::kTruncatingToInt32)) {
        can_truncate = false;
        break;
      }
    }
    can_deopt = !can_truncate;
  }

  LInstruction* result =
      DefineSameAsFirst(new LShiftI(op, left, right, can_deopt));
  if (can_deopt) AssignEnvironment(result);
  return result;
}


LInstruction* LChunkBuilder::DoArithmeticD(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->left()->representation().IsDouble());
  ASSERT(instr->right()->representation().IsDouble());
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterAtStart(instr->right());
  LArithmeticD* result = new LArithmeticD(op, left, right);
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
  LOperand* left_operand = UseFixed(left, r1);
  LOperand* right_operand = UseFixed(right, r0);
  LInstruction* result = new LArithmeticT(op, left_operand, right_operand);
  return MarkAsCall(DefineFixed(result, r0), instr);
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
  LInstruction* result = new LGoto(instr->FirstSuccessor()->block_id(),
                                   instr->include_stack_check());
  if (instr->include_stack_check())  result = AssignPointerMap(result);
  return result;
}


LInstruction* LChunkBuilder::DoBranch(HBranch* instr) {
  HValue* v = instr->value();
  HBasicBlock* first = instr->FirstSuccessor();
  HBasicBlock* second = instr->SecondSuccessor();
  ASSERT(first != NULL && second != NULL);
  int first_id = first->block_id();
  int second_id = second->block_id();

  if (v->EmitAtUses()) {
    if (v->IsClassOfTest()) {
      HClassOfTest* compare = HClassOfTest::cast(v);
      ASSERT(compare->value()->representation().IsTagged());

      return new LClassOfTestAndBranch(UseTempRegister(compare->value()),
                                       TempRegister(),
                                       first_id,
                                       second_id);
    } else if (v->IsCompare()) {
      HCompare* compare = HCompare::cast(v);
      Token::Value op = compare->token();
      HValue* left = compare->left();
      HValue* right = compare->right();
      Representation r = compare->GetInputRepresentation();
      if (r.IsInteger32()) {
        ASSERT(left->representation().IsInteger32());
        ASSERT(right->representation().IsInteger32());
        return new LCmpIDAndBranch(UseRegisterAtStart(left),
                                   UseOrConstantAtStart(right),
                                   first_id,
                                   second_id);
      } else if (r.IsDouble()) {
        ASSERT(left->representation().IsDouble());
        ASSERT(right->representation().IsDouble());
        return new LCmpIDAndBranch(UseRegisterAtStart(left),
                                   UseRegisterAtStart(right),
                                   first_id,
                                   second_id);
      } else {
        ASSERT(left->representation().IsTagged());
        ASSERT(right->representation().IsTagged());
        bool reversed = op == Token::GT || op == Token::LTE;
        LOperand* left_operand = UseFixed(left, reversed ? r0 : r1);
        LOperand* right_operand = UseFixed(right, reversed ? r1 : r0);
        LInstruction* result = new LCmpTAndBranch(left_operand,
                                                  right_operand,
                                                  first_id,
                                                  second_id);
        return MarkAsCall(result, instr);
      }
    } else if (v->IsIsSmi()) {
      HIsSmi* compare = HIsSmi::cast(v);
      ASSERT(compare->value()->representation().IsTagged());

      return new LIsSmiAndBranch(Use(compare->value()),
                                 first_id,
                                 second_id);
    } else if (v->IsHasInstanceType()) {
      HHasInstanceType* compare = HHasInstanceType::cast(v);
      ASSERT(compare->value()->representation().IsTagged());

      return new LHasInstanceTypeAndBranch(UseRegisterAtStart(compare->value()),
                                           first_id,
                                           second_id);
    } else if (v->IsHasCachedArrayIndex()) {
      HHasCachedArrayIndex* compare = HHasCachedArrayIndex::cast(v);
      ASSERT(compare->value()->representation().IsTagged());

      return new LHasCachedArrayIndexAndBranch(
          UseRegisterAtStart(compare->value()), first_id, second_id);
    } else if (v->IsIsNull()) {
      HIsNull* compare = HIsNull::cast(v);
      ASSERT(compare->value()->representation().IsTagged());

      return new LIsNullAndBranch(UseRegisterAtStart(compare->value()),
                                  first_id,
                                  second_id);
    } else if (v->IsIsObject()) {
      HIsObject* compare = HIsObject::cast(v);
      ASSERT(compare->value()->representation().IsTagged());

      LOperand* temp1 = TempRegister();
      LOperand* temp2 = TempRegister();
      return new LIsObjectAndBranch(UseRegisterAtStart(compare->value()),
                                    temp1,
                                    temp2,
                                    first_id,
                                    second_id);
    } else if (v->IsCompareJSObjectEq()) {
      HCompareJSObjectEq* compare = HCompareJSObjectEq::cast(v);
      return new LCmpJSObjectEqAndBranch(UseRegisterAtStart(compare->left()),
                                         UseRegisterAtStart(compare->right()),
                                         first_id,
                                         second_id);
    } else if (v->IsInstanceOf()) {
      HInstanceOf* instance_of = HInstanceOf::cast(v);
      LInstruction* result =
          new LInstanceOfAndBranch(Use(instance_of->left()),
                                   Use(instance_of->right()),
                                   first_id,
                                   second_id);
      return MarkAsCall(result, instr);
    } else if (v->IsTypeofIs()) {
      HTypeofIs* typeof_is = HTypeofIs::cast(v);
      return new LTypeofIsAndBranch(UseTempRegister(typeof_is->value()),
                                    first_id,
                                    second_id);
    } else {
      if (v->IsConstant()) {
        if (HConstant::cast(v)->handle()->IsTrue()) {
          return new LGoto(first_id);
        } else if (HConstant::cast(v)->handle()->IsFalse()) {
          return new LGoto(second_id);
        }
      }
      Abort("Undefined compare before branch");
      return NULL;
    }
  }
  return new LBranch(UseRegisterAtStart(v), first_id, second_id);
}


LInstruction* LChunkBuilder::DoCompareMapAndBranch(
    HCompareMapAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  return new LCmpMapAndBranch(value, temp);
}


LInstruction* LChunkBuilder::DoArgumentsLength(HArgumentsLength* length) {
  return DefineAsRegister(new LArgumentsLength(UseRegister(length->value())));
}


LInstruction* LChunkBuilder::DoArgumentsElements(HArgumentsElements* elems) {
  return DefineAsRegister(new LArgumentsElements);
}


LInstruction* LChunkBuilder::DoInstanceOf(HInstanceOf* instr) {
  LInstruction* result =
      new LInstanceOf(UseFixed(instr->left(), r0),
                      UseFixed(instr->right(), r1));
  return MarkAsCall(DefineFixed(result, r0), instr);
}


LInstruction* LChunkBuilder::DoInstanceOfKnownGlobal(
    HInstanceOfKnownGlobal* instr) {
  LInstruction* result =
      new LInstanceOfKnownGlobal(UseFixed(instr->value(), r0));
  return MarkAsCall(DefineFixed(result, r0), instr);
}


LInstruction* LChunkBuilder::DoApplyArguments(HApplyArguments* instr) {
  LOperand* function = UseFixed(instr->function(), r1);
  LOperand* receiver = UseFixed(instr->receiver(), r0);
  LOperand* length = UseRegisterAtStart(instr->length());
  LOperand* elements = UseRegisterAtStart(instr->elements());
  LInstruction* result = new LApplyArguments(function,
                                             receiver,
                                             length,
                                             elements);
  return MarkAsCall(DefineFixed(result, r0), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoPushArgument(HPushArgument* instr) {
  ++argument_count_;
  LOperand* argument = Use(instr->argument());
  return new LPushArgument(argument);
}


LInstruction* LChunkBuilder::DoGlobalObject(HGlobalObject* instr) {
  return DefineAsRegister(new LGlobalObject);
}


LInstruction* LChunkBuilder::DoGlobalReceiver(HGlobalReceiver* instr) {
  return DefineAsRegister(new LGlobalReceiver);
}


LInstruction* LChunkBuilder::DoCallConstantFunction(
    HCallConstantFunction* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallConstantFunction, r0), instr);
}


LInstruction* LChunkBuilder::DoUnaryMathOperation(HUnaryMathOperation* instr) {
  BuiltinFunctionId op = instr->op();
  LOperand* input = UseRegisterAtStart(instr->value());
  LOperand* temp = (op == kMathFloor) ? TempRegister() : NULL;
  LInstruction* result = new LUnaryMathOperation(input, temp);
  switch (op) {
    case kMathAbs:
      return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
    case kMathFloor:
      return AssignEnvironment(DefineAsRegister(result));
    case kMathSqrt:
      return DefineSameAsFirst(result);
    case kMathRound:
      Abort("MathRound LUnaryMathOperation not implemented");
      return NULL;
    case kMathPowHalf:
      Abort("MathPowHalf LUnaryMathOperation not implemented");
      return NULL;
    case kMathLog:
      Abort("MathLog LUnaryMathOperation not implemented");
      return NULL;
    case kMathCos:
      Abort("MathCos LUnaryMathOperation not implemented");
      return NULL;
    case kMathSin:
      Abort("MathSin LUnaryMathOperation not implemented");
      return NULL;
    default:
      UNREACHABLE();
      return NULL;
  }
}


LInstruction* LChunkBuilder::DoCallKeyed(HCallKeyed* instr) {
  ASSERT(instr->key()->representation().IsTagged());
  argument_count_ -= instr->argument_count();
  UseFixed(instr->key(), r2);
  return MarkAsCall(DefineFixed(new LCallKeyed, r0), instr);
}


LInstruction* LChunkBuilder::DoCallNamed(HCallNamed* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallNamed, r0), instr);
}


LInstruction* LChunkBuilder::DoCallGlobal(HCallGlobal* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallGlobal, r0), instr);
}


LInstruction* LChunkBuilder::DoCallKnownGlobal(HCallKnownGlobal* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallKnownGlobal, r0), instr);
}


LInstruction* LChunkBuilder::DoCallNew(HCallNew* instr) {
  LOperand* constructor = UseFixed(instr->constructor(), r1);
  argument_count_ -= instr->argument_count();
  LInstruction* result = new LCallNew(constructor);
  return MarkAsCall(DefineFixed(result, r0), instr);
}


LInstruction* LChunkBuilder::DoCallFunction(HCallFunction* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallFunction, r0), instr);
}


LInstruction* LChunkBuilder::DoCallRuntime(HCallRuntime* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallRuntime, r0), instr);
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


LInstruction* LChunkBuilder::DoBitAnd(HBitAnd* instr) {
  return DoBit(Token::BIT_AND, instr);
}


LInstruction* LChunkBuilder::DoBitNot(HBitNot* instr) {
  ASSERT(instr->value()->representation().IsInteger32());
  ASSERT(instr->representation().IsInteger32());
  return DefineSameAsFirst(new LBitNotI(UseRegisterAtStart(instr->value())));
}


LInstruction* LChunkBuilder::DoBitOr(HBitOr* instr) {
  return DoBit(Token::BIT_OR, instr);
}


LInstruction* LChunkBuilder::DoBitXor(HBitXor* instr) {
  return DoBit(Token::BIT_XOR, instr);
}


LInstruction* LChunkBuilder::DoDiv(HDiv* instr) {
  if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::DIV, instr);
  } else if (instr->representation().IsInteger32()) {
    // TODO(1042) The fixed register allocation
    // is needed because we call GenericBinaryOpStub from
    // the generated code, which requires registers r0
    // and r1 to be used. We should remove that
    // when we provide a native implementation.
    LOperand* value = UseFixed(instr->left(), r0);
    LOperand* divisor = UseFixed(instr->right(), r1);
    return AssignEnvironment(AssignPointerMap(
             DefineFixed(new LDivI(value, divisor), r0)));
  } else {
    return DoArithmeticT(Token::DIV, instr);
  }
}


LInstruction* LChunkBuilder::DoMod(HMod* instr) {
  if (instr->representation().IsInteger32()) {
    // TODO(1042) The fixed register allocation
    // is needed because we call GenericBinaryOpStub from
    // the generated code, which requires registers r0
    // and r1 to be used. We should remove that
    // when we provide a native implementation.
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* value = UseFixed(instr->left(), r0);
    LOperand* divisor = UseFixed(instr->right(), r1);
    LInstruction* result = DefineFixed(new LModI(value, divisor), r0);
    result = AssignEnvironment(AssignPointerMap(result));
    return result;
  } else if (instr->representation().IsTagged()) {
    return DoArithmeticT(Token::MOD, instr);
  } else {
    ASSERT(instr->representation().IsDouble());
    // We call a C function for double modulo. It can't trigger a GC.
    // We need to use fixed result register for the call.
    // TODO(fschneider): Allow any register as input registers.
    LOperand* left = UseFixedDouble(instr->left(), d1);
    LOperand* right = UseFixedDouble(instr->right(), d2);
    LArithmeticD* result = new LArithmeticD(Token::MOD, left, right);
    return MarkAsCall(DefineFixedDouble(result, d1), instr);
  }
}


LInstruction* LChunkBuilder::DoMul(HMul* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
    LOperand* right = UseOrConstant(instr->MostConstantOperand());
    LOperand* temp = NULL;
    if (instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
      temp = TempRegister();
    }
    LMulI* mul = new LMulI(left, right, temp);
    return AssignEnvironment(DefineSameAsFirst(mul));
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::MUL, instr);
  } else {
    return DoArithmeticT(Token::MUL, instr);
  }
}


LInstruction* LChunkBuilder::DoSub(HSub* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
    LOperand* right = UseOrConstantAtStart(instr->MostConstantOperand());
    LSubI* sub = new LSubI(left, right);
    LInstruction* result = DefineSameAsFirst(sub);
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


LInstruction* LChunkBuilder::DoAdd(HAdd* instr) {
  if (instr->representation().IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->LeastConstantOperand());
    LOperand* right = UseOrConstantAtStart(instr->MostConstantOperand());
    LAddI* add = new LAddI(left, right);
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
}


LInstruction* LChunkBuilder::DoPower(HPower* instr) {
  Abort("LPower instruction not implemented on ARM");
  return NULL;
}


LInstruction* LChunkBuilder::DoCompare(HCompare* instr) {
  Token::Value op = instr->token();
  Representation r = instr->GetInputRepresentation();
  if (r.IsInteger32()) {
    ASSERT(instr->left()->representation().IsInteger32());
    ASSERT(instr->right()->representation().IsInteger32());
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    return DefineAsRegister(new LCmpID(left, right));
  } else if (r.IsDouble()) {
    ASSERT(instr->left()->representation().IsDouble());
    ASSERT(instr->right()->representation().IsDouble());
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseRegisterAtStart(instr->right());
    return DefineAsRegister(new LCmpID(left, right));
  } else {
    ASSERT(instr->left()->representation().IsTagged());
    ASSERT(instr->right()->representation().IsTagged());
    bool reversed = (op == Token::GT || op == Token::LTE);
    LOperand* left = UseFixed(instr->left(), reversed ? r0 : r1);
    LOperand* right = UseFixed(instr->right(), reversed ? r1 : r0);
    LInstruction* result = new LCmpT(left, right);
    return MarkAsCall(DefineFixed(result, r0), instr);
  }
}


LInstruction* LChunkBuilder::DoCompareJSObjectEq(
    HCompareJSObjectEq* instr) {
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseRegisterAtStart(instr->right());
  LInstruction* result = new LCmpJSObjectEq(left, right);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoIsNull(HIsNull* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());

  return DefineAsRegister(new LIsNull(value));
}


LInstruction* LChunkBuilder::DoIsObject(HIsObject* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());

  return DefineAsRegister(new LIsObject(value, TempRegister()));
}


LInstruction* LChunkBuilder::DoIsSmi(HIsSmi* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseAtStart(instr->value());

  return DefineAsRegister(new LIsSmi(value));
}


LInstruction* LChunkBuilder::DoHasInstanceType(HHasInstanceType* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());

  return DefineAsRegister(new LHasInstanceType(value));
}


LInstruction* LChunkBuilder::DoHasCachedArrayIndex(
    HHasCachedArrayIndex* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegister(instr->value());

  return DefineAsRegister(new LHasCachedArrayIndex(value));
}


LInstruction* LChunkBuilder::DoClassOfTest(HClassOfTest* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseTempRegister(instr->value());
  return DefineSameAsFirst(new LClassOfTest(value));
}


LInstruction* LChunkBuilder::DoJSArrayLength(HJSArrayLength* instr) {
  LOperand* array = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new LJSArrayLength(array));
}


LInstruction* LChunkBuilder::DoFixedArrayLength(HFixedArrayLength* instr) {
  LOperand* array = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new LFixedArrayLength(array));
}


LInstruction* LChunkBuilder::DoValueOf(HValueOf* instr) {
  LOperand* object = UseRegister(instr->value());
  LInstruction* result = new LValueOf(object, TempRegister());
  return AssignEnvironment(DefineSameAsFirst(result));
}


LInstruction* LChunkBuilder::DoBoundsCheck(HBoundsCheck* instr) {
  return AssignEnvironment(new LBoundsCheck(UseRegisterAtStart(instr->index()),
                                            UseRegister(instr->length())));
}


LInstruction* LChunkBuilder::DoThrow(HThrow* instr) {
  LOperand* value = UseFixed(instr->value(), r0);
  return MarkAsCall(new LThrow(value), instr);
}


LInstruction* LChunkBuilder::DoChange(HChange* instr) {
  Representation from = instr->from();
  Representation to = instr->to();
  if (from.IsTagged()) {
    if (to.IsDouble()) {
      LOperand* value = UseRegister(instr->value());
      LInstruction* res = new LNumberUntagD(value);
      return AssignEnvironment(DefineAsRegister(res));
    } else {
      ASSERT(to.IsInteger32());
      LOperand* value = UseRegister(instr->value());
      bool needs_check = !instr->value()->type().IsSmi();
      LInstruction* res = NULL;
      if (needs_check) {
        res = DefineSameAsFirst(new LTaggedToI(value, FixedTemp(d1)));
      } else {
        res = DefineSameAsFirst(new LSmiUntag(value, needs_check));
      }
      if (needs_check) {
        res = AssignEnvironment(res);
      }
      return res;
    }
  } else if (from.IsDouble()) {
    if (to.IsTagged()) {
      LOperand* value = UseRegister(instr->value());
      LOperand* temp1 = TempRegister();
      LOperand* temp2 = TempRegister();

      // Make sure that the temp and result_temp registers are
      // different.
      LUnallocated* result_temp = TempRegister();
      LInstruction* result = new LNumberTagD(value, temp1, temp2);
      Define(result, result_temp);
      return AssignPointerMap(result);
    } else {
      ASSERT(to.IsInteger32());
      LOperand* value = UseRegister(instr->value());
      LInstruction* res = new LDoubleToI(value);
      return AssignEnvironment(DefineAsRegister(res));
    }
  } else if (from.IsInteger32()) {
    if (to.IsTagged()) {
      HValue* val = instr->value();
      LOperand* value = UseRegister(val);
      if (val->HasRange() && val->range()->IsInSmiRange()) {
        return DefineSameAsFirst(new LSmiTag(value));
      } else {
        LInstruction* result = new LNumberTagI(value);
        return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
      }
    } else {
      ASSERT(to.IsDouble());
      LOperand* value = Use(instr->value());
      return DefineAsRegister(new LInteger32ToDouble(value));
    }
  }
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckNonSmi(HCheckNonSmi* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new LCheckSmi(value, eq));
}


LInstruction* LChunkBuilder::DoCheckInstanceType(HCheckInstanceType* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LInstruction* result = new LCheckInstanceType(value);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckPrototypeMaps(HCheckPrototypeMaps* instr) {
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();
  LInstruction* result = new LCheckPrototypeMaps(temp1, temp2);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckSmi(HCheckSmi* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new LCheckSmi(value, ne));
}


LInstruction* LChunkBuilder::DoCheckFunction(HCheckFunction* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new LCheckFunction(value));
}


LInstruction* LChunkBuilder::DoCheckMap(HCheckMap* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LInstruction* result = new LCheckMap(value);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoReturn(HReturn* instr) {
  return new LReturn(UseFixed(instr->value(), r0));
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
  LInstruction* result = new LLoadGlobal();
  return instr->check_hole_value()
      ? AssignEnvironment(DefineAsRegister(result))
      : DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoStoreGlobal(HStoreGlobal* instr) {
  return new LStoreGlobal(UseRegisterAtStart(instr->value()));
}


LInstruction* LChunkBuilder::DoLoadContextSlot(HLoadContextSlot* instr) {
  return DefineAsRegister(new LLoadContextSlot);
}


LInstruction* LChunkBuilder::DoLoadNamedField(HLoadNamedField* instr) {
  return DefineAsRegister(
      new LLoadNamedField(UseRegisterAtStart(instr->object())));
}


LInstruction* LChunkBuilder::DoLoadNamedGeneric(HLoadNamedGeneric* instr) {
  LOperand* object = UseFixed(instr->object(), r0);
  LInstruction* result = DefineFixed(new LLoadNamedGeneric(object), r0);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoLoadFunctionPrototype(
    HLoadFunctionPrototype* instr) {
  return AssignEnvironment(DefineAsRegister(
      new LLoadFunctionPrototype(UseRegister(instr->function()))));
}


LInstruction* LChunkBuilder::DoLoadElements(HLoadElements* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  return DefineSameAsFirst(new LLoadElements(input));
}


LInstruction* LChunkBuilder::DoLoadKeyedFastElement(
    HLoadKeyedFastElement* instr) {
  ASSERT(instr->representation().IsTagged());
  ASSERT(instr->key()->representation().IsInteger32());
  LOperand* obj = UseRegisterAtStart(instr->object());
  LOperand* key = UseRegisterAtStart(instr->key());
  LInstruction* result = new LLoadKeyedFastElement(obj, key);
  return AssignEnvironment(DefineSameAsFirst(result));
}


LInstruction* LChunkBuilder::DoLoadKeyedGeneric(HLoadKeyedGeneric* instr) {
  LOperand* object = UseFixed(instr->object(), r1);
  LOperand* key = UseFixed(instr->key(), r0);

  LInstruction* result =
      DefineFixed(new LLoadKeyedGeneric(object, key), r0);
  return MarkAsCall(result, instr);
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

  return AssignEnvironment(new LStoreKeyedFastElement(obj, key, val));
}


LInstruction* LChunkBuilder::DoStoreKeyedGeneric(HStoreKeyedGeneric* instr) {
  LOperand* obj = UseFixed(instr->object(), r2);
  LOperand* key = UseFixed(instr->key(), r1);
  LOperand* val = UseFixed(instr->value(), r0);

  ASSERT(instr->object()->representation().IsTagged());
  ASSERT(instr->key()->representation().IsTagged());
  ASSERT(instr->value()->representation().IsTagged());

  return MarkAsCall(new LStoreKeyedGeneric(obj, key, val), instr);
}


LInstruction* LChunkBuilder::DoStoreNamedField(HStoreNamedField* instr) {
  bool needs_write_barrier = instr->NeedsWriteBarrier();

  LOperand* obj = needs_write_barrier
      ? UseTempRegister(instr->object())
      : UseRegisterAtStart(instr->object());

  LOperand* val = needs_write_barrier
      ? UseTempRegister(instr->value())
      : UseRegister(instr->value());

  return new LStoreNamedField(obj, val);
}


LInstruction* LChunkBuilder::DoStoreNamedGeneric(HStoreNamedGeneric* instr) {
  LOperand* obj = UseFixed(instr->object(), r1);
  LOperand* val = UseFixed(instr->value(), r0);

  LInstruction* result = new LStoreNamedGeneric(obj, val);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoArrayLiteral(HArrayLiteral* instr) {
  return MarkAsCall(DefineFixed(new LArrayLiteral, r0), instr);
}


LInstruction* LChunkBuilder::DoObjectLiteral(HObjectLiteral* instr) {
  return MarkAsCall(DefineFixed(new LObjectLiteral, r0), instr);
}


LInstruction* LChunkBuilder::DoRegExpLiteral(HRegExpLiteral* instr) {
  return MarkAsCall(DefineFixed(new LRegExpLiteral, r0), instr);
}


LInstruction* LChunkBuilder::DoFunctionLiteral(HFunctionLiteral* instr) {
  return MarkAsCall(DefineFixed(new LFunctionLiteral, r0), instr);
}


LInstruction* LChunkBuilder::DoDeleteProperty(HDeleteProperty* instr) {
  LOperand* object = UseRegisterAtStart(instr->object());
  LOperand* key = UseRegisterAtStart(instr->key());
  LInstruction* result = new LDeleteProperty(object, key);
  return MarkAsCall(DefineFixed(result, r0), instr);
}


LInstruction* LChunkBuilder::DoOsrEntry(HOsrEntry* instr) {
  allocator_->MarkAsOsrEntry();
  current_block_->last_environment()->set_ast_id(instr->ast_id());
  return AssignEnvironment(new LOsrEntry);
}


LInstruction* LChunkBuilder::DoParameter(HParameter* instr) {
  int spill_index = chunk()->GetParameterStackSlot(instr->index());
  return DefineAsSpilled(new LParameter, spill_index);
}


LInstruction* LChunkBuilder::DoUnknownOSRValue(HUnknownOSRValue* instr) {
  int spill_index = chunk()->GetNextSpillIndex(false);  // Not double-width.
  return DefineAsSpilled(new LUnknownOSRValue, spill_index);
}


LInstruction* LChunkBuilder::DoCallStub(HCallStub* instr) {
  argument_count_ -= instr->argument_count();
  return MarkAsCall(DefineFixed(new LCallStub, r0), instr);
}


LInstruction* LChunkBuilder::DoArgumentsObject(HArgumentsObject* instr) {
  // There are no real uses of the arguments object (we bail out in all other
  // cases).
  return NULL;
}


LInstruction* LChunkBuilder::DoAccessArgumentsAt(HAccessArgumentsAt* instr) {
  LOperand* arguments = UseRegister(instr->arguments());
  LOperand* length = UseTempRegister(instr->length());
  LOperand* index = UseRegister(instr->index());
  LInstruction* result = new LAccessArgumentsAt(arguments, length, index);
  return DefineAsRegister(AssignEnvironment(result));
}


LInstruction* LChunkBuilder::DoTypeof(HTypeof* instr) {
  LInstruction* result = new LTypeof(UseRegisterAtStart(instr->value()));
  return MarkAsCall(DefineFixed(result, r0), instr);
}


LInstruction* LChunkBuilder::DoTypeofIs(HTypeofIs* instr) {
  return DefineSameAsFirst(new LTypeofIs(UseRegister(instr->value())));
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
    LInstruction* result = new LLazyBailout;
    result = AssignEnvironment(result);
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
  HEnvironment* outer = current_block_->last_environment();
  HConstant* undefined = graph()->GetConstantUndefined();
  HEnvironment* inner = outer->CopyForInlining(instr->closure(),
                                               instr->function(),
                                               false,
                                               undefined);
  current_block_->UpdateEnvironment(inner);
  chunk_->AddInlinedClosure(instr->closure());
  return NULL;
}


LInstruction* LChunkBuilder::DoLeaveInlined(HLeaveInlined* instr) {
  HEnvironment* outer = current_block_->last_environment()->outer();
  current_block_->UpdateEnvironment(outer);
  return NULL;
}


} }  // namespace v8::internal
