// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-code-generator.h"

#include "src/codegen/code-desc.h"
#include "src/codegen/register.h"
#include "src/codegen/safepoint-table.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"

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
  static constexpr bool kNeedsCheckpointStates = true;

  explicit MaglevCodeGeneratingNodeProcessor(MaglevCodeGenState* code_gen_state)
      : code_gen_state_(code_gen_state) {}

  void PreProcessGraph(MaglevCompilationUnit*, Graph* graph) {
    if (FLAG_maglev_break_on_entry) {
      __ int3();
    }

    __ EnterFrame(StackFrame::BASELINE);

    // Save arguments in frame.
    // TODO(leszeks): Consider eliding this frame if we don't make any calls
    // that could clobber these registers.
    __ Push(kContextRegister);
    __ Push(kJSFunctionRegister);              // Callee's JS function.
    __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

    // Extend rsp by the size of the frame.
    code_gen_state_->SetVregSlots(graph->stack_slots());
    __ subq(rsp, Immediate(code_gen_state_->vreg_slots() * kSystemPointerSize));

    // Initialize stack slots.
    // TODO(jgruber): Update logic once the register allocator is further along.
    {
      ASM_CODE_COMMENT_STRING(masm(), "Initializing stack slots");
      __ Move(rax, Immediate(0));
      __ Move(rcx, Immediate(code_gen_state_->vreg_slots()));
      __ leaq(rdi, GetStackSlot(code_gen_state_->vreg_slots() - 1));
      __ repstosq();
    }

    // We don't emit proper safepoint data yet; instead, define a single
    // safepoint at the end of the code object, with all-tagged stack slots.
    // TODO(jgruber): Real safepoint handling.
    SafepointTableBuilder::Safepoint safepoint =
        safepoint_table_builder()->DefineSafepoint(masm());
    for (int i = 0; i < code_gen_state_->vreg_slots(); i++) {
      safepoint.DefineTaggedStackSlot(GetSafepointIndexForStackSlot(i));
    }
  }

  void PostProcessGraph(MaglevCompilationUnit*, Graph* graph) {
    code_gen_state_->EmitDeferredCode();
  }

  void PreProcessBasicBlock(MaglevCompilationUnit*, BasicBlock* block) {
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
        if (!source.IsStackSlot()) {
          if (FLAG_code_comments) __ RecordComment("--   Spill:");
          DCHECK(!source.IsStackSlot());
          __ movq(GetStackSlot(value_node->spill_slot()), ToRegister(source));
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
    __ movq(target, GetStackSlot(compiler::AllocatedOperand::cast(source)));
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
      __ movq(GetStackSlot(target), source_reg);
    } else {
      __ movq(kScratchRegister, GetStackSlot(source));
      __ movq(GetStackSlot(target), kScratchRegister);
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

    for (auto entry : target->state()->register_state()) {
      RegisterMerge* merge;
      if (LoadMergeState(entry.state, &merge)) {
        compiler::AllocatedOperand source = merge->operand(predecessor_id);
        Register target_reg = entry.reg;

        if (FLAG_code_comments) {
          std::stringstream ss;
          ss << "--   * " << source << " → " << target_reg;
          __ RecordComment(ss.str());
        }
        RecordGapMove(source, target_reg, register_moves,
                      stack_to_register_moves);
      }
    }

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
  static MaybeHandle<Code> Generate(MaglevCompilationUnit* compilation_unit,
                                    Graph* graph) {
    return MaglevCodeGeneratorImpl(compilation_unit, graph).Generate();
  }

 private:
  MaglevCodeGeneratorImpl(MaglevCompilationUnit* compilation_unit, Graph* graph)
      : safepoint_table_builder_(compilation_unit->zone()),
        code_gen_state_(compilation_unit, safepoint_table_builder()),
        processor_(compilation_unit, &code_gen_state_),
        graph_(graph) {}

  MaybeHandle<Code> Generate() {
    EmitCode();
    if (code_gen_state_.found_unsupported_code_paths()) return {};
    EmitMetadata();
    return BuildCodeObject();
  }

  void EmitCode() { processor_.ProcessGraph(graph_); }

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
        .TryBuild();
  }

  int stack_slot_count() const { return code_gen_state_.vreg_slots(); }
  int stack_slot_count_with_fixed_frame() const {
    return stack_slot_count() + StandardFrameConstants::kFixedSlotCount;
  }

  Isolate* isolate() const {
    return code_gen_state_.compilation_unit()->isolate();
  }
  MacroAssembler* masm() { return code_gen_state_.masm(); }
  SafepointTableBuilder* safepoint_table_builder() {
    return &safepoint_table_builder_;
  }

  SafepointTableBuilder safepoint_table_builder_;
  MaglevCodeGenState code_gen_state_;
  GraphProcessor<MaglevCodeGeneratingNodeProcessor> processor_;
  Graph* const graph_;
};

// static
MaybeHandle<Code> MaglevCodeGenerator::Generate(
    MaglevCompilationUnit* compilation_unit, Graph* graph) {
  return MaglevCodeGeneratorImpl::Generate(compilation_unit, graph);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
