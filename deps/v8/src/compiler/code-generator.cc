// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/address-map.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/frames-inl.h"

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
    : frame_access_state_(new (code->zone()) FrameAccessState(frame)),
      linkage_(linkage),
      code_(code),
      info_(info),
      labels_(zone()->NewArray<Label>(code->InstructionBlockCount())),
      current_block_(RpoNumber::Invalid()),
      current_source_position_(SourcePosition::Unknown()),
      masm_(info->isolate(), nullptr, 0, CodeObjectRequired::kYes),
      resolver_(this),
      safepoints_(code->zone()),
      handlers_(code->zone()),
      deoptimization_states_(code->zone()),
      deoptimization_literals_(code->zone()),
      inlined_function_count_(0),
      translations_(code->zone()),
      last_lazy_deopt_pc_(0),
      jump_tables_(nullptr),
      ools_(nullptr),
      osr_pc_offset_(-1) {
  for (int i = 0; i < code->InstructionBlockCount(); ++i) {
    new (&labels_[i]) Label;
  }
  if (code->ContainsCall()) {
    frame->MarkNeedsFrame();
  }
}


Handle<Code> CodeGenerator::GenerateCode() {
  CompilationInfo* info = this->info();

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in AssemblePrologue).
  FrameScope frame_scope(masm(), StackFrame::MANUAL);

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

  // Define deoptimization literals for all inlined functions.
  DCHECK_EQ(0u, deoptimization_literals_.size());
  for (auto& inlined : info->inlined_functions()) {
    if (!inlined.shared_info.is_identical_to(info->shared_info())) {
      DefineDeoptimizationLiteral(inlined.shared_info);
    }
  }
  inlined_function_count_ = deoptimization_literals_.size();

  // Define deoptimization literals for all unoptimized code objects of inlined
  // functions. This ensures unoptimized code is kept alive by optimized code.
  for (auto& inlined : info->inlined_functions()) {
    if (!inlined.shared_info.is_identical_to(info->shared_info())) {
      DefineDeoptimizationLiteral(inlined.inlined_code_object_root);
    }
  }

  // Assemble all non-deferred blocks, followed by deferred ones.
  for (int deferred = 0; deferred < 2; ++deferred) {
    for (auto const block : code()->instruction_blocks()) {
      if (block->IsDeferred() == (deferred == 0)) {
        continue;
      }
      // Align loop headers on 16-byte boundaries.
      if (block->IsLoopHeader()) masm()->Align(16);
      // Ensure lazy deopt doesn't patch handler entry points.
      if (block->IsHandler()) EnsureSpaceForLazyDeopt();
      // Bind a label for a block.
      current_block_ = block->rpo_number();
      if (FLAG_code_comments) {
        // TODO(titzer): these code comments are a giant memory leak.
        Vector<char> buffer = Vector<char>::New(200);
        char* buffer_start = buffer.start();

        int next = SNPrintF(
            buffer, "-- B%d start%s%s%s%s", block->rpo_number().ToInt(),
            block->IsDeferred() ? " (deferred)" : "",
            block->needs_frame() ? "" : " (no frame)",
            block->must_construct_frame() ? " (construct frame)" : "",
            block->must_deconstruct_frame() ? " (deconstruct frame)" : "");

        buffer = buffer.SubVector(next, buffer.length());

        if (block->IsLoopHeader()) {
          next =
              SNPrintF(buffer, " (loop up to %d)", block->loop_end().ToInt());
          buffer = buffer.SubVector(next, buffer.length());
        }
        if (block->loop_header().IsValid()) {
          next =
              SNPrintF(buffer, " (in loop %d)", block->loop_header().ToInt());
          buffer = buffer.SubVector(next, buffer.length());
        }
        SNPrintF(buffer, " --");
        masm()->RecordComment(buffer_start);
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
  if (info->ShouldEnsureSpaceForLazyDeopt()) {
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

  Handle<Code> result =
      v8::internal::CodeGenerator::MakeCodeEpilogue(masm(), info);
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
      int position = handlers_[i].handler->pos();
      HandlerTable::CatchPrediction prediction = handlers_[i].caught_locally
                                                     ? HandlerTable::CAUGHT
                                                     : HandlerTable::UNCAUGHT;
      table->SetReturnOffset(static_cast<int>(i), handlers_[i].pc_offset);
      table->SetReturnHandler(static_cast<int>(i), position, prediction);
    }
    result->set_handler_table(*table);
  }

  PopulateDeoptimizationData(result);

  // Ensure there is space for lazy deoptimization in the relocation info.
  if (info->ShouldEnsureSpaceForLazyDeopt()) {
    Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(result);
  }

  // Emit a code line info recording stop event.
  void* line_info = recorder->DetachJITHandlerData();
  LOG_CODE_EVENT(isolate(), CodeEndLinePosInfoRecordEvent(*result, line_info));

  return result;
}


bool CodeGenerator::IsNextInAssemblyOrder(RpoNumber block) const {
  return code()
      ->InstructionBlockAt(current_block_)
      ->ao_number()
      .IsNext(code()->InstructionBlockAt(block)->ao_number());
}


void CodeGenerator::RecordSafepoint(ReferenceMap* references,
                                    Safepoint::Kind kind, int arguments,
                                    Safepoint::DeoptMode deopt_mode) {
  Safepoint safepoint =
      safepoints()->DefineSafepoint(masm(), kind, arguments, deopt_mode);
  int stackSlotToSpillSlotDelta =
      frame()->GetTotalFrameSlotCount() - frame()->GetSpillSlotCount();
  for (auto& operand : references->reference_operands()) {
    if (operand.IsStackSlot()) {
      int index = LocationOperand::cast(operand).index();
      DCHECK(index >= 0);
      // Safepoint table indices are 0-based from the beginning of the spill
      // slot area, adjust appropriately.
      index -= stackSlotToSpillSlotDelta;
      safepoint.DefinePointerSlot(index, zone());
    } else if (operand.IsRegister() && (kind & Safepoint::kWithRegisters)) {
      Register reg = LocationOperand::cast(operand).GetRegister();
      safepoint.DefinePointerRegister(reg, zone());
    }
  }
}


bool CodeGenerator::IsMaterializableFromFrame(Handle<HeapObject> object,
                                              int* offset_return) {
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    if (info()->has_context() && object.is_identical_to(info()->context()) &&
        !info()->is_osr()) {
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
  const CallDescriptor* incoming_descriptor =
      linkage()->GetIncomingDescriptor();
  if (incoming_descriptor->flags() & CallDescriptor::kCanUseRoots) {
    RootIndexMap map(isolate());
    int root_index = map.Lookup(*object);
    if (root_index != RootIndexMap::kInvalidRootIndex) {
      *index_return = static_cast<Heap::RootListIndex>(root_index);
      return true;
    }
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
  current_source_position_ = source_position;
  if (source_position.IsUnknown()) return;
  int code_pos = source_position.raw();
  masm()->positions_recorder()->RecordPosition(code_pos);
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
  data->SetInlinedFunctionCount(
      Smi::FromInt(static_cast<int>(inlined_function_count_)));
  data->SetOptimizationId(Smi::FromInt(info->optimization_id()));

  if (info->has_shared_info()) {
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
    bool caught = flags & CallDescriptor::kHasLocalCatchHandler;
    RpoNumber handler_rpo = i.InputRpo(instr->InputCount() - 1);
    handlers_.push_back({caught, GetLabel(handler_rpo), masm()->pc_offset()});
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


void CodeGenerator::TranslateStateValueDescriptor(
    StateValueDescriptor* desc, Translation* translation,
    InstructionOperandIterator* iter) {
  if (desc->IsNested()) {
    translation->BeginCapturedObject(static_cast<int>(desc->size()));
    for (size_t index = 0; index < desc->fields().size(); index++) {
      TranslateStateValueDescriptor(&desc->fields()[index], translation, iter);
    }
  } else if (desc->IsDuplicate()) {
    translation->DuplicateObject(static_cast<int>(desc->id()));
  } else {
    DCHECK(desc->IsPlain());
    AddTranslationForOperand(translation, iter->instruction(), iter->Advance(),
                             desc->type());
  }
}


void CodeGenerator::TranslateFrameStateDescriptorOperands(
    FrameStateDescriptor* desc, InstructionOperandIterator* iter,
    OutputFrameStateCombine combine, Translation* translation) {
  for (size_t index = 0; index < desc->GetSize(combine); index++) {
    switch (combine.kind()) {
      case OutputFrameStateCombine::kPushOutput: {
        DCHECK(combine.GetPushCount() <= iter->instruction()->OutputCount());
        size_t size_without_output =
            desc->GetSize(OutputFrameStateCombine::Ignore());
        // If the index is past the existing stack items in values_.
        if (index >= size_without_output) {
          // Materialize the result of the call instruction in this slot.
          AddTranslationForOperand(
              translation, iter->instruction(),
              iter->instruction()->OutputAt(index - size_without_output),
              MachineType::AnyTagged());
          continue;
        }
        break;
      }
      case OutputFrameStateCombine::kPokeAt:
        // The result of the call should be placed at position
        // [index_from_top] in the stack (overwriting whatever was
        // previously there).
        size_t index_from_top =
            desc->GetSize(combine) - 1 - combine.GetOffsetToPokeAt();
        if (index >= index_from_top &&
            index < index_from_top + iter->instruction()->OutputCount()) {
          AddTranslationForOperand(
              translation, iter->instruction(),
              iter->instruction()->OutputAt(index - index_from_top),
              MachineType::AnyTagged());
          iter->Advance();  // We do not use this input, but we need to
                            // advace, as the input got replaced.
          continue;
        }
        break;
    }
    StateValueDescriptor* value_desc = desc->GetStateValueDescriptor();
    TranslateStateValueDescriptor(&value_desc->fields()[index], translation,
                                  iter);
  }
}


void CodeGenerator::BuildTranslationForFrameStateDescriptor(
    FrameStateDescriptor* descriptor, InstructionOperandIterator* iter,
    Translation* translation, OutputFrameStateCombine state_combine) {
  // Outer-most state must be added to translation first.
  if (descriptor->outer_state() != nullptr) {
    BuildTranslationForFrameStateDescriptor(descriptor->outer_state(), iter,
                                            translation,
                                            OutputFrameStateCombine::Ignore());
  }

  Handle<SharedFunctionInfo> shared_info;
  if (!descriptor->shared_info().ToHandle(&shared_info)) {
    if (!info()->has_shared_info()) {
      return;  // Stub with no SharedFunctionInfo.
    }
    shared_info = info()->shared_info();
  }
  int shared_info_id = DefineDeoptimizationLiteral(shared_info);

  switch (descriptor->type()) {
    case FrameStateType::kJavaScriptFunction:
      translation->BeginJSFrame(
          descriptor->bailout_id(), shared_info_id,
          static_cast<unsigned int>(descriptor->GetSize(state_combine) -
                                    (1 + descriptor->parameters_count())));
      break;
    case FrameStateType::kInterpretedFunction:
      translation->BeginInterpretedFrame(
          descriptor->bailout_id(), shared_info_id,
          static_cast<unsigned int>(descriptor->locals_count()));
      break;
    case FrameStateType::kArgumentsAdaptor:
      translation->BeginArgumentsAdaptorFrame(
          shared_info_id,
          static_cast<unsigned int>(descriptor->parameters_count()));
      break;
    case FrameStateType::kConstructStub:
      translation->BeginConstructStubFrame(
          shared_info_id,
          static_cast<unsigned int>(descriptor->parameters_count()));
      break;
  }

  TranslateFrameStateDescriptorOperands(descriptor, iter, state_combine,
                                        translation);
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
  InstructionOperandIterator iter(instr, frame_state_offset);
  BuildTranslationForFrameStateDescriptor(descriptor, &iter, &translation,
                                          state_combine);

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
    if (type.representation() == MachineRepresentation::kBit) {
      translation->StoreBoolStackSlot(LocationOperand::cast(op)->index());
    } else if (type == MachineType::Int8() || type == MachineType::Int16() ||
               type == MachineType::Int32()) {
      translation->StoreInt32StackSlot(LocationOperand::cast(op)->index());
    } else if (type == MachineType::Uint8() || type == MachineType::Uint16() ||
               type == MachineType::Uint32()) {
      translation->StoreUint32StackSlot(LocationOperand::cast(op)->index());
    } else if (type.representation() == MachineRepresentation::kTagged) {
      translation->StoreStackSlot(LocationOperand::cast(op)->index());
    } else {
      CHECK(false);
    }
  } else if (op->IsDoubleStackSlot()) {
    DCHECK(IsFloatingPoint(type.representation()));
    translation->StoreDoubleStackSlot(LocationOperand::cast(op)->index());
  } else if (op->IsRegister()) {
    InstructionOperandConverter converter(this, instr);
    if (type.representation() == MachineRepresentation::kBit) {
      translation->StoreBoolRegister(converter.ToRegister(op));
    } else if (type == MachineType::Int8() || type == MachineType::Int16() ||
               type == MachineType::Int32()) {
      translation->StoreInt32Register(converter.ToRegister(op));
    } else if (type == MachineType::Uint8() || type == MachineType::Uint16() ||
               type == MachineType::Uint32()) {
      translation->StoreUint32Register(converter.ToRegister(op));
    } else if (type.representation() == MachineRepresentation::kTagged) {
      translation->StoreRegister(converter.ToRegister(op));
    } else {
      CHECK(false);
    }
  } else if (op->IsDoubleRegister()) {
    DCHECK(IsFloatingPoint(type.representation()));
    InstructionOperandConverter converter(this, instr);
    translation->StoreDoubleRegister(converter.ToDoubleRegister(op));
  } else if (op->IsImmediate()) {
    InstructionOperandConverter converter(this, instr);
    Constant constant = converter.ToConstant(op);
    Handle<Object> constant_object;
    switch (constant.type()) {
      case Constant::kInt32:
        DCHECK(type == MachineType::Int32() || type == MachineType::Uint32() ||
               type.representation() == MachineRepresentation::kBit);
        constant_object =
            isolate()->factory()->NewNumberFromInt(constant.ToInt32());
        break;
      case Constant::kFloat32:
        DCHECK(type.representation() == MachineRepresentation::kFloat32 ||
               type.representation() == MachineRepresentation::kTagged);
        constant_object = isolate()->factory()->NewNumber(constant.ToFloat32());
        break;
      case Constant::kFloat64:
        DCHECK(type.representation() == MachineRepresentation::kFloat64 ||
               type.representation() == MachineRepresentation::kTagged);
        constant_object = isolate()->factory()->NewNumber(constant.ToFloat64());
        break;
      case Constant::kHeapObject:
        DCHECK(type.representation() == MachineRepresentation::kTagged);
        constant_object = constant.ToHeapObject();
        break;
      default:
        CHECK(false);
    }
    if (constant_object.is_identical_to(info()->closure())) {
      translation->StoreJSFrameFunction();
    } else {
      int literal_id = DefineDeoptimizationLiteral(constant_object);
      translation->StoreLiteral(literal_id);
    }
  } else {
    CHECK(false);
  }
}


void CodeGenerator::MarkLazyDeoptSite() {
  last_lazy_deopt_pc_ = masm()->pc_offset();
}


int CodeGenerator::TailCallFrameStackSlotDelta(int stack_param_delta) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int spill_slots = frame()->GetSpillSlotCount();
  bool has_frame = descriptor->IsJSFunctionCall() || spill_slots > 0;
  // Leave the PC on the stack on platforms that have that as part of their ABI
  int pc_slots = V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0;
  int sp_slot_delta =
      has_frame ? (frame()->GetTotalFrameSlotCount() - pc_slots) : 0;
  // Discard only slots that won't be used by new parameters.
  sp_slot_delta += stack_param_delta;
  return sp_slot_delta;
}


OutOfLineCode::OutOfLineCode(CodeGenerator* gen)
    : frame_(gen->frame()), masm_(gen->masm()), next_(gen->ools_) {
  gen->ools_ = this;
}


OutOfLineCode::~OutOfLineCode() {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
