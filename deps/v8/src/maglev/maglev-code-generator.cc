// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-code-generator.h"

#include <algorithm>

#include "src/base/hashmap.h"
#include "src/codegen/code-desc.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/safepoint-table.h"
#include "src/codegen/source-position.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/deoptimizer/translation-array.h"
#include "src/execution/frame-constants.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-assembler-inl.h"
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

template <typename RegisterT>
struct RegisterTHelper;
template <>
struct RegisterTHelper<Register> {
  static constexpr Register kScratch = kScratchRegister;
  static constexpr RegList kAllocatableRegisters = kAllocatableGeneralRegisters;
};
template <>
struct RegisterTHelper<DoubleRegister> {
  static constexpr DoubleRegister kScratch = kScratchDoubleReg;
  static constexpr DoubleRegList kAllocatableRegisters =
      kAllocatableDoubleRegisters;
};

// The ParallelMoveResolver is used to resolve multiple moves between registers
// and stack slots that are intended to happen, semantically, in parallel. It
// finds chains of moves that would clobber each other, and emits them in a non
// clobbering order; it also detects cycles of moves and breaks them by moving
// to a temporary.
//
// For example, given the moves:
//
//     r1 -> r2
//     r2 -> r3
//     r3 -> r4
//     r4 -> r1
//     r4 -> r5
//
// These can be represented as a move graph
//
//     r2 → r3
//     ↑     ↓
//     r1 ← r4 → r5
//
// and safely emitted (breaking the cycle with a temporary) as
//
//     r1 -> tmp
//     r4 -> r1
//     r4 -> r5
//     r3 -> r4
//     r2 -> r3
//    tmp -> r2
//
// It additionally keeps track of materialising moves, which don't have a stack
// slot but rather materialise a value from, e.g., a constant. These can safely
// be emitted at the end, once all the parallel moves are done.
template <typename RegisterT>
class ParallelMoveResolver {
  static constexpr RegisterT kScratchRegT =
      RegisterTHelper<RegisterT>::kScratch;

  static constexpr auto kAllocatableRegistersT =
      RegisterTHelper<RegisterT>::kAllocatableRegisters;

 public:
  explicit ParallelMoveResolver(MaglevAssembler* masm) : masm_(masm) {}

  void RecordMove(ValueNode* source_node, compiler::InstructionOperand source,
                  compiler::AllocatedOperand target) {
    if (target.IsRegister()) {
      RecordMoveToRegister(source_node, source, ToRegisterT<RegisterT>(target));
    } else {
      RecordMoveToStackSlot(source_node, source,
                            masm_->GetFramePointerOffsetForStackSlot(target));
    }
  }

  void RecordMove(ValueNode* source_node, compiler::InstructionOperand source,
                  RegisterT target_reg) {
    RecordMoveToRegister(source_node, source, target_reg);
  }

  void EmitMoves() {
    for (RegisterT reg : kAllocatableRegistersT) {
      StartEmitMoveChain(reg);
      ValueNode* materializing_register_move =
          materializing_register_moves_[reg.code()];
      if (materializing_register_move) {
        materializing_register_move->LoadToRegister(masm_, reg);
      }
    }
    // Emit stack moves until the move set is empty -- each EmitMoveChain will
    // pop entries off the moves_from_stack_slot map so we can't use a simple
    // iteration here.
    while (!moves_from_stack_slot_.empty()) {
      StartEmitMoveChain(moves_from_stack_slot_.begin()->first);
    }
    for (auto [stack_slot, node] : materializing_stack_slot_moves_) {
      node->LoadToRegister(masm_, kScratchRegT);
      EmitStackMove(stack_slot, kScratchRegT);
    }
  }

  ParallelMoveResolver(ParallelMoveResolver&&) = delete;
  ParallelMoveResolver operator=(ParallelMoveResolver&&) = delete;
  ParallelMoveResolver(const ParallelMoveResolver&) = delete;
  ParallelMoveResolver operator=(const ParallelMoveResolver&) = delete;

 private:
  // The targets of moves from a source, i.e. the set of outgoing edges for a
  // node in the move graph.
  struct GapMoveTargets {
    RegListBase<RegisterT> registers;
    base::SmallVector<uint32_t, 1> stack_slots =
        base::SmallVector<uint32_t, 1>{};

    GapMoveTargets() = default;
    GapMoveTargets(GapMoveTargets&&) V8_NOEXCEPT = default;
    GapMoveTargets& operator=(GapMoveTargets&&) V8_NOEXCEPT = default;
    GapMoveTargets(const GapMoveTargets&) = delete;
    GapMoveTargets& operator=(const GapMoveTargets&) = delete;

    bool is_empty() const {
      return registers.is_empty() && stack_slots.empty();
    }
  };

#ifdef DEBUG
  void CheckNoExistingMoveToRegister(RegisterT target_reg) {
    for (RegisterT reg : kAllocatableRegistersT) {
      if (moves_from_register_[reg.code()].registers.has(target_reg)) {
        FATAL("Existing move from %s to %s", RegisterName(reg),
              RegisterName(target_reg));
      }
    }
    for (auto& [stack_slot, targets] : moves_from_stack_slot_) {
      if (targets.registers.has(target_reg)) {
        FATAL("Existing move from stack slot %d to %s", stack_slot,
              RegisterName(target_reg));
      }
    }
    if (materializing_register_moves_[target_reg.code()] != nullptr) {
      FATAL("Existing materialization of %p to %s",
            materializing_register_moves_[target_reg.code()],
            RegisterName(target_reg));
    }
  }

  void CheckNoExistingMoveToStackSlot(uint32_t target_slot) {
    for (Register reg : kAllocatableRegistersT) {
      auto& stack_slots = moves_from_register_[reg.code()].stack_slots;
      if (std::any_of(stack_slots.begin(), stack_slots.end(),
                      [&](uint32_t slot) { return slot == target_slot; })) {
        FATAL("Existing move from %s to stack slot %d", RegisterName(reg),
              target_slot);
      }
    }
    for (auto& [stack_slot, targets] : moves_from_stack_slot_) {
      auto& stack_slots = targets.stack_slots;
      if (std::any_of(stack_slots.begin(), stack_slots.end(),
                      [&](uint32_t slot) { return slot == target_slot; })) {
        FATAL("Existing move from stack slot %d to stack slot %d", stack_slot,
              target_slot);
      }
    }
    for (auto& [stack_slot, node] : materializing_stack_slot_moves_) {
      if (stack_slot == target_slot) {
        FATAL("Existing materialization of %p to stack slot %d", node,
              stack_slot);
      }
    }
  }
#else
  void CheckNoExistingMoveToRegister(RegisterT target_reg) {}
  void CheckNoExistingMoveToStackSlot(uint32_t target_slot) {}
#endif

  void RecordMoveToRegister(ValueNode* node,
                            compiler::InstructionOperand source,
                            RegisterT target_reg) {
    // There shouldn't have been another move to this register already.
    CheckNoExistingMoveToRegister(target_reg);

    if (source.IsAnyRegister()) {
      RegisterT source_reg = ToRegisterT<RegisterT>(source);
      if (target_reg != source_reg) {
        moves_from_register_[source_reg.code()].registers.set(target_reg);
      }
    } else if (source.IsAnyStackSlot()) {
      uint32_t source_slot = masm_->GetFramePointerOffsetForStackSlot(
          compiler::AllocatedOperand::cast(source));
      moves_from_stack_slot_[source_slot].registers.set(target_reg);
    } else {
      DCHECK(source.IsConstant());
      DCHECK(IsConstantNode(node->opcode()));
      materializing_register_moves_[target_reg.code()] = node;
    }
  }

  void RecordMoveToStackSlot(ValueNode* node,
                             compiler::InstructionOperand source,
                             uint32_t target_slot) {
    // There shouldn't have been another move to this stack slot already.
    CheckNoExistingMoveToStackSlot(target_slot);

    if (source.IsAnyRegister()) {
      RegisterT source_reg = ToRegisterT<RegisterT>(source);
      moves_from_register_[source_reg.code()].stack_slots.push_back(
          target_slot);
    } else if (source.IsAnyStackSlot()) {
      uint32_t source_slot = masm_->GetFramePointerOffsetForStackSlot(
          compiler::AllocatedOperand::cast(source));
      if (source_slot != target_slot) {
        moves_from_stack_slot_[source_slot].stack_slots.push_back(target_slot);
      }
    } else {
      DCHECK(source.IsConstant());
      DCHECK(IsConstantNode(node->opcode()));
      materializing_stack_slot_moves_.emplace_back(target_slot, node);
    }
  }

  // Finds and clears the targets for a given source. In terms of move graph,
  // this returns and removes all outgoing edges from the source.
  GapMoveTargets PopTargets(RegisterT source_reg) {
    return std::exchange(moves_from_register_[source_reg.code()],
                         GapMoveTargets{});
  }
  GapMoveTargets PopTargets(uint32_t source_slot) {
    auto handle = moves_from_stack_slot_.extract(source_slot);
    if (handle.empty()) return {};
    DCHECK(!handle.mapped().is_empty());
    return std::move(handle.mapped());
  }

  // Emit a single move chain starting at the given source (either a register or
  // a stack slot). This is a destructive operation on the move graph, and
  // removes the emitted edges from the graph. Subsequent calls with the same
  // source should emit no code.
  template <typename SourceT>
  void StartEmitMoveChain(SourceT source) {
    DCHECK(!scratch_has_cycle_start_);
    GapMoveTargets targets = PopTargets(source);
    if (targets.is_empty()) return;

    // Start recursively emitting the move chain, with this source as the start
    // of the chain.
    bool has_cycle = RecursivelyEmitMoveChainTargets(source, targets);

    // Each connected component in the move graph can only have one cycle
    // (proof: each target can only have one incoming edge, so cycles in the
    // graph can only have outgoing edges, so there's no way to connect two
    // cycles). This means that if there's a cycle, the saved value must be the
    // chain start.
    if (has_cycle) {
      if (!scratch_has_cycle_start_) {
        Pop(kScratchRegT);
      }
      EmitMovesFromSource(kScratchRegT, targets);
      scratch_has_cycle_start_ = false;
      __ RecordComment("--   * End of cycle");
    } else {
      EmitMovesFromSource(source, targets);
      __ RecordComment("--   * Chain emitted with no cycles");
    }
  }

  template <typename ChainStartT, typename SourceT>
  bool ContinueEmitMoveChain(ChainStartT chain_start, SourceT source) {
    if constexpr (std::is_same_v<ChainStartT, SourceT>) {
      // If the recursion has returned to the start of the chain, then this must
      // be a cycle.
      if (chain_start == source) {
        __ RecordComment("--   * Cycle");
        DCHECK(!scratch_has_cycle_start_);
        if constexpr (std::is_same_v<ChainStartT, uint32_t>) {
          EmitStackMove(kScratchRegT, chain_start);
        } else {
          __ Move(kScratchRegT, chain_start);
        }
        scratch_has_cycle_start_ = true;
        return true;
      }
    }

    GapMoveTargets targets = PopTargets(source);
    if (targets.is_empty()) {
      __ RecordComment("--   * End of chain");
      return false;
    }

    bool has_cycle = RecursivelyEmitMoveChainTargets(chain_start, targets);

    EmitMovesFromSource(source, targets);
    return has_cycle;
  }

  // Calls RecursivelyEmitMoveChain for each target of a source. This is used to
  // share target visiting code between StartEmitMoveChain and
  // ContinueEmitMoveChain.
  template <typename ChainStartT>
  bool RecursivelyEmitMoveChainTargets(ChainStartT chain_start,
                                       GapMoveTargets& targets) {
    bool has_cycle = false;
    for (auto target : targets.registers) {
      has_cycle |= ContinueEmitMoveChain(chain_start, target);
    }
    for (uint32_t target_slot : targets.stack_slots) {
      has_cycle |= ContinueEmitMoveChain(chain_start, target_slot);
    }
    return has_cycle;
  }

  void EmitMovesFromSource(RegisterT source_reg,
                           const GapMoveTargets& targets) {
    DCHECK(moves_from_register_[source_reg.code()].is_empty());
    for (RegisterT target_reg : targets.registers) {
      DCHECK(moves_from_register_[target_reg.code()].is_empty());
      __ Move(target_reg, source_reg);
    }
    for (uint32_t target_slot : targets.stack_slots) {
      DCHECK_EQ(moves_from_stack_slot_.find(target_slot),
                moves_from_stack_slot_.end());
      EmitStackMove(target_slot, source_reg);
    }
  }

  void EmitMovesFromSource(uint32_t source_slot,
                           const GapMoveTargets& targets) {
    DCHECK_EQ(moves_from_stack_slot_.find(source_slot),
              moves_from_stack_slot_.end());
    for (RegisterT target_reg : targets.registers) {
      DCHECK(moves_from_register_[target_reg.code()].is_empty());
      EmitStackMove(target_reg, source_slot);
    }
    if (scratch_has_cycle_start_ && !targets.stack_slots.empty()) {
      Push(kScratchRegT);
    }
    for (uint32_t target_slot : targets.stack_slots) {
      DCHECK_EQ(moves_from_stack_slot_.find(target_slot),
                moves_from_stack_slot_.end());
      EmitStackMove(kScratchRegT, source_slot);
      EmitStackMove(target_slot, kScratchRegT);
    }
  }

  // The slot index used for representing slots in the move graph is the offset
  // from the frame pointer. These helpers help translate this into an actual
  // machine move.
  void EmitStackMove(uint32_t target_slot, Register source_reg) {
    __ movq(MemOperand(rbp, target_slot), source_reg);
  }
  void EmitStackMove(uint32_t target_slot, DoubleRegister source_reg) {
    __ Movsd(MemOperand(rbp, target_slot), source_reg);
  }
  void EmitStackMove(Register target_reg, uint32_t source_slot) {
    __ movq(target_reg, MemOperand(rbp, source_slot));
  }
  void EmitStackMove(DoubleRegister target_reg, uint32_t source_slot) {
    __ Movsd(target_reg, MemOperand(rbp, source_slot));
  }

  void Push(Register reg) { __ Push(reg); }
  void Push(DoubleRegister reg) { __ PushAll({reg}); }
  void Push(uint32_t stack_slot) {
    __ movq(kScratchRegister, MemOperand(rbp, stack_slot));
    __ movq(MemOperand(rsp, -1), kScratchRegister);
  }
  void Pop(Register reg) { __ Pop(reg); }
  void Pop(DoubleRegister reg) { __ PopAll({reg}); }
  void Pop(uint32_t stack_slot) {
    __ movq(kScratchRegister, MemOperand(rsp, -1));
    __ movq(MemOperand(rbp, stack_slot), kScratchRegister);
  }

  MacroAssembler* masm() const { return masm_; }

  MaglevAssembler* const masm_;

  // Keep moves to/from registers and stack slots separate -- there are a fixed
  // number of registers but an infinite number of stack slots, so the register
  // moves can be kept in a fixed size array while the stack slot moves need a
  // map.

  // moves_from_register_[source] = target.
  std::array<GapMoveTargets, RegisterT::kNumRegisters> moves_from_register_ =
      {};

  // moves_from_stack_slot_[source] = target.
  std::unordered_map<uint32_t, GapMoveTargets> moves_from_stack_slot_;

  // materializing_register_moves[target] = node.
  std::array<ValueNode*, RegisterT::kNumRegisters>
      materializing_register_moves_ = {};

  // materializing_stack_slot_moves = {(node,target), ... }.
  std::vector<std::pair<uint32_t, ValueNode*>> materializing_stack_slot_moves_;

  bool scratch_has_cycle_start_ = false;
};

class ExceptionHandlerTrampolineBuilder {
 public:
  explicit ExceptionHandlerTrampolineBuilder(MaglevAssembler* masm)
      : masm_(masm) {}

  void EmitTrampolineFor(NodeBase* node) {
    DCHECK(node->properties().can_throw());

    ExceptionHandlerInfo* handler_info = node->exception_handler_info();
    DCHECK(handler_info->HasExceptionHandler());

    BasicBlock* block = handler_info->catch_block.block_ptr();
    LazyDeoptInfo* deopt_info = node->lazy_deopt_info();

    __ bind(&handler_info->trampoline_entry);
    ClearState();
    // TODO(v8:7700): Handle inlining.
    RecordMoves(deopt_info->unit, block, deopt_info->state.register_frame);
    // We do moves that need to materialise values first, since we might need to
    // call a builtin to create a HeapNumber, and therefore we would need to
    // spill all registers.
    DoMaterialiseMoves();
    // Move the rest, we will not call HeapNumber anymore.
    DoDirectMoves();
    // Jump to the catch block.
    __ jmp(block->label());
  }

 private:
  MaglevAssembler* const masm_;
  using Move = std::pair<const ValueLocation&, ValueNode*>;
  base::SmallVector<Move, 16> direct_moves_;
  base::SmallVector<Move, 16> materialisation_moves_;
  bool save_accumulator_ = false;

  MacroAssembler* masm() const { return masm_; }

  void ClearState() {
    direct_moves_.clear();
    materialisation_moves_.clear();
    save_accumulator_ = false;
  }

  void RecordMoves(const MaglevCompilationUnit& unit, BasicBlock* block,
                   const CompactInterpreterFrameState* register_frame) {
    for (Phi* phi : *block->phis()) {
      DCHECK_EQ(phi->input_count(), 0);
      if (!phi->has_valid_live_range()) continue;
      if (phi->owner() == interpreter::Register::virtual_accumulator()) {
        // If the accumulator is live, then it is the exception object located
        // at kReturnRegister0. This is also the first phi in the list.
        DCHECK_EQ(phi->result().AssignedGeneralRegister(), kReturnRegister0);
        save_accumulator_ = true;
        continue;
      }
      ValueNode* value = register_frame->GetValueOf(phi->owner(), unit);
      DCHECK_NOT_NULL(value);
      switch (value->properties().value_representation()) {
        case ValueRepresentation::kTagged:
          // All registers should have been spilled due to the call.
          DCHECK(!value->allocation().IsRegister());
          direct_moves_.emplace_back(phi->result(), value);
          break;
        case ValueRepresentation::kInt32:
          if (value->allocation().IsConstant()) {
            direct_moves_.emplace_back(phi->result(), value);
          } else {
            materialisation_moves_.emplace_back(phi->result(), value);
          }
          break;
        case ValueRepresentation::kFloat64:
          materialisation_moves_.emplace_back(phi->result(), value);
          break;
      }
    }
  }

  void DoMaterialiseMoves() {
    if (materialisation_moves_.size() == 0) return;
    if (save_accumulator_) {
      __ Push(kReturnRegister0);
    }
    for (auto it = materialisation_moves_.begin();
         it < materialisation_moves_.end(); it++) {
      switch (it->second->properties().value_representation()) {
        case ValueRepresentation::kInt32: {
          EmitMoveInt32ToReturnValue0(it->second);
          break;
        }
        case ValueRepresentation::kFloat64:
          EmitMoveFloat64ToReturnValue0(it->second);
          break;
        case ValueRepresentation::kTagged:
          UNREACHABLE();
      }
      if (it->first.operand().IsStackSlot()) {
        // If the target is in a stack sot, we can immediately move
        // the result to it.
        __ movq(ToMemOperand(it->first), kReturnRegister0);
      } else {
        // We spill the result to the stack, in order to be able to call the
        // NewHeapNumber builtin again, however we don't need to push the result
        // of the last one.
        if (it != materialisation_moves_.end() - 1) {
          __ Push(kReturnRegister0);
        }
      }
    }
    // If the last move target is a register, the result should be in
    // kReturnValue0, so so we emit a simple move. Otherwise it has already been
    // moved.
    const ValueLocation& last_move_target =
        materialisation_moves_.rbegin()->first;
    if (last_move_target.operand().IsRegister()) {
      __ Move(last_move_target.AssignedGeneralRegister(), kReturnRegister0);
    }
    // And then pop the rest.
    for (auto it = materialisation_moves_.rbegin() + 1;
         it < materialisation_moves_.rend(); it++) {
      if (it->first.operand().IsRegister()) {
        __ Pop(it->first.AssignedGeneralRegister());
      }
    }
    if (save_accumulator_) {
      __ Pop(kReturnRegister0);
    }
  }

  void DoDirectMoves() {
    for (auto& [target, value] : direct_moves_) {
      if (value->allocation().IsConstant()) {
        if (Int32Constant* constant = value->TryCast<Int32Constant>()) {
          EmitMove(target, Smi::FromInt(constant->value()));
        } else {
          // Int32 and Float64 constants should have already been dealt with.
          DCHECK_EQ(value->properties().value_representation(),
                    ValueRepresentation::kTagged);
          EmitConstantLoad(target, value);
        }
      } else {
        EmitMove(target, ToMemOperand(value));
      }
    }
  }

  void EmitMoveInt32ToReturnValue0(ValueNode* value) {
    // We consider Int32Constants together with tagged values.
    DCHECK(!value->allocation().IsConstant());
    using D = NewHeapNumberDescriptor;
    Label done;
    __ movq(kReturnRegister0, ToMemOperand(value));
    __ addl(kReturnRegister0, kReturnRegister0);
    __ j(no_overflow, &done);
    // If we overflow, instead of bailing out (deopting), we change
    // representation to a HeapNumber.
    __ Cvtlsi2sd(D::GetDoubleRegisterParameter(D::kValue), ToMemOperand(value));
    __ CallBuiltin(Builtin::kNewHeapNumber);
    __ bind(&done);
  }

  void EmitMoveFloat64ToReturnValue0(ValueNode* value) {
    using D = NewHeapNumberDescriptor;
    if (Float64Constant* constant = value->TryCast<Float64Constant>()) {
      __ Move(D::GetDoubleRegisterParameter(D::kValue), constant->value());
    } else {
      __ Movsd(D::GetDoubleRegisterParameter(D::kValue), ToMemOperand(value));
    }
    __ CallBuiltin(Builtin::kNewHeapNumber);
  }

  MemOperand ToMemOperand(ValueNode* node) {
    DCHECK(node->allocation().IsAnyStackSlot());
    return masm_->ToMemOperand(node->allocation());
  }

  MemOperand ToMemOperand(const ValueLocation& location) {
    DCHECK(location.operand().IsStackSlot());
    return masm_->ToMemOperand(location.operand());
  }

  template <typename Operand>
  void EmitMove(const ValueLocation& dst, Operand src) {
    if (dst.operand().IsRegister()) {
      __ Move(dst.AssignedGeneralRegister(), src);
    } else {
      __ Move(kScratchRegister, src);
      __ movq(ToMemOperand(dst), kScratchRegister);
    }
  }

  void EmitConstantLoad(const ValueLocation& dst, ValueNode* value) {
    DCHECK(value->allocation().IsConstant());
    if (dst.operand().IsRegister()) {
      value->LoadToRegister(masm_, dst.AssignedGeneralRegister());
    } else {
      value->LoadToRegister(masm_, kScratchRegister);
      __ movq(ToMemOperand(dst), kScratchRegister);
    }
  }
};

class MaglevCodeGeneratingNodeProcessor {
 public:
  explicit MaglevCodeGeneratingNodeProcessor(MaglevAssembler* masm)
      : masm_(masm) {}

  void PreProcessGraph(Graph* graph) {
    if (FLAG_maglev_break_on_entry) {
      __ int3();
    }

    __ BailoutIfDeoptimized(rbx);

    // Tiering support.
    // TODO(jgruber): Extract to a builtin (the tiering prologue is ~230 bytes
    // per Maglev code object on x64).
    {
      // Scratch registers. Don't clobber regs related to the calling
      // convention (e.g. kJavaScriptCallArgCountRegister).
      Register flags = rcx;
      Register feedback_vector = r9;

      // Load the feedback vector.
      __ LoadTaggedPointerField(
          feedback_vector,
          FieldOperand(kJSFunctionRegister, JSFunction::kFeedbackCellOffset));
      __ LoadTaggedPointerField(
          feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
      __ AssertFeedbackVector(feedback_vector);

      Label flags_need_processing, next;
      __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
          flags, feedback_vector, CodeKind::MAGLEV, &flags_need_processing);
      __ jmp(&next);

      __ bind(&flags_need_processing);
      {
        ASM_CODE_COMMENT_STRING(masm(), "Optimized marker check");
        __ MaybeOptimizeCodeOrTailCallOptimizedCodeSlot(
            flags, feedback_vector, kJSFunctionRegister, JumpMode::kJump);
        __ Trap();
      }

      __ bind(&next);
    }

    __ EnterFrame(StackFrame::MAGLEV);

    // Save arguments in frame.
    // TODO(leszeks): Consider eliding this frame if we don't make any calls
    // that could clobber these registers.
    __ Push(kContextRegister);
    __ Push(kJSFunctionRegister);              // Callee's JS function.
    __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

    code_gen_state()->set_untagged_slots(graph->untagged_stack_slots());
    code_gen_state()->set_tagged_slots(graph->tagged_stack_slots());

    {
      ASM_CODE_COMMENT_STRING(masm(), " Stack/interrupt check");
      // Stack check. This folds the checks for both the interrupt stack limit
      // check and the real stack limit into one by just checking for the
      // interrupt limit. The interrupt limit is either equal to the real stack
      // limit or tighter. By ensuring we have space until that limit after
      // building the frame we can quickly precheck both at once.
      __ Move(kScratchRegister, rsp);
      // TODO(leszeks): Include a max call argument size here.
      __ subq(kScratchRegister,
              Immediate(code_gen_state()->stack_slots() * kSystemPointerSize));
      __ cmpq(kScratchRegister,
              __ StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));

      __ j(below, &deferred_call_stack_guard_);
      __ bind(&deferred_call_stack_guard_return_);
    }

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
  }

  void PostProcessGraph(Graph*) {
    __ int3();
    __ bind(&deferred_call_stack_guard_);
    ASM_CODE_COMMENT_STRING(masm(), "Stack/interrupt call");
    // Save any registers that can be referenced by RegisterInput.
    // TODO(leszeks): Only push those that are used by the graph.
    __ PushAll(RegisterInput::kAllowedRegisters);
    // Push the frame size
    __ Push(Immediate(
        Smi::FromInt(code_gen_state()->stack_slots() * kSystemPointerSize)));
    __ CallRuntime(Runtime::kStackGuardWithGap, 1);
    __ PopAll(RegisterInput::kAllowedRegisters);
    __ jmp(&deferred_call_stack_guard_return_);
  }

  void PreProcessBasicBlock(BasicBlock* block) {
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

    if (FLAG_debug_code) {
      __ movq(kScratchRegister, rbp);
      __ subq(kScratchRegister, rsp);
      __ cmpq(kScratchRegister,
              Immediate(code_gen_state()->stack_slots() * kSystemPointerSize +
                        StandardFrameConstants::kFixedFrameSizeFromFp));
      __ Assert(equal, AbortReason::kStackAccessBelowStackPointer);
    }

    // Emit Phi moves before visiting the control node.
    if (std::is_base_of<UnconditionalControlNode, NodeT>::value) {
      EmitBlockEndGapMoves(node->template Cast<UnconditionalControlNode>(),
                           state);
    }

    node->GenerateCode(masm(), state);

    if (std::is_base_of<ValueNode, NodeT>::value) {
      ValueNode* value_node = node->template Cast<ValueNode>();
      if (value_node->is_spilled()) {
        compiler::AllocatedOperand source =
            compiler::AllocatedOperand::cast(value_node->result().operand());
        // We shouldn't spill nodes which already output to the stack.
        if (!source.IsAnyStackSlot()) {
          if (FLAG_code_comments) __ RecordComment("--   Spill:");
          if (source.IsRegister()) {
            __ movq(masm()->GetStackSlot(value_node->spill_slot()),
                    ToRegister(source));
          } else {
            __ Movsd(masm()->GetStackSlot(value_node->spill_slot()),
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

  void EmitBlockEndGapMoves(UnconditionalControlNode* node,
                            const ProcessingState& state) {
    BasicBlock* target = node->target();
    if (!target->has_state()) {
      __ RecordComment("--   Target has no state, must be a fallthrough");
      return;
    }

    int predecessor_id = state.block()->predecessor_id();

    // TODO(leszeks): Move these to fields, to allow their data structure
    // allocations to be reused. Will need some sort of state resetting.
    ParallelMoveResolver<Register> register_moves(masm_);
    ParallelMoveResolver<DoubleRegister> double_register_moves(masm_);

    // Remember what registers were assigned to by a Phi, to avoid clobbering
    // them with RegisterMoves.
    RegList registers_set_by_phis;

    __ RecordComment("--   Gap moves:");

    if (target->has_phi()) {
      Phi::List* phis = target->phis();
      for (Phi* phi : *phis) {
        // Ignore dead phis.
        // TODO(leszeks): We should remove dead phis entirely and turn this into
        // a DCHECK.
        if (!phi->has_valid_live_range()) {
          if (FLAG_code_comments) {
            std::stringstream ss;
            ss << "--   * "
               << phi->input(state.block()->predecessor_id()).operand() << " → "
               << target << " (n" << graph_labeller()->NodeId(phi)
               << ") [DEAD]";
            __ RecordComment(ss.str());
          }
          continue;
        }
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
        register_moves.RecordMove(node, source, target);
        if (target.IsAnyRegister()) {
          registers_set_by_phis.set(target.GetRegister());
        }
      }
    }

    target->state()->register_state().ForEachGeneralRegister(
        [&](Register reg, RegisterState& state) {
          // Don't clobber registers set by a Phi.
          if (registers_set_by_phis.has(reg)) return;

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
            register_moves.RecordMove(node, source, reg);
          }
        });

    register_moves.EmitMoves();

    __ RecordComment("--   Double gap moves:");

    target->state()->register_state().ForEachDoubleRegister(
        [&](DoubleRegister reg, RegisterState& state) {
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
            double_register_moves.RecordMove(node, source, reg);
          }
        });

    double_register_moves.EmitMoves();
  }

  Isolate* isolate() const { return masm_->isolate(); }
  MaglevAssembler* masm() const { return masm_; }
  MaglevCodeGenState* code_gen_state() const {
    return masm()->code_gen_state();
  }
  MaglevGraphLabeller* graph_labeller() const {
    return code_gen_state()->graph_labeller();
  }
  MaglevSafepointTableBuilder* safepoint_table_builder() const {
    return code_gen_state()->safepoint_table_builder();
  }

 private:
  MaglevAssembler* const masm_;
  Label deferred_call_stack_guard_;
  Label deferred_call_stack_guard_return_;
};

}  // namespace

class MaglevCodeGeneratorImpl final {
 public:
  static MaybeHandle<Code> Generate(MaglevCompilationInfo* compilation_info,
                                    Graph* graph) {
    return MaglevCodeGeneratorImpl(compilation_info, graph).Generate();
  }

 private:
  MaglevCodeGeneratorImpl(MaglevCompilationInfo* compilation_info, Graph* graph)
      : safepoint_table_builder_(compilation_info->zone(),
                                 graph->tagged_stack_slots(),
                                 graph->untagged_stack_slots()),
        code_gen_state_(compilation_info, safepoint_table_builder()),
        masm_(&code_gen_state_),
        processor_(&masm_),
        graph_(graph) {}

  MaybeHandle<Code> Generate() {
    EmitCode();
    EmitMetadata();
    return BuildCodeObject();
  }

  void EmitCode() {
    processor_.ProcessGraph(graph_);
    EmitDeferredCode();
    EmitDeopts();
    EmitExceptionHandlersTrampolines();
  }

  void EmitDeferredCode() {
    for (DeferredCodeInfo* deferred_code : code_gen_state_.deferred_code()) {
      __ RecordComment("-- Deferred block");
      __ bind(&deferred_code->deferred_code_label);
      deferred_code->Generate(masm(), &deferred_code->return_label);
      __ Trap();
    }
  }

  void EmitDeopts() {
    deopt_exit_start_offset_ = __ pc_offset();

    int deopt_index = 0;

    __ RecordComment("-- Non-lazy deopts");
    for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
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

  void EmitExceptionHandlersTrampolines() {
    if (code_gen_state_.handlers().size() == 0) return;
    ExceptionHandlerTrampolineBuilder builder(masm());
    __ RecordComment("-- Exception handlers trampolines");
    for (NodeBase* node : code_gen_state_.handlers()) {
      builder.EmitTrampolineFor(node);
    }
  }

  void EmitMetadata() {
    // Final alignment before starting on the metadata section.
    masm()->Align(Code::kMetadataAlignment);

    safepoint_table_builder()->Emit(masm());

    // Exception handler table.
    handler_table_offset_ = HandlerTable::EmitReturnTableStart(masm());
    for (NodeBase* node : code_gen_state_.handlers()) {
      ExceptionHandlerInfo* info = node->exception_handler_info();
      HandlerTable::EmitReturnEntry(masm(), info->pc_offset,
                                    info->trampoline_entry.pos());
    }
  }

  MaybeHandle<Code> BuildCodeObject() {
    CodeDesc desc;
    masm()->GetCode(isolate(), &desc, safepoint_table_builder(),
                    handler_table_offset_);
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
        code_gen_state_.compilation_info()
            ->translation_array_builder()
            .ToTranslationArray(isolate()->factory());
    {
      DisallowGarbageCollection no_gc;
      auto raw_data = *data;

      raw_data.SetTranslationByteArray(*translation_array);
      // TODO(leszeks): Fix with the real inlined function count.
      raw_data.SetInlinedFunctionCount(Smi::zero());
      // TODO(leszeks): Support optimization IDs
      raw_data.SetOptimizationId(Smi::zero());

      DCHECK_NE(deopt_exit_start_offset_, -1);
      raw_data.SetDeoptExitStart(Smi::FromInt(deopt_exit_start_offset_));
      raw_data.SetEagerDeoptCount(Smi::FromInt(eager_deopt_count));
      raw_data.SetLazyDeoptCount(Smi::FromInt(lazy_deopt_count));

      raw_data.SetSharedFunctionInfo(*code_gen_state_.compilation_info()
                                          ->toplevel_compilation_unit()
                                          ->shared_function_info()
                                          .object());
    }

    IdentityMap<int, base::DefaultAllocationPolicy>& deopt_literals =
        code_gen_state_.compilation_info()->deopt_literals();
    Handle<DeoptimizationLiteralArray> literals =
        isolate()->factory()->NewDeoptimizationLiteralArray(
            deopt_literals.size() + 1);
    // TODO(leszeks): Fix with the real inlining positions.
    Handle<PodArray<InliningPosition>> inlining_positions =
        PodArray<InliningPosition>::New(isolate(), 0);
    DisallowGarbageCollection no_gc;

    auto raw_literals = *literals;
    auto raw_data = *data;
    IdentityMap<int, base::DefaultAllocationPolicy>::IteratableScope iterate(
        &deopt_literals);
    for (auto it = iterate.begin(); it != iterate.end(); ++it) {
      raw_literals.set(*it.entry(), it.key());
    }
    // Add the bytecode to the deopt literals to make sure it's held strongly.
    // TODO(leszeks): Do this for inlined functions too.
    raw_literals.set(deopt_literals.size(), *code_gen_state_.compilation_info()
                                                 ->toplevel_compilation_unit()
                                                 ->bytecode()
                                                 .object());
    raw_data.SetLiteralArray(raw_literals);

    // TODO(leszeks): Fix with the real inlining positions.
    raw_data.SetInliningPositions(*inlining_positions);

    // TODO(leszeks): Fix once we have OSR.
    BytecodeOffset osr_offset = BytecodeOffset::None();
    raw_data.SetOsrBytecodeOffset(Smi::FromInt(osr_offset.ToInt()));
    raw_data.SetOsrPcOffset(Smi::FromInt(-1));

    // Populate deoptimization entries.
    int i = 0;
    for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
      DCHECK_NE(deopt_info->translation_index, -1);
      raw_data.SetBytecodeOffset(i, deopt_info->state.bytecode_position);
      raw_data.SetTranslationIndex(i,
                                   Smi::FromInt(deopt_info->translation_index));
      raw_data.SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label.pos()));
#ifdef DEBUG
      raw_data.SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
      i++;
    }
    for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
      DCHECK_NE(deopt_info->translation_index, -1);
      raw_data.SetBytecodeOffset(i, deopt_info->state.bytecode_position);
      raw_data.SetTranslationIndex(i,
                                   Smi::FromInt(deopt_info->translation_index));
      raw_data.SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label.pos()));
#ifdef DEBUG
      raw_data.SetNodeId(i, Smi::FromInt(i));
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
  MaglevAssembler* masm() { return &masm_; }
  MaglevSafepointTableBuilder* safepoint_table_builder() {
    return &safepoint_table_builder_;
  }

  MaglevSafepointTableBuilder safepoint_table_builder_;
  MaglevCodeGenState code_gen_state_;
  MaglevAssembler masm_;
  GraphProcessor<MaglevCodeGeneratingNodeProcessor> processor_;
  Graph* const graph_;

  int deopt_exit_start_offset_ = -1;
  int handler_table_offset_ = 0;
};

// static
MaybeHandle<Code> MaglevCodeGenerator::Generate(
    MaglevCompilationInfo* compilation_info, Graph* graph) {
  return MaglevCodeGeneratorImpl::Generate(compilation_info, graph);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
