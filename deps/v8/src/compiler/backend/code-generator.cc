// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/code-generator.h"

#include "src/base/iterator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/string-constants.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/globals.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler.h"
#include "src/diagnostics/eh-frame.h"
#include "src/execution/frames.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/smi.h"
#include "src/utils/address-map.h"

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
    Zone* codegen_zone, Frame* frame, Linkage* linkage,
    InstructionSequence* instructions, OptimizedCompilationInfo* info,
    Isolate* isolate, base::Optional<OsrHelper> osr_helper,
    int start_source_position, JumpOptimizationInfo* jump_opt,
    PoisoningMitigationLevel poisoning_level, const AssemblerOptions& options,
    int32_t builtin_index, size_t max_unoptimized_frame_height,
    size_t max_pushed_argument_count, std::unique_ptr<AssemblerBuffer> buffer)
    : zone_(codegen_zone),
      isolate_(isolate),
      frame_access_state_(nullptr),
      linkage_(linkage),
      instructions_(instructions),
      unwinding_info_writer_(zone()),
      info_(info),
      labels_(zone()->NewArray<Label>(instructions->InstructionBlockCount())),
      current_block_(RpoNumber::Invalid()),
      start_source_position_(start_source_position),
      current_source_position_(SourcePosition::Unknown()),
      tasm_(isolate, options, CodeObjectRequired::kNo, std::move(buffer)),
      resolver_(this),
      safepoints_(zone()),
      handlers_(zone()),
      deoptimization_exits_(zone()),
      deoptimization_literals_(zone()),
      translations_(zone()),
      max_unoptimized_frame_height_(max_unoptimized_frame_height),
      max_pushed_argument_count_(max_pushed_argument_count),
      caller_registers_saved_(false),
      jump_tables_(nullptr),
      ools_(nullptr),
      osr_helper_(std::move(osr_helper)),
      osr_pc_offset_(-1),
      optimized_out_literal_id_(-1),
      source_position_table_builder_(
          SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS),
      protected_instructions_(zone()),
      result_(kSuccess),
      poisoning_level_(poisoning_level),
      block_starts_(zone()),
      instr_starts_(zone()) {
  for (int i = 0; i < instructions->InstructionBlockCount(); ++i) {
    new (&labels_[i]) Label;
  }
  CreateFrameAccessState(frame);
  CHECK_EQ(info->is_osr(), osr_helper_.has_value());
  tasm_.set_jump_optimization_info(jump_opt);
  Code::Kind code_kind = info->code_kind();
  if (code_kind == Code::WASM_FUNCTION ||
      code_kind == Code::WASM_TO_CAPI_FUNCTION ||
      code_kind == Code::WASM_TO_JS_FUNCTION ||
      code_kind == Code::WASM_INTERPRETER_ENTRY ||
      code_kind == Code::JS_TO_WASM_FUNCTION) {
    tasm_.set_abort_hard(true);
  }
  tasm_.set_builtin_index(builtin_index);
}

bool CodeGenerator::wasm_runtime_exception_support() const {
  DCHECK_NOT_NULL(info_);
  return info_->wasm_runtime_exception_support();
}

void CodeGenerator::AddProtectedInstructionLanding(uint32_t instr_offset,
                                                   uint32_t landing_offset) {
  protected_instructions_.push_back({instr_offset, landing_offset});
}

void CodeGenerator::CreateFrameAccessState(Frame* frame) {
  FinishFrame(frame);
  frame_access_state_ = new (zone()) FrameAccessState(frame);
}

bool CodeGenerator::ShouldApplyOffsetToStackCheck(Instruction* instr,
                                                  uint32_t* offset) {
  DCHECK_EQ(instr->arch_opcode(), kArchStackPointerGreaterThan);

  StackCheckKind kind =
      static_cast<StackCheckKind>(MiscField::decode(instr->opcode()));
  if (kind != StackCheckKind::kJSFunctionEntry) return false;

  uint32_t stack_check_offset = *offset = GetStackCheckOffset();
  return stack_check_offset > kStackLimitSlackForDeoptimizationInBytes;
}

uint32_t CodeGenerator::GetStackCheckOffset() {
  if (!frame_access_state()->has_frame()) {
    DCHECK_EQ(max_unoptimized_frame_height_, 0);
    DCHECK_EQ(max_pushed_argument_count_, 0);
    return 0;
  }

  int32_t optimized_frame_height =
      frame()->GetTotalFrameSlotCount() * kSystemPointerSize;
  DCHECK(is_int32(max_unoptimized_frame_height_));
  int32_t signed_max_unoptimized_frame_height =
      static_cast<int32_t>(max_unoptimized_frame_height_);

  // The offset is either the delta between the optimized frames and the
  // interpreted frame, or the maximal number of bytes pushed to the stack
  // while preparing for function calls, whichever is bigger.
  uint32_t frame_height_delta = static_cast<uint32_t>(std::max(
      signed_max_unoptimized_frame_height - optimized_frame_height, 0));
  uint32_t max_pushed_argument_bytes =
      static_cast<uint32_t>(max_pushed_argument_count_ * kSystemPointerSize);
  return std::max(frame_height_delta, max_pushed_argument_bytes);
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    DeoptimizationExit* exit) {
  int deoptimization_id = exit->deoptimization_id();
  if (deoptimization_id > Deoptimizer::kMaxNumberOfEntries) {
    return kTooManyDeoptimizationBailouts;
  }

  DeoptimizeKind deopt_kind = exit->kind();

  DeoptimizeReason deoptimization_reason = exit->reason();
  Address deopt_entry =
      Deoptimizer::GetDeoptimizationEntry(tasm()->isolate(), deopt_kind);
  if (info()->is_source_positions_enabled()) {
    tasm()->RecordDeoptReason(deoptimization_reason, exit->pos(),
                              deoptimization_id);
  }

  if (deopt_kind == DeoptimizeKind::kLazy) {
    tasm()->BindExceptionHandler(exit->label());
  } else {
    ++non_lazy_deopt_count_;
    tasm()->bind(exit->label());
  }

  tasm()->CallForDeoptimization(deopt_entry, deoptimization_id, exit->label(),
                                deopt_kind);
  exit->set_emitted();
  return kSuccess;
}

void CodeGenerator::MaybeEmitOutOfLineConstantPool() {
  tasm()->MaybeEmitOutOfLineConstantPool();
}

void CodeGenerator::AssembleCode() {
  OptimizedCompilationInfo* info = this->info();

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in AssemblePrologue).
  FrameScope frame_scope(tasm(), StackFrame::MANUAL);

  if (info->is_source_positions_enabled()) {
    AssembleSourcePosition(start_source_position());
  }
  offsets_info_.code_start_register_check = tasm()->pc_offset();

  tasm()->CodeEntry();

  // Check that {kJavaScriptCallCodeStartRegister} has been set correctly.
  if (FLAG_debug_code && (info->code_kind() == Code::OPTIMIZED_FUNCTION ||
                          info->code_kind() == Code::BYTECODE_HANDLER)) {
    tasm()->RecordComment("-- Prologue: check code start register --");
    AssembleCodeStartRegisterCheck();
  }

  offsets_info_.deopt_check = tasm()->pc_offset();
  // We want to bailout only from JS functions, which are the only ones
  // that are optimized.
  if (info->IsOptimizing()) {
    DCHECK(linkage()->GetIncomingDescriptor()->IsJSFunctionCall());
    tasm()->RecordComment("-- Prologue: check for deoptimization --");
    BailoutIfDeoptimized();
  }

  offsets_info_.init_poison = tasm()->pc_offset();
  InitializeSpeculationPoison();

  // Define deoptimization literals for all inlined functions.
  DCHECK_EQ(0u, deoptimization_literals_.size());
  for (OptimizedCompilationInfo::InlinedFunctionHolder& inlined :
       info->inlined_functions()) {
    if (!inlined.shared_info.equals(info->shared_info())) {
      int index = DefineDeoptimizationLiteral(
          DeoptimizationLiteral(inlined.shared_info));
      inlined.RegisterInlinedFunctionId(index);
    }
  }
  inlined_function_count_ = deoptimization_literals_.size();

  // Define deoptimization literals for all BytecodeArrays to which we might
  // deopt to ensure they are strongly held by the optimized code.
  if (info->has_bytecode_array()) {
    DefineDeoptimizationLiteral(DeoptimizationLiteral(info->bytecode_array()));
  }
  for (OptimizedCompilationInfo::InlinedFunctionHolder& inlined :
       info->inlined_functions()) {
    DefineDeoptimizationLiteral(DeoptimizationLiteral(inlined.bytecode_array));
  }

  unwinding_info_writer_.SetNumberOfInstructionBlocks(
      instructions()->InstructionBlockCount());

  if (info->trace_turbo_json_enabled()) {
    block_starts_.assign(instructions()->instruction_blocks().size(), -1);
    instr_starts_.assign(instructions()->instructions().size(), {});
  }
  // Assemble instructions in assembly order.
  offsets_info_.blocks_start = tasm()->pc_offset();
  for (const InstructionBlock* block : instructions()->ao_blocks()) {
    // Align loop headers on vendor recommended boundaries.
    if (block->ShouldAlign() && !tasm()->jump_optimization_info()) {
      tasm()->CodeTargetAlign();
    }
    if (info->trace_turbo_json_enabled()) {
      block_starts_[block->rpo_number().ToInt()] = tasm()->pc_offset();
    }
    // Bind a label for a block.
    current_block_ = block->rpo_number();
    unwinding_info_writer_.BeginInstructionBlock(tasm()->pc_offset(), block);
    if (FLAG_code_comments) {
      std::ostringstream buffer;
      buffer << "-- B" << block->rpo_number().ToInt() << " start";
      if (block->IsDeferred()) buffer << " (deferred)";
      if (!block->needs_frame()) buffer << " (no frame)";
      if (block->must_construct_frame()) buffer << " (construct frame)";
      if (block->must_deconstruct_frame()) buffer << " (deconstruct frame)";

      if (block->IsLoopHeader()) {
        buffer << " (loop up to " << block->loop_end().ToInt() << ")";
      }
      if (block->loop_header().IsValid()) {
        buffer << " (in loop " << block->loop_header().ToInt() << ")";
      }
      buffer << " --";
      tasm()->RecordComment(buffer.str().c_str());
    }

    frame_access_state()->MarkHasFrame(block->needs_frame());

    tasm()->bind(GetLabel(current_block_));

    TryInsertBranchPoisoning(block);

    if (block->must_construct_frame()) {
      AssembleConstructFrame();
      // We need to setup the root register after we assemble the prologue, to
      // avoid clobbering callee saved registers in case of C linkage and
      // using the roots.
      // TODO(mtrofin): investigate how we can avoid doing this repeatedly.
      if (linkage()->GetIncomingDescriptor()->InitializeRootRegister()) {
        tasm()->InitializeRootRegister();
      }
    }

    if (FLAG_enable_embedded_constant_pool && !block->needs_frame()) {
      ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
      result_ = AssembleBlock(block);
    } else {
      result_ = AssembleBlock(block);
    }
    if (result_ != kSuccess) return;
    unwinding_info_writer_.EndInstructionBlock(block);
  }

  // Assemble all out-of-line code.
  offsets_info_.out_of_line_code = tasm()->pc_offset();
  if (ools_) {
    tasm()->RecordComment("-- Out of line code --");
    for (OutOfLineCode* ool = ools_; ool; ool = ool->next()) {
      tasm()->bind(ool->entry());
      ool->Generate();
      if (ool->exit()->is_bound()) tasm()->jmp(ool->exit());
    }
  }

  // This nop operation is needed to ensure that the trampoline is not
  // confused with the pc of the call before deoptimization.
  // The test regress/regress-259 is an example of where we need it.
  tasm()->nop();

  // For some targets, we must make sure that constant and veneer pools are
  // emitted before emitting the deoptimization exits.
  PrepareForDeoptimizationExits(static_cast<int>(deoptimization_exits_.size()));

  if (Deoptimizer::kSupportsFixedDeoptExitSizes) {
    deopt_exit_start_offset_ = tasm()->pc_offset();
  }

  // Assemble deoptimization exits.
  offsets_info_.deoptimization_exits = tasm()->pc_offset();
  int last_updated = 0;
  // We sort the deoptimization exits here so that the lazy ones will
  // be visited last. We need this as on architectures where
  // Deoptimizer::kSupportsFixedDeoptExitSizes is true, lazy deopts
  // might need additional instructions.
  auto cmp = [](const DeoptimizationExit* a, const DeoptimizationExit* b) {
    static_assert(DeoptimizeKind::kLazy > DeoptimizeKind::kEager,
                  "lazy deopts are expected to be emitted last");
    static_assert(DeoptimizeKind::kLazy > DeoptimizeKind::kSoft,
                  "lazy deopts are expected to be emitted last");
    if (a->kind() != b->kind()) {
      return a->kind() < b->kind();
    }
    return a->pc_offset() < b->pc_offset();
  };
  if (Deoptimizer::kSupportsFixedDeoptExitSizes) {
    std::sort(deoptimization_exits_.begin(), deoptimization_exits_.end(), cmp);
  }

  for (DeoptimizationExit* exit : deoptimization_exits_) {
    if (exit->emitted()) continue;
    if (Deoptimizer::kSupportsFixedDeoptExitSizes) {
      exit->set_deoptimization_id(next_deoptimization_id_++);
    }
    result_ = AssembleDeoptimizerCall(exit);
    if (result_ != kSuccess) return;

    // UpdateDeoptimizationInfo expects lazy deopts to be visited in pc_offset
    // order, which is always the case since they are added to
    // deoptimization_exits_ in that order, and the optional sort operation
    // above preserves that order.
    if (exit->kind() == DeoptimizeKind::kLazy) {
      int trampoline_pc = exit->label()->pos();
      last_updated = safepoints()->UpdateDeoptimizationInfo(
          exit->pc_offset(), trampoline_pc, last_updated,
          exit->deoptimization_id());
    }
  }

  offsets_info_.pools = tasm()->pc_offset();
  // TODO(jgruber): Move all inlined metadata generation into a new,
  // architecture-independent version of FinishCode. Currently, this includes
  // the safepoint table, handler table, constant pool, and code comments, in
  // that order.
  FinishCode();

  offsets_info_.jump_tables = tasm()->pc_offset();
  // Emit the jump tables.
  if (jump_tables_) {
    tasm()->Align(kSystemPointerSize);
    for (JumpTable* table = jump_tables_; table; table = table->next()) {
      tasm()->bind(table->label());
      AssembleJumpTable(table->targets(), table->target_count());
    }
  }

  // The PerfJitLogger logs code up until here, excluding the safepoint
  // table. Resolve the unwinding info now so it is aware of the same code
  // size as reported by perf.
  unwinding_info_writer_.Finish(tasm()->pc_offset());

  safepoints()->Emit(tasm(), frame()->GetTotalFrameSlotCount());

  // Emit the exception handler table.
  if (!handlers_.empty()) {
    handler_table_offset_ = HandlerTable::EmitReturnTableStart(tasm());
    for (size_t i = 0; i < handlers_.size(); ++i) {
      HandlerTable::EmitReturnEntry(tasm(), handlers_[i].pc_offset,
                                    handlers_[i].handler->pos());
    }
  }

  tasm()->MaybeEmitOutOfLineConstantPool();
  tasm()->FinalizeJumpOptimizationInfo();

  result_ = kSuccess;
}

void CodeGenerator::TryInsertBranchPoisoning(const InstructionBlock* block) {
  // See if our predecessor was a basic block terminated by a branch_and_poison
  // instruction. If yes, then perform the masking based on the flags.
  if (block->PredecessorCount() != 1) return;
  RpoNumber pred_rpo = (block->predecessors())[0];
  const InstructionBlock* pred = instructions()->InstructionBlockAt(pred_rpo);
  if (pred->code_start() == pred->code_end()) return;
  Instruction* instr = instructions()->InstructionAt(pred->code_end() - 1);
  FlagsMode mode = FlagsModeField::decode(instr->opcode());
  switch (mode) {
    case kFlags_branch_and_poison: {
      BranchInfo branch;
      RpoNumber target = ComputeBranchInfo(&branch, instr);
      if (!target.IsValid()) {
        // Non-trivial branch, add the masking code.
        FlagsCondition condition = branch.condition;
        if (branch.false_label == GetLabel(block->rpo_number())) {
          condition = NegateFlagsCondition(condition);
        }
        AssembleBranchPoisoning(condition, instr);
      }
      break;
    }
    case kFlags_deoptimize_and_poison: {
      UNREACHABLE();
    }
    default:
      break;
  }
}

void CodeGenerator::AssembleArchBinarySearchSwitchRange(
    Register input, RpoNumber def_block, std::pair<int32_t, Label*>* begin,
    std::pair<int32_t, Label*>* end) {
  if (end - begin < kBinarySearchSwitchMinimalCases) {
    while (begin != end) {
      tasm()->JumpIfEqual(input, begin->first, begin->second);
      ++begin;
    }
    AssembleArchJump(def_block);
    return;
  }
  auto middle = begin + (end - begin) / 2;
  Label less_label;
  tasm()->JumpIfLessThan(input, middle->first, &less_label);
  AssembleArchBinarySearchSwitchRange(input, def_block, middle, end);
  tasm()->bind(&less_label);
  AssembleArchBinarySearchSwitchRange(input, def_block, begin, middle);
}

OwnedVector<byte> CodeGenerator::GetSourcePositionTable() {
  return source_position_table_builder_.ToSourcePositionTableVector();
}

OwnedVector<byte> CodeGenerator::GetProtectedInstructionsData() {
  return OwnedVector<byte>::Of(
      Vector<byte>::cast(VectorOf(protected_instructions_)));
}

MaybeHandle<Code> CodeGenerator::FinalizeCode() {
  if (result_ != kSuccess) {
    tasm()->AbortedCodeGeneration();
    return MaybeHandle<Code>();
  }

  // Allocate the source position table.
  Handle<ByteArray> source_positions =
      source_position_table_builder_.ToSourcePositionTable(isolate());

  // Allocate deoptimization data.
  Handle<DeoptimizationData> deopt_data = GenerateDeoptimizationData();

  // Allocate and install the code.
  CodeDesc desc;
  tasm()->GetCode(isolate(), &desc, safepoints(), handler_table_offset_);

#if defined(V8_OS_WIN64)
  if (Builtins::IsBuiltinId(info_->builtin_index())) {
    isolate_->SetBuiltinUnwindData(info_->builtin_index(),
                                   tasm()->GetUnwindInfo());
  }
#endif  // V8_OS_WIN64

  if (unwinding_info_writer_.eh_frame_writer()) {
    unwinding_info_writer_.eh_frame_writer()->GetEhFrame(&desc);
  }

  MaybeHandle<Code> maybe_code =
      Factory::CodeBuilder(isolate(), desc, info()->code_kind())
          .set_builtin_index(info()->builtin_index())
          .set_source_position_table(source_positions)
          .set_deoptimization_data(deopt_data)
          .set_is_turbofanned()
          .set_stack_slots(frame()->GetTotalFrameSlotCount())
          .TryBuild();

  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    tasm()->AbortedCodeGeneration();
    return MaybeHandle<Code>();
  }

  isolate()->counters()->total_compiled_code_size()->Increment(
      code->raw_instruction_size());

  LOG_CODE_EVENT(isolate(),
                 CodeLinePosInfoRecordEvent(code->raw_instruction_start(),
                                            *source_positions));

  return code;
}

bool CodeGenerator::IsNextInAssemblyOrder(RpoNumber block) const {
  return instructions()
      ->InstructionBlockAt(current_block_)
      ->ao_number()
      .IsNext(instructions()->InstructionBlockAt(block)->ao_number());
}

void CodeGenerator::RecordSafepoint(ReferenceMap* references,
                                    Safepoint::DeoptMode deopt_mode) {
  Safepoint safepoint = safepoints()->DefineSafepoint(tasm(), deopt_mode);
  int stackSlotToSpillSlotDelta =
      frame()->GetTotalFrameSlotCount() - frame()->GetSpillSlotCount();
  for (const InstructionOperand& operand : references->reference_operands()) {
    if (operand.IsStackSlot()) {
      int index = LocationOperand::cast(operand).index();
      DCHECK_LE(0, index);
      // We might index values in the fixed part of the frame (i.e. the
      // closure pointer or the context pointer); these are not spill slots
      // and therefore don't work with the SafepointTable currently, but
      // we also don't need to worry about them, since the GC has special
      // knowledge about those fields anyway.
      if (index < stackSlotToSpillSlotDelta) continue;
      safepoint.DefinePointerSlot(index);
    }
  }
}

bool CodeGenerator::IsMaterializableFromRoot(Handle<HeapObject> object,
                                             RootIndex* index_return) {
  const CallDescriptor* incoming_descriptor =
      linkage()->GetIncomingDescriptor();
  if (incoming_descriptor->flags() & CallDescriptor::kCanUseRoots) {
    return isolate()->roots_table().IsRootHandle(object, index_return) &&
           RootsTable::IsImmortalImmovable(*index_return);
  }
  return false;
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleBlock(
    const InstructionBlock* block) {
  if (block->IsHandler()) {
    tasm()->ExceptionHandler();
  }
  for (int i = block->code_start(); i < block->code_end(); ++i) {
    CodeGenResult result = AssembleInstruction(i, block);
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
  if (source.IsRegister() &&
      ((push_type & CodeGenerator::kRegisterPush) != 0)) {
    return true;
  }
  if (source.IsStackSlot() &&
      ((push_type & CodeGenerator::kStackSlotPush) != 0)) {
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
        if (source.IsStackSlot() && LocationOperand::cast(source).index() >=
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

CodeGenerator::MoveType::Type CodeGenerator::MoveType::InferMove(
    InstructionOperand* source, InstructionOperand* destination) {
  if (source->IsConstant()) {
    if (destination->IsAnyRegister()) {
      return MoveType::kConstantToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kConstantToStack;
    }
  }
  DCHECK(LocationOperand::cast(source)->IsCompatible(
      LocationOperand::cast(destination)));
  if (source->IsAnyRegister()) {
    if (destination->IsAnyRegister()) {
      return MoveType::kRegisterToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kRegisterToStack;
    }
  } else {
    DCHECK(source->IsAnyStackSlot());
    if (destination->IsAnyRegister()) {
      return MoveType::kStackToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kStackToStack;
    }
  }
}

CodeGenerator::MoveType::Type CodeGenerator::MoveType::InferSwap(
    InstructionOperand* source, InstructionOperand* destination) {
  DCHECK(LocationOperand::cast(source)->IsCompatible(
      LocationOperand::cast(destination)));
  if (source->IsAnyRegister()) {
    if (destination->IsAnyRegister()) {
      return MoveType::kRegisterToRegister;
    } else {
      DCHECK(destination->IsAnyStackSlot());
      return MoveType::kRegisterToStack;
    }
  } else {
    DCHECK(source->IsAnyStackSlot());
    DCHECK(destination->IsAnyStackSlot());
    return MoveType::kStackToStack;
  }
}

RpoNumber CodeGenerator::ComputeBranchInfo(BranchInfo* branch,
                                           Instruction* instr) {
  // Assemble a branch after this instruction.
  InstructionOperandConverter i(this, instr);
  RpoNumber true_rpo = i.InputRpo(instr->InputCount() - 2);
  RpoNumber false_rpo = i.InputRpo(instr->InputCount() - 1);

  if (true_rpo == false_rpo) {
    return true_rpo;
  }
  FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
  if (IsNextInAssemblyOrder(true_rpo)) {
    // true block is next, can fall through if condition negated.
    std::swap(true_rpo, false_rpo);
    condition = NegateFlagsCondition(condition);
  }
  branch->condition = condition;
  branch->true_label = GetLabel(true_rpo);
  branch->false_label = GetLabel(false_rpo);
  branch->fallthru = IsNextInAssemblyOrder(false_rpo);
  return RpoNumber::Invalid();
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleInstruction(
    int instruction_index, const InstructionBlock* block) {
  Instruction* instr = instructions()->InstructionAt(instruction_index);
  if (info()->trace_turbo_json_enabled()) {
    instr_starts_[instruction_index].gap_pc_offset = tasm()->pc_offset();
  }
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
      instr != instructions()->InstructionAt(block->last_instruction_index()) ||
          instr->IsRet() || instr->IsJump());
  if (instr->IsJump() && block->must_deconstruct_frame()) {
    AssembleDeconstructFrame();
  }
  if (info()->trace_turbo_json_enabled()) {
    instr_starts_[instruction_index].arch_instr_pc_offset = tasm()->pc_offset();
  }
  // Assemble architecture-specific code for the instruction.
  CodeGenResult result = AssembleArchInstruction(instr);
  if (result != kSuccess) return result;

  if (info()->trace_turbo_json_enabled()) {
    instr_starts_[instruction_index].condition_pc_offset = tasm()->pc_offset();
  }

  FlagsCondition condition = FlagsConditionField::decode(instr->opcode());
  switch (mode) {
    case kFlags_branch:
    case kFlags_branch_and_poison: {
      BranchInfo branch;
      RpoNumber target = ComputeBranchInfo(&branch, instr);
      if (target.IsValid()) {
        // redundant branch.
        if (!IsNextInAssemblyOrder(target)) {
          AssembleArchJump(target);
        }
        return kSuccess;
      }
      // Assemble architecture-specific branch.
      AssembleArchBranch(instr, &branch);
      break;
    }
    case kFlags_deoptimize:
    case kFlags_deoptimize_and_poison: {
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
      AssembleArchDeoptBranch(instr, &branch);
      tasm()->bind(&continue_label);
      if (mode == kFlags_deoptimize_and_poison) {
        AssembleBranchPoisoning(NegateFlagsCondition(branch.condition), instr);
      }
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

  // TODO(jarin) We should thread the flag through rather than set it.
  if (instr->IsCall()) {
    ResetSpeculationPoison();
  }

  return kSuccess;
}

void CodeGenerator::AssembleSourcePosition(Instruction* instr) {
  SourcePosition source_position = SourcePosition::Unknown();
  if (instr->IsNop() && instr->AreMovesRedundant()) return;
  if (!instructions()->GetSourcePosition(instr, &source_position)) return;
  AssembleSourcePosition(source_position);
}

void CodeGenerator::AssembleSourcePosition(SourcePosition source_position) {
  if (source_position == current_source_position_) return;
  current_source_position_ = source_position;
  if (!source_position.IsKnown()) return;
  source_position_table_builder_.AddPosition(tasm()->pc_offset(),
                                             source_position, false);
  if (FLAG_code_comments) {
    OptimizedCompilationInfo* info = this->info();
    if (info->IsNotOptimizedFunctionOrWasmFunction()) return;
    std::ostringstream buffer;
    buffer << "-- ";
    // Turbolizer only needs the source position, as it can reconstruct
    // the inlining stack from other information.
    if (info->trace_turbo_json_enabled() || !tasm()->isolate() ||
        tasm()->isolate()->concurrent_recompilation_enabled()) {
      buffer << source_position;
    } else {
      AllowHeapAllocation allocation;
      AllowHandleAllocation handles;
      AllowHandleDereference deref;
      buffer << source_position.InliningStack(info);
    }
    buffer << " --";
    tasm()->RecordComment(buffer.str().c_str());
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

StubCallMode CodeGenerator::DetermineStubCallMode() const {
  Code::Kind code_kind = info()->code_kind();
  return (code_kind == Code::WASM_FUNCTION ||
          code_kind == Code::WASM_TO_CAPI_FUNCTION ||
          code_kind == Code::WASM_TO_JS_FUNCTION)
             ? StubCallMode::kCallWasmRuntimeStub
             : StubCallMode::kCallCodeObject;
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
    OptimizedCompilationInfo* info, Isolate* isolate) {
  const OptimizedCompilationInfo::InlinedFunctionList& inlined_functions =
      info->inlined_functions();
  if (inlined_functions.size() == 0) {
    return Handle<PodArray<InliningPosition>>::cast(
        isolate->factory()->empty_byte_array());
  }
  Handle<PodArray<InliningPosition>> inl_positions =
      PodArray<InliningPosition>::New(
          isolate, static_cast<int>(inlined_functions.size()),
          AllocationType::kOld);
  for (size_t i = 0; i < inlined_functions.size(); ++i) {
    inl_positions->set(static_cast<int>(i), inlined_functions[i].position);
  }
  return inl_positions;
}

}  // namespace

Handle<DeoptimizationData> CodeGenerator::GenerateDeoptimizationData() {
  OptimizedCompilationInfo* info = this->info();
  int deopt_count = static_cast<int>(deoptimization_exits_.size());
  if (deopt_count == 0 && !info->is_osr()) {
    return DeoptimizationData::Empty(isolate());
  }
  Handle<DeoptimizationData> data =
      DeoptimizationData::New(isolate(), deopt_count, AllocationType::kOld);

  Handle<ByteArray> translation_array =
      translations_.CreateByteArray(isolate()->factory());

  data->SetTranslationByteArray(*translation_array);
  data->SetInlinedFunctionCount(
      Smi::FromInt(static_cast<int>(inlined_function_count_)));
  data->SetOptimizationId(Smi::FromInt(info->optimization_id()));

  data->SetDeoptExitStart(Smi::FromInt(deopt_exit_start_offset_));
  data->SetNonLazyDeoptCount(Smi::FromInt(non_lazy_deopt_count_));

  if (info->has_shared_info()) {
    data->SetSharedFunctionInfo(*info->shared_info());
  } else {
    data->SetSharedFunctionInfo(Smi::zero());
  }

  Handle<FixedArray> literals = isolate()->factory()->NewFixedArray(
      static_cast<int>(deoptimization_literals_.size()), AllocationType::kOld);
  for (unsigned i = 0; i < deoptimization_literals_.size(); i++) {
    Handle<Object> object = deoptimization_literals_[i].Reify(isolate());
    CHECK(!object.is_null());
    literals->set(i, *object);
  }
  data->SetLiteralArray(*literals);

  Handle<PodArray<InliningPosition>> inl_pos =
      CreateInliningPositions(info, isolate());
  data->SetInliningPositions(*inl_pos);

  if (info->is_osr()) {
    DCHECK_LE(0, osr_pc_offset_);
    data->SetOsrBytecodeOffset(Smi::FromInt(info_->osr_offset().ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(osr_pc_offset_));
  } else {
    BailoutId osr_offset = BailoutId::None();
    data->SetOsrBytecodeOffset(Smi::FromInt(osr_offset.ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(-1));
  }

  // Populate deoptimization entries.
  for (int i = 0; i < deopt_count; i++) {
    DeoptimizationExit* deoptimization_exit = deoptimization_exits_[i];
    CHECK_NOT_NULL(deoptimization_exit);
    DCHECK_EQ(i, deoptimization_exit->deoptimization_id());
    data->SetBytecodeOffset(i, deoptimization_exit->bailout_id());
    data->SetTranslationIndex(
        i, Smi::FromInt(deoptimization_exit->translation_id()));
    data->SetPc(i, Smi::FromInt(deoptimization_exit->pc_offset()));
  }

  return data;
}

Label* CodeGenerator::AddJumpTable(Label** targets, size_t target_count) {
  jump_tables_ = new (zone()) JumpTable(jump_tables_, targets, target_count);
  return jump_tables_->label();
}

void CodeGenerator::RecordCallPosition(Instruction* instr) {
  CallDescriptor::Flags flags(MiscField::decode(instr->opcode()));

  bool needs_frame_state = (flags & CallDescriptor::kNeedsFrameState);

  RecordSafepoint(instr->reference_map(), needs_frame_state
                                              ? Safepoint::kLazyDeopt
                                              : Safepoint::kNoLazyDeopt);

  if (flags & CallDescriptor::kHasExceptionHandler) {
    InstructionOperandConverter i(this, instr);
    RpoNumber handler_rpo = i.InputRpo(instr->InputCount() - 1);
    DCHECK(instructions()->InstructionBlockAt(handler_rpo)->IsHandler());
    handlers_.push_back({GetLabel(handler_rpo), tasm()->pc_offset()});
  }

  if (needs_frame_state) {
    MarkLazyDeoptSite();
    // If the frame state is present, it starts at argument 2 - after
    // the code address and the poison-alias index.
    size_t frame_state_offset = 2;
    FrameStateDescriptor* descriptor =
        GetDeoptimizationEntry(instr, frame_state_offset).descriptor();
    int pc_offset = tasm()->pc_offset();
    BuildTranslation(instr, pc_offset, frame_state_offset,
                     descriptor->state_combine());
  }
}

int CodeGenerator::DefineDeoptimizationLiteral(DeoptimizationLiteral literal) {
  int result = static_cast<int>(deoptimization_literals_.size());
  for (unsigned i = 0; i < deoptimization_literals_.size(); ++i) {
    if (deoptimization_literals_[i] == literal) return i;
  }
  deoptimization_literals_.push_back(literal);
  return result;
}

DeoptimizationEntry const& CodeGenerator::GetDeoptimizationEntry(
    Instruction* instr, size_t frame_state_offset) {
  InstructionOperandConverter i(this, instr);
  int const state_id = i.InputInt32(frame_state_offset);
  return instructions()->GetDeoptimizationEntry(state_id);
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
  } else if (desc->IsArgumentsElements()) {
    if (translation != nullptr) {
      translation->ArgumentsElements(desc->arguments_type());
    }
  } else if (desc->IsArgumentsLength()) {
    if (translation != nullptr) {
      translation->ArgumentsLength(desc->arguments_type());
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
        optimized_out_literal_id_ = DefineDeoptimizationLiteral(
            DeoptimizationLiteral(isolate()->factory()->optimized_out()));
      }
      translation->StoreLiteral(optimized_out_literal_id_);
    }
  }
}

void CodeGenerator::TranslateFrameStateDescriptorOperands(
    FrameStateDescriptor* desc, InstructionOperandIterator* iter,
    Translation* translation) {
  size_t index = 0;
  StateValueList* values = desc->GetStateValueDescriptors();
  for (StateValueList::iterator it = values->begin(); it != values->end();
       ++it, ++index) {
    TranslateStateValueDescriptor((*it).desc, (*it).nested, translation, iter);
  }
  DCHECK_EQ(desc->GetSize(), index);
}

void CodeGenerator::BuildTranslationForFrameStateDescriptor(
    FrameStateDescriptor* descriptor, InstructionOperandIterator* iter,
    Translation* translation, OutputFrameStateCombine state_combine) {
  // Outer-most state must be added to translation first.
  if (descriptor->outer_state() != nullptr) {
    BuildTranslationForFrameStateDescriptor(descriptor->outer_state(), iter,
                                            translation, state_combine);
  }

  Handle<SharedFunctionInfo> shared_info;
  if (!descriptor->shared_info().ToHandle(&shared_info)) {
    if (!info()->has_shared_info()) {
      return;  // Stub with no SharedFunctionInfo.
    }
    shared_info = info()->shared_info();
  }

  const BailoutId bailout_id = descriptor->bailout_id();
  const int shared_info_id =
      DefineDeoptimizationLiteral(DeoptimizationLiteral(shared_info));
  const unsigned int height =
      static_cast<unsigned int>(descriptor->GetHeight());

  switch (descriptor->type()) {
    case FrameStateType::kInterpretedFunction: {
      int return_offset = 0;
      int return_count = 0;
      if (!state_combine.IsOutputIgnored()) {
        return_offset = static_cast<int>(state_combine.GetOffsetToPokeAt());
        return_count = static_cast<int>(iter->instruction()->OutputCount());
      }
      translation->BeginInterpretedFrame(bailout_id, shared_info_id, height,
                                         return_offset, return_count);
      break;
    }
    case FrameStateType::kArgumentsAdaptor:
      translation->BeginArgumentsAdaptorFrame(shared_info_id, height);
      break;
    case FrameStateType::kConstructStub:
      DCHECK(bailout_id.IsValidForConstructStub());
      translation->BeginConstructStubFrame(bailout_id, shared_info_id, height);
      break;
    case FrameStateType::kBuiltinContinuation: {
      translation->BeginBuiltinContinuationFrame(bailout_id, shared_info_id,
                                                 height);
      break;
    }
    case FrameStateType::kJavaScriptBuiltinContinuation: {
      translation->BeginJavaScriptBuiltinContinuationFrame(
          bailout_id, shared_info_id, height);
      break;
    }
    case FrameStateType::kJavaScriptBuiltinContinuationWithCatch: {
      translation->BeginJavaScriptBuiltinContinuationWithCatchFrame(
          bailout_id, shared_info_id, height);
      break;
    }
  }

  TranslateFrameStateDescriptorOperands(descriptor, iter, translation);
}

DeoptimizationExit* CodeGenerator::BuildTranslation(
    Instruction* instr, int pc_offset, size_t frame_state_offset,
    OutputFrameStateCombine state_combine) {
  DeoptimizationEntry const& entry =
      GetDeoptimizationEntry(instr, frame_state_offset);
  FrameStateDescriptor* const descriptor = entry.descriptor();
  frame_state_offset++;

  int update_feedback_count = entry.feedback().IsValid() ? 1 : 0;
  Translation translation(&translations_,
                          static_cast<int>(descriptor->GetFrameCount()),
                          static_cast<int>(descriptor->GetJSFrameCount()),
                          update_feedback_count, zone());
  if (entry.feedback().IsValid()) {
    DeoptimizationLiteral literal =
        DeoptimizationLiteral(entry.feedback().vector);
    int literal_id = DefineDeoptimizationLiteral(literal);
    translation.AddUpdateFeedback(literal_id, entry.feedback().slot.ToInt());
  }
  InstructionOperandIterator iter(instr, frame_state_offset);
  BuildTranslationForFrameStateDescriptor(descriptor, &iter, &translation,
                                          state_combine);

  DeoptimizationExit* const exit = new (zone()) DeoptimizationExit(
      current_source_position_, descriptor->bailout_id(), translation.index(),
      pc_offset, entry.kind(), entry.reason());

  if (!Deoptimizer::kSupportsFixedDeoptExitSizes) {
    exit->set_deoptimization_id(next_deoptimization_id_++);
  }

  deoptimization_exits_.push_back(exit);
  return exit;
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
    } else if (type == MachineType::Int64()) {
      translation->StoreInt64StackSlot(LocationOperand::cast(op)->index());
    } else {
#if defined(V8_COMPRESS_POINTERS)
      CHECK(MachineRepresentation::kTagged == type.representation() ||
            MachineRepresentation::kCompressed == type.representation());
#else
      CHECK(MachineRepresentation::kTagged == type.representation());
#endif
      translation->StoreStackSlot(LocationOperand::cast(op)->index());
    }
  } else if (op->IsFPStackSlot()) {
    if (type.representation() == MachineRepresentation::kFloat64) {
      translation->StoreDoubleStackSlot(LocationOperand::cast(op)->index());
    } else {
      CHECK_EQ(MachineRepresentation::kFloat32, type.representation());
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
    } else if (type == MachineType::Int64()) {
      translation->StoreInt64Register(converter.ToRegister(op));
    } else {
#if defined(V8_COMPRESS_POINTERS)
      CHECK(MachineRepresentation::kTagged == type.representation() ||
            MachineRepresentation::kCompressed == type.representation());
#else
      CHECK(MachineRepresentation::kTagged == type.representation());
#endif
      translation->StoreRegister(converter.ToRegister(op));
    }
  } else if (op->IsFPRegister()) {
    InstructionOperandConverter converter(this, instr);
    if (type.representation() == MachineRepresentation::kFloat64) {
      translation->StoreDoubleRegister(converter.ToDoubleRegister(op));
    } else {
      CHECK_EQ(MachineRepresentation::kFloat32, type.representation());
      translation->StoreFloatRegister(converter.ToFloatRegister(op));
    }
  } else {
    CHECK(op->IsImmediate());
    InstructionOperandConverter converter(this, instr);
    Constant constant = converter.ToConstant(op);
    DeoptimizationLiteral literal;
    switch (constant.type()) {
      case Constant::kInt32:
        if (type.representation() == MachineRepresentation::kTagged) {
          // When pointers are 4 bytes, we can use int32 constants to represent
          // Smis.
          DCHECK_EQ(4, kSystemPointerSize);
          Smi smi(static_cast<Address>(constant.ToInt32()));
          DCHECK(smi.IsSmi());
          literal = DeoptimizationLiteral(smi.value());
        } else if (type.representation() == MachineRepresentation::kBit) {
          if (constant.ToInt32() == 0) {
            literal =
                DeoptimizationLiteral(isolate()->factory()->false_value());
          } else {
            DCHECK_EQ(1, constant.ToInt32());
            literal = DeoptimizationLiteral(isolate()->factory()->true_value());
          }
        } else {
          DCHECK(type == MachineType::Int32() ||
                 type == MachineType::Uint32() ||
                 type.representation() == MachineRepresentation::kWord32 ||
                 type.representation() == MachineRepresentation::kNone);
          DCHECK(type.representation() != MachineRepresentation::kNone ||
                 constant.ToInt32() == FrameStateDescriptor::kImpossibleValue);
          if (type == MachineType::Uint32()) {
            literal = DeoptimizationLiteral(
                static_cast<uint32_t>(constant.ToInt32()));
          } else {
            literal = DeoptimizationLiteral(constant.ToInt32());
          }
        }
        break;
      case Constant::kInt64:
        DCHECK_EQ(8, kSystemPointerSize);
        if (type.representation() == MachineRepresentation::kWord64) {
          literal =
              DeoptimizationLiteral(static_cast<double>(constant.ToInt64()));
        } else {
          // When pointers are 8 bytes, we can use int64 constants to represent
          // Smis.
          DCHECK_EQ(MachineRepresentation::kTagged, type.representation());
          Smi smi(static_cast<Address>(constant.ToInt64()));
          DCHECK(smi.IsSmi());
          literal = DeoptimizationLiteral(smi.value());
        }
        break;
      case Constant::kFloat32:
        DCHECK(type.representation() == MachineRepresentation::kFloat32 ||
               type.representation() == MachineRepresentation::kTagged);
        literal = DeoptimizationLiteral(constant.ToFloat32());
        break;
      case Constant::kFloat64:
        DCHECK(type.representation() == MachineRepresentation::kFloat64 ||
               type.representation() == MachineRepresentation::kTagged);
        literal = DeoptimizationLiteral(constant.ToFloat64().value());
        break;
      case Constant::kHeapObject:
        DCHECK_EQ(MachineRepresentation::kTagged, type.representation());
        literal = DeoptimizationLiteral(constant.ToHeapObject());
        break;
      case Constant::kCompressedHeapObject:
        DCHECK_EQ(MachineType::AnyTagged(), type);
        literal = DeoptimizationLiteral(constant.ToHeapObject());
        break;
      case Constant::kDelayedStringConstant:
        DCHECK_EQ(MachineRepresentation::kTagged, type.representation());
        literal = DeoptimizationLiteral(constant.ToDelayedStringConstant());
        break;
      default:
        UNREACHABLE();
    }
    if (literal.object().equals(info()->closure())) {
      translation->StoreJSFrameFunction();
    } else {
      int literal_id = DefineDeoptimizationLiteral(literal);
      translation->StoreLiteral(literal_id);
    }
  }
}

void CodeGenerator::MarkLazyDeoptSite() {
  last_lazy_deopt_pc_ = tasm()->pc_offset();
}

DeoptimizationExit* CodeGenerator::AddDeoptimizationExit(
    Instruction* instr, size_t frame_state_offset) {
  return BuildTranslation(instr, -1, frame_state_offset,
                          OutputFrameStateCombine::Ignore());
}

void CodeGenerator::InitializeSpeculationPoison() {
  if (poisoning_level_ == PoisoningMitigationLevel::kDontPoison) return;

  // Initialize {kSpeculationPoisonRegister} either by comparing the expected
  // with the actual call target, or by unconditionally using {-1} initially.
  // Masking register arguments with it only makes sense in the first case.
  if (info()->called_with_code_start_register()) {
    tasm()->RecordComment("-- Prologue: generate speculation poison --");
    GenerateSpeculationPoisonFromCodeStartRegister();
    if (info()->is_poisoning_register_arguments()) {
      AssembleRegisterArgumentPoisoning();
    }
  } else {
    ResetSpeculationPoison();
  }
}

void CodeGenerator::ResetSpeculationPoison() {
  if (poisoning_level_ != PoisoningMitigationLevel::kDontPoison) {
    tasm()->ResetSpeculationPoisonRegister();
  }
}

OutOfLineCode::OutOfLineCode(CodeGenerator* gen)
    : frame_(gen->frame()), tasm_(gen->tasm()), next_(gen->ools_) {
  gen->ools_ = this;
}

OutOfLineCode::~OutOfLineCode() = default;

Handle<Object> DeoptimizationLiteral::Reify(Isolate* isolate) const {
  switch (kind_) {
    case DeoptimizationLiteralKind::kObject: {
      return object_;
    }
    case DeoptimizationLiteralKind::kNumber: {
      return isolate->factory()->NewNumber(number_);
    }
    case DeoptimizationLiteralKind::kString: {
      return string_->AllocateStringConstant(isolate);
    }
  }
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
