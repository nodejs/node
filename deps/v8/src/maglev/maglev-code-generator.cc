// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-code-generator.h"

#include "src/base/hashmap.h"
#include "src/codegen/code-desc.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/safepoint-table.h"
#include "src/codegen/source-position.h"
#include "src/codegen/x64/register-x64.h"
#include "src/common/globals.h"
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

using RegisterMoves = std::array<RegList, Register::kNumRegisters>;
using RegisterReloads = std::array<ValueNode*, Register::kNumRegisters>;

class MaglevCodeGeneratingNodeProcessor {
 public:
  explicit MaglevCodeGeneratingNodeProcessor(MaglevCodeGenState* code_gen_state)
      : code_gen_state_(code_gen_state) {}

  void PreProcessGraph(MaglevCompilationInfo*, Graph* graph) {
    if (FLAG_maglev_break_on_entry) {
      __ int3();
    }

    __ BailoutIfDeoptimized(rbx);

    __ EnterFrame(StackFrame::MAGLEV);

    // Save arguments in frame.
    // TODO(leszeks): Consider eliding this frame if we don't make any calls
    // that could clobber these registers.
    __ Push(kContextRegister);
    __ Push(kJSFunctionRegister);              // Callee's JS function.
    __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

    // TODO(v8:7700): Handle TieringState and cached optimized code. See also:
    // LoadTieringStateAndJumpIfNeedsProcessing and
    // MaybeOptimizeCodeOrTailCallOptimizedCodeSlot.

    code_gen_state_->set_untagged_slots(graph->untagged_stack_slots());
    code_gen_state_->set_tagged_slots(graph->tagged_stack_slots());

    // Initialize stack slots.
    if (graph->tagged_stack_slots() > 0) {
      ASM_CODE_COMMENT_STRING(masm(), "Initializing stack slots");
      // TODO(leszeks): Consider filling with xmm + movdqa instead.
      __ Move(rax, Immediate(0));

      // Magic value. Experimentally, an unroll size of 8 doesn't seem any worse
      // than fully unrolled pushes.
      const int kLoopUnrollSize = 8;
      int tagged_slots = graph->tagged_stack_slots();
      if (tagged_slots < 2 * kLoopUnrollSize) {
        // If the frame is small enough, just unroll the frame fill completely.
        for (int i = 0; i < tagged_slots; ++i) {
          __ pushq(rax);
        }
      } else {
        // Extract the first few slots to round to the unroll size.
        int first_slots = tagged_slots % kLoopUnrollSize;
        for (int i = 0; i < first_slots; ++i) {
          __ pushq(rax);
        }
        __ Move(rbx, Immediate(tagged_slots / kLoopUnrollSize));
        // We enter the loop unconditionally, so make sure we need to loop at
        // least once.
        DCHECK_GT(tagged_slots / kLoopUnrollSize, 0);
        Label loop;
        __ bind(&loop);
        for (int i = 0; i < kLoopUnrollSize; ++i) {
          __ pushq(rax);
        }
        __ decl(rbx);
        __ j(greater, &loop);
      }
    }
    if (graph->untagged_stack_slots() > 0) {
      // Extend rsp by the size of the remaining untagged part of the frame, no
      // need to initialise these.
      __ subq(rsp,
              Immediate(graph->untagged_stack_slots() * kSystemPointerSize));
    }

    // Define a single safepoint at the end of the code object.
    safepoint_table_builder()->DefineSafepoint(masm());
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

  void EmitSingleParallelMove(Register source, RegList targets,
                              RegisterMoves& moves) {
    for (Register target : targets) {
      DCHECK(moves[target.code()].is_empty());
      __ movq(target, source);
    }
    moves[source.code()] = kEmptyRegList;
  }

  bool RecursivelyEmitParallelMoveChain(Register chain_start, Register source,
                                        RegList targets, RegisterMoves& moves) {
    if (targets.has(chain_start)) {
      // The target of this move is the start of the move chain -- this
      // means that there is a cycle, and we have to break it by moving
      // the chain start into a temporary.

      __ RecordComment("--   * Cycle");
      EmitSingleParallelMove(chain_start, {kScratchRegister}, moves);
      EmitSingleParallelMove(source, targets, moves);
      return true;
    }
    bool has_cycle = false;
    for (Register target : targets) {
      if (!moves[target.code()].is_empty()) {
        bool is_cycle = RecursivelyEmitParallelMoveChain(
            chain_start, target, moves[target.code()], moves);
        // There can only be one cycle in a connected graph.
        DCHECK_IMPLIES(has_cycle, !is_cycle);
        has_cycle |= is_cycle;
      } else {
        __ RecordComment("--   * Chain start");
      }
    }
    if (has_cycle && source == chain_start) {
      EmitSingleParallelMove(kScratchRegister, targets, moves);
      __ RecordComment("--   * end cycle");
    } else {
      EmitSingleParallelMove(source, targets, moves);
    }
    return has_cycle;
  }

  void EmitParallelMoveChain(Register source, RegisterMoves& moves) {
    RegList targets = moves[source.code()];
    if (targets.is_empty()) return;

    DCHECK(!targets.has(source));
    RecursivelyEmitParallelMoveChain(source, source, targets, moves);
  }

  void EmitRegisterReload(ValueNode* node, Register target) {
    if (node == nullptr) return;
    node->LoadToRegister(code_gen_state_, target);
  }

  void RecordGapMove(ValueNode* node, compiler::InstructionOperand source,
                     Register target_reg, RegisterMoves& register_moves,
                     RegisterReloads& register_reloads) {
    DCHECK(!source.IsDoubleRegister());
    if (source.IsAnyRegister()) {
      // For reg->reg moves, don't emit the move yet, but instead record the
      // move in the set of parallel register moves, to be resolved later.
      Register source_reg = ToRegister(source);
      if (target_reg != source_reg) {
        DCHECK(!register_moves[source_reg.code()].has(target_reg));
        register_moves[source_reg.code()].set(target_reg);
      }
    } else {
      // For register loads from memory, don't emit the move yet, but instead
      // record the move in the set of register reloads, to be executed after
      // the reg->reg parallel moves.
      register_reloads[target_reg.code()] = node;
    }
  }

  void RecordGapMove(ValueNode* node, compiler::InstructionOperand source,
                     compiler::AllocatedOperand target,
                     RegisterMoves& register_moves,
                     RegisterReloads& stack_to_register_moves) {
    if (target.IsRegister()) {
      RecordGapMove(node, source, ToRegister(target), register_moves,
                    stack_to_register_moves);
      return;
    }

    // memory->stack and reg->stack moves should be executed before registers
    // are clobbered by reg->reg or memory->reg, so emit them immediately.
    if (source.IsRegister()) {
      Register source_reg = ToRegister(source);
      __ movq(code_gen_state_->GetStackSlot(target), source_reg);
    } else {
      EmitRegisterReload(node, kScratchRegister);
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
    RegisterMoves register_moves = {};

    // Save registers restored from a memory location in an array, so that we
    // can execute them after the parallel moves have read the register values.
    // Note that the mapping is:
    //
    //     register_reloads[target] = node.
    RegisterReloads register_reloads = {};

    __ RecordComment("--   Gap moves:");

    target->state()->register_state().ForEachGeneralRegister(
        [&](Register reg, RegisterState& state) {
          ValueNode* node;
          RegisterMerge* merge;
          if (LoadMergeState(state, &node, &merge)) {
            compiler::InstructionOperand source =
                merge->operand(predecessor_id);
            if (FLAG_code_comments) {
              std::stringstream ss;
              ss << "--   * " << source << " → " << reg;
              __ RecordComment(ss.str());
            }
            RecordGapMove(node, source, reg, register_moves, register_reloads);
          }
        });

    if (target->has_phi()) {
      Phi::List* phis = target->phis();
      for (Phi* phi : *phis) {
        Input& input = phi->input(state.block()->predecessor_id());
        ValueNode* node = input.node();
        compiler::InstructionOperand source = input.operand();
        compiler::AllocatedOperand target =
            compiler::AllocatedOperand::cast(phi->result().operand());
        if (FLAG_code_comments) {
          std::stringstream ss;
          ss << "--   * " << source << " → " << target << " (n"
             << graph_labeller()->NodeId(phi) << ")";
          __ RecordComment(ss.str());
        }
        RecordGapMove(node, source, target, register_moves, register_reloads);
      }
    }

#define EMIT_MOVE_FOR_REG(Name) EmitParallelMoveChain(Name, register_moves);
    ALLOCATABLE_GENERAL_REGISTERS(EMIT_MOVE_FOR_REG)
#undef EMIT_MOVE_FOR_REG

#define EMIT_MOVE_FOR_REG(Name) \
  EmitRegisterReload(register_reloads[Name.code()], Name);
    ALLOCATABLE_GENERAL_REGISTERS(EMIT_MOVE_FOR_REG)
#undef EMIT_MOVE_FOR_REG
  }

  Isolate* isolate() const { return code_gen_state_->isolate(); }
  MacroAssembler* masm() const { return code_gen_state_->masm(); }
  MaglevGraphLabeller* graph_labeller() const {
    return code_gen_state_->graph_labeller();
  }
  MaglevSafepointTableBuilder* safepoint_table_builder() const {
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
      : safepoint_table_builder_(compilation_info->zone(),
                                 graph->tagged_stack_slots(),
                                 graph->untagged_stack_slots()),
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

    // Add the bytecode to the deopt literals to make sure it's held strongly.
    // TODO(leszeks): Do this fo inlined functions too.
    GetDeoptLiteral(*code_gen_state_.compilation_info()
                         ->toplevel_compilation_unit()
                         ->bytecode()
                         .object());
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

      // TODO(leszeks): Record source positions.
      __ RecordDeoptReason(deopt_info->reason, 0, SourcePosition::Unknown(),
                           deopt_index);
      __ bind(&deopt_info->deopt_entry_label);
      __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Eager, deopt_index,
                               &deopt_info->deopt_entry_label,
                               DeoptimizeKind::kEager, nullptr, nullptr);
      deopt_index++;
    }

    __ RecordComment("-- Lazy deopts");
    int last_updated_safepoint = 0;
    for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
      EmitLazyDeopt(deopt_info);

      __ bind(&deopt_info->deopt_entry_label);
      __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Lazy, deopt_index,
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
    if (input_location.operand().IsConstant()) {
      translation_array_builder_.StoreLiteral(
          GetDeoptLiteral(*value->Reify(isolate())));
    } else {
      const compiler::AllocatedOperand& operand =
          compiler::AllocatedOperand::cast(input_location.operand());
      ValueRepresentation repr = value->properties().value_representation();
      if (operand.IsAnyRegister()) {
        EmitDeoptStoreRegister(operand, repr);
      } else {
        EmitDeoptStoreStackSlot(operand, repr);
      }
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
    // as parameters+context+locals+accumulator are accessed here. We should
    // make this clearer and guard against this invariant failing.
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
    ValueNode* value = checkpoint_state->context(compilation_unit);
    EmitDeoptFrameSingleValue(value, *input_location);
    input_location++;

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

    safepoint_table_builder()->Emit(masm());
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
  MaglevSafepointTableBuilder* safepoint_table_builder() {
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

  MaglevSafepointTableBuilder safepoint_table_builder_;
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
