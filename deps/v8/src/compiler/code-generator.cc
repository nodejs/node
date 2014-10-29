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

CodeGenerator::CodeGenerator(InstructionSequence* code)
    : code_(code),
      current_block_(NULL),
      current_source_position_(SourcePosition::Invalid()),
      masm_(code->zone()->isolate(), NULL, 0),
      resolver_(this),
      safepoints_(code->zone()),
      deoptimization_states_(code->zone()),
      deoptimization_literals_(code->zone()),
      translations_(code->zone()),
      last_lazy_deopt_pc_(0) {}


Handle<Code> CodeGenerator::GenerateCode() {
  CompilationInfo* info = linkage()->info();

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

  // Assemble all instructions.
  for (InstructionSequence::const_iterator i = code()->begin();
       i != code()->end(); ++i) {
    AssembleInstruction(*i);
  }

  FinishCode(masm());

  // Ensure there is space for lazy deopt.
  if (!info->IsStub()) {
    int target_offset = masm()->pc_offset() + Deoptimizer::patch_size();
    while (masm()->pc_offset() < target_offset) {
      masm()->nop();
    }
  }

  safepoints()->Emit(masm(), frame()->GetSpillSlotCount());

  // TODO(titzer): what are the right code flags here?
  Code::Kind kind = Code::STUB;
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    kind = Code::OPTIMIZED_FUNCTION;
  }
  Handle<Code> result = v8::internal::CodeGenerator::MakeCodeEpilogue(
      masm(), Code::ComputeFlags(kind), info);
  result->set_is_turbofanned(true);
  result->set_stack_slots(frame()->GetSpillSlotCount());
  result->set_safepoint_table_offset(safepoints()->GetCodeOffset());

  PopulateDeoptimizationData(result);

  // Emit a code line info recording stop event.
  void* line_info = recorder->DetachJITHandlerData();
  LOG_CODE_EVENT(isolate(), CodeEndLinePosInfoRecordEvent(*result, line_info));

  return result;
}


void CodeGenerator::RecordSafepoint(PointerMap* pointers, Safepoint::Kind kind,
                                    int arguments,
                                    Safepoint::DeoptMode deopt_mode) {
  const ZoneList<InstructionOperand*>* operands =
      pointers->GetNormalizedOperands();
  Safepoint safepoint =
      safepoints()->DefineSafepoint(masm(), kind, arguments, deopt_mode);
  for (int i = 0; i < operands->length(); i++) {
    InstructionOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index(), zone());
    } else if (pointer->IsRegister() && (kind & Safepoint::kWithRegisters)) {
      Register reg = Register::FromAllocationIndex(pointer->index());
      safepoint.DefinePointerRegister(reg, zone());
    }
  }
}


void CodeGenerator::AssembleInstruction(Instruction* instr) {
  if (instr->IsBlockStart()) {
    // Bind a label for a block start and handle parallel moves.
    BlockStartInstruction* block_start = BlockStartInstruction::cast(instr);
    current_block_ = block_start->block();
    if (FLAG_code_comments) {
      // TODO(titzer): these code comments are a giant memory leak.
      Vector<char> buffer = Vector<char>::New(32);
      SNPrintF(buffer, "-- B%d start --", block_start->block()->id());
      masm()->RecordComment(buffer.start());
    }
    masm()->bind(block_start->label());
  }
  if (instr->IsGapMoves()) {
    // Handle parallel moves associated with the gap instruction.
    AssembleGap(GapInstruction::cast(instr));
  } else if (instr->IsSourcePosition()) {
    AssembleSourcePosition(SourcePositionInstruction::cast(instr));
  } else {
    // Assemble architecture-specific code for the instruction.
    AssembleArchInstruction(instr);

    // Assemble branches or boolean materializations after this instruction.
    FlagsMode mode = FlagsModeField::decode(instr->opcode());
    FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
    switch (mode) {
      case kFlags_none:
        return;
      case kFlags_set:
        return AssembleArchBoolean(instr, condition);
      case kFlags_branch:
        return AssembleArchBranch(instr, condition);
    }
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSourcePosition(SourcePositionInstruction* instr) {
  SourcePosition source_position = instr->source_position();
  if (source_position == current_source_position_) return;
  DCHECK(!source_position.IsInvalid());
  if (!source_position.IsUnknown()) {
    int code_pos = source_position.raw();
    masm()->positions_recorder()->RecordPosition(source_position.raw());
    masm()->positions_recorder()->WriteRecordedPositions();
    if (FLAG_code_comments) {
      Vector<char> buffer = Vector<char>::New(256);
      CompilationInfo* info = linkage()->info();
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
  current_source_position_ = source_position;
}


void CodeGenerator::AssembleGap(GapInstruction* instr) {
  for (int i = GapInstruction::FIRST_INNER_POSITION;
       i <= GapInstruction::LAST_INNER_POSITION; i++) {
    GapInstruction::InnerPosition inner_pos =
        static_cast<GapInstruction::InnerPosition>(i);
    ParallelMove* move = instr->GetParallelMove(inner_pos);
    if (move != NULL) resolver()->Resolve(move);
  }
}


void CodeGenerator::PopulateDeoptimizationData(Handle<Code> code_object) {
  CompilationInfo* info = linkage()->info();
  int deopt_count = static_cast<int>(deoptimization_states_.size());
  if (deopt_count == 0) return;
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

  // No OSR in Turbofan yet...
  BailoutId osr_ast_id = BailoutId::None();
  data->SetOsrAstId(Smi::FromInt(osr_ast_id.ToInt()));
  data->SetOsrPcOffset(Smi::FromInt(-1));

  // Populate deoptimization entries.
  for (int i = 0; i < deopt_count; i++) {
    DeoptimizationState* deoptimization_state = deoptimization_states_[i];
    data->SetAstId(i, deoptimization_state->bailout_id());
    CHECK_NE(NULL, deoptimization_states_[i]);
    data->SetTranslationIndex(
        i, Smi::FromInt(deoptimization_states_[i]->translation_id()));
    data->SetArgumentsStackHeight(i, Smi::FromInt(0));
    data->SetPc(i, Smi::FromInt(deoptimization_state->pc_offset()));
  }

  code_object->set_deoptimization_data(*data);
}


void CodeGenerator::AddSafepointAndDeopt(Instruction* instr) {
  CallDescriptor::Flags flags(MiscField::decode(instr->opcode()));

  bool needs_frame_state = (flags & CallDescriptor::kNeedsFrameState);

  RecordSafepoint(
      instr->pointer_map(), Safepoint::kSimple, 0,
      needs_frame_state ? Safepoint::kLazyDeopt : Safepoint::kNoLazyDeopt);

  if (flags & CallDescriptor::kNeedsNopAfterCall) {
    AddNopForSmiCodeInlining();
  }

  if (needs_frame_state) {
    MarkLazyDeoptSite();
    // If the frame state is present, it starts at argument 1
    // (just after the code address).
    InstructionOperandConverter converter(this, instr);
    // Deoptimization info starts at argument 1
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
    if (descriptor->state_combine() != kIgnoreOutput) {
      deopt_state_id =
          BuildTranslation(instr, -1, frame_state_offset, kIgnoreOutput);
    }
#if DEBUG
    // Make sure all the values live in stack slots or they are immediates.
    // (The values should not live in register because registers are clobbered
    // by calls.)
    for (size_t i = 0; i < descriptor->size(); i++) {
      InstructionOperand* op = instr->InputAt(frame_state_offset + 1 + i);
      CHECK(op->IsStackSlot() || op->IsImmediate());
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
  InstructionSequence::StateId state_id = InstructionSequence::StateId::FromInt(
      i.InputInt32(static_cast<int>(frame_state_offset)));
  return code()->GetFrameStateDescriptor(state_id);
}


void CodeGenerator::BuildTranslationForFrameStateDescriptor(
    FrameStateDescriptor* descriptor, Instruction* instr,
    Translation* translation, size_t frame_state_offset,
    OutputFrameStateCombine state_combine) {
  // Outer-most state must be added to translation first.
  if (descriptor->outer_state() != NULL) {
    BuildTranslationForFrameStateDescriptor(descriptor->outer_state(), instr,
                                            translation, frame_state_offset,
                                            kIgnoreOutput);
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
          static_cast<unsigned int>(descriptor->GetHeight(state_combine)));
      break;
    case ARGUMENTS_ADAPTOR:
      translation->BeginArgumentsAdaptorFrame(
          id, static_cast<unsigned int>(descriptor->parameters_count()));
      break;
  }

  frame_state_offset += descriptor->outer_state()->GetTotalSize();
  for (size_t i = 0; i < descriptor->size(); i++) {
    AddTranslationForOperand(
        translation, instr,
        instr->InputAt(static_cast<int>(frame_state_offset + i)));
  }

  switch (state_combine) {
    case kPushOutput:
      DCHECK(instr->OutputCount() == 1);
      AddTranslationForOperand(translation, instr, instr->OutputAt(0));
      break;
    case kIgnoreOutput:
      break;
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
                                             InstructionOperand* op) {
  if (op->IsStackSlot()) {
    translation->StoreStackSlot(op->index());
  } else if (op->IsDoubleStackSlot()) {
    translation->StoreDoubleStackSlot(op->index());
  } else if (op->IsRegister()) {
    InstructionOperandConverter converter(this, instr);
    translation->StoreRegister(converter.ToRegister(op));
  } else if (op->IsDoubleRegister()) {
    InstructionOperandConverter converter(this, instr);
    translation->StoreDoubleRegister(converter.ToDoubleRegister(op));
  } else if (op->IsImmediate()) {
    InstructionOperandConverter converter(this, instr);
    Constant constant = converter.ToConstant(op);
    Handle<Object> constant_object;
    switch (constant.type()) {
      case Constant::kInt32:
        constant_object =
            isolate()->factory()->NewNumberFromInt(constant.ToInt32());
        break;
      case Constant::kFloat64:
        constant_object = isolate()->factory()->NewNumber(constant.ToFloat64());
        break;
      case Constant::kHeapObject:
        constant_object = constant.ToHeapObject();
        break;
      default:
        UNREACHABLE();
    }
    int literal_id = DefineDeoptimizationLiteral(constant_object);
    translation->StoreLiteral(literal_id);
  } else {
    UNREACHABLE();
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
                                       FlagsCondition condition) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  UNIMPLEMENTED();
}


void CodeGenerator::AssembleDeoptimizerCall(int deoptimization_id) {
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

#endif  // !V8_TURBOFAN_BACKEND

}  // namespace compiler
}  // namespace internal
}  // namespace v8
