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
      lazy_deoptimization_entries_(
          LazyDeoptimizationEntries::allocator_type(code->zone())),
      deoptimization_states_(
          DeoptimizationStates::allocator_type(code->zone())),
      deoptimization_literals_(Literals::allocator_type(code->zone())),
      translations_(code->zone()) {
  deoptimization_states_.resize(code->GetDeoptimizationEntryCount(), NULL);
}


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
  int deopt_count = code()->GetDeoptimizationEntryCount();
  int patch_count = static_cast<int>(lazy_deoptimization_entries_.size());
  if (patch_count == 0 && deopt_count == 0) return;
  Handle<DeoptimizationInputData> data = DeoptimizationInputData::New(
      isolate(), deopt_count, patch_count, TENURED);

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
    FrameStateDescriptor* descriptor = code()->GetDeoptimizationEntry(i);
    data->SetAstId(i, descriptor->bailout_id());
    CHECK_NE(NULL, deoptimization_states_[i]);
    data->SetTranslationIndex(
        i, Smi::FromInt(deoptimization_states_[i]->translation_id_));
    data->SetArgumentsStackHeight(i, Smi::FromInt(0));
    data->SetPc(i, Smi::FromInt(-1));
  }

  // Populate the return address patcher entries.
  for (int i = 0; i < patch_count; ++i) {
    LazyDeoptimizationEntry entry = lazy_deoptimization_entries_[i];
    DCHECK(entry.position_after_call() == entry.continuation()->pos() ||
           IsNopForSmiCodeInlining(code_object, entry.position_after_call(),
                                   entry.continuation()->pos()));
    data->SetReturnAddressPc(i, Smi::FromInt(entry.position_after_call()));
    data->SetPatchedAddressPc(i, Smi::FromInt(entry.deoptimization()->pos()));
  }

  code_object->set_deoptimization_data(*data);
}


void CodeGenerator::RecordLazyDeoptimizationEntry(Instruction* instr) {
  InstructionOperandConverter i(this, instr);

  Label after_call;
  masm()->bind(&after_call);

  // The continuation and deoptimization are the last two inputs:
  BasicBlock* cont_block =
      i.InputBlock(static_cast<int>(instr->InputCount()) - 2);
  BasicBlock* deopt_block =
      i.InputBlock(static_cast<int>(instr->InputCount()) - 1);

  Label* cont_label = code_->GetLabel(cont_block);
  Label* deopt_label = code_->GetLabel(deopt_block);

  lazy_deoptimization_entries_.push_back(
      LazyDeoptimizationEntry(after_call.pos(), cont_label, deopt_label));
}


int CodeGenerator::DefineDeoptimizationLiteral(Handle<Object> literal) {
  int result = static_cast<int>(deoptimization_literals_.size());
  for (unsigned i = 0; i < deoptimization_literals_.size(); ++i) {
    if (deoptimization_literals_[i].is_identical_to(literal)) return i;
  }
  deoptimization_literals_.push_back(literal);
  return result;
}


void CodeGenerator::BuildTranslation(Instruction* instr,
                                     int deoptimization_id) {
  // We should build translation only once.
  DCHECK_EQ(NULL, deoptimization_states_[deoptimization_id]);

  FrameStateDescriptor* descriptor =
      code()->GetDeoptimizationEntry(deoptimization_id);
  Translation translation(&translations_, 1, 1, zone());
  translation.BeginJSFrame(descriptor->bailout_id(),
                           Translation::kSelfLiteralId,
                           descriptor->size() - descriptor->parameters_count());

  for (int i = 0; i < descriptor->size(); i++) {
    AddTranslationForOperand(&translation, instr, instr->InputAt(i));
  }

  deoptimization_states_[deoptimization_id] =
      new (zone()) DeoptimizationState(translation.index());
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
        constant_object =
            isolate()->factory()->NewHeapNumber(constant.ToFloat64());
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


#ifdef DEBUG
bool CodeGenerator::IsNopForSmiCodeInlining(Handle<Code> code, int start_pc,
                                            int end_pc) {
  UNIMPLEMENTED();
  return false;
}
#endif

#endif  // !V8_TURBOFAN_BACKEND

}  // namespace compiler
}  // namespace internal
}  // namespace v8
