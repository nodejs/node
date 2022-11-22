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
#include "src/deoptimizer/deoptimize-reason.h"
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
        scratch_has_cycle_start_ = true;
      }
      EmitMovesFromSource(kScratchRegT, std::move(targets));
      scratch_has_cycle_start_ = false;
      __ RecordComment("--   * End of cycle");
    } else {
      EmitMovesFromSource(source, std::move(targets));
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

    EmitMovesFromSource(source, std::move(targets));
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

  void EmitMovesFromSource(RegisterT source_reg, GapMoveTargets&& targets) {
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

  void EmitMovesFromSource(uint32_t source_slot, GapMoveTargets&& targets) {
    DCHECK_EQ(moves_from_stack_slot_.find(source_slot),
              moves_from_stack_slot_.end());

    // Cache the slot value on a register.
    RegisterT register_with_slot_value = RegisterT::no_reg();
    if (!targets.registers.is_empty()) {
      // If one of the targets is a register, we can move our value into it and
      // optimize the moves from this stack slot to always be via that register.
      register_with_slot_value = targets.registers.PopFirst();
    } else {
      DCHECK(!targets.stack_slots.empty());
      // Otherwise, cache the slot value on the scratch register, clobbering it
      // if necessary.
      if (scratch_has_cycle_start_) {
        Push(kScratchRegT);
        scratch_has_cycle_start_ = false;
      }
      register_with_slot_value = kScratchRegT;
    }

    // Now emit moves from that cached register instead of from the stack slot.
    DCHECK(register_with_slot_value.is_valid());
    DCHECK(moves_from_register_[register_with_slot_value.code()].is_empty());
    EmitStackMove(register_with_slot_value, source_slot);
    EmitMovesFromSource(register_with_slot_value, std::move(targets));
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
  static void Build(MaglevAssembler* masm, NodeBase* node) {
    ExceptionHandlerTrampolineBuilder builder(masm);
    builder.EmitTrampolineFor(node);
  }

 private:
  explicit ExceptionHandlerTrampolineBuilder(MaglevAssembler* masm)
      : masm_(masm) {}

  struct Move {
    explicit Move(const ValueLocation& target, ValueNode* source)
        : target(target), source(source) {}
    const ValueLocation& target;
    ValueNode* const source;
  };
  using MoveVector = base::SmallVector<Move, 16>;

  void EmitTrampolineFor(NodeBase* node) {
    DCHECK(node->properties().can_throw());

    ExceptionHandlerInfo* const handler_info = node->exception_handler_info();
    DCHECK(handler_info->HasExceptionHandler());
    BasicBlock* const catch_block = handler_info->catch_block.block_ptr();
    LazyDeoptInfo* const deopt_info = node->lazy_deopt_info();

    // The exception handler trampoline resolves moves for exception phis and
    // then jumps to the actual catch block. There are a few points worth
    // noting:
    //
    // - All source locations are assumed to be stack slots, except the
    // accumulator which is stored in kReturnRegister0. We don't emit an
    // explicit move for it, instead it is pushed and popped at the boundaries
    // of the entire move sequence (necessary due to materialisation).
    //
    // - Some values may require materialisation, i.e. heap number construction
    // through calls to the NewHeapNumber builtin. To avoid potential conflicts
    // with other moves (which may happen due to stack slot reuse, i.e. a
    // target location of move A may equal source location of move B), we
    // materialise and push results to new temporary stack slots before the
    // main move sequence, and then pop results into their final target
    // locations afterwards. Note this is only safe because a) materialised
    // values are tagged and b) the stack walk treats unknown stack slots as
    // tagged.

    const InterpretedDeoptFrame& lazy_frame =
        deopt_info->top_frame().type() ==
                DeoptFrame::FrameType::kBuiltinContinuationFrame
            ? deopt_info->top_frame().parent()->as_interpreted()
            : deopt_info->top_frame().as_interpreted();

    // TODO(v8:7700): Handle inlining.

    ParallelMoveResolver<Register> direct_moves(masm_);
    MoveVector materialising_moves;
    bool save_accumulator = false;
    RecordMoves(lazy_frame.unit(), catch_block, lazy_frame.frame_state(),
                &direct_moves, &materialising_moves, &save_accumulator);

    __ bind(&handler_info->trampoline_entry);
    __ RecordComment("-- Exception handler trampoline START");
    EmitMaterialisationsAndPushResults(materialising_moves, save_accumulator);
    __ RecordComment("EmitMoves");
    direct_moves.EmitMoves();
    EmitPopMaterialisedResults(materialising_moves, save_accumulator);
    __ jmp(catch_block->label());
    __ RecordComment("-- Exception handler trampoline END");
  }

  MacroAssembler* masm() const { return masm_; }

  void RecordMoves(const MaglevCompilationUnit& unit, BasicBlock* catch_block,
                   const CompactInterpreterFrameState* register_frame,
                   ParallelMoveResolver<Register>* direct_moves,
                   MoveVector* materialising_moves, bool* save_accumulator) {
    for (Phi* phi : *catch_block->phis()) {
      DCHECK(phi->is_exception_phi());
      if (!phi->has_valid_live_range()) continue;

      const ValueLocation& target = phi->result();
      if (phi->owner() == interpreter::Register::virtual_accumulator()) {
        // If the accumulator is live, then it is the exception object located
        // at kReturnRegister0.  We don't emit a move for it since the value is
        // already in the right spot, but we do have to ensure it isn't
        // clobbered by calls to the NewHeapNumber builtin during
        // materialisation.
        DCHECK_EQ(target.AssignedGeneralRegister(), kReturnRegister0);
        *save_accumulator = true;
        continue;
      }

      ValueNode* const source = register_frame->GetValueOf(phi->owner(), unit);
      DCHECK_NOT_NULL(source);
      // All registers must have been spilled due to the call.
      // TODO(jgruber): Which call? Because any throw requires at least a call
      // to Runtime::kThrowFoo?
      DCHECK(!source->allocation().IsRegister());

      switch (source->properties().value_representation()) {
        case ValueRepresentation::kTagged:
          direct_moves->RecordMove(
              source, source->allocation(),
              compiler::AllocatedOperand::cast(target.operand()));
          break;
        case ValueRepresentation::kInt32:
          if (source->allocation().IsConstant()) {
            // TODO(jgruber): Why is it okay for Int32 constants to remain
            // untagged while non-constants are unconditionally smi-tagged or
            // converted to a HeapNumber during materialisation?
            direct_moves->RecordMove(
                source, source->allocation(),
                compiler::AllocatedOperand::cast(target.operand()));
          } else {
            materialising_moves->emplace_back(target, source);
          }
          break;
        case ValueRepresentation::kFloat64:
          materialising_moves->emplace_back(target, source);
          break;
      }
    }
  }

  void EmitMaterialisationsAndPushResults(const MoveVector& moves,
                                          bool save_accumulator) const {
    if (moves.size() == 0) return;

    // It's possible to optimize this further, at the cost of additional
    // complexity:
    //
    // - If the target location is a register, we could theoretically move the
    // materialised result there immediately, with the additional complication
    // that following calls to NewHeapNumber may clobber the register.
    //
    // - If the target location is a stack slot which is neither a source nor
    // target slot for any other moves (direct or materialising), we could move
    // the result there directly instead of pushing and later popping it. This
    // doesn't seem worth the extra code complexity though, given we are
    // talking about a presumably infrequent case for exception handlers.

    __ RecordComment("EmitMaterialisationsAndPushResults");
    if (save_accumulator) __ Push(kReturnRegister0);
    for (const Move& move : moves) {
      MaterialiseTo(move.source, kReturnRegister0);
      __ Push(kReturnRegister0);
    }
  }

  void EmitPopMaterialisedResults(const MoveVector& moves,
                                  bool save_accumulator) const {
    if (moves.size() == 0) return;
    __ RecordComment("EmitPopMaterialisedResults");
    for (auto it = moves.rbegin(); it < moves.rend(); it++) {
      const ValueLocation& target = it->target;
      if (target.operand().IsRegister()) {
        __ Pop(target.AssignedGeneralRegister());
      } else {
        DCHECK(target.operand().IsStackSlot());
        __ Pop(kScratchRegister);
        __ movq(masm_->ToMemOperand(target.operand()), kScratchRegister);
      }
    }

    if (save_accumulator) __ Pop(kReturnRegister0);
  }

  void MaterialiseTo(ValueNode* value, Register dst) const {
    using D = NewHeapNumberDescriptor;
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kInt32: {
        // We consider Int32Constants together with tagged values.
        DCHECK(!value->allocation().IsConstant());
        Label done;
        __ movq(dst, ToMemOperand(value));
        __ addl(dst, dst);
        __ j(no_overflow, &done);
        // If we overflow, instead of bailing out (deopting), we change
        // representation to a HeapNumber.
        __ Cvtlsi2sd(D::GetDoubleRegisterParameter(D::kValue),
                     ToMemOperand(value));
        __ CallBuiltin(Builtin::kNewHeapNumber);
        __ Move(dst, kReturnRegister0);
        __ bind(&done);
        break;
      }
      case ValueRepresentation::kFloat64:
        if (Float64Constant* constant = value->TryCast<Float64Constant>()) {
          __ Move(D::GetDoubleRegisterParameter(D::kValue), constant->value());
        } else {
          __ Movsd(D::GetDoubleRegisterParameter(D::kValue),
                   ToMemOperand(value));
        }
        __ CallBuiltin(Builtin::kNewHeapNumber);
        __ Move(dst, kReturnRegister0);
        break;
      case ValueRepresentation::kTagged:
        UNREACHABLE();
    }
  }

  MemOperand ToMemOperand(ValueNode* node) const {
    DCHECK(node->allocation().IsAnyStackSlot());
    return masm_->ToMemOperand(node->allocation());
  }

  MemOperand ToMemOperand(const ValueLocation& location) const {
    DCHECK(location.operand().IsStackSlot());
    return masm_->ToMemOperand(location.operand());
  }

  MaglevAssembler* const masm_;
};

class MaglevCodeGeneratingNodeProcessor {
 public:
  explicit MaglevCodeGeneratingNodeProcessor(MaglevAssembler* masm)
      : masm_(masm) {}

  void PreProcessGraph(Graph* graph) {
    code_gen_state()->set_untagged_slots(graph->untagged_stack_slots());
    code_gen_state()->set_tagged_slots(graph->tagged_stack_slots());

    if (v8_flags.maglev_break_on_entry) {
      __ int3();
    }

    if (v8_flags.maglev_ool_prologue) {
      // Call the out-of-line prologue (with parameters passed on the stack).
      __ Push(Immediate(code_gen_state()->stack_slots() * kSystemPointerSize));
      __ Push(Immediate(code_gen_state()->tagged_slots() * kSystemPointerSize));
      __ CallBuiltin(Builtin::kMaglevOutOfLinePrologue);
    } else {
      __ BailoutIfDeoptimized(rbx);

      // Tiering support.
      // TODO(jgruber): Extract to a builtin (the tiering prologue is ~230 bytes
      // per Maglev code object on x64).
      {
        // Scratch registers. Don't clobber regs related to the calling
        // convention (e.g. kJavaScriptCallArgCountRegister). Keep up-to-date
        // with deferred flags code.
        Register flags = rcx;
        Register feedback_vector = r9;

        // Load the feedback vector.
        __ LoadTaggedPointerField(
            feedback_vector,
            FieldOperand(kJSFunctionRegister, JSFunction::kFeedbackCellOffset));
        __ LoadTaggedPointerField(
            feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));
        __ AssertFeedbackVector(feedback_vector);

        __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
            flags, feedback_vector, CodeKind::MAGLEV,
            &deferred_flags_need_processing_);
      }

      __ EnterFrame(StackFrame::MAGLEV);

      // Save arguments in frame.
      // TODO(leszeks): Consider eliding this frame if we don't make any calls
      // that could clobber these registers.
      __ Push(kContextRegister);
      __ Push(kJSFunctionRegister);              // Callee's JS function.
      __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

      {
        ASM_CODE_COMMENT_STRING(masm(), " Stack/interrupt check");
        // Stack check. This folds the checks for both the interrupt stack limit
        // check and the real stack limit into one by just checking for the
        // interrupt limit. The interrupt limit is either equal to the real
        // stack limit or tighter. By ensuring we have space until that limit
        // after building the frame we can quickly precheck both at once.
        __ Move(kScratchRegister, rsp);
        // TODO(leszeks): Include a max call argument size here.
        __ subq(kScratchRegister, Immediate(code_gen_state()->stack_slots() *
                                            kSystemPointerSize));
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

        // Magic value. Experimentally, an unroll size of 8 doesn't seem any
        // worse than fully unrolled pushes.
        const int kLoopUnrollSize = 8;
        int tagged_slots = graph->tagged_stack_slots();
        if (tagged_slots < 2 * kLoopUnrollSize) {
          // If the frame is small enough, just unroll the frame fill
          // completely.
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
        // Extend rsp by the size of the remaining untagged part of the frame,
        // no need to initialise these.
        __ subq(rsp,
                Immediate(graph->untagged_stack_slots() * kSystemPointerSize));
      }
    }
  }

  void PostProcessGraph(Graph*) {
    __ int3();

    if (!v8_flags.maglev_ool_prologue) {
      __ bind(&deferred_call_stack_guard_);
      {
        ASM_CODE_COMMENT_STRING(masm(), "Stack/interrupt call");
        // Save any registers that can be referenced by RegisterInput.
        // TODO(leszeks): Only push those that are used by the graph.
        __ PushAll(RegisterInput::kAllowedRegisters);
        // Push the frame size
        __ Push(Immediate(Smi::FromInt(code_gen_state()->stack_slots() *
                                       kSystemPointerSize)));
        __ CallRuntime(Runtime::kStackGuardWithGap, 1);
        __ PopAll(RegisterInput::kAllowedRegisters);
        __ jmp(&deferred_call_stack_guard_return_);
      }

      __ bind(&deferred_flags_need_processing_);
      {
        ASM_CODE_COMMENT_STRING(masm(), "Optimized marker check");
        // See PreProcessGraph.
        Register flags = rcx;
        Register feedback_vector = r9;
        // TODO(leszeks): This could definitely be a builtin that we tail-call.
        __ OptimizeCodeOrTailCallOptimizedCodeSlot(
            flags, feedback_vector, kJSFunctionRegister, JumpMode::kJump);
        __ Trap();
      }
    }
  }

  void PreProcessBasicBlock(BasicBlock* block) {
    if (v8_flags.code_comments) {
      std::stringstream ss;
      ss << "-- Block b" << graph_labeller()->BlockId(block);
      __ RecordComment(ss.str());
    }

    __ bind(block->label());
  }

  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state) {
    if (v8_flags.code_comments) {
      std::stringstream ss;
      ss << "--   " << graph_labeller()->NodeId(node) << ": "
         << PrintNode(graph_labeller(), node);
      __ RecordComment(ss.str());
    }

    if (v8_flags.debug_code) {
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
          if (v8_flags.code_comments) __ RecordComment("--   Spill:");
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
          if (v8_flags.code_comments) {
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
        if (v8_flags.code_comments) {
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
            if (v8_flags.code_comments) {
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
            if (v8_flags.code_comments) {
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

 private:
  MaglevAssembler* const masm_;
  Label deferred_call_stack_guard_;
  Label deferred_call_stack_guard_return_;
  Label deferred_flags_need_processing_;
};

class SafepointingNodeProcessor {
 public:
  explicit SafepointingNodeProcessor(LocalIsolate* local_isolate)
      : local_isolate_(local_isolate) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block) {}
  void Process(NodeBase* node, const ProcessingState& state) {
    local_isolate_->heap()->Safepoint();
  }

 private:
  LocalIsolate* local_isolate_;
};

namespace {
int GetFrameCount(const DeoptFrame& deopt_frame) {
  switch (deopt_frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return 1 + deopt_frame.as_interpreted().unit().inlining_depth();
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      return 1 + GetFrameCount(*deopt_frame.parent());
  }
}
BytecodeOffset GetBytecodeOffset(const DeoptFrame& deopt_frame) {
  switch (deopt_frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return deopt_frame.as_interpreted().bytecode_position();
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      return Builtins::GetContinuationBytecodeOffset(
          deopt_frame.as_builtin_continuation().builtin_id());
  }
}
SourcePosition GetSourcePosition(const DeoptFrame& deopt_frame) {
  switch (deopt_frame.type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return deopt_frame.as_interpreted().source_position();
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      return SourcePosition::Unknown();
  }
}
}  // namespace

class MaglevTranslationArrayBuilder {
 public:
  MaglevTranslationArrayBuilder(
      LocalIsolate* local_isolate, MaglevAssembler* masm,
      TranslationArrayBuilder* translation_array_builder,
      IdentityMap<int, base::DefaultAllocationPolicy>* deopt_literals)
      : local_isolate_(local_isolate),
        masm_(masm),
        translation_array_builder_(translation_array_builder),
        deopt_literals_(deopt_literals) {}

  void BuildEagerDeopt(EagerDeoptInfo* deopt_info) {
    int frame_count = GetFrameCount(deopt_info->top_frame());
    int jsframe_count = frame_count;
    int update_feedback_count = 0;
    deopt_info->set_translation_index(
        translation_array_builder_->BeginTranslation(frame_count, jsframe_count,
                                                     update_feedback_count));

    const InputLocation* current_input_location = deopt_info->input_locations();
    BuildDeoptFrame(deopt_info->top_frame(), current_input_location);
  }

  void BuildLazyDeopt(LazyDeoptInfo* deopt_info) {
    int frame_count = GetFrameCount(deopt_info->top_frame());
    int jsframe_count = frame_count;
    int update_feedback_count = 0;
    deopt_info->set_translation_index(
        translation_array_builder_->BeginTranslation(frame_count, jsframe_count,
                                                     update_feedback_count));

    const InputLocation* current_input_location = deopt_info->input_locations();

    if (deopt_info->top_frame().parent()) {
      // Deopt input locations are in the order of deopt frame emission, so
      // update the pointer after emitting the parent frame.
      BuildDeoptFrame(*deopt_info->top_frame().parent(),
                      current_input_location);
    }

    const DeoptFrame& top_frame = deopt_info->top_frame();
    switch (top_frame.type()) {
      case DeoptFrame::FrameType::kInterpretedFrame: {
        const InterpretedDeoptFrame& interpreted_frame =
            top_frame.as_interpreted();

        // Return offsets are counted from the end of the translation frame,
        // which is the array [parameters..., locals..., accumulator]. Since
        // it's the end, we don't need to worry about earlier frames.
        int return_offset;
        if (deopt_info->result_location() ==
            interpreter::Register::virtual_accumulator()) {
          return_offset = 0;
        } else if (deopt_info->result_location().is_parameter()) {
          // This is slightly tricky to reason about because of zero indexing
          // and fence post errors. As an example, consider a frame with 2
          // locals and 2 parameters, where we want argument index 1 -- looking
          // at the array in reverse order we have:
          //   [acc, r1, r0, a1, a0]
          //                  ^
          // and this calculation gives, correctly:
          //   2 + 2 - 1 = 3
          return_offset = interpreted_frame.unit().register_count() +
                          interpreted_frame.unit().parameter_count() -
                          deopt_info->result_location().ToParameterIndex();
        } else {
          return_offset = interpreted_frame.unit().register_count() -
                          deopt_info->result_location().index();
        }
        translation_array_builder_->BeginInterpretedFrame(
            interpreted_frame.bytecode_position(),
            GetDeoptLiteral(
                *interpreted_frame.unit().shared_function_info().object()),
            interpreted_frame.unit().register_count(), return_offset,
            deopt_info->result_size());

        BuildDeoptFrameValues(
            interpreted_frame.unit(), interpreted_frame.frame_state(),
            current_input_location, deopt_info->result_location(),
            deopt_info->result_size());
        break;
      }
      case DeoptFrame::FrameType::kBuiltinContinuationFrame: {
        const BuiltinContinuationDeoptFrame& builtin_continuation_frame =
            top_frame.as_builtin_continuation();

        translation_array_builder_->BeginBuiltinContinuationFrame(
            Builtins::GetContinuationBytecodeOffset(
                builtin_continuation_frame.builtin_id()),
            GetDeoptLiteral(*builtin_continuation_frame.parent()
                                 ->as_interpreted()
                                 .unit()
                                 .shared_function_info()
                                 .object()),
            builtin_continuation_frame.parameters().length());

        // Closure
        translation_array_builder_->StoreOptimizedOut();

        // Parameters
        for (ValueNode* value : builtin_continuation_frame.parameters()) {
          BuildDeoptFrameSingleValue(value, *current_input_location);
          current_input_location++;
        }

        // Context
        ValueNode* value = builtin_continuation_frame.context();
        BuildDeoptFrameSingleValue(value, *current_input_location);
        current_input_location++;
      }
    }
  }

 private:
  constexpr int DeoptStackSlotIndexFromFPOffset(int offset) {
    return 1 - offset / kSystemPointerSize;
  }

  int DeoptStackSlotFromStackSlot(const compiler::AllocatedOperand& operand) {
    return DeoptStackSlotIndexFromFPOffset(
        masm_->GetFramePointerOffsetForStackSlot(operand));
  }

  bool InReturnValues(interpreter::Register reg,
                      interpreter::Register result_location, int result_size) {
    if (result_size == 0 || !result_location.is_valid()) {
      return false;
    }
    return base::IsInRange(reg.index(), result_location.index(),
                           result_location.index() + result_size - 1);
  }

  void BuildDeoptFrame(const DeoptFrame& frame,
                       const InputLocation*& current_input_location) {
    if (frame.parent()) {
      // Deopt input locations are in the order of deopt frame emission, so
      // update the pointer after emitting the parent frame.
      BuildDeoptFrame(*frame.parent(), current_input_location);
    }

    switch (frame.type()) {
      case DeoptFrame::FrameType::kInterpretedFrame: {
        const InterpretedDeoptFrame& interpreted_frame = frame.as_interpreted();
        // Returns are used for updating an accumulator or register after a
        // lazy deopt.
        const int return_offset = 0;
        const int return_count = 0;
        translation_array_builder_->BeginInterpretedFrame(
            interpreted_frame.bytecode_position(),
            GetDeoptLiteral(
                *interpreted_frame.unit().shared_function_info().object()),
            interpreted_frame.unit().register_count(), return_offset,
            return_count);

        BuildDeoptFrameValues(
            interpreted_frame.unit(), interpreted_frame.frame_state(),
            current_input_location, interpreter::Register::invalid_value(),
            return_count);
        break;
      }
      case DeoptFrame::FrameType::kBuiltinContinuationFrame: {
        const BuiltinContinuationDeoptFrame& builtin_continuation_frame =
            frame.as_builtin_continuation();

        translation_array_builder_->BeginBuiltinContinuationFrame(
            Builtins::GetContinuationBytecodeOffset(
                builtin_continuation_frame.builtin_id()),
            GetDeoptLiteral(*builtin_continuation_frame.parent()
                                 ->as_interpreted()
                                 .unit()
                                 .shared_function_info()
                                 .object()),
            builtin_continuation_frame.parameters().length());

        // Closure
        translation_array_builder_->StoreOptimizedOut();

        // Parameters
        for (ValueNode* value : builtin_continuation_frame.parameters()) {
          BuildDeoptFrameSingleValue(value, *current_input_location);
          current_input_location++;
        }

        // Context
        ValueNode* value = builtin_continuation_frame.context();
        BuildDeoptFrameSingleValue(value, *current_input_location);
        current_input_location++;

        break;
      }
    }
  }

  void BuildDeoptStoreRegister(const compiler::AllocatedOperand& operand,
                               ValueRepresentation repr) {
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder_->StoreRegister(operand.GetRegister());
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder_->StoreInt32Register(operand.GetRegister());
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder_->StoreDoubleRegister(
            operand.GetDoubleRegister());
        break;
    }
  }

  void BuildDeoptStoreStackSlot(const compiler::AllocatedOperand& operand,
                                ValueRepresentation repr) {
    int stack_slot = DeoptStackSlotFromStackSlot(operand);
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder_->StoreStackSlot(stack_slot);
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder_->StoreInt32StackSlot(stack_slot);
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder_->StoreDoubleStackSlot(stack_slot);
        break;
    }
  }

  void BuildDeoptFrameSingleValue(ValueNode* value,
                                  const InputLocation& input_location) {
    if (input_location.operand().IsConstant()) {
      translation_array_builder_->StoreLiteral(
          GetDeoptLiteral(*value->Reify(local_isolate_)));
    } else {
      const compiler::AllocatedOperand& operand =
          compiler::AllocatedOperand::cast(input_location.operand());
      ValueRepresentation repr = value->properties().value_representation();
      if (operand.IsAnyRegister()) {
        BuildDeoptStoreRegister(operand, repr);
      } else {
        BuildDeoptStoreStackSlot(operand, repr);
      }
    }
  }

  void BuildDeoptFrameValues(
      const MaglevCompilationUnit& compilation_unit,
      const CompactInterpreterFrameState* checkpoint_state,
      const InputLocation*& input_location,
      interpreter::Register result_location, int result_size) {
    // Closure
    if (compilation_unit.inlining_depth() == 0) {
      int closure_index = DeoptStackSlotIndexFromFPOffset(
          StandardFrameConstants::kFunctionOffset);
      translation_array_builder_->StoreStackSlot(closure_index);
    } else {
      translation_array_builder_->StoreLiteral(
          GetDeoptLiteral(*compilation_unit.function().object()));
    }

    // TODO(leszeks): The input locations array happens to be in the same order
    // as parameters+context+locals+accumulator are accessed here. We should
    // make this clearer and guard against this invariant failing.

    // Parameters
    {
      int i = 0;
      checkpoint_state->ForEachParameter(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_EQ(reg.ToParameterIndex(), i);
            if (InReturnValues(reg, result_location, result_size)) {
              translation_array_builder_->StoreOptimizedOut();
            } else {
              BuildDeoptFrameSingleValue(value, *input_location);
              input_location++;
            }
            i++;
          });
    }

    // Context
    ValueNode* value = checkpoint_state->context(compilation_unit);
    BuildDeoptFrameSingleValue(value, *input_location);
    input_location++;

    // Locals
    {
      int i = 0;
      checkpoint_state->ForEachLocal(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_LE(i, reg.index());
            if (InReturnValues(reg, result_location, result_size)) return;
            while (i < reg.index()) {
              translation_array_builder_->StoreOptimizedOut();
              i++;
            }
            DCHECK_EQ(i, reg.index());
            BuildDeoptFrameSingleValue(value, *input_location);
            input_location++;
            i++;
          });
      while (i < compilation_unit.register_count()) {
        translation_array_builder_->StoreOptimizedOut();
        i++;
      }
    }

    // Accumulator
    {
      if (checkpoint_state->liveness()->AccumulatorIsLive() &&
          !InReturnValues(interpreter::Register::virtual_accumulator(),
                          result_location, result_size)) {
        ValueNode* value = checkpoint_state->accumulator(compilation_unit);
        BuildDeoptFrameSingleValue(value, *input_location);
        input_location++;
      } else {
        translation_array_builder_->StoreOptimizedOut();
      }
    }
  }

  int GetDeoptLiteral(Object obj) {
    IdentityMapFindResult<int> res = deopt_literals_->FindOrInsert(obj);
    if (!res.already_exists) {
      DCHECK_EQ(0, *res.entry);
      *res.entry = deopt_literals_->size() - 1;
    }
    return *res.entry;
  }

  LocalIsolate* local_isolate_;
  MaglevAssembler* masm_;
  TranslationArrayBuilder* translation_array_builder_;
  IdentityMap<int, base::DefaultAllocationPolicy>* deopt_literals_;
};

}  // namespace

MaglevCodeGenerator::MaglevCodeGenerator(
    LocalIsolate* isolate, MaglevCompilationInfo* compilation_info,
    Graph* graph)
    : local_isolate_(isolate),
      safepoint_table_builder_(compilation_info->zone(),
                               graph->tagged_stack_slots(),
                               graph->untagged_stack_slots()),
      translation_array_builder_(compilation_info->zone()),
      code_gen_state_(compilation_info, &safepoint_table_builder_),
      masm_(isolate->GetMainThreadIsolateUnsafe(), &code_gen_state_),
      graph_(graph),
      deopt_literals_(isolate->heap()->heap()) {}

void MaglevCodeGenerator::Assemble() {
  EmitCode();
  EmitMetadata();
}

MaybeHandle<Code> MaglevCodeGenerator::Generate(Isolate* isolate) {
  return BuildCodeObject(isolate);
}

void MaglevCodeGenerator::EmitCode() {
  GraphProcessor<NodeMultiProcessor<SafepointingNodeProcessor,
                                    MaglevCodeGeneratingNodeProcessor>>
      processor(SafepointingNodeProcessor{local_isolate_},
                MaglevCodeGeneratingNodeProcessor{masm()});
  processor.ProcessGraph(graph_);
  EmitDeferredCode();
  EmitDeopts();
  EmitExceptionHandlerTrampolines();
}

void MaglevCodeGenerator::EmitDeferredCode() {
  // Loop over deferred_code() multiple times, clearing the vector on each
  // outer loop, so that deferred code can itself emit deferred code.
  while (!code_gen_state_.deferred_code().empty()) {
    for (DeferredCodeInfo* deferred_code : code_gen_state_.TakeDeferredCode()) {
      __ RecordComment("-- Deferred block");
      __ bind(&deferred_code->deferred_code_label);
      deferred_code->Generate(masm());
      __ Trap();
    }
  }
}

void MaglevCodeGenerator::EmitDeopts() {
  MaglevTranslationArrayBuilder translation_builder(
      local_isolate_, &masm_, &translation_array_builder_, &deopt_literals_);

  deopt_exit_start_offset_ = __ pc_offset();

  int deopt_index = 0;

  __ RecordComment("-- Non-lazy deopts");
  for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
    local_isolate_->heap()->Safepoint();
    translation_builder.BuildEagerDeopt(deopt_info);

    if (masm_.compilation_info()->collect_source_positions()) {
      __ RecordDeoptReason(deopt_info->reason(), 0,
                           GetSourcePosition(deopt_info->top_frame()),
                           deopt_index);
    }
    __ bind(deopt_info->deopt_entry_label());
    __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Eager, deopt_index,
                             deopt_info->deopt_entry_label(),
                             DeoptimizeKind::kEager, nullptr, nullptr);
    deopt_index++;
  }

  __ RecordComment("-- Lazy deopts");
  int last_updated_safepoint = 0;
  for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
    local_isolate_->heap()->Safepoint();
    translation_builder.BuildLazyDeopt(deopt_info);

    if (masm_.compilation_info()->collect_source_positions()) {
      __ RecordDeoptReason(DeoptimizeReason::kUnknown, 0,
                           GetSourcePosition(deopt_info->top_frame()),
                           deopt_index);
    }
    __ bind(deopt_info->deopt_entry_label());
    __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Lazy, deopt_index,
                             deopt_info->deopt_entry_label(),
                             DeoptimizeKind::kLazy, nullptr, nullptr);

    last_updated_safepoint = safepoint_table_builder_.UpdateDeoptimizationInfo(
        deopt_info->deopting_call_return_pc(),
        deopt_info->deopt_entry_label()->pos(), last_updated_safepoint,
        deopt_index);
    deopt_index++;
  }
}

void MaglevCodeGenerator::EmitExceptionHandlerTrampolines() {
  if (code_gen_state_.handlers().size() == 0) return;
  __ RecordComment("-- Exception handler trampolines");
  for (NodeBase* node : code_gen_state_.handlers()) {
    ExceptionHandlerTrampolineBuilder::Build(masm(), node);
  }
}

void MaglevCodeGenerator::EmitMetadata() {
  // Final alignment before starting on the metadata section.
  masm()->Align(Code::kMetadataAlignment);

  safepoint_table_builder_.Emit(masm());

  // Exception handler table.
  handler_table_offset_ = HandlerTable::EmitReturnTableStart(masm());
  for (NodeBase* node : code_gen_state_.handlers()) {
    ExceptionHandlerInfo* info = node->exception_handler_info();
    HandlerTable::EmitReturnEntry(masm(), info->pc_offset,
                                  info->trampoline_entry.pos());
  }
}

MaybeHandle<Code> MaglevCodeGenerator::BuildCodeObject(Isolate* isolate) {
  CodeDesc desc;
  masm()->GetCode(isolate, &desc, &safepoint_table_builder_,
                  handler_table_offset_);
  return Factory::CodeBuilder{isolate, desc, CodeKind::MAGLEV}
      .set_stack_slots(stack_slot_count_with_fixed_frame())
      .set_deoptimization_data(GenerateDeoptimizationData(isolate))
      .TryBuild();
}

Handle<DeoptimizationData> MaglevCodeGenerator::GenerateDeoptimizationData(
    Isolate* isolate) {
  int eager_deopt_count =
      static_cast<int>(code_gen_state_.eager_deopts().size());
  int lazy_deopt_count = static_cast<int>(code_gen_state_.lazy_deopts().size());
  int deopt_count = lazy_deopt_count + eager_deopt_count;
  if (deopt_count == 0) {
    return DeoptimizationData::Empty(isolate);
  }
  Handle<DeoptimizationData> data =
      DeoptimizationData::New(isolate, deopt_count, AllocationType::kOld);

  Handle<TranslationArray> translation_array =
      translation_array_builder_.ToTranslationArray(isolate->factory());
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

  Handle<DeoptimizationLiteralArray> literals =
      isolate->factory()->NewDeoptimizationLiteralArray(deopt_literals_.size() +
                                                        1);
  // TODO(leszeks): Fix with the real inlining positions.
  Handle<PodArray<InliningPosition>> inlining_positions =
      PodArray<InliningPosition>::New(isolate, 0);
  DisallowGarbageCollection no_gc;

  auto raw_literals = *literals;
  auto raw_data = *data;
  IdentityMap<int, base::DefaultAllocationPolicy>::IteratableScope iterate(
      &deopt_literals_);
  for (auto it = iterate.begin(); it != iterate.end(); ++it) {
    raw_literals.set(*it.entry(), it.key());
  }
  // Add the bytecode to the deopt literals to make sure it's held strongly.
  // TODO(leszeks): Do this for inlined functions too.
  raw_literals.set(deopt_literals_.size(), *code_gen_state_.compilation_info()
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
    DCHECK_NE(deopt_info->translation_index(), -1);
    raw_data.SetBytecodeOffset(i, GetBytecodeOffset(deopt_info->top_frame()));
    raw_data.SetTranslationIndex(i,
                                 Smi::FromInt(deopt_info->translation_index()));
    raw_data.SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label()->pos()));
#ifdef DEBUG
    raw_data.SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
    i++;
  }
  for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
    DCHECK_NE(deopt_info->translation_index(), -1);
    raw_data.SetBytecodeOffset(i, GetBytecodeOffset(deopt_info->top_frame()));
    raw_data.SetTranslationIndex(i,
                                 Smi::FromInt(deopt_info->translation_index()));
    raw_data.SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label()->pos()));
#ifdef DEBUG
    raw_data.SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
    i++;
  }

  return data;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
