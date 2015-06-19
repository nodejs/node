// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/compiler/code-generator-impl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

class CodeGenerator::JumpTable final : public ZoneObject {
 public:
  JumpTable(JumpTable* next, Label** targets, size_t target_count)
      : next_(next), targets_(targets), target_count_(target_count) {}

  Label* label() { return &label_; }
  JumpTable* next() const { return next_; }
  Label** targets() const { return targets_; }
  size_t target_count() const { return target_count_; }

 private:
  Label label_;
  JumpTable* const next_;
  Label** const targets_;
  size_t const target_count_;
};


CodeGenerator::CodeGenerator(Frame* frame, Linkage* linkage,
                             InstructionSequence* code, CompilationInfo* info)
    : frame_(frame),
      linkage_(linkage),
      code_(code),
      info_(info),
      labels_(zone()->NewArray<Label>(code->InstructionBlockCount())),
      current_block_(RpoNumber::Invalid()),
      current_source_position_(SourcePosition::Invalid()),
      masm_(info->isolate(), NULL, 0),
      resolver_(this),
      safepoints_(code->zone()),
      handlers_(code->zone()),
      deoptimization_states_(code->zone()),
      deoptimization_literals_(code->zone()),
      translations_(code->zone()),
      last_lazy_deopt_pc_(0),
      jump_tables_(nullptr),
      ools_(nullptr),
      osr_pc_offset_(-1) {
  for (int i = 0; i < code->InstructionBlockCount(); ++i) {
    new (&labels_[i]) Label;
  }
}


Handle<Code> CodeGenerator::GenerateCode() {
  CompilationInfo* info = this->info();

  // Emit a code line info recording start event.
  PositionsRecorder* recorder = masm()->positions_recorder();
  LOG_CODE_EVENT(isolate(), CodeStartLinePosInfoRecordEvent(recorder));

  // Place function entry hook if requested to do so.
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm());
  }

  // Architecture-specific, linkage-specific prologue.
  info->set_prologue_offset(masm()->pc_offset());
  AssemblePrologue();

  // Assemble all non-deferred blocks, followed by deferred ones.
  for (int deferred = 0; deferred < 2; ++deferred) {
    for (auto const block : code()->instruction_blocks()) {
      if (block->IsDeferred() == (deferred == 0)) {
        continue;
      }
      // Align loop headers on 16-byte boundaries.
      if (block->IsLoopHeader()) masm()->Align(16);
      // Bind a label for a block.
      current_block_ = block->rpo_number();
      if (FLAG_code_comments) {
        // TODO(titzer): these code comments are a giant memory leak.
        Vector<char> buffer = Vector<char>::New(200);
        SNPrintF(buffer, "-- B%d start%s%s%s%s --", block->rpo_number().ToInt(),
                 block->IsDeferred() ? " (deferred)" : "",
                 block->needs_frame() ? "" : " (no frame)",
                 block->must_construct_frame() ? " (construct frame)" : "",
                 block->must_deconstruct_frame() ? " (deconstruct frame)" : "");
        masm()->RecordComment(buffer.start());
      }
      masm()->bind(GetLabel(current_block_));
      for (int i = block->code_start(); i < block->code_end(); ++i) {
        AssembleInstruction(code()->InstructionAt(i));
      }
    }
  }

  // Assemble all out-of-line code.
  if (ools_) {
    masm()->RecordComment("-- Out of line code --");
    for (OutOfLineCode* ool = ools_; ool; ool = ool->next()) {
      masm()->bind(ool->entry());
      ool->Generate();
      if (ool->exit()->is_bound()) masm()->jmp(ool->exit());
    }
  }

  // Ensure there is space for lazy deoptimization in the code.
  if (!info->IsStub()) {
    int target_offset = masm()->pc_offset() + Deoptimizer::patch_size();
    while (masm()->pc_offset() < target_offset) {
      masm()->nop();
    }
  }

  FinishCode(masm());

  // Emit the jump tables.
  if (jump_tables_) {
    masm()->Align(kPointerSize);
    for (JumpTable* table = jump_tables_; table; table = table->next()) {
      masm()->bind(table->label());
      AssembleJumpTable(table->targets(), table->target_count());
    }
  }

  safepoints()->Emit(masm(), frame()->GetSpillSlotCount());

  Handle<Code> result = v8::internal::CodeGenerator::MakeCodeEpilogue(
      masm(), info->flags(), info);
  result->set_is_turbofanned(true);
  result->set_stack_slots(frame()->GetSpillSlotCount());
  result->set_safepoint_table_offset(safepoints()->GetCodeOffset());

  // Emit exception handler table.
  if (!handlers_.empty()) {
    Handle<HandlerTable> table =
        Handle<HandlerTable>::cast(isolate()->factory()->NewFixedArray(
            HandlerTable::LengthForReturn(static_cast<int>(handlers_.size())),
            TENURED));
    for (size_t i = 0; i < handlers_.size(); ++i) {
      table->SetReturnOffset(static_cast<int>(i), handlers_[i].pc_offset);
      table->SetReturnHandler(static_cast<int>(i), handlers_[i].handler->pos());
    }
    result->set_handler_table(*table);
  }

  PopulateDeoptimizationData(result);

  // Ensure there is space for lazy deoptimization in the relocation info.
  if (!info->IsStub()) {
    Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(result);
  }

  // Emit a code line info recording stop event.
  void* line_info = recorder->DetachJITHandlerData();
  LOG_CODE_EVENT(isolate(), CodeEndLinePosInfoRecordEvent(*result, line_info));

  return result;
}


bool CodeGenerator::IsNextInAssemblyOrder(RpoNumber block) const {
  return code()->InstructionBlockAt(current_block_)->ao_number().IsNext(
      code()->InstructionBlockAt(block)->ao_number());
}


void CodeGenerator::RecordSafepoint(ReferenceMap* references,
                                    Safepoint::Kind kind, int arguments,
                                    Safepoint::DeoptMode deopt_mode) {
  Safepoint safepoint =
      safepoints()->DefineSafepoint(masm(), kind, arguments, deopt_mode);
  for (auto& operand : references->reference_operands()) {
    if (operand.IsStackSlot()) {
      safepoint.DefinePointerSlot(StackSlotOperand::cast(operand).index(),
                                  zone());
    } else if (operand.IsRegister() && (kind & Safepoint::kWithRegisters)) {
      Register reg =
          Register::FromAllocationIndex(RegisterOperand::cast(operand).index());
      safepoint.DefinePointerRegister(reg, zone());
    }
  }
}


bool CodeGenerator::IsMaterializableFromFrame(Handle<HeapObject> object,
                                              int* offset_return) {
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    if (object.is_identical_to(info()->context())) {
      *offset_return = StandardFrameConstants::kContextOffset;
      return true;
    } else if (object.is_identical_to(info()->closure())) {
      *offset_return = JavaScriptFrameConstants::kFunctionOffset;
      return true;
    }
  }
  return false;
}


bool CodeGenerator::IsMaterializableFromRoot(
    Handle<HeapObject> object, Heap::RootListIndex* index_return) {
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    return isolate()->heap()->GetRootListIndex(object, index_return);
  }
  return false;
}


void CodeGenerator::AssembleInstruction(Instruction* instr) {
  AssembleGaps(instr);
  AssembleSourcePosition(instr);
  // Assemble architecture-specific code for the instruction.
  AssembleArchInstruction(instr);

  FlagsMode mode = FlagsModeField::decode(instr->opcode());
  FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
  if (mode == kFlags_branch) {
    // Assemble a branch after this instruction.
    InstructionOperandConverter i(this, instr);
    RpoNumber true_rpo = i.InputRpo(instr->InputCount() - 2);
    RpoNumber false_rpo = i.InputRpo(instr->InputCount() - 1);

    if (true_rpo == false_rpo) {
      // redundant branch.
      if (!IsNextInAssemblyOrder(true_rpo)) {
        AssembleArchJump(true_rpo);
      }
      return;
    }
    if (IsNextInAssemblyOrder(true_rpo)) {
      // true block is next, can fall through if condition negated.
      std::swap(true_rpo, false_rpo);
      condition = NegateFlagsCondition(condition);
    }
    BranchInfo branch;
    branch.condition = condition;
    branch.true_label = GetLabel(true_rpo);
    branch.false_label = GetLabel(false_rpo);
    branch.fallthru = IsNextInAssemblyOrder(false_rpo);
    // Assemble architecture-specific branch.
    AssembleArchBranch(instr, &branch);
  } else if (mode == kFlags_set) {
    // Assemble a boolean materialization after this instruction.
    AssembleArchBoolean(instr, condition);
  }
}


void CodeGenerator::AssembleSourcePosition(Instruction* instr) {
  SourcePosition source_position;
  if (!code()->GetSourcePosition(instr, &source_position)) return;
  if (source_position == current_source_position_) return;
  DCHECK(!source_position.IsInvalid());
  current_source_position_ = source_position;
  if (source_position.IsUnknown()) return;
  int code_pos = source_position.raw();
  masm()->positions_recorder()->RecordPosition(source_position.raw());
  masm()->positions_recorder()->WriteRecordedPositions();
  if (FLAG_code_comments) {
    Vector<char> buffer = Vector<char>::New(256);
    CompilationInfo* info = this->info();
    int ln = Script::GetLineNumber(info->script(), code_pos);
    int cn = Script::GetColumnNumber(info->script(), code_pos);
    if (info->script()->name()->IsString()) {
      Handle<String> file(String::cast(info->script()->name()));
      base::OS::SNPrintF(buffer.start(), buffer.length(), "-- %s:%d:%d --",
                         file->ToCString().get(), ln, cn);
    } else {
      base::OS::SNPrintF(buffer.start(), buffer.length(),
                         "-- <unknown>:%d:%d --", ln, cn);
    }
    masm()->RecordComment(buffer.start());
  }
}


void CodeGenerator::AssembleGaps(Instruction* instr) {
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    ParallelMove* move = instr->GetParallelMove(inner_pos);
    if (move != nullptr) resolver()->Resolve(move);
  }
}


void CodeGenerator::PopulateDeoptimizationData(Handle<Code> code_object) {
  CompilationInfo* info = this->info();
  int deopt_count = static_cast<int>(deoptimization_states_.size());
  if (deopt_count == 0 && !info->is_osr()) return;
  Handle<DeoptimizationInputData> data =
      DeoptimizationInputData::New(isolate(), deopt_count, TENURED);

  Handle<ByteArray> translation_array =
      translations_.CreateByteArray(isolate()->factory());

  data->SetTranslationByteArray(*translation_array);
  data->SetInlinedFunctionCount(Smi::FromInt(0));
  data->SetOptimizationId(Smi::FromInt(info->optimization_id()));
  // TODO(jarin) The following code was copied over from Lithium, not sure
  // whether the scope or the IsOptimizing condition are really needed.
  if (info->IsOptimizing()) {
    // Reference to shared function info does not change between phases.
    AllowDeferredHandleDereference allow_handle_dereference;
    data->SetSharedFunctionInfo(*info->shared_info());
  } else {
    data->SetSharedFunctionInfo(Smi::FromInt(0));
  }

  Handle<FixedArray> literals = isolate()->factory()->NewFixedArray(
      static_cast<int>(deoptimization_literals_.size()), TENURED);
  {
    AllowDeferredHandleDereference copy_handles;
    for (unsigned i = 0; i < deoptimization_literals_.size(); i++) {
      literals->set(i, *deoptimization_literals_[i]);
    }
    data->SetLiteralArray(*literals);
  }

  if (info->is_osr()) {
    DCHECK(osr_pc_offset_ >= 0);
    data->SetOsrAstId(Smi::FromInt(info_->osr_ast_id().ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(osr_pc_offset_));
  } else {
    BailoutId osr_ast_id = BailoutId::None();
    data->SetOsrAstId(Smi::FromInt(osr_ast_id.ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(-1));
  }

  // Populate deoptimization entries.
  for (int i = 0; i < deopt_count; i++) {
    DeoptimizationState* deoptimization_state = deoptimization_states_[i];
    data->SetAstId(i, deoptimization_state->bailout_id());
    CHECK(deoptimization_states_[i]);
    data->SetTranslationIndex(
        i, Smi::FromInt(deoptimization_states_[i]->translation_id()));
    data->SetArgumentsStackHeight(i, Smi::FromInt(0));
    data->SetPc(i, Smi::FromInt(deoptimization_state->pc_offset()));
  }

  code_object->set_deoptimization_data(*data);
}


Label* CodeGenerator::AddJumpTable(Label** targets, size_t target_count) {
  jump_tables_ = new (zone()) JumpTable(jump_tables_, targets, target_count);
  return jump_tables_->label();
}


void CodeGenerator::RecordCallPosition(Instruction* instr) {
  CallDescriptor::Flags flags(MiscField::decode(instr->opcode()));

  bool needs_frame_state = (flags & CallDescriptor::kNeedsFrameState);

  RecordSafepoint(
      instr->reference_map(), Safepoint::kSimple, 0,
      needs_frame_state ? Safepoint::kLazyDeopt : Safepoint::kNoLazyDeopt);

  if (flags & CallDescriptor::kHasExceptionHandler) {
    InstructionOperandConverter i(this, instr);
    RpoNumber handler_rpo =
        i.InputRpo(static_cast<int>(instr->InputCount()) - 1);
    handlers_.push_back({GetLabel(handler_rpo), masm()->pc_offset()});
  }

  if (flags & CallDescriptor::kNeedsNopAfterCall) {
    AddNopForSmiCodeInlining();
  }

  if (needs_frame_state) {
    MarkLazyDeoptSite();
    // If the frame state is present, it starts at argument 1 (just after the
    // code address).
    size_t frame_state_offset = 1;
    FrameStateDescriptor* descriptor =
        GetFrameStateDescriptor(instr, frame_state_offset);
    int pc_offset = masm()->pc_offset();
    int deopt_state_id = BuildTranslation(instr, pc_offset, frame_state_offset,
                                          descriptor->state_combine());
    // If the pre-call frame state differs from the post-call one, produce the
    // pre-call frame state, too.
    // TODO(jarin) We might want to avoid building the pre-call frame state
    // because it is only used to get locals and arguments (by the debugger and
    // f.arguments), and those are the same in the pre-call and post-call
    // states.
    if (!descriptor->state_combine().IsOutputIgnored()) {
      deopt_state_id = BuildTranslation(instr, -1, frame_state_offset,
                                        OutputFrameStateCombine::Ignore());
    }
#if DEBUG
    // Make sure all the values live in stack slots or they are immediates.
    // (The values should not live in register because registers are clobbered
    // by calls.)
    for (size_t i = 0; i < descriptor->GetSize(); i++) {
      InstructionOperand* op = instr->InputAt(frame_state_offset + 1 + i);
      CHECK(op->IsStackSlot() || op->IsDoubleStackSlot() || op->IsImmediate());
    }
#endif
    safepoints()->RecordLazyDeoptimizationIndex(deopt_state_id);
  }
}


int CodeGenerator::DefineDeoptimizationLiteral(Handle<Object> literal) {
  int result = static_cast<int>(deoptimization_literals_.size());
  for (unsigned i = 0; i < deoptimization_literals_.size(); ++i) {
    if (deoptimization_literals_[i].is_identical_to(literal)) return i;
  }
  deoptimization_literals_.push_back(literal);
  return result;
}


FrameStateDescriptor* CodeGenerator::GetFrameStateDescriptor(
    Instruction* instr, size_t frame_state_offset) {
  InstructionOperandConverter i(this, instr);
  InstructionSequence::StateId state_id =
      InstructionSequence::StateId::FromInt(i.InputInt32(frame_state_offset));
  return code()->GetFrameStateDescriptor(state_id);
}

struct OperandAndType {
  OperandAndType(InstructionOperand* operand, MachineType type)
      : operand_(operand), type_(type) {}

  InstructionOperand* operand_;
  MachineType type_;
};

static OperandAndType TypedOperandForFrameState(
    FrameStateDescriptor* descriptor, Instruction* instr,
    size_t frame_state_offset, size_t index, OutputFrameStateCombine combine) {
  DCHECK(index < descriptor->GetSize(combine));
  switch (combine.kind()) {
    case OutputFrameStateCombine::kPushOutput: {
      DCHECK(combine.GetPushCount() <= instr->OutputCount());
      size_t size_without_output =
          descriptor->GetSize(OutputFrameStateCombine::Ignore());
      // If the index is past the existing stack items, return the output.
      if (index >= size_without_output) {
        return OperandAndType(instr->OutputAt(index - size_without_output),
                              kMachAnyTagged);
      }
      break;
    }
    case OutputFrameStateCombine::kPokeAt:
      size_t index_from_top =
          descriptor->GetSize(combine) - 1 - combine.GetOffsetToPokeAt();
      if (index >= index_from_top &&
          index < index_from_top + instr->OutputCount()) {
        return OperandAndType(instr->OutputAt(index - index_from_top),
                              kMachAnyTagged);
      }
      break;
  }
  return OperandAndType(instr->InputAt(frame_state_offset + index),
                        descriptor->GetType(index));
}


void CodeGenerator::BuildTranslationForFrameStateDescriptor(
    FrameStateDescriptor* descriptor, Instruction* instr,
    Translation* translation, size_t frame_state_offset,
    OutputFrameStateCombine state_combine) {
  // Outer-most state must be added to translation first.
  if (descriptor->outer_state() != NULL) {
    BuildTranslationForFrameStateDescriptor(descriptor->outer_state(), instr,
                                            translation, frame_state_offset,
                                            OutputFrameStateCombine::Ignore());
  }

  int id = Translation::kSelfLiteralId;
  if (!descriptor->jsfunction().is_null()) {
    id = DefineDeoptimizationLiteral(
        Handle<Object>::cast(descriptor->jsfunction().ToHandleChecked()));
  }

  switch (descriptor->type()) {
    case JS_FRAME:
      translation->BeginJSFrame(
          descriptor->bailout_id(), id,
          static_cast<unsigned int>(descriptor->GetSize(state_combine) -
                                    descriptor->parameters_count()));
      break;
    case ARGUMENTS_ADAPTOR:
      translation->BeginArgumentsAdaptorFrame(
          id, static_cast<unsigned int>(descriptor->parameters_count()));
      break;
  }

  frame_state_offset += descriptor->outer_state()->GetTotalSize();
  for (size_t i = 0; i < descriptor->GetSize(state_combine); i++) {
    OperandAndType op = TypedOperandForFrameState(
        descriptor, instr, frame_state_offset, i, state_combine);
    AddTranslationForOperand(translation, instr, op.operand_, op.type_);
  }
}


int CodeGenerator::BuildTranslation(Instruction* instr, int pc_offset,
                                    size_t frame_state_offset,
                                    OutputFrameStateCombine state_combine) {
  FrameStateDescriptor* descriptor =
      GetFrameStateDescriptor(instr, frame_state_offset);
  frame_state_offset++;

  Translation translation(
      &translations_, static_cast<int>(descriptor->GetFrameCount()),
      static_cast<int>(descriptor->GetJSFrameCount()), zone());
  BuildTranslationForFrameStateDescriptor(descriptor, instr, &translation,
                                          frame_state_offset, state_combine);

  int deoptimization_id = static_cast<int>(deoptimization_states_.size());

  deoptimization_states_.push_back(new (zone()) DeoptimizationState(
      descriptor->bailout_id(), translation.index(), pc_offset));

  return deoptimization_id;
}


void CodeGenerator::AddTranslationForOperand(Translation* translation,
                                             Instruction* instr,
                                             InstructionOperand* op,
                                             MachineType type) {
  if (op->IsStackSlot()) {
    if (type == kMachBool || type == kRepBit) {
      translation->StoreBoolStackSlot(StackSlotOperand::cast(op)->index());
    } else if (type == kMachInt32 || type == kMachInt8 || type == kMachInt16) {
      translation->StoreInt32StackSlot(StackSlotOperand::cast(op)->index());
    } else if (type == kMachUint32 || type == kMachUint16 ||
               type == kMachUint8) {
      translation->StoreUint32StackSlot(StackSlotOperand::cast(op)->index());
    } else if ((type & kRepMask) == kRepTagged) {
      translation->StoreStackSlot(StackSlotOperand::cast(op)->index());
    } else {
      CHECK(false);
    }
  } else if (op->IsDoubleStackSlot()) {
    DCHECK((type & (kRepFloat32 | kRepFloat64)) != 0);
    translation->StoreDoubleStackSlot(
        DoubleStackSlotOperand::cast(op)->index());
  } else if (op->IsRegister()) {
    InstructionOperandConverter converter(this, instr);
    if (type == kMachBool || type == kRepBit) {
      translation->StoreBoolRegister(converter.ToRegister(op));
    } else if (type == kMachInt32 || type == kMachInt8 || type == kMachInt16) {
      translation->StoreInt32Register(converter.ToRegister(op));
    } else if (type == kMachUint32 || type == kMachUint16 ||
               type == kMachUint8) {
      translation->StoreUint32Register(converter.ToRegister(op));
    } else if ((type & kRepMask) == kRepTagged) {
      translation->StoreRegister(converter.ToRegister(op));
    } else {
      CHECK(false);
    }
  } else if (op->IsDoubleRegister()) {
    DCHECK((type & (kRepFloat32 | kRepFloat64)) != 0);
    InstructionOperandConverter converter(this, instr);
    translation->StoreDoubleRegister(converter.ToDoubleRegister(op));
  } else if (op->IsImmediate()) {
    InstructionOperandConverter converter(this, instr);
    Constant constant = converter.ToConstant(op);
    Handle<Object> constant_object;
    switch (constant.type()) {
      case Constant::kInt32:
        DCHECK(type == kMachInt32 || type == kMachUint32 || type == kRepBit);
        constant_object =
            isolate()->factory()->NewNumberFromInt(constant.ToInt32());
        break;
      case Constant::kFloat64:
        DCHECK(type == kMachFloat64 || type == kMachAnyTagged ||
               type == kRepTagged || type == (kTypeNumber | kRepTagged) ||
               type == (kTypeInt32 | kRepTagged) ||
               type == (kTypeUint32 | kRepTagged));
        constant_object = isolate()->factory()->NewNumber(constant.ToFloat64());
        break;
      case Constant::kHeapObject:
        DCHECK((type & kRepMask) == kRepTagged);
        constant_object = constant.ToHeapObject();
        break;
      default:
        CHECK(false);
    }
    int literal_id = DefineDeoptimizationLiteral(constant_object);
    translation->StoreLiteral(literal_id);
  } else {
    CHECK(false);
  }
}


void CodeGenerator::MarkLazyDeoptSite() {
  last_lazy_deopt_pc_ = masm()->pc_offset();
}

#if !V8_TURBOFAN_BACKEND

void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssembleArchBranch(Instruction* instr,
                                       BranchInfo* branch) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssembleArchJump(RpoNumber target) { UNIMPLEMENTED(); }


void CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssemblePrologue() { UNIMPLEMENTED(); }


void CodeGenerator::AssembleReturn() { UNIMPLEMENTED(); }


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  UNIMPLEMENTED();
}


void CodeGenerator::AddNopForSmiCodeInlining() { UNIMPLEMENTED(); }


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  UNIMPLEMENTED();
}

#endif  // !V8_TURBOFAN_BACKEND


OutOfLineCode::OutOfLineCode(CodeGenerator* gen)
    : masm_(gen->masm()), next_(gen->ools_) {
  gen->ools_ = this;
}


OutOfLineCode::~OutOfLineCode() {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
