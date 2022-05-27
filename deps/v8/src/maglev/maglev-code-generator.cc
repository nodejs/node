// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-code-generator.h"

#include "src/base/hashmap.h"
#include "src/codegen/code-desc.h"
#include "src/codegen/register.h"
#include "src/codegen/safepoint-table.h"
#include "src/deoptimizer/translation-array.h"
#include "src/execution/frame-constants.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"
#include "src/objects/code-inl.h"
#include "src/utils/identity-map.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm()->

namespace {

template <typename T, size_t... Is>
std::array<T, sizeof...(Is)> repeat(T value, std::index_sequence<Is...>) {
  return {((void)Is, value)...};
}

template <size_t N, typename T>
std::array<T, N> repeat(T value) {
  return repeat<T>(value, std::make_index_sequence<N>());
}

using RegisterMoves = std::array<Register, Register::kNumRegisters>;
using StackToRegisterMoves =
    std::array<compiler::InstructionOperand, Register::kNumRegisters>;

class MaglevCodeGeneratingNodeProcessor {
 public:
  explicit MaglevCodeGeneratingNodeProcessor(MaglevCodeGenState* code_gen_state)
      : code_gen_state_(code_gen_state) {}

  void PreProcessGraph(MaglevCompilationInfo*, Graph* graph) {
    if (FLAG_maglev_break_on_entry) {
      __ int3();
    }

    __ BailoutIfDeoptimized(rbx);

    __ EnterFrame(StackFrame::BASELINE);

    // Save arguments in frame.
    // TODO(leszeks): Consider eliding this frame if we don't make any calls
    // that could clobber these registers.
    __ Push(kContextRegister);
    __ Push(kJSFunctionRegister);              // Callee's JS function.
    __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

    // TODO(v8:7700): Handle TieringState and cached optimized code. See also:
    // LoadTieringStateAndJumpIfNeedsProcessing and
    // MaybeOptimizeCodeOrTailCallOptimizedCodeSlot.

    // Extend rsp by the size of the frame.
    code_gen_state_->set_untagged_slots(graph->untagged_stack_slots());
    code_gen_state_->set_tagged_slots(graph->tagged_stack_slots());
    __ subq(rsp,
            Immediate(code_gen_state_->stack_slots() * kSystemPointerSize));

    // Initialize stack slots.
    // TODO(jgruber): Update logic once the register allocator is further along.
    {
      ASM_CODE_COMMENT_STRING(masm(), "Initializing stack slots");
      __ Move(rax, Immediate(0));
      __ Move(rcx, Immediate(code_gen_state_->stack_slots()));
      __ leaq(rdi, code_gen_state_->TopOfStack());
      __ repstosq();
    }

    // We don't emit proper safepoint data yet; instead, define a single
    // safepoint at the end of the code object.
    // TODO(v8:7700): Add better safepoint handling when we support stack reuse.
    SafepointTableBuilder::Safepoint safepoint =
        safepoint_table_builder()->DefineSafepoint(masm());
    code_gen_state_->DefineSafepointStackSlots(safepoint);
  }

  void PostProcessGraph(MaglevCompilationInfo*, Graph*) {}

  void PreProcessBasicBlock(MaglevCompilationInfo*, BasicBlock* block) {
    if (FLAG_code_comments) {
      std::stringstream ss;
      ss << "-- Block b" << graph_labeller()->BlockId(block);
      __ RecordComment(ss.str());
    }

    __ bind(block->label());
  }

  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state) {
    if (FLAG_code_comments) {
      std::stringstream ss;
      ss << "--   " << graph_labeller()->NodeId(node) << ": "
         << PrintNode(graph_labeller(), node);
      __ RecordComment(ss.str());
    }

    // Emit Phi moves before visiting the control node.
    if (std::is_base_of<UnconditionalControlNode, NodeT>::value) {
      EmitBlockEndGapMoves(node->template Cast<UnconditionalControlNode>(),
                           state);
    }

    node->GenerateCode(code_gen_state_, state);

    if (std::is_base_of<ValueNode, NodeT>::value) {
      ValueNode* value_node = node->template Cast<ValueNode>();
      if (value_node->is_spilled()) {
        compiler::AllocatedOperand source =
            compiler::AllocatedOperand::cast(value_node->result().operand());
        // We shouldn't spill nodes which already output to the stack.
        if (!source.IsAnyStackSlot()) {
          if (FLAG_code_comments) __ RecordComment("--   Spill:");
          if (source.IsRegister()) {
            __ movq(code_gen_state_->GetStackSlot(value_node->spill_slot()),
                    ToRegister(source));
          } else {
            __ Movsd(code_gen_state_->GetStackSlot(value_node->spill_slot()),
                     ToDoubleRegister(source));
          }
        } else {
          // Otherwise, the result source stack slot should be equal to the
          // spill slot.
          DCHECK_EQ(source.index(), value_node->spill_slot().index());
        }
      }
    }
  }

  void EmitSingleParallelMove(Register source, Register target,
                              RegisterMoves& moves) {
    DCHECK(!moves[target.code()].is_valid());
    __ movq(target, source);
    moves[source.code()] = Register::no_reg();
  }

  bool RecursivelyEmitParallelMoveChain(Register chain_start, Register source,
                                        Register target, RegisterMoves& moves) {
    if (target == chain_start) {
      // The target of this move is the start of the move chain -- this
      // means that there is a cycle, and we have to break it by moving
      // the chain start into a temporary.

      __ RecordComment("--   * Cycle");
      EmitSingleParallelMove(target, kScratchRegister, moves);
      EmitSingleParallelMove(source, target, moves);
      return true;
    }
    bool is_cycle = false;
    if (moves[target.code()].is_valid()) {
      is_cycle = RecursivelyEmitParallelMoveChain(chain_start, target,
                                                  moves[target.code()], moves);
    } else {
      __ RecordComment("--   * Chain start");
    }
    if (is_cycle && source == chain_start) {
      EmitSingleParallelMove(kScratchRegister, target, moves);
      __ RecordComment("--   * end cycle");
    } else {
      EmitSingleParallelMove(source, target, moves);
    }
    return is_cycle;
  }

  void EmitParallelMoveChain(Register source, RegisterMoves& moves) {
    Register target = moves[source.code()];
    if (!target.is_valid()) return;

    DCHECK_NE(source, target);
    RecursivelyEmitParallelMoveChain(source, source, target, moves);
  }

  void EmitStackToRegisterGapMove(compiler::InstructionOperand source,
                                  Register target) {
    if (!source.IsAllocated()) return;
    __ movq(target, code_gen_state_->GetStackSlot(
                        compiler::AllocatedOperand::cast(source)));
  }

  void RecordGapMove(compiler::AllocatedOperand source, Register target_reg,
                     RegisterMoves& register_moves,
                     StackToRegisterMoves& stack_to_register_moves) {
    if (source.IsStackSlot()) {
      // For stack->reg moves, don't emit the move yet, but instead record the
      // move in the set of stack-to-register moves, to be executed after the
      // reg->reg parallel moves.
      stack_to_register_moves[target_reg.code()] = source;
    } else {
      // For reg->reg moves, don't emit the move yet, but instead record the
      // move in the set of parallel register moves, to be resolved later.
      Register source_reg = ToRegister(source);
      if (target_reg != source_reg) {
        DCHECK(!register_moves[source_reg.code()].is_valid());
        register_moves[source_reg.code()] = target_reg;
      }
    }
  }

  void RecordGapMove(compiler::AllocatedOperand source,
                     compiler::AllocatedOperand target,
                     RegisterMoves& register_moves,
                     StackToRegisterMoves& stack_to_register_moves) {
    if (target.IsRegister()) {
      RecordGapMove(source, ToRegister(target), register_moves,
                    stack_to_register_moves);
      return;
    }

    // stack->stack and reg->stack moves should be executed before registers are
    // clobbered by reg->reg or stack->reg, so emit them immediately.
    if (source.IsRegister()) {
      Register source_reg = ToRegister(source);
      __ movq(code_gen_state_->GetStackSlot(target), source_reg);
    } else {
      __ movq(kScratchRegister, code_gen_state_->GetStackSlot(source));
      __ movq(code_gen_state_->GetStackSlot(target), kScratchRegister);
    }
  }

  void EmitBlockEndGapMoves(UnconditionalControlNode* node,
                            const ProcessingState& state) {
    BasicBlock* target = node->target();
    if (!target->has_state()) {
      __ RecordComment("--   Target has no state, must be a fallthrough");
      return;
    }

    int predecessor_id = state.block()->predecessor_id();

    // Save register moves in an array, so that we can resolve them as parallel
    // moves. Note that the mapping is:
    //
    //     register_moves[source] = target.
    RegisterMoves register_moves =
        repeat<Register::kNumRegisters>(Register::no_reg());

    // Save stack to register moves in an array, so that we can execute them
    // after the parallel moves have read the register values. Note that the
    // mapping is:
    //
    //     stack_to_register_moves[target] = source.
    StackToRegisterMoves stack_to_register_moves;

    __ RecordComment("--   Gap moves:");

    target->state()->register_state().ForEachGeneralRegister(
        [&](Register reg, RegisterState& state) {
          RegisterMerge* merge;
          if (LoadMergeState(state, &merge)) {
            compiler::AllocatedOperand source = merge->operand(predecessor_id);
            if (FLAG_code_comments) {
              std::stringstream ss;
              ss << "--   * " << source << " → " << reg;
              __ RecordComment(ss.str());
            }
            RecordGapMove(source, reg, register_moves, stack_to_register_moves);
          }
        });

    if (target->has_phi()) {
      Phi::List* phis = target->phis();
      for (Phi* phi : *phis) {
        compiler::AllocatedOperand source = compiler::AllocatedOperand::cast(
            phi->input(state.block()->predecessor_id()).operand());
        compiler::AllocatedOperand target =
            compiler::AllocatedOperand::cast(phi->result().operand());
        if (FLAG_code_comments) {
          std::stringstream ss;
          ss << "--   * " << source << " → " << target << " (n"
             << graph_labeller()->NodeId(phi) << ")";
          __ RecordComment(ss.str());
        }
        RecordGapMove(source, target, register_moves, stack_to_register_moves);
      }
    }

#define EMIT_MOVE_FOR_REG(Name) EmitParallelMoveChain(Name, register_moves);
    ALLOCATABLE_GENERAL_REGISTERS(EMIT_MOVE_FOR_REG)
#undef EMIT_MOVE_FOR_REG

#define EMIT_MOVE_FOR_REG(Name) \
  EmitStackToRegisterGapMove(stack_to_register_moves[Name.code()], Name);
    ALLOCATABLE_GENERAL_REGISTERS(EMIT_MOVE_FOR_REG)
#undef EMIT_MOVE_FOR_REG
  }

  Isolate* isolate() const { return code_gen_state_->isolate(); }
  MacroAssembler* masm() const { return code_gen_state_->masm(); }
  MaglevGraphLabeller* graph_labeller() const {
    return code_gen_state_->graph_labeller();
  }
  SafepointTableBuilder* safepoint_table_builder() const {
    return code_gen_state_->safepoint_table_builder();
  }

 private:
  MaglevCodeGenState* code_gen_state_;
};

}  // namespace

class MaglevCodeGeneratorImpl final {
 public:
  static MaybeHandle<Code> Generate(MaglevCompilationInfo* compilation_info,
                                    Graph* graph) {
    return MaglevCodeGeneratorImpl(compilation_info, graph).Generate();
  }

 private:
  static constexpr int kOptimizedOutConstantIndex = 0;

  MaglevCodeGeneratorImpl(MaglevCompilationInfo* compilation_info, Graph* graph)
      : safepoint_table_builder_(compilation_info->zone()),
        translation_array_builder_(compilation_info->zone()),
        code_gen_state_(compilation_info, safepoint_table_builder()),
        processor_(compilation_info, &code_gen_state_),
        graph_(graph),
        deopt_literals_(compilation_info->isolate()->heap()) {}

  MaybeHandle<Code> Generate() {
    EmitCode();
    if (code_gen_state_.found_unsupported_code_paths()) return {};
    EmitMetadata();
    return BuildCodeObject();
  }

  void EmitCode() {
    processor_.ProcessGraph(graph_);
    EmitDeferredCode();
    EmitDeopts();
  }

  void EmitDeferredCode() {
    for (DeferredCodeInfo* deferred_code : code_gen_state_.deferred_code()) {
      __ RecordComment("-- Deferred block");
      __ bind(&deferred_code->deferred_code_label);
      deferred_code->Generate(&code_gen_state_, &deferred_code->return_label);
      __ Trap();
    }
  }

  void EmitDeopts() {
    deopt_exit_start_offset_ = __ pc_offset();

    // We'll emit the optimized out constant a bunch of times, so to avoid
    // looking it up in the literal map every time, add it now with the fixed
    // offset 0.
    int optimized_out_constant_index =
        GetDeoptLiteral(ReadOnlyRoots(isolate()).optimized_out());
    USE(optimized_out_constant_index);
    DCHECK_EQ(kOptimizedOutConstantIndex, optimized_out_constant_index);

    int deopt_index = 0;

    __ RecordComment("-- Non-lazy deopts");
    for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
      EmitEagerDeopt(deopt_info);

      __ bind(&deopt_info->deopt_entry_label);
      __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Eager, 0,
                               &deopt_info->deopt_entry_label,
                               DeoptimizeKind::kEager, nullptr, nullptr);
      deopt_index++;
    }

    __ RecordComment("-- Lazy deopts");
    int last_updated_safepoint = 0;
    for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
      EmitLazyDeopt(deopt_info);

      __ bind(&deopt_info->deopt_entry_label);
      __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Lazy, 0,
                               &deopt_info->deopt_entry_label,
                               DeoptimizeKind::kLazy, nullptr, nullptr);

      last_updated_safepoint =
          safepoint_table_builder_.UpdateDeoptimizationInfo(
              deopt_info->deopting_call_return_pc,
              deopt_info->deopt_entry_label.pos(), last_updated_safepoint,
              deopt_index);
      deopt_index++;
    }
  }

  const InputLocation* EmitDeoptFrame(const MaglevCompilationUnit& unit,
                                      const CheckpointedInterpreterState& state,
                                      const InputLocation* input_locations) {
    if (state.parent) {
      // Deopt input locations are in the order of deopt frame emission, so
      // update the pointer after emitting the parent frame.
      input_locations =
          EmitDeoptFrame(*unit.caller(), *state.parent, input_locations);
    }

    // Returns are used for updating an accumulator or register after a lazy
    // deopt.
    const int return_offset = 0;
    const int return_count = 0;
    translation_array_builder_.BeginInterpretedFrame(
        state.bytecode_position,
        GetDeoptLiteral(*unit.shared_function_info().object()),
        unit.register_count(), return_offset, return_count);

    return EmitDeoptFrameValues(unit, state.register_frame, input_locations,
                                interpreter::Register::invalid_value());
  }

  void EmitEagerDeopt(EagerDeoptInfo* deopt_info) {
    int frame_count = 1 + deopt_info->unit.inlining_depth();
    int jsframe_count = frame_count;
    int update_feedback_count = 0;
    deopt_info->translation_index = translation_array_builder_.BeginTranslation(
        frame_count, jsframe_count, update_feedback_count);

    EmitDeoptFrame(deopt_info->unit, deopt_info->state,
                   deopt_info->input_locations);
  }

  void EmitLazyDeopt(LazyDeoptInfo* deopt_info) {
    const MaglevCompilationUnit& unit = deopt_info->unit;
    DCHECK_NULL(unit.caller());
    DCHECK_EQ(unit.inlining_depth(), 0);

    int frame_count = 1;
    int jsframe_count = 1;
    int update_feedback_count = 0;
    deopt_info->translation_index = translation_array_builder_.BeginTranslation(
        frame_count, jsframe_count, update_feedback_count);

    // Return offsets are counted from the end of the translation frame, which
    // is the array [parameters..., locals..., accumulator].
    int return_offset;
    if (deopt_info->result_location ==
        interpreter::Register::virtual_accumulator()) {
      return_offset = 0;
    } else if (deopt_info->result_location.is_parameter()) {
      // This is slightly tricky to reason about because of zero indexing and
      // fence post errors. As an example, consider a frame with 2 locals and
      // 2 parameters, where we want argument index 1 -- looking at the array
      // in reverse order we have:
      //   [acc, r1, r0, a1, a0]
      //                  ^
      // and this calculation gives, correctly:
      //   2 + 2 - 1 = 3
      return_offset = unit.register_count() + unit.parameter_count() -
                      deopt_info->result_location.ToParameterIndex();
    } else {
      return_offset =
          unit.register_count() - deopt_info->result_location.index();
    }
    // TODO(leszeks): Support lazy deopts with multiple return values.
    int return_count = 1;
    translation_array_builder_.BeginInterpretedFrame(
        deopt_info->state.bytecode_position,
        GetDeoptLiteral(*unit.shared_function_info().object()),
        unit.register_count(), return_offset, return_count);

    EmitDeoptFrameValues(unit, deopt_info->state.register_frame,
                         deopt_info->input_locations,
                         deopt_info->result_location);
  }

  void EmitDeoptStoreRegister(const compiler::AllocatedOperand& operand,
                              ValueRepresentation repr) {
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder_.StoreRegister(operand.GetRegister());
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder_.StoreInt32Register(operand.GetRegister());
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder_.StoreDoubleRegister(
            operand.GetDoubleRegister());
        break;
    }
  }

  void EmitDeoptStoreStackSlot(const compiler::AllocatedOperand& operand,
                               ValueRepresentation repr) {
    int stack_slot = DeoptStackSlotFromStackSlot(operand);
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder_.StoreStackSlot(stack_slot);
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder_.StoreInt32StackSlot(stack_slot);
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder_.StoreDoubleStackSlot(stack_slot);
        break;
    }
  }

  void EmitDeoptFrameSingleValue(ValueNode* value,
                                 const InputLocation& input_location) {
    const compiler::AllocatedOperand& operand =
        compiler::AllocatedOperand::cast(input_location.operand());
    ValueRepresentation repr = value->properties().value_representation();
    if (operand.IsRegister()) {
      EmitDeoptStoreRegister(operand, repr);
    } else {
      EmitDeoptStoreStackSlot(operand, repr);
    }
  }

  constexpr int DeoptStackSlotIndexFromFPOffset(int offset) {
    return 1 - offset / kSystemPointerSize;
  }

  int DeoptStackSlotFromStackSlot(const compiler::AllocatedOperand& operand) {
    return DeoptStackSlotIndexFromFPOffset(
        code_gen_state_.GetFramePointerOffsetForStackSlot(operand));
  }

  const InputLocation* EmitDeoptFrameValues(
      const MaglevCompilationUnit& compilation_unit,
      const CompactInterpreterFrameState* checkpoint_state,
      const InputLocation* input_locations,
      interpreter::Register result_location) {
    // Closure
    if (compilation_unit.inlining_depth() == 0) {
      int closure_index = DeoptStackSlotIndexFromFPOffset(
          StandardFrameConstants::kFunctionOffset);
      translation_array_builder_.StoreStackSlot(closure_index);
    } else {
      translation_array_builder_.StoreLiteral(
          GetDeoptLiteral(*compilation_unit.function().object()));
    }

    // TODO(leszeks): The input locations array happens to be in the same order
    // as parameters+locals+accumulator are accessed here. We should make this
    // clearer and guard against this invariant failing.
    const InputLocation* input_location = input_locations;

    // Parameters
    {
      int i = 0;
      checkpoint_state->ForEachParameter(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_EQ(reg.ToParameterIndex(), i);
            if (reg != result_location) {
              EmitDeoptFrameSingleValue(value, *input_location);
            } else {
              translation_array_builder_.StoreLiteral(
                  kOptimizedOutConstantIndex);
            }
            i++;
            input_location++;
          });
    }

    // Context
    int context_index =
        DeoptStackSlotIndexFromFPOffset(StandardFrameConstants::kContextOffset);
    translation_array_builder_.StoreStackSlot(context_index);

    // Locals
    {
      int i = 0;
      checkpoint_state->ForEachLocal(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_LE(i, reg.index());
            if (reg == result_location) {
              input_location++;
              return;
            }
            while (i < reg.index()) {
              translation_array_builder_.StoreLiteral(
                  kOptimizedOutConstantIndex);
              i++;
            }
            DCHECK_EQ(i, reg.index());
            EmitDeoptFrameSingleValue(value, *input_location);
            i++;
            input_location++;
          });
      while (i < compilation_unit.register_count()) {
        translation_array_builder_.StoreLiteral(kOptimizedOutConstantIndex);
        i++;
      }
    }

    // Accumulator
    {
      if (checkpoint_state->liveness()->AccumulatorIsLive() &&
          result_location != interpreter::Register::virtual_accumulator()) {
        ValueNode* value = checkpoint_state->accumulator(compilation_unit);
        EmitDeoptFrameSingleValue(value, *input_location);
      } else {
        translation_array_builder_.StoreLiteral(kOptimizedOutConstantIndex);
      }
    }

    return input_location;
  }

  void EmitMetadata() {
    // Final alignment before starting on the metadata section.
    masm()->Align(Code::kMetadataAlignment);

    safepoint_table_builder()->Emit(masm(),
                                    stack_slot_count_with_fixed_frame());
  }

  MaybeHandle<Code> BuildCodeObject() {
    CodeDesc desc;
    static constexpr int kNoHandlerTableOffset = 0;
    masm()->GetCode(isolate(), &desc, safepoint_table_builder(),
                    kNoHandlerTableOffset);
    return Factory::CodeBuilder{isolate(), desc, CodeKind::MAGLEV}
        .set_stack_slots(stack_slot_count_with_fixed_frame())
        .set_deoptimization_data(GenerateDeoptimizationData())
        .TryBuild();
  }

  Handle<DeoptimizationData> GenerateDeoptimizationData() {
    int eager_deopt_count =
        static_cast<int>(code_gen_state_.eager_deopts().size());
    int lazy_deopt_count =
        static_cast<int>(code_gen_state_.lazy_deopts().size());
    int deopt_count = lazy_deopt_count + eager_deopt_count;
    if (deopt_count == 0) {
      return DeoptimizationData::Empty(isolate());
    }
    Handle<DeoptimizationData> data =
        DeoptimizationData::New(isolate(), deopt_count, AllocationType::kOld);

    Handle<TranslationArray> translation_array =
        translation_array_builder_.ToTranslationArray(isolate()->factory());

    data->SetTranslationByteArray(*translation_array);
    // TODO(leszeks): Fix with the real inlined function count.
    data->SetInlinedFunctionCount(Smi::zero());
    // TODO(leszeks): Support optimization IDs
    data->SetOptimizationId(Smi::zero());

    DCHECK_NE(deopt_exit_start_offset_, -1);
    data->SetDeoptExitStart(Smi::FromInt(deopt_exit_start_offset_));
    data->SetEagerDeoptCount(Smi::FromInt(eager_deopt_count));
    data->SetLazyDeoptCount(Smi::FromInt(lazy_deopt_count));

    data->SetSharedFunctionInfo(*code_gen_state_.compilation_info()
                                     ->toplevel_compilation_unit()
                                     ->shared_function_info()
                                     .object());

    Handle<DeoptimizationLiteralArray> literals =
        isolate()->factory()->NewDeoptimizationLiteralArray(
            deopt_literals_.size());
    IdentityMap<int, base::DefaultAllocationPolicy>::IteratableScope iterate(
        &deopt_literals_);
    for (auto it = iterate.begin(); it != iterate.end(); ++it) {
      literals->set(*it.entry(), it.key());
    }
    data->SetLiteralArray(*literals);

    // TODO(leszeks): Fix with the real inlining positions.
    Handle<PodArray<InliningPosition>> inlining_positions =
        PodArray<InliningPosition>::New(isolate(), 0);
    data->SetInliningPositions(*inlining_positions);

    // TODO(leszeks): Fix once we have OSR.
    BytecodeOffset osr_offset = BytecodeOffset::None();
    data->SetOsrBytecodeOffset(Smi::FromInt(osr_offset.ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(-1));

    // Populate deoptimization entries.
    int i = 0;
    for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
      DCHECK_NE(deopt_info->translation_index, -1);
      data->SetBytecodeOffset(i, deopt_info->state.bytecode_position);
      data->SetTranslationIndex(i, Smi::FromInt(deopt_info->translation_index));
      data->SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label.pos()));
#ifdef DEBUG
      data->SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
      i++;
    }
    for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
      DCHECK_NE(deopt_info->translation_index, -1);
      data->SetBytecodeOffset(i, deopt_info->state.bytecode_position);
      data->SetTranslationIndex(i, Smi::FromInt(deopt_info->translation_index));
      data->SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label.pos()));
#ifdef DEBUG
      data->SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
      i++;
    }

    return data;
  }

  int stack_slot_count() const { return code_gen_state_.stack_slots(); }
  int stack_slot_count_with_fixed_frame() const {
    return stack_slot_count() + StandardFrameConstants::kFixedSlotCount;
  }

  Isolate* isolate() const {
    return code_gen_state_.compilation_info()->isolate();
  }
  MacroAssembler* masm() { return code_gen_state_.masm(); }
  SafepointTableBuilder* safepoint_table_builder() {
    return &safepoint_table_builder_;
  }
  TranslationArrayBuilder* translation_array_builder() {
    return &translation_array_builder_;
  }

  int GetDeoptLiteral(Object obj) {
    IdentityMapFindResult<int> res = deopt_literals_.FindOrInsert(obj);
    if (!res.already_exists) {
      DCHECK_EQ(0, *res.entry);
      *res.entry = deopt_literals_.size() - 1;
    }
    return *res.entry;
  }

  SafepointTableBuilder safepoint_table_builder_;
  TranslationArrayBuilder translation_array_builder_;
  MaglevCodeGenState code_gen_state_;
  GraphProcessor<MaglevCodeGeneratingNodeProcessor> processor_;
  Graph* const graph_;
  IdentityMap<int, base::DefaultAllocationPolicy> deopt_literals_;

  int deopt_exit_start_offset_ = -1;
};

// static
MaybeHandle<Code> MaglevCodeGenerator::Generate(
    MaglevCompilationInfo* compilation_info, Graph* graph) {
  return MaglevCodeGeneratorImpl::Generate(compilation_info, graph);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
