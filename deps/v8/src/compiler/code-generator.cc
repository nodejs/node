// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/address-map.h"
#include "src/base/adapters.h"
#include "src/compilation-info.h"
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

CodeGenerator::CodeGenerator(
    Frame* frame, Linkage* linkage, InstructionSequence* code,
    CompilationInfo* info,
    ZoneVector<trap_handler::ProtectedInstructionData>* protected_instructions)
    : frame_access_state_(nullptr),
      linkage_(linkage),
      code_(code),
      unwinding_info_writer_(zone()),
      info_(info),
      labels_(zone()->NewArray<Label>(code->InstructionBlockCount())),
      current_block_(RpoNumber::Invalid()),
      current_source_position_(SourcePosition::Unknown()),
      masm_(info->isolate(), nullptr, 0, CodeObjectRequired::kNo),
      resolver_(this),
      safepoints_(code->zone()),
      handlers_(code->zone()),
      deoptimization_exits_(code->zone()),
      deoptimization_states_(code->zone()),
      deoptimization_literals_(code->zone()),
      inlined_function_count_(0),
      translations_(code->zone()),
      last_lazy_deopt_pc_(0),
      jump_tables_(nullptr),
      ools_(nullptr),
      osr_pc_offset_(-1),
      optimized_out_literal_id_(-1),
      source_position_table_builder_(code->zone(),
                                     info->SourcePositionRecordingMode()),
      protected_instructions_(protected_instructions) {
  for (int i = 0; i < code->InstructionBlockCount(); ++i) {
    new (&labels_[i]) Label;
  }
  CreateFrameAccessState(frame);
}

Isolate* CodeGenerator::isolate() const { return info_->isolate(); }

void CodeGenerator::CreateFrameAccessState(Frame* frame) {
  FinishFrame(frame);
  frame_access_state_ = new (code()->zone()) FrameAccessState(frame);
}

void CodeGenerator::AddProtectedInstruction(int instr_offset,
                                            int landing_offset) {
  if (protected_instructions_ != nullptr) {
    trap_handler::ProtectedInstructionData data = {instr_offset,
                                                   landing_offset};
    protected_instructions_->emplace_back(data);
  }
}

Handle<Code> CodeGenerator::GenerateCode() {
  CompilationInfo* info = this->info();

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in AssemblePrologue).
  FrameScope frame_scope(masm(), StackFrame::MANUAL);

  if (info->is_source_positions_enabled()) {
    SourcePosition source_position(info->shared_info()->start_position());
    AssembleSourcePosition(source_position);
  }

  // Place function entry hook if requested to do so.
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm());
  }
  // Architecture-specific, linkage-specific prologue.
  info->set_prologue_offset(masm()->pc_offset());

  // Define deoptimization literals for all inlined functions.
  DCHECK_EQ(0u, deoptimization_literals_.size());
  for (CompilationInfo::InlinedFunctionHolder& inlined :
       info->inlined_functions()) {
    if (!inlined.shared_info.is_identical_to(info->shared_info())) {
      int index = DefineDeoptimizationLiteral(inlined.shared_info);
      inlined.RegisterInlinedFunctionId(index);
    }
  }
  inlined_function_count_ = deoptimization_literals_.size();

  // Define deoptimization literals for all unoptimized code objects of inlined
  // functions. This ensures unoptimized code is kept alive by optimized code.
  for (const CompilationInfo::InlinedFunctionHolder& inlined :
       info->inlined_functions()) {
    if (!inlined.shared_info.is_identical_to(info->shared_info())) {
      DefineDeoptimizationLiteral(inlined.inlined_code_object_root);
    }
  }

  unwinding_info_writer_.SetNumberOfInstructionBlocks(
      code()->InstructionBlockCount());

  // Assemble all non-deferred blocks, followed by deferred ones.
  for (int deferred = 0; deferred < 2; ++deferred) {
    for (const InstructionBlock* block : code()->instruction_blocks()) {
      if (block->IsDeferred() == (deferred == 0)) {
        continue;
      }
      // Align loop headers on 16-byte boundaries.
      if (block->IsLoopHeader()) masm()->Align(16);
      // Ensure lazy deopt doesn't patch handler entry points.
      if (block->IsHandler()) EnsureSpaceForLazyDeopt();
      // Bind a label for a block.
      current_block_ = block->rpo_number();
      unwinding_info_writer_.BeginInstructionBlock(masm()->pc_offset(), block);
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

      frame_access_state()->MarkHasFrame(block->needs_frame());

      masm()->bind(GetLabel(current_block_));
      if (block->must_construct_frame()) {
        AssembleConstructFrame();
        // We need to setup the root register after we assemble the prologue, to
        // avoid clobbering callee saved registers in case of C linkage and
        // using the roots.
        // TODO(mtrofin): investigate how we can avoid doing this repeatedly.
        if (linkage()->GetIncomingDescriptor()->InitializeRootRegister()) {
          masm()->InitializeRootRegister();
        }
      }

      CodeGenResult result;
      if (FLAG_enable_embedded_constant_pool && !block->needs_frame()) {
        ConstantPoolUnavailableScope constant_pool_unavailable(masm());
        result = AssembleBlock(block);
      } else {
        result = AssembleBlock(block);
      }
      if (result != kSuccess) return Handle<Code>();
      unwinding_info_writer_.EndInstructionBlock(block);
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

  // Assemble all eager deoptimization exits.
  for (DeoptimizationExit* exit : deoptimization_exits_) {
    masm()->bind(exit->label());
    AssembleDeoptimizerCall(exit->deoptimization_id(), Deoptimizer::EAGER,
                            exit->pos());
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

  safepoints()->Emit(masm(), frame()->GetTotalFrameSlotCount());

  unwinding_info_writer_.Finish(masm()->pc_offset());

  Handle<Code> result = v8::internal::CodeGenerator::MakeCodeEpilogue(
      masm(), unwinding_info_writer_.eh_frame_writer(), info, Handle<Object>());
  result->set_is_turbofanned(true);
  result->set_stack_slots(frame()->GetTotalFrameSlotCount());
  result->set_safepoint_table_offset(safepoints()->GetCodeOffset());
  Handle<ByteArray> source_positions =
      source_position_table_builder_.ToSourcePositionTable(
          isolate(), Handle<AbstractCode>::cast(result));
  result->set_source_position_table(*source_positions);

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
  if (info->ShouldEnsureSpaceForLazyDeopt()) {
    Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(result);
  }

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
  for (const InstructionOperand& operand : references->reference_operands()) {
    if (operand.IsStackSlot()) {
      int index = LocationOperand::cast(operand).index();
      DCHECK(index >= 0);
      // We might index values in the fixed part of the frame (i.e. the
      // closure pointer or the context pointer); these are not spill slots
      // and therefore don't work with the SafepointTable currently, but
      // we also don't need to worry about them, since the GC has special
      // knowledge about those fields anyway.
      if (index < stackSlotToSpillSlotDelta) continue;
      safepoint.DefinePointerSlot(index, zone());
    } else if (operand.IsRegister() && (kind & Safepoint::kWithRegisters)) {
      Register reg = LocationOperand::cast(operand).GetRegister();
      safepoint.DefinePointerRegister(reg, zone());
    }
  }
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

CodeGenerator::CodeGenResult CodeGenerator::AssembleBlock(
    const InstructionBlock* block) {
  for (int i = block->code_start(); i < block->code_end(); ++i) {
    Instruction* instr = code()->InstructionAt(i);
    CodeGenResult result = AssembleInstruction(instr, block);
    if (result != kSuccess) return result;
  }
  return kSuccess;
}

bool CodeGenerator::IsValidPush(InstructionOperand source,
                                CodeGenerator::PushTypeFlags push_type) {
  if (source.IsImmediate() &&
      ((push_type & CodeGenerator::kImmediatePush) != 0)) {
    return true;
  }
  if ((source.IsRegister() || source.IsStackSlot()) &&
      ((push_type & CodeGenerator::kScalarPush) != 0)) {
    return true;
  }
  if ((source.IsFloatRegister() || source.IsFloatStackSlot()) &&
      ((push_type & CodeGenerator::kFloat32Push) != 0)) {
    return true;
  }
  if ((source.IsDoubleRegister() || source.IsFloatStackSlot()) &&
      ((push_type & CodeGenerator::kFloat64Push) != 0)) {
    return true;
  }
  return false;
}

void CodeGenerator::GetPushCompatibleMoves(Instruction* instr,
                                           PushTypeFlags push_type,
                                           ZoneVector<MoveOperands*>* pushes) {
  pushes->clear();
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; ++i) {
    Instruction::GapPosition inner_pos =
        static_cast<Instruction::GapPosition>(i);
    ParallelMove* parallel_move = instr->GetParallelMove(inner_pos);
    if (parallel_move != nullptr) {
      for (auto move : *parallel_move) {
        InstructionOperand source = move->source();
        InstructionOperand destination = move->destination();
        int first_push_compatible_index =
            V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0;
        // If there are any moves from slots that will be overridden by pushes,
        // then the full gap resolver must be used since optimization with
        // pushes don't participate in the parallel move and might clobber
        // values needed for the gap resolve.
        if (source.IsStackSlot() &&
            LocationOperand::cast(source).index() >=
                first_push_compatible_index) {
          pushes->clear();
          return;
        }
        // TODO(danno): Right now, only consider moves from the FIRST gap for
        // pushes. Theoretically, we could extract pushes for both gaps (there
        // are cases where this happens), but the logic for that would also have
        // to check to make sure that non-memory inputs to the pushes from the
        // LAST gap don't get clobbered in the FIRST gap.
        if (i == Instruction::FIRST_GAP_POSITION) {
          if (destination.IsStackSlot() &&
              LocationOperand::cast(destination).index() >=
                  first_push_compatible_index) {
            int index = LocationOperand::cast(destination).index();
            if (IsValidPush(source, push_type)) {
              if (index >= static_cast<int>(pushes->size())) {
                pushes->resize(index + 1);
              }
              (*pushes)[index] = move;
            }
          }
        }
      }
    }
  }

  // For now, only support a set of continuous pushes at the end of the list.
  size_t push_count_upper_bound = pushes->size();
  size_t push_begin = push_count_upper_bound;
  for (auto move : base::Reversed(*pushes)) {
    if (move == nullptr) break;
    push_begin--;
  }
  size_t push_count = pushes->size() - push_begin;
  std::copy(pushes->begin() + push_begin,
            pushes->begin() + push_begin + push_count, pushes->begin());
  pushes->resize(push_count);
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleInstruction(
    Instruction* instr, const InstructionBlock* block) {
  int first_unused_stack_slot;
  FlagsMode mode = FlagsModeField::decode(instr->opcode());
  if (mode != kFlags_trap) {
    AssembleSourcePosition(instr);
  }
  bool adjust_stack =
      GetSlotAboveSPBeforeTailCall(instr, &first_unused_stack_slot);
  if (adjust_stack) AssembleTailCallBeforeGap(instr, first_unused_stack_slot);
  AssembleGaps(instr);
  if (adjust_stack) AssembleTailCallAfterGap(instr, first_unused_stack_slot);
  DCHECK_IMPLIES(
      block->must_deconstruct_frame(),
      instr != code()->InstructionAt(block->last_instruction_index()) ||
          instr->IsRet() || instr->IsJump());
  if (instr->IsJump() && block->must_deconstruct_frame()) {
    AssembleDeconstructFrame();
  }
  // Assemble architecture-specific code for the instruction.
  CodeGenResult result = AssembleArchInstruction(instr);
  if (result != kSuccess) return result;

  FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
  switch (mode) {
    case kFlags_branch: {
      // Assemble a branch after this instruction.
      InstructionOperandConverter i(this, instr);
      RpoNumber true_rpo = i.InputRpo(instr->InputCount() - 2);
      RpoNumber false_rpo = i.InputRpo(instr->InputCount() - 1);

      if (true_rpo == false_rpo) {
        // redundant branch.
        if (!IsNextInAssemblyOrder(true_rpo)) {
          AssembleArchJump(true_rpo);
        }
        return kSuccess;
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
      break;
    }
    case kFlags_deoptimize: {
      // Assemble a conditional eager deoptimization after this instruction.
      InstructionOperandConverter i(this, instr);
      size_t frame_state_offset = MiscField::decode(instr->opcode());
      DeoptimizationExit* const exit =
          AddDeoptimizationExit(instr, frame_state_offset);
      Label continue_label;
      BranchInfo branch;
      branch.condition = condition;
      branch.true_label = exit->label();
      branch.false_label = &continue_label;
      branch.fallthru = true;
      // Assemble architecture-specific branch.
      AssembleArchBranch(instr, &branch);
      masm()->bind(&continue_label);
      break;
    }
    case kFlags_set: {
      // Assemble a boolean materialization after this instruction.
      AssembleArchBoolean(instr, condition);
      break;
    }
    case kFlags_trap: {
      AssembleArchTrap(instr, condition);
      break;
    }
    case kFlags_none: {
      break;
    }
  }
  return kSuccess;
}

void CodeGenerator::AssembleSourcePosition(Instruction* instr) {
  SourcePosition source_position = SourcePosition::Unknown();
  if (instr->IsNop() && instr->AreMovesRedundant()) return;
  if (!code()->GetSourcePosition(instr, &source_position)) return;
  AssembleSourcePosition(source_position);
}

void CodeGenerator::AssembleSourcePosition(SourcePosition source_position) {
  if (source_position == current_source_position_) return;
  current_source_position_ = source_position;
  if (!source_position.IsKnown()) return;
  source_position_table_builder_.AddPosition(masm()->pc_offset(),
                                             source_position, false);
  if (FLAG_code_comments) {
    CompilationInfo* info = this->info();
    if (!info->parse_info()) return;
    std::ostringstream buffer;
    buffer << "-- ";
    if (FLAG_trace_turbo) {
      buffer << source_position;
    } else {
      buffer << source_position.InliningStack(info);
    }
    buffer << " --";
    masm()->RecordComment(StrDup(buffer.str().c_str()));
  }
}

bool CodeGenerator::GetSlotAboveSPBeforeTailCall(Instruction* instr,
                                                 int* slot) {
  if (instr->IsTailCall()) {
    InstructionOperandConverter g(this, instr);
    *slot = g.InputInt32(instr->InputCount() - 1);
    return true;
  } else {
    return false;
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

namespace {

Handle<PodArray<InliningPosition>> CreateInliningPositions(
    CompilationInfo* info) {
  const CompilationInfo::InlinedFunctionList& inlined_functions =
      info->inlined_functions();
  if (inlined_functions.size() == 0) {
    return Handle<PodArray<InliningPosition>>::cast(
        info->isolate()->factory()->empty_byte_array());
  }
  Handle<PodArray<InliningPosition>> inl_positions =
      PodArray<InliningPosition>::New(
          info->isolate(), static_cast<int>(inlined_functions.size()), TENURED);
  for (size_t i = 0; i < inlined_functions.size(); ++i) {
    inl_positions->set(static_cast<int>(i), inlined_functions[i].position);
  }
  return inl_positions;
}

}  // namespace

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
    data->SetSharedFunctionInfo(Smi::kZero);
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

  Handle<PodArray<InliningPosition>> inl_pos = CreateInliningPositions(info);
  data->SetInliningPositions(*inl_pos);

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
    data->SetArgumentsStackHeight(i, Smi::kZero);
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
    RpoNumber handler_rpo = i.InputRpo(instr->InputCount() - 1);
    handlers_.push_back({GetLabel(handler_rpo), masm()->pc_offset()});
  }

  if (needs_frame_state) {
    MarkLazyDeoptSite();
    // If the frame state is present, it starts at argument 1 (just after the
    // code address).
    size_t frame_state_offset = 1;
    FrameStateDescriptor* descriptor =
        GetDeoptimizationEntry(instr, frame_state_offset).descriptor();
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

DeoptimizationEntry const& CodeGenerator::GetDeoptimizationEntry(
    Instruction* instr, size_t frame_state_offset) {
  InstructionOperandConverter i(this, instr);
  int const state_id = i.InputInt32(frame_state_offset);
  return code()->GetDeoptimizationEntry(state_id);
}

DeoptimizeReason CodeGenerator::GetDeoptimizationReason(
    int deoptimization_id) const {
  size_t const index = static_cast<size_t>(deoptimization_id);
  DCHECK_LT(index, deoptimization_states_.size());
  return deoptimization_states_[index]->reason();
}

void CodeGenerator::TranslateStateValueDescriptor(
    StateValueDescriptor* desc, StateValueList* nested,
    Translation* translation, InstructionOperandIterator* iter) {
  // Note:
  // If translation is null, we just skip the relevant instruction operands.
  if (desc->IsNested()) {
    if (translation != nullptr) {
      translation->BeginCapturedObject(static_cast<int>(nested->size()));
    }
    for (auto field : *nested) {
      TranslateStateValueDescriptor(field.desc, field.nested, translation,
                                    iter);
    }
  } else if (desc->IsDuplicate()) {
    if (translation != nullptr) {
      translation->DuplicateObject(static_cast<int>(desc->id()));
    }
  } else if (desc->IsPlain()) {
    InstructionOperand* op = iter->Advance();
    if (translation != nullptr) {
      AddTranslationForOperand(translation, iter->instruction(), op,
                               desc->type());
    }
  } else {
    DCHECK(desc->IsOptimizedOut());
    if (translation != nullptr) {
      if (optimized_out_literal_id_ == -1) {
        optimized_out_literal_id_ =
            DefineDeoptimizationLiteral(isolate()->factory()->optimized_out());
      }
      translation->StoreLiteral(optimized_out_literal_id_);
    }
  }
}


void CodeGenerator::TranslateFrameStateDescriptorOperands(
    FrameStateDescriptor* desc, InstructionOperandIterator* iter,
    OutputFrameStateCombine combine, Translation* translation) {
  size_t index = 0;
  StateValueList* values = desc->GetStateValueDescriptors();
  for (StateValueList::iterator it = values->begin(); it != values->end();
       ++it, ++index) {
    StateValueDescriptor* value_desc = (*it).desc;
    if (combine.kind() == OutputFrameStateCombine::kPokeAt) {
      // The result of the call should be placed at position
      // [index_from_top] in the stack (overwriting whatever was
      // previously there).
      size_t index_from_top =
          desc->GetSize(combine) - 1 - combine.GetOffsetToPokeAt();
      if (index >= index_from_top &&
          index < index_from_top + iter->instruction()->OutputCount()) {
        DCHECK_NOT_NULL(translation);
        AddTranslationForOperand(
            translation, iter->instruction(),
            iter->instruction()->OutputAt(index - index_from_top),
            MachineType::AnyTagged());
        // Skip the instruction operands.
        TranslateStateValueDescriptor(value_desc, (*it).nested, nullptr, iter);
        continue;
      }
    }
    TranslateStateValueDescriptor(value_desc, (*it).nested, translation, iter);
  }
  DCHECK_EQ(desc->GetSize(OutputFrameStateCombine::Ignore()), index);

  if (combine.kind() == OutputFrameStateCombine::kPushOutput) {
    DCHECK(combine.GetPushCount() <= iter->instruction()->OutputCount());
    for (size_t output = 0; output < combine.GetPushCount(); output++) {
      // Materialize the result of the call instruction in this slot.
      AddTranslationForOperand(translation, iter->instruction(),
                               iter->instruction()->OutputAt(output),
                               MachineType::AnyTagged());
    }
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
          static_cast<unsigned int>(descriptor->locals_count() + 1));
      break;
    case FrameStateType::kArgumentsAdaptor:
      translation->BeginArgumentsAdaptorFrame(
          shared_info_id,
          static_cast<unsigned int>(descriptor->parameters_count()));
      break;
    case FrameStateType::kTailCallerFunction:
      translation->BeginTailCallerFrame(shared_info_id);
      break;
    case FrameStateType::kConstructStub:
      translation->BeginConstructStubFrame(
          shared_info_id,
          static_cast<unsigned int>(descriptor->parameters_count()));
      break;
    case FrameStateType::kGetterStub:
      translation->BeginGetterStubFrame(shared_info_id);
      break;
    case FrameStateType::kSetterStub:
      translation->BeginSetterStubFrame(shared_info_id);
      break;
  }

  TranslateFrameStateDescriptorOperands(descriptor, iter, state_combine,
                                        translation);
}


int CodeGenerator::BuildTranslation(Instruction* instr, int pc_offset,
                                    size_t frame_state_offset,
                                    OutputFrameStateCombine state_combine) {
  DeoptimizationEntry const& entry =
      GetDeoptimizationEntry(instr, frame_state_offset);
  FrameStateDescriptor* const descriptor = entry.descriptor();
  frame_state_offset++;

  Translation translation(
      &translations_, static_cast<int>(descriptor->GetFrameCount()),
      static_cast<int>(descriptor->GetJSFrameCount()), zone());
  InstructionOperandIterator iter(instr, frame_state_offset);
  BuildTranslationForFrameStateDescriptor(descriptor, &iter, &translation,
                                          state_combine);

  int deoptimization_id = static_cast<int>(deoptimization_states_.size());

  deoptimization_states_.push_back(new (zone()) DeoptimizationState(
      descriptor->bailout_id(), translation.index(), pc_offset,
      entry.reason()));

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
    } else if (IsAnyTagged(type.representation())) {
      translation->StoreStackSlot(LocationOperand::cast(op)->index());
    } else {
      CHECK(false);
    }
  } else if (op->IsFPStackSlot()) {
    if (type.representation() == MachineRepresentation::kFloat64) {
      translation->StoreDoubleStackSlot(LocationOperand::cast(op)->index());
    } else {
      DCHECK_EQ(MachineRepresentation::kFloat32, type.representation());
      translation->StoreFloatStackSlot(LocationOperand::cast(op)->index());
    }
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
    } else if (IsAnyTagged(type.representation())) {
      translation->StoreRegister(converter.ToRegister(op));
    } else {
      CHECK(false);
    }
  } else if (op->IsFPRegister()) {
    InstructionOperandConverter converter(this, instr);
    if (type.representation() == MachineRepresentation::kFloat64) {
      translation->StoreDoubleRegister(converter.ToDoubleRegister(op));
    } else {
      DCHECK_EQ(MachineRepresentation::kFloat32, type.representation());
      translation->StoreFloatRegister(converter.ToFloatRegister(op));
    }
  } else if (op->IsImmediate()) {
    InstructionOperandConverter converter(this, instr);
    Constant constant = converter.ToConstant(op);
    Handle<Object> constant_object;
    switch (constant.type()) {
      case Constant::kInt32:
        if (type.representation() == MachineRepresentation::kTagged ||
            type.representation() == MachineRepresentation::kTaggedSigned) {
          // When pointers are 4 bytes, we can use int32 constants to represent
          // Smis.
          DCHECK_EQ(4, kPointerSize);
          constant_object =
              handle(reinterpret_cast<Smi*>(constant.ToInt32()), isolate());
          DCHECK(constant_object->IsSmi());
        } else if (type.representation() == MachineRepresentation::kBit) {
          if (constant.ToInt32() == 0) {
            constant_object = isolate()->factory()->false_value();
          } else {
            DCHECK_EQ(1, constant.ToInt32());
            constant_object = isolate()->factory()->true_value();
          }
        } else {
          // TODO(jarin,bmeurer): We currently pass in raw pointers to the
          // JSFunction::entry here. We should really consider fixing this.
          DCHECK(type == MachineType::Int32() ||
                 type == MachineType::Uint32() ||
                 type.representation() == MachineRepresentation::kWord32 ||
                 type.representation() == MachineRepresentation::kNone);
          DCHECK(type.representation() != MachineRepresentation::kNone ||
                 constant.ToInt32() == FrameStateDescriptor::kImpossibleValue);

          constant_object =
              isolate()->factory()->NewNumberFromInt(constant.ToInt32());
        }
        break;
      case Constant::kInt64:
        // When pointers are 8 bytes, we can use int64 constants to represent
        // Smis.
        // TODO(jarin,bmeurer): We currently pass in raw pointers to the
        // JSFunction::entry here. We should really consider fixing this.
        DCHECK(type.representation() == MachineRepresentation::kWord64 ||
               type.representation() == MachineRepresentation::kTagged ||
               type.representation() == MachineRepresentation::kTaggedSigned);
        DCHECK_EQ(8, kPointerSize);
        constant_object =
            handle(reinterpret_cast<Smi*>(constant.ToInt64()), isolate());
        DCHECK(constant_object->IsSmi());
        break;
      case Constant::kFloat32:
        if (type.representation() == MachineRepresentation::kTaggedSigned) {
          DCHECK(IsSmiDouble(constant.ToFloat32()));
        } else {
          DCHECK(type.representation() == MachineRepresentation::kFloat32 ||
                 CanBeTaggedPointer(type.representation()));
        }
        constant_object = isolate()->factory()->NewNumber(constant.ToFloat32());
        break;
      case Constant::kFloat64:
        if (type.representation() == MachineRepresentation::kTaggedSigned) {
          DCHECK(IsSmiDouble(constant.ToFloat64()));
        } else {
          DCHECK(type.representation() == MachineRepresentation::kFloat64 ||
                 CanBeTaggedPointer(type.representation()));
        }
        constant_object = isolate()->factory()->NewNumber(constant.ToFloat64());
        break;
      case Constant::kHeapObject:
        DCHECK(CanBeTaggedPointer(type.representation()));
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

DeoptimizationExit* CodeGenerator::AddDeoptimizationExit(
    Instruction* instr, size_t frame_state_offset) {
  int const deoptimization_id = BuildTranslation(
      instr, -1, frame_state_offset, OutputFrameStateCombine::Ignore());
  DeoptimizationExit* const exit = new (zone())
      DeoptimizationExit(deoptimization_id, current_source_position_);
  deoptimization_exits_.push_back(exit);
  return exit;
}

OutOfLineCode::OutOfLineCode(CodeGenerator* gen)
    : frame_(gen->frame()), masm_(gen->masm()), next_(gen->ools_) {
  gen->ools_ = this;
}


OutOfLineCode::~OutOfLineCode() {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
