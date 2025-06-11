// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-selector.h"

#include <limits>
#include <optional>

#include "include/v8-internal.h"
#include "src/base/iterator.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/tick-counter.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-selector-adapter.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/globals.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/state-values-utils.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/numbers/conversions-inl.h"
#include "src/zone/zone-containers.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/simd-shuffle.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {
namespace compiler {

#define VISIT_UNSUPPORTED_OP(op) \
  void InstructionSelectorT::Visit##op(OpIndex) { UNIMPLEMENTED(); }

using namespace turboshaft;  // NOLINT(build/namespaces)

namespace {
// Here we really want the raw Bits of the mask, but the `.bits()` method is
// not constexpr, and so users of this constant need to call it.
// TODO(turboshaft): EffectDimensions could probably be defined via
// base::Flags<> instead, which should solve this.
constexpr EffectDimensions kTurboshaftEffectLevelMask =
    OpEffects().CanReadMemory().produces;
}

InstructionSelectorT::InstructionSelectorT(
    Zone* zone, size_t node_count, Linkage* linkage,
    InstructionSequence* sequence, Graph* schedule,
    source_position_table_t* source_positions, Frame* frame,
    InstructionSelector::EnableSwitchJumpTable enable_switch_jump_table,
    TickCounter* tick_counter, JSHeapBroker* broker,
    size_t* max_unoptimized_frame_height, size_t* max_pushed_argument_count,
    InstructionSelector::SourcePositionMode source_position_mode,
    Features features, InstructionSelector::EnableScheduling enable_scheduling,
    InstructionSelector::EnableRootsRelativeAddressing
        enable_roots_relative_addressing,
    InstructionSelector::EnableTraceTurboJson trace_turbo)
    : TurboshaftAdapter(schedule),
      zone_(zone),
      linkage_(linkage),
      sequence_(sequence),
      source_positions_(source_positions),
      source_position_mode_(source_position_mode),
      features_(features),
      schedule_(schedule),
      current_block_(nullptr),
      instructions_(zone),
      continuation_inputs_(sequence->zone()),
      continuation_outputs_(sequence->zone()),
      continuation_temps_(sequence->zone()),
      defined_(static_cast<int>(node_count), zone),
      used_(static_cast<int>(node_count), zone),
      effect_level_(node_count, 0, zone),
      virtual_registers_(node_count,
                         InstructionOperand::kInvalidVirtualRegister, zone),
      virtual_register_rename_(zone),
      scheduler_(nullptr),
      enable_scheduling_(enable_scheduling),
      enable_roots_relative_addressing_(enable_roots_relative_addressing),
      enable_switch_jump_table_(enable_switch_jump_table),
      state_values_cache_(zone),
      frame_(frame),
      instruction_selection_failed_(false),
      instr_origins_(sequence->zone()),
      trace_turbo_(trace_turbo),
      tick_counter_(tick_counter),
      broker_(broker),
      max_unoptimized_frame_height_(max_unoptimized_frame_height),
      max_pushed_argument_count_(max_pushed_argument_count)
#if V8_TARGET_ARCH_64_BIT
      ,
      node_count_(node_count),
      phi_states_(zone)
#endif
{
    turboshaft_use_map_.emplace(*schedule_, zone);
    protected_loads_to_remove_.emplace(static_cast<int>(node_count), zone);
    additional_protected_instructions_.emplace(static_cast<int>(node_count),
                                               zone);

  DCHECK_EQ(*max_unoptimized_frame_height, 0);  // Caller-initialized.

  instructions_.reserve(node_count);
  continuation_inputs_.reserve(5);
  continuation_outputs_.reserve(2);

  if (trace_turbo_ == InstructionSelector::kEnableTraceTurboJson) {
    instr_origins_.assign(node_count, {-1, 0});
  }
}

std::optional<BailoutReason> InstructionSelectorT::SelectInstructions() {
  // Mark the inputs of all phis in loop headers as used.
  ZoneVector<Block*> blocks = rpo_order(schedule());
  for (const Block* block : blocks) {
    if (!IsLoopHeader(block)) continue;
    DCHECK_LE(2u, PredecessorCount(block));
    for (OpIndex node : nodes(block)) {
      const PhiOp* phi = TryCast<PhiOp>(node);
      if (!phi) continue;

      // Mark all inputs as used.
      for (OpIndex input : phi->inputs()) {
        MarkAsUsed(input);
      }
    }
  }

  // Visit each basic block in post order.
  for (auto i = blocks.rbegin(); i != blocks.rend(); ++i) {
    VisitBlock(*i);
    if (instruction_selection_failed())
      return BailoutReason::kTurbofanCodeGenerationFailed;
  }

  // Schedule the selected instructions.
  if (UseInstructionScheduling()) {
    scheduler_ = zone()->template New<InstructionScheduler>(zone(), sequence());
  }

  for (const Block* block : blocks) {
    InstructionBlock* instruction_block =
        sequence()->InstructionBlockAt(this->rpo_number(block));
    for (size_t i = 0; i < instruction_block->phis().size(); i++) {
      UpdateRenamesInPhi(instruction_block->PhiAt(i));
    }
    size_t end = instruction_block->code_end();
    size_t start = instruction_block->code_start();
    DCHECK_LE(end, start);
    StartBlock(this->rpo_number(block));
    if (end != start) {
      while (start-- > end + 1) {
        UpdateRenames(instructions_[start]);
        AddInstruction(instructions_[start]);
      }
      UpdateRenames(instructions_[end]);
      AddTerminator(instructions_[end]);
    }
    EndBlock(this->rpo_number(block));
  }
#if DEBUG
  sequence()->ValidateSSA();
#endif
  return std::nullopt;
}

void InstructionSelectorT::StartBlock(RpoNumber rpo) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->StartBlock(rpo);
  } else {
    sequence()->StartBlock(rpo);
  }
}

void InstructionSelectorT::EndBlock(RpoNumber rpo) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->EndBlock(rpo);
  } else {
    sequence()->EndBlock(rpo);
  }
}

void InstructionSelectorT::AddTerminator(Instruction* instr) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->AddTerminator(instr);
  } else {
    sequence()->AddInstruction(instr);
  }
}

void InstructionSelectorT::AddInstruction(Instruction* instr) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->AddInstruction(instr);
  } else {
    sequence()->AddInstruction(instr);
  }
}

Instruction* InstructionSelectorT::Emit(InstructionCode opcode,
                                        InstructionOperand output,
                                        size_t temp_count,
                                        InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  return Emit(opcode, output_count, &output, 0, nullptr, temp_count, temps);
}

Instruction* InstructionSelectorT::Emit(InstructionCode opcode,
                                        InstructionOperand output,
                                        InstructionOperand a, size_t temp_count,
                                        InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  return Emit(opcode, output_count, &output, 1, &a, temp_count, temps);
}

Instruction* InstructionSelectorT::Emit(InstructionCode opcode,
                                        InstructionOperand output,
                                        InstructionOperand a,
                                        InstructionOperand b, size_t temp_count,
                                        InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

Instruction* InstructionSelectorT::Emit(InstructionCode opcode,
                                        InstructionOperand output,
                                        InstructionOperand a,
                                        InstructionOperand b,
                                        InstructionOperand c, size_t temp_count,
                                        InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

Instruction* InstructionSelectorT::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

Instruction* InstructionSelectorT::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    InstructionOperand e, size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d, e};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

Instruction* InstructionSelectorT::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    InstructionOperand e, InstructionOperand f, size_t temp_count,
    InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d, e, f};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

Instruction* InstructionSelectorT::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    InstructionOperand e, InstructionOperand f, InstructionOperand g,
    InstructionOperand h, size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d, e, f, g, h};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

Instruction* InstructionSelectorT::Emit(
    InstructionCode opcode, size_t output_count, InstructionOperand* outputs,
    size_t input_count, InstructionOperand* inputs, size_t temp_count,
    InstructionOperand* temps) {
  if (output_count >= Instruction::kMaxOutputCount ||
      input_count >= Instruction::kMaxInputCount ||
      temp_count >= Instruction::kMaxTempCount) {
    set_instruction_selection_failed();
    return nullptr;
  }

  Instruction* instr =
      Instruction::New(instruction_zone(), opcode, output_count, outputs,
                       input_count, inputs, temp_count, temps);
  return Emit(instr);
}

Instruction* InstructionSelectorT::Emit(Instruction* instr) {
  instructions_.push_back(instr);
  return instr;
}

namespace {
bool is_exclusive_user_of(const Graph* graph, OpIndex user, OpIndex value) {
  DCHECK(user.valid());
  DCHECK(value.valid());
  const Operation& value_op = graph->Get(value);
  const Operation& user_op = graph->Get(user);
  size_t use_count = base::count_if(
      user_op.inputs(), [value](OpIndex input) { return input == value; });
  if (V8_UNLIKELY(use_count == 0)) {
    // We have a special case here:
    //
    //         value
    //           |
    // TruncateWord64ToWord32
    //           |
    //         user
    //
    // If emitting user performs the truncation implicitly, we end up calling
    // CanCover with value and user such that user might have no (direct) uses
    // of value. There are cases of other unnecessary operations that can lead
    // to the same situation (e.g. bitwise and, ...). In this case, we still
    // cover if value has only a single use and this is one of the direct
    // inputs of user, which also only has a single use (in user).
    // TODO(nicohartmann@): We might generalize this further if we see use
    // cases.
    if (!value_op.saturated_use_count.IsOne()) return false;
    for (auto input : user_op.inputs()) {
      const Operation& input_op = graph->Get(input);
      const size_t indirect_use_count = base::count_if(
          input_op.inputs(), [value](OpIndex input) { return input == value; });
      if (indirect_use_count > 0) {
        return input_op.saturated_use_count.IsOne();
      }
    }
    return false;
  }
  if (value_op.Is<ProjectionOp>()) {
    // Projections always have a Tuple use, but it shouldn't count as a use as
    // far as is_exclusive_user_of is concerned, since no instructions are
    // emitted for the TupleOp, which is just a Turboshaft "meta operation".
    // We thus increase the use_count by 1, to attribute the TupleOp use to
    // the current operation.
    use_count++;
  }
  DCHECK_LE(use_count, graph->Get(value).saturated_use_count.Get());
  return (value_op.saturated_use_count.Get() == use_count) &&
         !value_op.saturated_use_count.IsSaturated();
}
}  // namespace

bool InstructionSelectorT::CanCover(OpIndex user, OpIndex node) const {
  // 1. Both {user} and {node} must be in the same basic block.
  if (block(schedule(), node) != current_block_) {
    return false;
  }

  const Operation& op = Get(node);
  // 2. If node does not produce anything, it can be covered.
  if (op.Effects().produces.bits() == 0) {
    return is_exclusive_user_of(schedule(), user, node);
  }

  // 3. Otherwise, the {node}'s effect level must match the {user}'s.
  if (GetEffectLevel(node) != current_effect_level_) {
    return false;
  }

  // 4. Only {node} must have value edges pointing to {user}.
  return is_exclusive_user_of(schedule(), user, node);
}

bool InstructionSelectorT::CanCoverProtectedLoad(OpIndex user,
                                                 OpIndex node) const {
  DCHECK(CanCover(user, node));
  const Graph* graph = this->turboshaft_graph();
  for (OpIndex next = graph->NextIndex(node); next.valid();
       next = graph->NextIndex(next)) {
    if (next == user) break;
    const Operation& op = graph->Get(next);
    OpEffects effects = op.Effects();
    if (effects.produces.control_flow || effects.required_when_unused) {
      return false;
    }
  }
  return true;
}

bool InstructionSelectorT::IsOnlyUserOfNodeInSameBlock(OpIndex user,
                                                       OpIndex node) const {
  Block* bb_user = this->block(schedule(), user);
  Block* bb_node = this->block(schedule(), node);
  if (bb_user != bb_node) return false;

  const Operation& node_op = this->turboshaft_graph()->Get(node);
  if (node_op.saturated_use_count.Get() == 1) return true;
  for (OpIndex use : turboshaft_uses(node)) {
    if (use == user) continue;
    if (this->block(schedule(), use) == bb_user) return false;
  }
    return true;
}

OptionalOpIndex InstructionSelectorT::FindProjection(OpIndex node,
                                                     size_t projection_index) {
  const Graph* graph = this->turboshaft_graph();
  // Projections are always emitted right after the operation.
  for (OpIndex next = graph->NextIndex(node); next.valid();
       next = graph->NextIndex(next)) {
    const ProjectionOp* projection = graph->Get(next).TryCast<ProjectionOp>();
    if (projection == nullptr) break;
    DCHECK(!projection->saturated_use_count.IsZero());
    if (projection->saturated_use_count.IsOne()) {
      // If the projection has a single use, it is the following tuple, so we
      // don't return it, since there is no point in emitting it.
      DCHECK(turboshaft_uses(next).size() == 1 &&
             graph->Get(turboshaft_uses(next)[0]).Is<TupleOp>());
      continue;
    }
    if (projection->index == projection_index) return next;
  }

  // If there is no Projection with index {projection_index} following the
  // operation, then there shouldn't be any such Projection in the graph. We
  // verify this in Debug mode.
#ifdef DEBUG
  for (OpIndex use : turboshaft_uses(node)) {
    if (const ProjectionOp* projection =
            this->Get(use).TryCast<ProjectionOp>()) {
      DCHECK_EQ(projection->input(), node);
      if (projection->index == projection_index) {
        // If we found the projection, it should have a single use: a Tuple
        // (which doesn't count as a regular use since it is just an artifact of
        // the Turboshaft graph).
        DCHECK(turboshaft_uses(use).size() == 1 &&
               graph->Get(turboshaft_uses(use)[0]).Is<TupleOp>());
      }
    }
  }
#endif  // DEBUG
  return OpIndex::Invalid();
}

void InstructionSelectorT::UpdateRenames(Instruction* instruction) {
  for (size_t i = 0; i < instruction->InputCount(); i++) {
    TryRename(instruction->InputAt(i));
  }
}

void InstructionSelectorT::UpdateRenamesInPhi(PhiInstruction* phi) {
  for (size_t i = 0; i < phi->operands().size(); i++) {
    int vreg = phi->operands()[i];
    int renamed = GetRename(vreg);
    if (vreg != renamed) {
      phi->RenameInput(i, renamed);
    }
  }
}

int InstructionSelectorT::GetRename(int virtual_register) {
  int rename = virtual_register;
  while (true) {
    if (static_cast<size_t>(rename) >= virtual_register_rename_.size()) break;
    int next = virtual_register_rename_[rename];
    if (next == InstructionOperand::kInvalidVirtualRegister) {
      break;
    }
    rename = next;
  }
  return rename;
}

void InstructionSelectorT::TryRename(InstructionOperand* op) {
  if (!op->IsUnallocated()) return;
  UnallocatedOperand* unalloc = UnallocatedOperand::cast(op);
  int vreg = unalloc->virtual_register();
  int rename = GetRename(vreg);
  if (rename != vreg) {
    *unalloc = UnallocatedOperand(*unalloc, rename);
  }
}

void InstructionSelectorT::SetRename(OpIndex node, OpIndex rename) {
  int vreg = GetVirtualRegister(node);
  if (static_cast<size_t>(vreg) >= virtual_register_rename_.size()) {
    int invalid = InstructionOperand::kInvalidVirtualRegister;
    virtual_register_rename_.resize(vreg + 1, invalid);
  }
  virtual_register_rename_[vreg] = GetVirtualRegister(rename);
}

int InstructionSelectorT::GetVirtualRegister(OpIndex node) {
  DCHECK(node.valid());
  size_t const id = node.id();
  DCHECK_LT(id, virtual_registers_.size());
  int virtual_register = virtual_registers_[id];
  if (virtual_register == InstructionOperand::kInvalidVirtualRegister) {
    virtual_register = sequence()->NextVirtualRegister();
    virtual_registers_[id] = virtual_register;
  }
  return virtual_register;
}

const std::map<uint32_t, int>
InstructionSelectorT::GetVirtualRegistersForTesting() const {
  std::map<uint32_t, int> virtual_registers;
  for (size_t n = 0; n < virtual_registers_.size(); ++n) {
    if (virtual_registers_[n] != InstructionOperand::kInvalidVirtualRegister) {
      const uint32_t id = static_cast<uint32_t>(n);
      virtual_registers.insert(std::make_pair(id, virtual_registers_[n]));
    }
  }
  return virtual_registers;
}

bool InstructionSelectorT::IsDefined(OpIndex node) const {
  DCHECK(node.valid());
  return defined_.Contains(node.id());
}

void InstructionSelectorT::MarkAsDefined(OpIndex node) {
  DCHECK(node.valid());
  defined_.Add(node.id());
}

bool InstructionSelectorT::IsUsed(OpIndex node) const {
  DCHECK(node.valid());
  if (!ShouldSkipOptimizationStep() && ShouldSkipOperation(this->Get(node))) {
    return false;
  }
  if (Get(node).IsRequiredWhenUnused()) return true;
  return used_.Contains(node.id());
}

bool InstructionSelectorT::IsReallyUsed(OpIndex node) const {
  DCHECK(node.valid());
  if (!ShouldSkipOptimizationStep() && ShouldSkipOperation(this->Get(node))) {
    return false;
  }
  return used_.Contains(node.id());
}

void InstructionSelectorT::MarkAsUsed(OpIndex node) {
  DCHECK(node.valid());
  used_.Add(node.id());
}

int InstructionSelectorT::GetEffectLevel(OpIndex node) const {
  DCHECK(node.valid());
  size_t const id = node.id();
  DCHECK_LT(id, effect_level_.size());
  return effect_level_[id];
}

int InstructionSelectorT::GetEffectLevel(OpIndex node,
                                         FlagsContinuation* cont) const {
  return cont->IsBranch() ? GetEffectLevel(this->block_terminator(
                                this->PredecessorAt(cont->true_block(), 0)))
                          : GetEffectLevel(node);
}

void InstructionSelectorT::SetEffectLevel(OpIndex node, int effect_level) {
  DCHECK(node.valid());
  size_t const id = node.id();
  DCHECK_LT(id, effect_level_.size());
  effect_level_[id] = effect_level;
}

bool InstructionSelectorT::CanAddressRelativeToRootsRegister(
    const ExternalReference& reference) const {
  // There are three things to consider here:
  // 1. CanUseRootsRegister: Is kRootRegister initialized?
  const bool root_register_is_available_and_initialized = CanUseRootsRegister();
  if (!root_register_is_available_and_initialized) return false;

  // 2. enable_roots_relative_addressing_: Can we address everything on the heap
  //    through the root register, i.e. are root-relative addresses to arbitrary
  //    addresses guaranteed not to change between code generation and
  //    execution?
  const bool all_root_relative_offsets_are_constant =
      (enable_roots_relative_addressing_ ==
       InstructionSelector::kEnableRootsRelativeAddressing);
  if (all_root_relative_offsets_are_constant) return true;

  // 3. IsAddressableThroughRootRegister: Is the target address guaranteed to
  //    have a fixed root-relative offset? If so, we can ignore 2.
  const bool this_root_relative_offset_is_constant =
      MacroAssemblerBase::IsAddressableThroughRootRegister(isolate(),
                                                           reference);
  return this_root_relative_offset_is_constant;
}

bool InstructionSelectorT::CanUseRootsRegister() const {
  return linkage()->GetIncomingDescriptor()->flags() &
         CallDescriptor::kCanUseRoots;
}

void InstructionSelectorT::MarkAsRepresentation(MachineRepresentation rep,
                                                const InstructionOperand& op) {
  UnallocatedOperand unalloc = UnallocatedOperand::cast(op);
  sequence()->MarkAsRepresentation(rep, unalloc.virtual_register());
}

void InstructionSelectorT::MarkAsRepresentation(MachineRepresentation rep,
                                                OpIndex node) {
  sequence()->MarkAsRepresentation(rep, GetVirtualRegister(node));
}

namespace {

InstructionOperand OperandForDeopt(Isolate* isolate, OperandGeneratorT* g,
                                   OpIndex input, FrameStateInputKind kind,
                                   MachineRepresentation rep) {
  if (rep == MachineRepresentation::kNone) {
    return g->TempImmediate(FrameStateDescriptor::kImpossibleValue);
  }

  const Operation& op = g->turboshaft_graph()->Get(input);
  if (const ConstantOp* constant = op.TryCast<ConstantOp>()) {
    using Kind = ConstantOp::Kind;
    switch (constant->kind) {
      case Kind::kWord32:
      case Kind::kWord64:
      case Kind::kSmi:
      case Kind::kFloat32:
      case Kind::kFloat64:
        return g->UseImmediate(input);
      case Kind::kNumber:
        if (rep == MachineRepresentation::kWord32) {
          const double d = constant->number().get_scalar();
          Tagged<Smi> smi = Smi::FromInt(static_cast<int32_t>(d));
          CHECK_EQ(smi.value(), d);
          return g->UseImmediate(static_cast<int32_t>(smi.ptr()));
        }
        return g->UseImmediate(input);
      case Kind::kHeapObject:
      case Kind::kCompressedHeapObject:
      case Kind::kTrustedHeapObject: {
        if (!CanBeTaggedOrCompressedPointer(rep)) {
          // If we have inconsistent static and dynamic types, e.g. if we
          // smi-check a string, we can get here with a heap object that
          // says it is a smi. In that case, we return an invalid instruction
          // operand, which will be interpreted as an optimized-out value.

          // TODO(jarin) Ideally, we should turn the current instruction
          // into an abort (we should never execute it).
          return InstructionOperand();
        }

        Handle<HeapObject> object = constant->handle();
        RootIndex root_index;
        if (isolate->roots_table().IsRootHandle(object, &root_index) &&
            root_index == RootIndex::kOptimizedOut) {
          // For an optimized-out object we return an invalid instruction
          // operand, so that we take the fast path for optimized-out values.
          return InstructionOperand();
        }

        return g->UseImmediate(input);
      }
      default:
        UNIMPLEMENTED();
    }
  } else if (const TaggedBitcastOp* bitcast =
                 op.TryCast<Opmask::kTaggedBitcastSmi>()) {
    const Operation& bitcast_input = g->Get(bitcast->input());
    if (const ConstantOp* cst =
            bitcast_input.TryCast<Opmask::kWord32Constant>()) {
      if constexpr (Is64()) {
        return g->UseImmediate64(cst->word32());
      } else {
        return g->UseImmediate(cst->word32());
      }
    } else if (Is64() && bitcast_input.Is<Opmask::kWord64Constant>()) {
      if (rep == MachineRepresentation::kWord32) {
        return g->UseImmediate(bitcast_input.Cast<ConstantOp>().word32());
      } else {
        return g->UseImmediate64(bitcast_input.Cast<ConstantOp>().word64());
      }
    }
  }

  switch (kind) {
    case FrameStateInputKind::kStackSlot:
      return g->UseUniqueSlot(input);
    case FrameStateInputKind::kAny:
      // Currently deopts "wrap" other operations, so the deopt's inputs
      // are potentially needed until the end of the deoptimising code.
      return g->UseAnyAtEnd(input);
  }
}

}  // namespace

enum class ObjectType { kRegularObject, kStringConcat };

class TurboshaftStateObjectDeduplicator {
 public:
  explicit TurboshaftStateObjectDeduplicator(Zone* zone)
      : objects_ids_mapping_(zone), string_ids_mapping_(zone) {}
  static constexpr size_t kNotDuplicated = std::numeric_limits<size_t>::max();

  size_t GetObjectId(uint32_t old_id, ObjectType type) {
    auto& ids_map = GetMapForType(type);
    auto it = ids_map.find(old_id);
    if (it == ids_map.end()) return kNotDuplicated;
    return it->second;
  }

  size_t InsertObject(uint32_t old_id, ObjectType type) {
    auto& ids_map = GetMapForType(type);
    uint32_t new_id = next_id_++;
    ids_map.insert({old_id, new_id});
    return new_id;
  }

  void InsertDummyForArgumentsElements() { next_id_++; }

 private:
  ZoneAbslFlatHashMap<uint32_t, uint32_t>& GetMapForType(ObjectType type) {
    switch (type) {
      case ObjectType::kRegularObject:
        return objects_ids_mapping_;
      case ObjectType::kStringConcat:
        return string_ids_mapping_;
    }
  }
  uint32_t next_id_ = 0;

  ZoneAbslFlatHashMap<uint32_t, uint32_t> objects_ids_mapping_;
  ZoneAbslFlatHashMap<uint32_t, uint32_t> string_ids_mapping_;
};

struct InstructionSelectorT::CachedStateValues : public ZoneObject {
 public:
  CachedStateValues(Zone* zone, StateValueList* values, size_t values_start,
                    InstructionOperandVector* inputs, size_t inputs_start)
      : inputs_(inputs->begin() + inputs_start, inputs->end(), zone),
        values_(values->MakeSlice(values_start)) {}

  size_t Emit(InstructionOperandVector* inputs, StateValueList* values) {
    inputs->insert(inputs->end(), inputs_.begin(), inputs_.end());
    values->PushCachedSlice(values_);
    return inputs_.size();
  }

 private:
  InstructionOperandVector inputs_;
  StateValueList::Slice values_;
};

size_t AddOperandToStateValueDescriptor(
    InstructionSelectorT* selector, StateValueList* values,
    InstructionOperandVector* inputs, OperandGeneratorT* g,
    TurboshaftStateObjectDeduplicator* deduplicator,
    FrameStateData::Iterator* it, FrameStateInputKind kind, Zone* zone) {
  switch (it->current_instr()) {
    case FrameStateData::Instr::kUnusedRegister:
      it->ConsumeUnusedRegister();
      values->PushOptimizedOut();
      return 0;
    case FrameStateData::Instr::kInput: {
      MachineType type;
      OpIndex input;
      it->ConsumeInput(&type, &input);
      const Operation& op = selector->Get(input);
      if (op.outputs_rep()[0] == RegisterRepresentation::Word64() &&
          type.representation() == MachineRepresentation::kWord32) {
        // 64 to 32-bit conversion is implicit in turboshaft.
        // TODO(nicohartmann@): Fix this once we have explicit truncations.
        UNIMPLEMENTED();
      }
      InstructionOperand instr_op = OperandForDeopt(
          selector->isolate(), g, input, kind, type.representation());
      if (instr_op.kind() == InstructionOperand::INVALID) {
        // Invalid operand means the value is impossible or optimized-out.
        values->PushOptimizedOut();
        return 0;
      } else {
        inputs->push_back(instr_op);
        values->PushPlain(type);
        return 1;
      }
    }
    case FrameStateData::Instr::kDematerializedObject: {
      uint32_t obj_id;
      uint32_t field_count;
      it->ConsumeDematerializedObject(&obj_id, &field_count);
      size_t id = deduplicator->GetObjectId(obj_id, ObjectType::kRegularObject);
      if (id == TurboshaftStateObjectDeduplicator::kNotDuplicated) {
        id = deduplicator->InsertObject(obj_id, ObjectType::kRegularObject);
        size_t entries = 0;
        StateValueList* nested = values->PushRecursiveField(zone, id);
        for (uint32_t i = 0; i < field_count; ++i) {
          entries += AddOperandToStateValueDescriptor(
              selector, nested, inputs, g, deduplicator, it, kind, zone);
        }
        return entries;
      } else {
        // Deoptimizer counts duplicate objects for the running id, so we have
        // to push the input again.
        deduplicator->InsertObject(obj_id, ObjectType::kRegularObject);
        values->PushDuplicate(id);
        return 0;
      }
    }
    case FrameStateData::Instr::kDematerializedObjectReference: {
      uint32_t obj_id;
      it->ConsumeDematerializedObjectReference(&obj_id);
      size_t id = deduplicator->GetObjectId(obj_id, ObjectType::kRegularObject);
      DCHECK_NE(id, TurboshaftStateObjectDeduplicator::kNotDuplicated);
      // Deoptimizer counts duplicate objects for the running id, so we have
      // to push the input again.
      deduplicator->InsertObject(obj_id, ObjectType::kRegularObject);
      values->PushDuplicate(id);
      return 0;
    }
    case FrameStateData::Instr::kDematerializedStringConcat: {
      DCHECK(v8_flags.turboshaft_string_concat_escape_analysis);
      uint32_t obj_id;
      it->ConsumeDematerializedStringConcat(&obj_id);
      size_t id = deduplicator->GetObjectId(obj_id, ObjectType::kStringConcat);
      if (id == TurboshaftStateObjectDeduplicator::kNotDuplicated) {
        id = deduplicator->InsertObject(obj_id, ObjectType::kStringConcat);
        StateValueList* nested = values->PushStringConcat(zone, id);
        static constexpr int kLeft = 1, kRight = 1;
        static constexpr int kInputCount = kLeft + kRight;
        size_t entries = 0;
        for (uint32_t i = 0; i < kInputCount; i++) {
          entries += AddOperandToStateValueDescriptor(
              selector, nested, inputs, g, deduplicator, it, kind, zone);
        }
        return entries;
      } else {
        // Deoptimizer counts duplicate objects for the running id, so we have
        // to push the input again.
        deduplicator->InsertObject(obj_id, ObjectType::kStringConcat);
        values->PushDuplicate(id);
        return 0;
      }
    }
    case FrameStateData::Instr::kDematerializedStringConcatReference: {
      DCHECK(v8_flags.turboshaft_string_concat_escape_analysis);
      uint32_t obj_id;
      it->ConsumeDematerializedStringConcatReference(&obj_id);
      size_t id = deduplicator->GetObjectId(obj_id, ObjectType::kStringConcat);
      DCHECK_NE(id, TurboshaftStateObjectDeduplicator::kNotDuplicated);
      // Deoptimizer counts duplicate objects for the running id, so we have
      // to push the input again.
      deduplicator->InsertObject(obj_id, ObjectType::kStringConcat);
      values->PushDuplicate(id);
      return 0;
    }
    case FrameStateData::Instr::kArgumentsElements: {
      CreateArgumentsType type;
      it->ConsumeArgumentsElements(&type);
      values->PushArgumentsElements(type);
      // The elements backing store of an arguments object participates in the
      // duplicate object counting, but can itself never appear duplicated.
      deduplicator->InsertDummyForArgumentsElements();
      return 0;
    }
    case FrameStateData::Instr::kArgumentsLength:
      it->ConsumeArgumentsLength();
      values->PushArgumentsLength();
      return 0;
    case FrameStateData::Instr::kRestLength:
      it->ConsumeRestLength();
      values->PushRestLength();
      return 0;
  }
  UNREACHABLE();
}

// Returns the number of instruction operands added to inputs.
size_t InstructionSelectorT::AddInputsToFrameStateDescriptor(
    FrameStateDescriptor* descriptor, OpIndex state_node, OperandGenerator* g,
    TurboshaftStateObjectDeduplicator* deduplicator,
    InstructionOperandVector* inputs, FrameStateInputKind kind, Zone* zone) {
  FrameStateOp& state =
      schedule()->Get(state_node).template Cast<FrameStateOp>();
  const FrameStateInfo& info = state.data->frame_state_info;
  USE(info);
  FrameStateData::Iterator it = state.data->iterator(state.state_values());

  size_t entries = 0;
  size_t initial_size = inputs->size();
  USE(initial_size);  // initial_size is only used for debug.
  if (descriptor->outer_state()) {
    entries += AddInputsToFrameStateDescriptor(
        descriptor->outer_state(), state.parent_frame_state(), g, deduplicator,
        inputs, kind, zone);
  }

  DCHECK_EQ(descriptor->parameters_count(), info.parameter_count());
  DCHECK_EQ(descriptor->locals_count(), info.local_count());
  DCHECK_EQ(descriptor->stack_count(), info.stack_count());

  StateValueList* values_descriptor = descriptor->GetStateValueDescriptors();

  DCHECK_EQ(values_descriptor->size(), 0u);
  values_descriptor->ReserveSize(descriptor->GetSize());

  // Function
  if (descriptor->HasClosure()) {
    entries += v8::internal::compiler::AddOperandToStateValueDescriptor(
        this, values_descriptor, inputs, g, deduplicator, &it,
        FrameStateInputKind::kStackSlot, zone);
  } else {
    // Advance the iterator either way.
    MachineType unused_type;
    OpIndex unused_input;
    it.ConsumeInput(&unused_type, &unused_input);
  }

  // Parameters
  for (size_t i = 0; i < descriptor->parameters_count(); ++i) {
    entries += v8::internal::compiler::AddOperandToStateValueDescriptor(
        this, values_descriptor, inputs, g, deduplicator, &it, kind, zone);
  }

  // Context
  if (descriptor->HasContext()) {
    entries += v8::internal::compiler::AddOperandToStateValueDescriptor(
        this, values_descriptor, inputs, g, deduplicator, &it,
        FrameStateInputKind::kStackSlot, zone);
  } else {
    // Advance the iterator either way.
    MachineType unused_type;
    OpIndex unused_input;
    it.ConsumeInput(&unused_type, &unused_input);
  }

  // Locals
  for (size_t i = 0; i < descriptor->locals_count(); ++i) {
    entries += v8::internal::compiler::AddOperandToStateValueDescriptor(
        this, values_descriptor, inputs, g, deduplicator, &it, kind, zone);
  }

  // Stack
  for (size_t i = 0; i < descriptor->stack_count(); ++i) {
    entries += v8::internal::compiler::AddOperandToStateValueDescriptor(
        this, values_descriptor, inputs, g, deduplicator, &it, kind, zone);
  }

  DCHECK_EQ(initial_size + entries, inputs->size());
  return entries;
}

Instruction* InstructionSelectorT::EmitWithContinuation(
    InstructionCode opcode, InstructionOperand a, FlagsContinuation* cont) {
  return EmitWithContinuation(opcode, 0, nullptr, 1, &a, cont);
}

Instruction* InstructionSelectorT::EmitWithContinuation(
    InstructionCode opcode, InstructionOperand a, InstructionOperand b,
    FlagsContinuation* cont) {
  InstructionOperand inputs[] = {a, b};
  return EmitWithContinuation(opcode, 0, nullptr, arraysize(inputs), inputs,
                              cont);
}

Instruction* InstructionSelectorT::EmitWithContinuation(
    InstructionCode opcode, InstructionOperand a, InstructionOperand b,
    InstructionOperand c, FlagsContinuation* cont) {
  InstructionOperand inputs[] = {a, b, c};
  return EmitWithContinuation(opcode, 0, nullptr, arraysize(inputs), inputs,
                              cont);
}

Instruction* InstructionSelectorT::EmitWithContinuation(
    InstructionCode opcode, size_t output_count, InstructionOperand* outputs,
    size_t input_count, InstructionOperand* inputs, FlagsContinuation* cont) {
  return EmitWithContinuation(opcode, output_count, outputs, input_count,
                              inputs, 0, nullptr, cont);
}

Instruction* InstructionSelectorT::EmitWithContinuation(
    InstructionCode opcode, size_t output_count, InstructionOperand* outputs,
    size_t input_count, InstructionOperand* inputs, size_t temp_count,
    InstructionOperand* temps, FlagsContinuation* cont) {
  OperandGenerator g(this);

  opcode = cont->Encode(opcode);

  continuation_inputs_.resize(0);
  for (size_t i = 0; i < input_count; i++) {
    continuation_inputs_.push_back(inputs[i]);
  }

  continuation_outputs_.resize(0);
  for (size_t i = 0; i < output_count; i++) {
    continuation_outputs_.push_back(outputs[i]);
  }

  continuation_temps_.resize(0);
  for (size_t i = 0; i < temp_count; i++) {
    continuation_temps_.push_back(temps[i]);
  }

  if (cont->IsBranch() || cont->IsConditionalBranch()) {
    continuation_inputs_.push_back(g.Label(cont->true_block()));
    continuation_inputs_.push_back(g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    int immediate_args_count = 0;
    opcode |= DeoptImmedArgsCountField::encode(immediate_args_count) |
              DeoptFrameStateOffsetField::encode(static_cast<int>(input_count));
    AppendDeoptimizeArguments(&continuation_inputs_, cont->reason(),
                              cont->node_id(), cont->feedback(),
                              cont->frame_state());
  } else if (cont->IsSet() || cont->IsConditionalSet()) {
    continuation_outputs_.push_back(g.DefineAsRegister(cont->result()));
  } else if (cont->IsSelect()) {
    // The {Select} should put one of two values into the output register,
    // depending on the result of the condition. The two result values are in
    // the last two input slots, the {false_value} in {input_count - 2}, and the
    // true_value in {input_count - 1}. The other inputs are used for the
    // condition.
    AddOutputToSelectContinuation(&g, static_cast<int>(input_count) - 2,
                                  cont->result());
  } else if (cont->IsTrap()) {
    int trap_id = static_cast<int>(cont->trap_id());
    continuation_inputs_.push_back(g.UseImmediate(trap_id));
  } else {
    DCHECK(cont->IsNone());
  }

  size_t const emit_inputs_size = continuation_inputs_.size();
  auto* emit_inputs =
      emit_inputs_size ? &continuation_inputs_.front() : nullptr;
  size_t const emit_outputs_size = continuation_outputs_.size();
  auto* emit_outputs =
      emit_outputs_size ? &continuation_outputs_.front() : nullptr;
  size_t const emit_temps_size = continuation_temps_.size();
  auto* emit_temps = emit_temps_size ? &continuation_temps_.front() : nullptr;
  return Emit(opcode, emit_outputs_size, emit_outputs, emit_inputs_size,
              emit_inputs, emit_temps_size, emit_temps);
}

void InstructionSelectorT::AppendDeoptimizeArguments(
    InstructionOperandVector* args, DeoptimizeReason reason, uint32_t node_id,
    FeedbackSource const& feedback, OpIndex frame_state, DeoptimizeKind kind) {
  OperandGenerator g(this);
  FrameStateDescriptor* const descriptor = GetFrameStateDescriptor(frame_state);
  int const state_id = sequence()->AddDeoptimizationEntry(
      descriptor, kind, reason, node_id, feedback);
  args->push_back(g.TempImmediate(state_id));
  StateObjectDeduplicator deduplicator(instruction_zone());
  AddInputsToFrameStateDescriptor(descriptor, frame_state, &g, &deduplicator,
                                  args, FrameStateInputKind::kAny,
                                  instruction_zone());
}

// An internal helper class for generating the operands to calls.
// TODO(bmeurer): Get rid of the CallBuffer business and make
// InstructionSelector::VisitCall platform independent instead.
struct CallBufferT {
  CallBufferT(Zone* zone, const CallDescriptor* call_descriptor,
              FrameStateDescriptor* frame_state)
      : descriptor(call_descriptor),
        frame_state_descriptor(frame_state),
        output_nodes(zone),
        outputs(zone),
        instruction_args(zone),
        pushed_nodes(zone) {
    output_nodes.reserve(call_descriptor->ReturnCount());
    outputs.reserve(call_descriptor->ReturnCount());
    pushed_nodes.reserve(input_count());
    instruction_args.reserve(input_count() + frame_state_value_count());
  }

  const CallDescriptor* descriptor;
  FrameStateDescriptor* frame_state_descriptor;
  ZoneVector<PushParameterT> output_nodes;
  InstructionOperandVector outputs;
  InstructionOperandVector instruction_args;
  ZoneVector<PushParameterT> pushed_nodes;

  size_t input_count() const { return descriptor->InputCount(); }

  size_t frame_state_count() const { return descriptor->FrameStateCount(); }

  size_t frame_state_value_count() const {
    return (frame_state_descriptor == nullptr)
               ? 0
               : (frame_state_descriptor->GetTotalSize() +
                  1);  // Include deopt id.
  }
};

// TODO(bmeurer): Get rid of the CallBuffer business and make
// InstructionSelector::VisitCall platform independent instead.
void InstructionSelectorT::InitializeCallBuffer(
    OpIndex node, CallBuffer* buffer, CallBufferFlags flags, OpIndex callee,
    OptionalOpIndex frame_state_opt, base::Vector<const OpIndex> arguments,
    int return_count, int stack_param_delta) {
  OperandGenerator g(this);
  size_t ret_count = buffer->descriptor->ReturnCount();
  bool is_tail_call = (flags & kCallTail) != 0;
  DCHECK_LE(return_count, ret_count);

  if (ret_count > 0) {
    // Collect the projections that represent multiple outputs from this call.
    if (ret_count == 1) {
      PushParameter result = {node, buffer->descriptor->GetReturnLocation(0)};
      buffer->output_nodes.push_back(result);
    } else {
      buffer->output_nodes.resize(ret_count);
      for (size_t i = 0; i < ret_count; ++i) {
        LinkageLocation location = buffer->descriptor->GetReturnLocation(i);
        buffer->output_nodes[i] = PushParameter({}, location);
      }
      for (OpIndex call_use : turboshaft_uses(node)) {
        const Operation& use_op = this->Get(call_use);
        if (use_op.Is<DidntThrowOp>()) {
          for (OpIndex use : turboshaft_uses(call_use)) {
            const ProjectionOp& projection = Cast<ProjectionOp>(use);
            size_t index = projection.index;
            DCHECK_LT(index, buffer->output_nodes.size());
            DCHECK(!buffer->output_nodes[index].node.valid());
            buffer->output_nodes[index].node = use;
          }
        } else {
          DCHECK(use_op.Is<CheckExceptionOp>());
        }
      }
      frame_->EnsureReturnSlots(
          static_cast<int>(buffer->descriptor->ReturnSlotCount()));
    }

    // Filter out the outputs that aren't live because no projection uses them.
    size_t outputs_needed_by_framestate =
        buffer->frame_state_descriptor == nullptr
            ? 0
            : buffer->frame_state_descriptor->state_combine()
                  .ConsumedOutputCount();
    for (size_t i = 0; i < buffer->output_nodes.size(); i++) {
      bool output_is_live = buffer->output_nodes[i].node.valid() ||
                            i < outputs_needed_by_framestate;
      if (output_is_live) {
        LinkageLocation location = buffer->output_nodes[i].location;
        MachineRepresentation rep = location.GetType().representation();

        OpIndex output = buffer->output_nodes[i].node;
        InstructionOperand op = !output.valid()
                                    ? g.TempLocation(location)
                                    : g.DefineAsLocation(output, location);
        MarkAsRepresentation(rep, op);

        if (!UnallocatedOperand::cast(op).HasFixedSlotPolicy()) {
          buffer->outputs.push_back(op);
          buffer->output_nodes[i].node = {};
        }
      }
    }
  }

  // The first argument is always the callee code.
  bool call_code_immediate = (flags & kCallCodeImmediate) != 0;
  bool call_address_immediate = (flags & kCallAddressImmediate) != 0;
  bool call_use_fixed_target_reg = (flags & kCallFixedTargetRegister) != 0;
  DeoptimizeKind deopt_kind = DeoptimizeKind::kLazy;
  switch (buffer->descriptor->kind()) {
    case CallDescriptor::kCallCodeObject:
      buffer->instruction_args.push_back(
          (call_code_immediate && this->IsHeapConstant(callee))
              ? g.UseImmediate(callee)
          : call_use_fixed_target_reg
              ? g.UseFixed(callee, kJavaScriptCallCodeStartRegister)
              : g.UseRegister(callee));
      break;
    case CallDescriptor::kCallAddress:
      // TODO(ahaas): Rename kLazyAfterFastCall and similarly called fields on
      // the isolate to reflect that they are used for every direct call to C++
      // and not just for fast API calls.
      deopt_kind = DeoptimizeKind::kLazyAfterFastCall;
      buffer->instruction_args.push_back(
          (call_address_immediate && this->IsExternalConstant(callee))
              ? g.UseImmediate(callee)
          : call_use_fixed_target_reg
              ? g.UseFixed(callee, kJavaScriptCallCodeStartRegister)
              : g.UseRegister(callee));
      break;
#if V8_ENABLE_WEBASSEMBLY
    case CallDescriptor::kCallWasmCapiFunction:
    case CallDescriptor::kCallWasmFunction:
    case CallDescriptor::kCallWasmFunctionIndirect:
    case CallDescriptor::kCallWasmImportWrapper:
      buffer->instruction_args.push_back(
          (call_address_immediate && this->IsRelocatableWasmConstant(callee))
              ? g.UseImmediate(callee)
          : call_use_fixed_target_reg
              ? g.UseFixed(callee, kJavaScriptCallCodeStartRegister)
              : g.UseRegister(callee));
      break;
#endif  // V8_ENABLE_WEBASSEMBLY
    case CallDescriptor::kCallBuiltinPointer: {
      // The common case for builtin pointers is to have the target in a
      // register. If we have a constant, we use a register anyway to simplify
      // related code.
      LinkageLocation location = buffer->descriptor->GetInputLocation(0);
      bool location_is_fixed_register =
          location.IsRegister() && !location.IsAnyRegister();
      InstructionOperand op;
      // If earlier phases specified a particular register, don't override
      // their choice.
      if (location_is_fixed_register) {
        op = g.UseLocation(callee, location);
      } else if (call_use_fixed_target_reg) {
        op = g.UseFixed(callee, kJavaScriptCallCodeStartRegister);
      } else {
        op = g.UseRegister(callee);
      }
      buffer->instruction_args.push_back(op);
      break;
    }
    case CallDescriptor::kCallJSFunction:
      buffer->instruction_args.push_back(
          g.UseLocation(callee, buffer->descriptor->GetInputLocation(0)));
      break;
  }
  DCHECK_EQ(1u, buffer->instruction_args.size());

  // If the call needs a frame state, we insert the state information as
  // follows (n is the number of value inputs to the frame state):
  // arg 1               : deoptimization id.
  // arg 2 - arg (n + 2) : value inputs to the frame state.
  size_t frame_state_entries = 0;
  USE(frame_state_entries);  // frame_state_entries is only used for debug.
  if (buffer->frame_state_descriptor != nullptr) {
    OpIndex frame_state = frame_state_opt.value();

    // If it was a syntactic tail call we need to drop the current frame and
    // all the frames on top of it that are either inlined extra arguments
    // or a tail caller frame.
    if (is_tail_call) {
      frame_state = Cast<FrameStateOp>(frame_state).parent_frame_state();
      buffer->frame_state_descriptor =
          buffer->frame_state_descriptor->outer_state();
      while (buffer->frame_state_descriptor != nullptr &&
             buffer->frame_state_descriptor->type() ==
                 FrameStateType::kInlinedExtraArguments) {
        frame_state = Cast<FrameStateOp>(frame_state).parent_frame_state();
        buffer->frame_state_descriptor =
            buffer->frame_state_descriptor->outer_state();
      }
    }

    int const state_id = sequence()->AddDeoptimizationEntry(
        buffer->frame_state_descriptor, deopt_kind, DeoptimizeReason::kUnknown,
        node.id(), FeedbackSource());
    buffer->instruction_args.push_back(g.TempImmediate(state_id));

    StateObjectDeduplicator deduplicator(instruction_zone());

    frame_state_entries =
        1 + AddInputsToFrameStateDescriptor(
                buffer->frame_state_descriptor, frame_state, &g, &deduplicator,
                &buffer->instruction_args, FrameStateInputKind::kStackSlot,
                instruction_zone());

    DCHECK_EQ(1 + frame_state_entries, buffer->instruction_args.size());
  }

  size_t input_count = buffer->input_count();

  // Split the arguments into pushed_nodes and instruction_args. Pushed
  // arguments require an explicit push instruction before the call and do
  // not appear as arguments to the call. Everything else ends up
  // as an InstructionOperand argument to the call.
  auto iter(arguments.begin());
  size_t pushed_count = 0;
  for (size_t index = 1; index < input_count; ++iter, ++index) {
    DCHECK_NE(iter, arguments.end());

    LinkageLocation location = buffer->descriptor->GetInputLocation(index);
    if (is_tail_call) {
      location = LinkageLocation::ConvertToTailCallerLocation(
          location, stack_param_delta);
    }
    InstructionOperand op = g.UseLocation(*iter, location);
    UnallocatedOperand unallocated = UnallocatedOperand::cast(op);
    if (unallocated.HasFixedSlotPolicy() && !is_tail_call) {
      int stack_index = buffer->descriptor->GetStackIndexFromSlot(
          unallocated.fixed_slot_index());
      // This can insert empty slots before stack_index and will insert enough
      // slots after stack_index to store the parameter.
      if (static_cast<size_t>(stack_index) >= buffer->pushed_nodes.size()) {
        int num_slots = location.GetSizeInPointers();
        buffer->pushed_nodes.resize(stack_index + num_slots);
      }
      PushParameter param = {*iter, location};
      buffer->pushed_nodes[stack_index] = param;
      pushed_count++;
    } else {
      if (location.IsNullRegister()) {
        EmitMoveFPRToParam(&op, location);
      }
      buffer->instruction_args.push_back(op);
    }
  }
  DCHECK_EQ(input_count, buffer->instruction_args.size() + pushed_count -
                             frame_state_entries);
  USE(pushed_count);
  if (V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK && is_tail_call &&
      stack_param_delta != 0) {
    // For tail calls that change the size of their parameter list and keep
    // their return address on the stack, move the return address to just above
    // the parameters.
    LinkageLocation saved_return_location =
        LinkageLocation::ForSavedCallerReturnAddress();
    InstructionOperand return_address =
        g.UsePointerLocation(LinkageLocation::ConvertToTailCallerLocation(
                                 saved_return_location, stack_param_delta),
                             saved_return_location);
    buffer->instruction_args.push_back(return_address);
  }
}

void InstructionSelectorT::UpdateSourcePosition(Instruction* instruction,
                                                OpIndex node) {
  sequence()->SetSourcePosition(instruction, (*source_positions_)[node]);
}

bool InstructionSelectorT::IsSourcePositionUsed(OpIndex node) {
  if (source_position_mode_ == InstructionSelector::kAllSourcePositions) {
    return true;
  }
  const Operation& operation = this->Get(node);
  // DidntThrow is where the actual call is generated.
  if (operation.Is<DidntThrowOp>()) return true;
  if (const LoadOp* load = operation.TryCast<LoadOp>()) {
    return load->kind.with_trap_handler;
  }
    if (const StoreOp* store = operation.TryCast<StoreOp>()) {
      return store->kind.with_trap_handler;
    }
#if V8_ENABLE_WEBASSEMBLY
    if (operation.Is<TrapIfOp>()) return true;
    if (const AtomicRMWOp* rmw = operation.TryCast<AtomicRMWOp>()) {
      return rmw->memory_access_kind ==
             MemoryAccessKind::kProtectedByTrapHandler;
    }
    if (const Simd128LoadTransformOp* lt =
            operation.TryCast<Simd128LoadTransformOp>()) {
      return lt->load_kind.with_trap_handler;
    }
#if V8_ENABLE_WASM_SIMD256_REVEC
    if (const Simd256LoadTransformOp* lt =
            operation.TryCast<Simd256LoadTransformOp>()) {
      return lt->load_kind.with_trap_handler;
    }
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
    if (const Simd128LaneMemoryOp* lm =
            operation.TryCast<Simd128LaneMemoryOp>()) {
      return lm->kind.with_trap_handler;
    }
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
    if (const Simd128LoadPairDeinterleaveOp* dl =
            operation.TryCast<Simd128LoadPairDeinterleaveOp>()) {
      return dl->load_kind.with_trap_handler;
    }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
#endif
    if (additional_protected_instructions_->Contains(node.id())) {
      return true;
    }
    return false;
}

bool InstructionSelectorT::IsCommutative(turboshaft::OpIndex node) const {
  const turboshaft::Operation& op = Get(node);
  if (const auto word_binop = op.TryCast<turboshaft::WordBinopOp>()) {
    return turboshaft::WordBinopOp::IsCommutative(word_binop->kind);
  } else if (const auto overflow_binop =
                 op.TryCast<turboshaft::OverflowCheckedBinopOp>()) {
    return turboshaft::OverflowCheckedBinopOp::IsCommutative(
        overflow_binop->kind);
  } else if (const auto float_binop = op.TryCast<turboshaft::FloatBinopOp>()) {
    return turboshaft::FloatBinopOp::IsCommutative(float_binop->kind);
  } else if (const auto comparison = op.TryCast<turboshaft::ComparisonOp>()) {
    return turboshaft::ComparisonOp::IsCommutative(comparison->kind);
  }
  return false;
}
namespace {
bool increment_effect_level_for_node(TurboshaftAdapter* adapter, OpIndex node) {
  // We need to increment the effect level if the operation consumes any of the
  // dimensions of the {kTurboshaftEffectLevelMask}.
  const Operation& op = adapter->Get(node);
  if (op.Is<RetainOp>()) {
    // Retain has CanWrite effect so that it's not reordered before the last
    // read it protects, but it shouldn't increment the effect level, since
    // doing a Load(x) after a Retain(x) is safe as long as there is not call
    // (or something that can trigger GC) in between Retain(x) and Load(x), and
    // if there were, then this call would increment the effect level, which
    // would prevent covering in the ISEL.
    return false;
  }
  return (op.Effects().consumes.bits() & kTurboshaftEffectLevelMask.bits()) !=
         0;
}
}  // namespace

void InstructionSelectorT::VisitBlock(const Block* block) {
  DCHECK(!current_block_);
  current_block_ = block;
  auto current_num_instructions = [&] {
    DCHECK_GE(kMaxInt, instructions_.size());
    return static_cast<int>(instructions_.size());
  };
  int current_block_end = current_num_instructions();

  int effect_level = 0;
  for (OpIndex node : this->nodes(block)) {
    SetEffectLevel(node, effect_level);
    if (increment_effect_level_for_node(this, node)) {
      ++effect_level;
    }
  }

  // We visit the control first, then the nodes in the block, so the block's
  // control input should be on the same effect level as the last node.
  if (OpIndex terminator = this->block_terminator(block); terminator.valid()) {
    SetEffectLevel(terminator, effect_level);
    current_effect_level_ = effect_level;
  }

  auto FinishEmittedInstructions = [&](OpIndex node, int instruction_start) {
    if (instruction_selection_failed()) return false;
    if (current_num_instructions() == instruction_start) return true;
    std::reverse(instructions_.begin() + instruction_start,
                 instructions_.end());
    if (!node.valid()) return true;
    if (!source_positions_) return true;

    SourcePosition source_position;
#if V8_ENABLE_WEBASSEMBLY && V8_TARGET_ARCH_X64
    if (const Simd128UnaryOp* op =
            TryCast<Opmask::kSimd128F64x2PromoteLowF32x4>(node);
        V8_UNLIKELY(op)) {
      // On x64 there exists an optimization that folds
      // `kF64x2PromoteLowF32x4` and `kS128Load64Zero` together into a single
      // instruction. If the instruction causes an out-of-bounds memory
      // access exception, then the stack trace has to show the source
      // position of the `kS128Load64Zero` and not of the
      // `kF64x2PromoteLowF32x4`.
      if (CanOptimizeF64x2PromoteLowF32x4(node)) {
        node = op->input();
      }
    }
#endif  // V8_ENABLE_WEBASSEMBLY && V8_TARGET_ARCH_X64
    source_position = (*source_positions_)[node];
    if (source_position.IsKnown() && IsSourcePositionUsed(node)) {
      sequence()->SetSourcePosition(instructions_.back(), source_position);
    }
    return true;
  };

  // Generate code for the block control "top down", but schedule the code
  // "bottom up".
  VisitControl(block);
  if (!FinishEmittedInstructions(this->block_terminator(block),
                                 current_block_end)) {
    return;
  }

  // Visit code in reverse control flow order, because architecture-specific
  // matching may cover more than one node at a time.
  for (OpIndex node : base::Reversed(this->nodes(block))) {
    int current_node_end = current_num_instructions();

    if (protected_loads_to_remove_->Contains(node.id()) &&
        !IsReallyUsed(node)) {
      MarkAsDefined(node);
    }

    if (!IsUsed(node)) {
      // Skip nodes that are unused, while marking them as Defined so that it's
      // clear that these unused nodes have been visited and will not be Defined
      // later.
      MarkAsDefined(node);
    } else if (!IsDefined(node)) {
      // Generate code for this node "top down", but schedule the code "bottom
      // up".
      current_effect_level_ = GetEffectLevel(node);
      VisitNode(node);
      if (!FinishEmittedInstructions(node, current_node_end)) return;
    }
    if (trace_turbo_ == InstructionSelector::kEnableTraceTurboJson) {
      instr_origins_[node.id()] = {current_num_instructions(),
                                   current_node_end};
    }
  }

  // We're done with the block.
  InstructionBlock* instruction_block =
      sequence()->InstructionBlockAt(this->rpo_number(block));
  if (current_num_instructions() == current_block_end) {
    // Avoid empty block: insert a {kArchNop} instruction.
    Emit(Instruction::New(sequence()->zone(), kArchNop));
  }
  instruction_block->set_code_start(current_num_instructions());
  instruction_block->set_code_end(current_block_end);
  current_block_ = nullptr;
}

FlagsCondition InstructionSelectorT::GetComparisonFlagCondition(
    const ComparisonOp& op) const {
  switch (op.kind) {
    case ComparisonOp::Kind::kEqual:
      return kEqual;
    case ComparisonOp::Kind::kSignedLessThan:
      return kSignedLessThan;
    case ComparisonOp::Kind::kSignedLessThanOrEqual:
      return kSignedLessThanOrEqual;
    case ComparisonOp::Kind::kUnsignedLessThan:
      return kUnsignedLessThan;
    case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
      return kUnsignedLessThanOrEqual;
  }
}

void InstructionSelectorT::MarkPairProjectionsAsWord32(OpIndex node) {
  OptionalOpIndex projection0 = FindProjection(node, 0);
  if (projection0.valid()) {
    MarkAsWord32(projection0.value());
  }
  OptionalOpIndex projection1 = FindProjection(node, 1);
  if (projection1.valid()) {
    MarkAsWord32(projection1.value());
  }
}

void InstructionSelectorT::ConsumeEqualZero(OpIndex* user, OpIndex* value,
                                            FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (const ComparisonOp* equal =
             TryCast<Opmask::kComparisonEqual>(*value)) {
    if (equal->rep == RegisterRepresentation::Word32()) {
      if (!MatchIntegralZero(equal->right())) return;
#ifdef V8_COMPRESS_POINTERS
    } else if (equal->rep == RegisterRepresentation::Tagged()) {
      static_assert(RegisterRepresentation::Tagged().MapTaggedToWord() ==
                    RegisterRepresentation::Word32());
      if (!MatchSmiZero(equal->right())) return;
#endif  // V8_COMPRESS_POINTERS
    } else {
      return;
    }
    if (!CanCover(*user, *value)) return;

    *user = *value;
    *value = equal->left();
    cont->Negate();
  }
}

#if V8_ENABLE_WEBASSEMBLY
void InstructionSelectorT::VisitI8x16RelaxedSwizzle(OpIndex node) {
  return VisitI8x16Swizzle(node);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitStackPointerGreaterThan(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kStackPointerGreaterThanCondition, node);
  VisitStackPointerGreaterThan(node, &cont);
}

void InstructionSelectorT::VisitLoadStackCheckOffset(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchStackCheckOffset, g.DefineAsRegister(node));
}

void InstructionSelectorT::VisitLoadFramePointer(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchFramePointer, g.DefineAsRegister(node));
}

#if V8_ENABLE_WEBASSEMBLY
void InstructionSelectorT::VisitLoadStackPointer(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchStackPointer, g.DefineAsRegister(node));
}
#endif  // V8_ENABLE_WEBASSEMBLY

void InstructionSelectorT::VisitLoadParentFramePointer(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchParentFramePointer, g.DefineAsRegister(node));
}

void InstructionSelectorT::VisitLoadRootRegister(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchRootPointer, g.DefineAsRegister(node));
}

void InstructionSelectorT::VisitFloat64Acos(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Acos);
}

void InstructionSelectorT::VisitFloat64Acosh(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Acosh);
}

void InstructionSelectorT::VisitFloat64Asin(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Asin);
}

void InstructionSelectorT::VisitFloat64Asinh(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Asinh);
}

void InstructionSelectorT::VisitFloat64Atan(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Atan);
}

void InstructionSelectorT::VisitFloat64Atanh(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Atanh);
}

void InstructionSelectorT::VisitFloat64Atan2(OpIndex node) {
  VisitFloat64Ieee754Binop(node, kIeee754Float64Atan2);
}

void InstructionSelectorT::VisitFloat64Cbrt(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cbrt);
}

void InstructionSelectorT::VisitFloat64Cos(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cos);
}

void InstructionSelectorT::VisitFloat64Cosh(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cosh);
}

void InstructionSelectorT::VisitFloat64Exp(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Exp);
}

void InstructionSelectorT::VisitFloat64Expm1(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Expm1);
}

void InstructionSelectorT::VisitFloat64Log(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log);
}

void InstructionSelectorT::VisitFloat64Log1p(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log1p);
}

void InstructionSelectorT::VisitFloat64Log2(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log2);
}

void InstructionSelectorT::VisitFloat64Log10(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log10);
}

void InstructionSelectorT::VisitFloat64Pow(OpIndex node) {
  VisitFloat64Ieee754Binop(node, kIeee754Float64Pow);
}

void InstructionSelectorT::VisitFloat64Sin(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Sin);
}

void InstructionSelectorT::VisitFloat64Sinh(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Sinh);
}

void InstructionSelectorT::VisitFloat64Tan(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Tan);
}

void InstructionSelectorT::VisitFloat64Tanh(OpIndex node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Tanh);
}

void InstructionSelectorT::MarkAsTableSwitchTarget(
    const turboshaft::Block* block) {
  sequence()
      ->InstructionBlockAt(this->rpo_number(block))
      ->set_table_switch_target(true);
}

void InstructionSelectorT::EmitTableSwitch(
    const SwitchInfo& sw, InstructionOperand const& index_operand) {
  OperandGenerator g(this);
  size_t input_count = 2 + sw.value_range();
  DCHECK_LE(sw.value_range(), std::numeric_limits<size_t>::max() - 2);
  auto* inputs =
      zone()->template AllocateArray<InstructionOperand>(input_count);
  inputs[0] = index_operand;
  InstructionOperand default_operand = g.Label(sw.default_branch());
  std::fill(&inputs[1], &inputs[input_count], default_operand);
  for (const CaseInfo& c : sw.CasesUnsorted()) {
    size_t value = c.value - sw.min_value();
    DCHECK_LE(0u, value);
    DCHECK_LT(value + 2, input_count);
    inputs[value + 2] = g.Label(c.branch);
    MarkAsTableSwitchTarget(c.branch);
  }
  // If the default operand still exists in the cases, to fill gaps, then we
  // need to mark the default block as table switch target.
  if (std::find(&inputs[2], &inputs[input_count], default_operand) !=
      &inputs[input_count]) {
    MarkAsTableSwitchTarget(sw.default_branch());
  }
  Emit(kArchTableSwitch, 0, nullptr, input_count, inputs, 0, nullptr);
}

void InstructionSelectorT::EmitBinarySearchSwitch(
    const SwitchInfo& sw, InstructionOperand const& value_operand) {
  OperandGenerator g(this);
  size_t input_count = 2 + sw.case_count() * 2;
  DCHECK_LE(sw.case_count(), (std::numeric_limits<size_t>::max() - 2) / 2);
  auto* inputs =
      zone()->template AllocateArray<InstructionOperand>(input_count);
  inputs[0] = value_operand;
  inputs[1] = g.Label(sw.default_branch());
  std::vector<CaseInfo> cases = sw.CasesSortedByValue();
  for (size_t index = 0; index < cases.size(); ++index) {
    const CaseInfo& c = cases[index];
    inputs[index * 2 + 2 + 0] = g.TempImmediate(c.value);
    inputs[index * 2 + 2 + 1] = g.Label(c.branch);
  }
  Emit(kArchBinarySearchSwitch, 0, nullptr, input_count, inputs, 0, nullptr);
}

void InstructionSelectorT::VisitBitcastTaggedToWord(OpIndex node) {
  EmitIdentity(node);
}

void InstructionSelectorT::VisitBitcastWordToTagged(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineSameAsFirst(node),
       g.Use(this->Get(node).Cast<TaggedBitcastOp>().input()));
}

void InstructionSelectorT::VisitBitcastSmiToWord(OpIndex node) {
  // TODO(dmercadier): using EmitIdentity here is not ideal, because users of
  // {node} will then use its input, which may not have the Word32
  // representation. This might in turn lead to the register allocator wrongly
  // tracking Tagged values that are in fact just Smis. However, using
  // Emit(kArchNop) hurts performance because it inserts a gap move which cannot
  // always be eliminated because the operands may have different sizes (and the
  // move is then truncating or extending). As a temporary work-around until the
  // register allocator is fixed, we use Emit(kArchNop) in DEBUG mode to silence
  // the register allocator verifier.
#ifdef DEBUG
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineSameAsFirst(node),
       g.Use(this->Get(node).Cast<TaggedBitcastOp>().input()));
#else
  EmitIdentity(node);
#endif
}

// 32 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_32_BIT

VISIT_UNSUPPORTED_OP(Word64And)
VISIT_UNSUPPORTED_OP(Word64Or)
VISIT_UNSUPPORTED_OP(Word64Xor)
VISIT_UNSUPPORTED_OP(Word64Shl)
VISIT_UNSUPPORTED_OP(Word64Shr)
VISIT_UNSUPPORTED_OP(Word64Sar)
VISIT_UNSUPPORTED_OP(Word64Rol)
VISIT_UNSUPPORTED_OP(Word64Ror)
VISIT_UNSUPPORTED_OP(Word64Clz)
VISIT_UNSUPPORTED_OP(Word64Ctz)
VISIT_UNSUPPORTED_OP(Word64ReverseBits)
VISIT_UNSUPPORTED_OP(Word64Popcnt)
VISIT_UNSUPPORTED_OP(Word64Equal)
VISIT_UNSUPPORTED_OP(Int64Add)
VISIT_UNSUPPORTED_OP(Int64Sub)
VISIT_UNSUPPORTED_OP(Int64Mul)
VISIT_UNSUPPORTED_OP(Int64MulHigh)
VISIT_UNSUPPORTED_OP(Uint64MulHigh)
VISIT_UNSUPPORTED_OP(Int64Div)
VISIT_UNSUPPORTED_OP(Int64Mod)
VISIT_UNSUPPORTED_OP(Uint64Div)
VISIT_UNSUPPORTED_OP(Uint64Mod)
VISIT_UNSUPPORTED_OP(Int64AddWithOverflow)
VISIT_UNSUPPORTED_OP(Int64MulWithOverflow)
VISIT_UNSUPPORTED_OP(Int64SubWithOverflow)
VISIT_UNSUPPORTED_OP(Int64LessThan)
VISIT_UNSUPPORTED_OP(Int64LessThanOrEqual)
VISIT_UNSUPPORTED_OP(Uint64LessThan)
VISIT_UNSUPPORTED_OP(Uint64LessThanOrEqual)
VISIT_UNSUPPORTED_OP(BitcastWord32ToWord64)
VISIT_UNSUPPORTED_OP(ChangeInt32ToInt64)
VISIT_UNSUPPORTED_OP(ChangeInt64ToFloat64)
VISIT_UNSUPPORTED_OP(ChangeUint32ToUint64)
VISIT_UNSUPPORTED_OP(ChangeFloat64ToInt64)
VISIT_UNSUPPORTED_OP(ChangeFloat64ToUint64)
VISIT_UNSUPPORTED_OP(TruncateFloat64ToInt64)
VISIT_UNSUPPORTED_OP(TruncateInt64ToInt32)
VISIT_UNSUPPORTED_OP(TryTruncateFloat32ToInt64)
VISIT_UNSUPPORTED_OP(TryTruncateFloat64ToInt64)
VISIT_UNSUPPORTED_OP(TryTruncateFloat32ToUint64)
VISIT_UNSUPPORTED_OP(TryTruncateFloat64ToUint64)
VISIT_UNSUPPORTED_OP(TryTruncateFloat64ToInt32)
VISIT_UNSUPPORTED_OP(TryTruncateFloat64ToUint32)
VISIT_UNSUPPORTED_OP(RoundInt64ToFloat32)
VISIT_UNSUPPORTED_OP(RoundInt64ToFloat64)
VISIT_UNSUPPORTED_OP(RoundUint64ToFloat32)
VISIT_UNSUPPORTED_OP(RoundUint64ToFloat64)
VISIT_UNSUPPORTED_OP(BitcastFloat64ToInt64)
VISIT_UNSUPPORTED_OP(BitcastInt64ToFloat64)
VISIT_UNSUPPORTED_OP(SignExtendWord8ToInt64)
VISIT_UNSUPPORTED_OP(SignExtendWord16ToInt64)
VISIT_UNSUPPORTED_OP(SignExtendWord32ToInt64)
#endif  // V8_TARGET_ARCH_32_BIT

// 64 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_64_BIT
VISIT_UNSUPPORTED_OP(Int32PairAdd)
VISIT_UNSUPPORTED_OP(Int32PairSub)
VISIT_UNSUPPORTED_OP(Int32PairMul)
VISIT_UNSUPPORTED_OP(Word32PairShl)
VISIT_UNSUPPORTED_OP(Word32PairShr)
VISIT_UNSUPPORTED_OP(Word32PairSar)
#endif  // V8_TARGET_ARCH_64_BIT

#if !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_RISCV32
void InstructionSelectorT::VisitWord32AtomicPairLoad(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairStore(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairAdd(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairSub(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairAnd(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairOr(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairXor(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairExchange(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32AtomicPairCompareExchange(OpIndex node) {
  UNIMPLEMENTED();
}
#endif  // !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM
        // && !V8_TARGET_ARCH_RISCV32

#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_MIPS64 && \
    !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64 &&                         \
    !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_LOONG64

VISIT_UNSUPPORTED_OP(Word64AtomicLoad)
VISIT_UNSUPPORTED_OP(Word64AtomicStore)
VISIT_UNSUPPORTED_OP(Word64AtomicAdd)
VISIT_UNSUPPORTED_OP(Word64AtomicSub)
VISIT_UNSUPPORTED_OP(Word64AtomicAnd)
VISIT_UNSUPPORTED_OP(Word64AtomicOr)
VISIT_UNSUPPORTED_OP(Word64AtomicXor)
VISIT_UNSUPPORTED_OP(Word64AtomicExchange)
VISIT_UNSUPPORTED_OP(Word64AtomicCompareExchange)

#endif  // !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_PPC64
        // !V8_TARGET_ARCH_MIPS64 && !V8_TARGET_ARCH_S390X &&
        // !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_LOONG64

#if !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_RISCV32
// This is only needed on 32-bit to split the 64-bit value into two operands.
IF_WASM(VISIT_UNSUPPORTED_OP, I64x2SplatI32Pair)
IF_WASM(VISIT_UNSUPPORTED_OP, I64x2ReplaceLaneI32Pair)
#endif  // !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM &&
        // !V8_TARGET_ARCH_RISCV32

#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64
#if !V8_TARGET_ARCH_ARM64
#if !V8_TARGET_ARCH_MIPS64 && !V8_TARGET_ARCH_LOONG64 && \
    !V8_TARGET_ARCH_RISCV32 && !V8_TARGET_ARCH_RISCV64

IF_WASM(VISIT_UNSUPPORTED_OP, I64x2Splat)
IF_WASM(VISIT_UNSUPPORTED_OP, I64x2ExtractLane)
IF_WASM(VISIT_UNSUPPORTED_OP, I64x2ReplaceLane)

#endif  // !V8_TARGET_ARCH_MIPS64 && !V8_TARGET_ARCH_LOONG64 &&
        // !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_RISCV32
#endif  // !V8_TARGET_ARCH_ARM64
#endif  // !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64

#if !V8_TARGET_ARCH_ARM64

IF_WASM(VISIT_UNSUPPORTED_OP, I8x16AddReduce)
IF_WASM(VISIT_UNSUPPORTED_OP, I16x8AddReduce)
IF_WASM(VISIT_UNSUPPORTED_OP, I32x4AddReduce)
IF_WASM(VISIT_UNSUPPORTED_OP, I64x2AddReduce)
IF_WASM(VISIT_UNSUPPORTED_OP, F32x4AddReduce)
IF_WASM(VISIT_UNSUPPORTED_OP, F64x2AddReduce)

IF_WASM(VISIT_UNSUPPORTED_OP, I8x2Shuffle)
IF_WASM(VISIT_UNSUPPORTED_OP, I8x4Shuffle)
IF_WASM(VISIT_UNSUPPORTED_OP, I8x8Shuffle)
#endif  // !V8_TARGET_ARCH_ARM64

void InstructionSelectorT::VisitParameter(OpIndex node) {
  const ParameterOp& parameter = Cast<ParameterOp>(node);
  const int index = parameter.parameter_index;
  OperandGenerator g(this);

  if (linkage()->GetParameterLocation(index).IsNullRegister()) {
    EmitMoveParamToFPR(node, index);
  } else {
    InstructionOperand op =
        linkage()->ParameterHasSecondaryLocation(index)
            ? g.DefineAsDualLocation(
                  node, linkage()->GetParameterLocation(index),
                  linkage()->GetParameterSecondaryLocation(index))
            : g.DefineAsLocation(node, linkage()->GetParameterLocation(index));
    Emit(kArchNop, op);
  }
}

namespace {

LinkageLocation ExceptionLocation() {
  return LinkageLocation::ForRegister(kReturnRegister0.code(),
                                      MachineType::TaggedPointer());
}

constexpr InstructionCode EncodeCallDescriptorFlags(
    InstructionCode opcode, CallDescriptor::Flags flags) {
  // Note: Not all bits of `flags` are preserved.
  static_assert(CallDescriptor::kFlagsBitsEncodedInInstructionCode ==
                MiscField::kSize);
  DCHECK(Instruction::IsCallWithDescriptorFlags(opcode));
  return opcode | MiscField::encode(flags & MiscField::kMax);
}

}  // namespace

void InstructionSelectorT::VisitIfException(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineAsLocation(node, ExceptionLocation()));
}

void InstructionSelectorT::VisitOsrValue(OpIndex node) {
  const OsrValueOp& osr_value = Cast<OsrValueOp>(node);
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineAsLocation(
                     node, linkage()->GetOsrValueLocation(osr_value.index)));
}

void InstructionSelectorT::VisitPhi(OpIndex node) {
  const Operation& op = Get(node);
  DCHECK_EQ(op.input_count, PredecessorCount(current_block_));
  PhiInstruction* phi = instruction_zone()->template New<PhiInstruction>(
      instruction_zone(), GetVirtualRegister(node),
      static_cast<size_t>(op.input_count));
  sequence()->InstructionBlockAt(rpo_number(current_block_))->AddPhi(phi);
  for (size_t i = 0; i < op.input_count; ++i) {
    OpIndex input = op.input(i);
    MarkAsUsed(input);
    phi->SetInput(i, GetVirtualRegister(input));
  }
}

void InstructionSelectorT::VisitProjection(OpIndex node) {
  const ProjectionOp& projection = this->Get(node).Cast<ProjectionOp>();
  const Operation& value_op = this->Get(projection.input());
  if (value_op.Is<OverflowCheckedBinopOp>() ||
      value_op.Is<OverflowCheckedUnaryOp>() || value_op.Is<TryChangeOp>() ||
      value_op.Is<Word32PairBinopOp>()) {
    if (projection.index == 0u) {
      EmitIdentity(node);
    } else {
      DCHECK_EQ(1u, projection.index);
      MarkAsUsed(projection.input());
    }
  } else if (value_op.Is<DidntThrowOp>()) {
    // Nothing to do here?
  } else if (value_op.Is<CallOp>()) {
    // Call projections need to be behind the call's DidntThrow.
    UNREACHABLE();
  } else if (value_op.Is<AtomicWord32PairOp>()) {
    // Nothing to do here.
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  } else if (value_op.Is<Simd128LoadPairDeinterleaveOp>()) {
    MarkAsUsed(projection.input());
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  } else {
    UNIMPLEMENTED();
  }
}

bool InstructionSelectorT::CanDoBranchIfOverflowFusion(OpIndex binop) {
  const Graph* graph = this->turboshaft_graph();
  DCHECK(graph->Get(binop).template Is<OverflowCheckedBinopOp>() ||
         graph->Get(binop).template Is<OverflowCheckedUnaryOp>());

  // Getting the 1st projection. Projections are always emitted right after the
  // operation, in ascending order.
  OpIndex projection0_index = graph->NextIndex(binop);
  const ProjectionOp& projection0 =
      graph->Get(projection0_index).Cast<ProjectionOp>();
  DCHECK_EQ(projection0.index, 0);

  if (IsDefined(projection0_index)) {
    // In Turboshaft, this can only happen if {projection0_index} has already
    // been eagerly scheduled somewhere else, like in
    // TryPrepareScheduleFirstProjection.
    return true;
  }

  if (projection0.saturated_use_count.IsOne()) {
    // If the projection has a single use, it is the following tuple, so we
    // don't care about the value, and can do branch-if-overflow fusion.
    DCHECK(turboshaft_uses(projection0_index).size() == 1 &&
           graph->Get(turboshaft_uses(projection0_index)[0]).Is<TupleOp>());
    return true;
  }

  if (this->block(schedule_, binop) != current_block_) {
    // {binop} is not supposed to be defined in the current block, so let's not
    // pull it in this block (the checks would need to be stronger, and it's
    // unlikely that it's doable because of effect levels and all).
    return false;
  }

  // We now need to make sure that all uses of {projection0} are already
  // defined, which will imply that it's fine to define {projection0} and
  // {binop} now.
  for (OpIndex use : turboshaft_uses(projection0_index)) {
    if (this->Get(use).template Is<TupleOp>()) {
      // The Tuple won't have any uses since it would have to be accessed
      // through Projections, and Projections on Tuples return the original
      // Projection instead (see Assembler::ReduceProjection in
      // turboshaft/assembler.h).
      DCHECK(this->Get(use).saturated_use_count.IsZero());
      continue;
    }
    if (IsDefined(use)) continue;
    if (this->block(schedule_, use) != current_block_) {
      // {use} is in a later block, so it should already have been visited. Note
      // that operations that don't produce values are not marked as Defined,
      // like Return for instance, so it's possible that {use} has been visited
      // but the previous `IsDefined` check didn't match.

#ifdef DEBUG
      if (this->block(schedule_, use)->index() < current_block_->index()) {
        // If {use} is in a previous block, then it has to be a loop Phi that
        // uses {projection0} as its backedge input. In that case, it's fine to
        // schedule the binop right now, even though it's after the use of its
        // 1st projection (since the use is conceptually after rather than
        // before because it goes through a backedge).
        DCHECK(this->Get(use).template Is<PhiOp>());
        DCHECK_EQ(this->Get(use).template Cast<PhiOp>().input(1),
                  projection0_index);
      }
#endif

      continue;
    }

    if (this->Get(use).template Is<PhiOp>()) {
      DCHECK_EQ(this->block(schedule_, use), current_block_);
      // If {projection0} is used by a Phi in the current block, then it has to
      // be a loop phi, and {projection0} has to be its backedge value. This
      // doesn't prevent scheduling {projection0} now, since anyways it
      // necessarily needs to be scheduled after the Phi.
      DCHECK(current_block_->IsLoop());
      continue;
    }

    // {use} is not defined yet (and is not a special case), which means that
    // {projection0} has a use that comes before {binop}, and we thus can't fuse
    // binop with a branch to do a branch-if-overflow.
    return false;
  }

  VisitProjection(projection0_index);
  return true;
}

void InstructionSelectorT::VisitConstant(OpIndex node) {
  // We must emit a NOP here because every live range needs a defining
  // instruction in the register allocator.
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineAsConstant(node));
}

void InstructionSelectorT::UpdateMaxPushedArgumentCount(size_t count) {
  *max_pushed_argument_count_ = std::max(count, *max_pushed_argument_count_);
}

void InstructionSelectorT::VisitCall(OpIndex node, Block* handler) {
  OperandGenerator g(this);
  const CallOp& call_op = Cast<CallOp>(node);
  const CallDescriptor* call_descriptor = call_op.descriptor->descriptor;
  SaveFPRegsMode mode = call_descriptor->NeedsCallerSavedFPRegisters()
                            ? SaveFPRegsMode::kSave
                            : SaveFPRegsMode::kIgnore;

  if (call_descriptor->NeedsCallerSavedRegisters()) {
    Emit(kArchSaveCallerRegisters | MiscField::encode(static_cast<int>(mode)),
         g.NoOutput());
  }

  FrameStateDescriptor* frame_state_descriptor = nullptr;
  if (call_descriptor->NeedsFrameState()) {
    frame_state_descriptor =
        GetFrameStateDescriptor(call_op.frame_state().value());
  }

  CallBuffer buffer(zone(), call_descriptor, frame_state_descriptor);
  CallDescriptor::Flags flags = call_descriptor->flags();

  // Compute InstructionOperands for inputs and outputs.
  // TODO(turbofan): on some architectures it's probably better to use
  // the code object in a register if there are multiple uses of it.
  // Improve constant pool and the heuristics in the register allocator
  // for where to emit constants.
  CallBufferFlags call_buffer_flags(kCallCodeImmediate | kCallAddressImmediate);
  if (flags & CallDescriptor::kFixedTargetRegister) {
    call_buffer_flags |= kCallFixedTargetRegister;
  }
  InitializeCallBuffer(node, &buffer, call_buffer_flags, call_op.callee(),
                       call_op.frame_state(), call_op.arguments(),
                       static_cast<int>(call_op.results_rep().size()));

  EmitPrepareArguments(&buffer.pushed_nodes, call_descriptor, node);
  UpdateMaxPushedArgumentCount(buffer.pushed_nodes.size());

  InstructionOperandVector temps(zone());

#if V8_ENABLE_WEBASSEMBLY
  if (call_descriptor->IsIndirectWasmFunctionCall()) {
    buffer.instruction_args.push_back(
        g.UseImmediate64(call_descriptor->signature_hash()));
  }
#endif

  if (call_descriptor->RequiresEntrypointTagForCall()) {
    DCHECK(!call_descriptor->IsJSFunctionCall());
    buffer.instruction_args.push_back(
        g.TempImmediate(call_descriptor->shifted_tag()));
  } else if (call_descriptor->IsJSFunctionCall()) {
    // For JSFunctions we need to know the number of pushed parameters during
    // code generation.
    uint32_t parameter_count =
        static_cast<uint32_t>(buffer.pushed_nodes.size());
    buffer.instruction_args.push_back(g.TempImmediate(parameter_count));
  }

  // Pass label of exception handler block.
  if (handler) {
    flags |= CallDescriptor::kHasExceptionHandler;
    buffer.instruction_args.push_back(g.Label(handler));
  } else {
    if (call_op.descriptor->lazy_deopt_on_throw == LazyDeoptOnThrow::kYes) {
      flags |= CallDescriptor::kHasExceptionHandler;
      buffer.instruction_args.push_back(
          g.UseImmediate(kLazyDeoptOnThrowSentinel));
    }
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  switch (call_descriptor->kind()) {
    case CallDescriptor::kCallAddress: {
      int gp_param_count =
          static_cast<int>(call_descriptor->GPParameterCount());
      int fp_param_count =
          static_cast<int>(call_descriptor->FPParameterCount());
#if ABI_USES_FUNCTION_DESCRIPTORS
      // Highest fp_param_count bit is used on AIX to indicate if a CFunction
      // call has function descriptor or not.
      static_assert(FPParamField::kSize == kHasFunctionDescriptorBitShift + 1);
      if (!call_descriptor->NoFunctionDescriptor()) {
        fp_param_count |= 1 << kHasFunctionDescriptorBitShift;
      }
#endif
      // We store the param counts as a separate input because they need too
      // many bits to be encoded in the opcode.
      buffer.instruction_args.push_back(
          g.UseImmediate(ParamField::encode(gp_param_count) |
                         FPParamField::encode(fp_param_count)));
      opcode = EncodeCallDescriptorFlags(kArchCallCFunction, flags);
      break;
    }
    case CallDescriptor::kCallCodeObject:
      opcode = EncodeCallDescriptorFlags(kArchCallCodeObject, flags);
      break;
    case CallDescriptor::kCallJSFunction:
      opcode = EncodeCallDescriptorFlags(kArchCallJSFunction, flags);
      break;
#if V8_ENABLE_WEBASSEMBLY
    case CallDescriptor::kCallWasmCapiFunction:
    case CallDescriptor::kCallWasmFunction:
    case CallDescriptor::kCallWasmImportWrapper:
      DCHECK(this->IsRelocatableWasmConstant(call_op.callee()));
      opcode = EncodeCallDescriptorFlags(kArchCallWasmFunction, flags);
      break;
    case CallDescriptor::kCallWasmFunctionIndirect:
      DCHECK(!this->IsRelocatableWasmConstant(call_op.callee()));
      opcode = EncodeCallDescriptorFlags(kArchCallWasmFunctionIndirect, flags);
      break;
#endif  // V8_ENABLE_WEBASSEMBLY
    case CallDescriptor::kCallBuiltinPointer:
      opcode = EncodeCallDescriptorFlags(kArchCallBuiltinPointer, flags);
      break;
  }

  // Emit the call instruction.
  size_t const output_count = buffer.outputs.size();
  auto* outputs = output_count ? &buffer.outputs.front() : nullptr;
  Instruction* call_instr =
      Emit(opcode, output_count, outputs, buffer.instruction_args.size(),
           &buffer.instruction_args.front());
  if (instruction_selection_failed()) return;
  call_instr->MarkAsCall();

  EmitPrepareResults(&(buffer.output_nodes), call_descriptor, node);

  if (call_descriptor->NeedsCallerSavedRegisters()) {
    Emit(
        kArchRestoreCallerRegisters | MiscField::encode(static_cast<int>(mode)),
        g.NoOutput());
  }
}

void InstructionSelectorT::VisitTailCall(OpIndex node) {
  OperandGenerator g(this);

  const TailCallOp& call_op = Cast<TailCallOp>(node);
  auto caller = linkage()->GetIncomingDescriptor();
  auto callee = call_op.descriptor->descriptor;
  DCHECK(caller->CanTailCall(callee));
  const int stack_param_delta = callee->GetStackParameterDelta(caller);
  CallBuffer buffer(zone(), callee, nullptr);

  // Compute InstructionOperands for inputs and outputs.
  CallBufferFlags flags(kCallCodeImmediate | kCallTail);
  if (IsTailCallAddressImmediate()) {
    flags |= kCallAddressImmediate;
  }
  if (callee->flags() & CallDescriptor::kFixedTargetRegister) {
    flags |= kCallFixedTargetRegister;
  }
  InitializeCallBuffer(node, &buffer, flags, call_op.callee(),
                       OptionalOpIndex::Nullopt(), call_op.arguments(),
                       static_cast<int>(call_op.outputs_rep().size()),
                       stack_param_delta);
  UpdateMaxPushedArgumentCount(stack_param_delta);

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  InstructionOperandVector temps(zone());
  switch (callee->kind()) {
    case CallDescriptor::kCallCodeObject:
      opcode = kArchTailCallCodeObject;
      break;
    case CallDescriptor::kCallAddress:
      DCHECK(!caller->IsJSFunctionCall());
      opcode = kArchTailCallAddress;
      break;
#if V8_ENABLE_WEBASSEMBLY
    case CallDescriptor::kCallWasmFunction:
      DCHECK(!caller->IsJSFunctionCall());
      DCHECK(this->IsRelocatableWasmConstant(call_op.callee()));
      opcode = kArchTailCallWasm;
      break;
    case CallDescriptor::kCallWasmFunctionIndirect:
      DCHECK(!caller->IsJSFunctionCall());
      DCHECK(!this->IsRelocatableWasmConstant(call_op.callee()));
      opcode = kArchTailCallWasmIndirect;
      break;
#endif  // V8_ENABLE_WEBASSEMBLY
    default:
      UNREACHABLE();
  }
  opcode = EncodeCallDescriptorFlags(opcode, callee->flags());

  Emit(kArchPrepareTailCall, g.NoOutput());

#if V8_ENABLE_WEBASSEMBLY
  if (callee->IsIndirectWasmFunctionCall()) {
    buffer.instruction_args.push_back(
        g.UseImmediate64(callee->signature_hash()));
  }
#endif

  if (callee->RequiresEntrypointTagForCall()) {
    buffer.instruction_args.push_back(g.TempImmediate(callee->shifted_tag()));
  }

  // Add an immediate operand that represents the offset to the first slot
  // that is unused with respect to the stack pointer that has been updated
  // for the tail call instruction. Backends that pad arguments can write the
  // padding value at this offset from the stack.
  const int optional_padding_offset =
      callee->GetOffsetToFirstUnusedStackSlot() - 1;
  buffer.instruction_args.push_back(g.TempImmediate(optional_padding_offset));

  const int first_unused_slot_offset =
      kReturnAddressStackSlotCount + stack_param_delta;
  buffer.instruction_args.push_back(g.TempImmediate(first_unused_slot_offset));

  // Emit the tailcall instruction.
  Emit(opcode, 0, nullptr, buffer.instruction_args.size(),
       &buffer.instruction_args.front(), temps.size(),
       temps.empty() ? nullptr : &temps.front());
}

void InstructionSelectorT::VisitGoto(Block* target) {
  // jump to the next block.
  OperandGenerator g(this);
  Emit(kArchJmp, g.NoOutput(), g.Label(target));
}

void InstructionSelectorT::VisitReturn(OpIndex node) {
  const ReturnOp& ret = schedule()->Get(node).Cast<ReturnOp>();

  OperandGenerator g(this);
  const size_t return_count = linkage()->GetIncomingDescriptor()->ReturnCount();
  const int input_count =
      return_count == 0 ? 1
                        : (1 + static_cast<int>(ret.return_values().size()));
  DCHECK_GE(input_count, 1);

  auto value_locations =
      zone()->template AllocateArray<InstructionOperand>(input_count);
  const Operation& pop_count = schedule()->Get(ret.pop_count());
  if (pop_count.Is<Opmask::kWord32Constant>() ||
      pop_count.Is<Opmask::kWord64Constant>()) {
    value_locations[0] = g.UseImmediate(ret.pop_count());
  } else {
    value_locations[0] = g.UseRegister(ret.pop_count());
  }
  for (size_t i = 0, return_value_idx = 0; i < return_count; ++i) {
    LinkageLocation loc = linkage()->GetReturnLocation(i);
    // Return values passed via frame slots have already been stored
    // on the stack by the GrowableStacksReducer.
    if (loc.IsCallerFrameSlot() && ret.spill_caller_frame_slots) {
      continue;
    }
    value_locations[return_value_idx + 1] =
        g.UseLocation(ret.return_values()[return_value_idx], loc);
    return_value_idx++;
  }
  Emit(kArchRet, 0, nullptr, input_count, value_locations);
}

void InstructionSelectorT::VisitBranch(OpIndex branch_node, Block* tbranch,
                                       Block* fbranch) {
  const BranchOp& branch = Cast<BranchOp>(branch_node);
  TryPrepareScheduleFirstProjection(branch.condition());

#if V8_ENABLE_WEBASSEMBLY
  FlagsContinuation cont = FlagsContinuation::ForHintedBranch(
      kNotEqual, tbranch, fbranch, branch.hint);
#else
  FlagsContinuation cont =
      FlagsContinuation::ForBranch(kNotEqual, tbranch, fbranch);
#endif  // V8_ENABLE_WEBASSEMBLY
  VisitWordCompareZero(branch_node, branch.condition(), &cont);
}

// When a DeoptimizeIf/DeoptimizeUnless/Branch depends on a BinopOverflow, the
// InstructionSelector can sometimes generate a fuse instruction covering both
// the BinopOverflow and the DeoptIf/Branch, and the final emitted code will
// look like:
//
//     r = BinopOverflow
//     jo branch_target/deopt_target
//
// When this fusing fails, the final code looks like:
//
//     r = BinopOverflow
//     o = sete  // sets overflow bit
//     cmp o, 0
//     jnz branch_target/deopt_target
//
// To be able to fuse tue BinopOverflow and the DeoptIf/Branch, the 1st
// projection (Projection[0], which contains the actual result) must already be
// scheduled (and a few other conditions must be satisfied, see
// InstructionSelectorXXX::VisitWordCompareZero).
// TryPrepareScheduleFirstProjection is thus called from
// VisitDeoptimizeIf/VisitBranch and detects if the 1st
// projection could be scheduled now, and, if so, defines it.
void InstructionSelectorT::TryPrepareScheduleFirstProjection(
    OpIndex maybe_projection) {
  // The DeoptimizeIf/Branch condition is not a projection.
  const ProjectionOp* projection = TryCast<ProjectionOp>(maybe_projection);
  if (!projection) return;

  if (projection->index != 1u) {
    // The DeoptimizeIf/Branch isn't on the Projection[1]
    // (ie, not on the overflow bit of a BinopOverflow).
    return;
  }

  DCHECK_EQ(projection->input_count, 1);
  OpIndex node = projection->input();
  if (block(schedule_, node) != current_block_) {
    // The projection input is not in the current block, so it shouldn't be
    // emitted now, so we don't need to eagerly schedule its Projection[0].
    return;
  }

  auto* binop = TryCast<OverflowCheckedBinopOp>(node);
  auto* unop = TryCast<OverflowCheckedUnaryOp>(node);
  if (binop == nullptr && unop == nullptr) return;
  if (binop) {
    DCHECK(binop->kind == OverflowCheckedBinopOp::Kind::kSignedAdd ||
           binop->kind == OverflowCheckedBinopOp::Kind::kSignedSub ||
           binop->kind == OverflowCheckedBinopOp::Kind::kSignedMul);
  } else {
    DCHECK_EQ(unop->kind, OverflowCheckedUnaryOp::Kind::kAbs);
  }

  OptionalOpIndex result = FindProjection(node, 0);
  if (!result.valid() || IsDefined(result.value())) {
    // No Projection(0), or it's already defined.
    return;
  }

  if (block(schedule_, result.value()) != current_block_) {
    // {result} wasn't planned to be scheduled in {current_block_}. To
    // avoid adding checks to see if it can still be scheduled now, we
    // just bail out.
    return;
  }

  // Checking if all uses of {result} that are in the current block have
  // already been Defined.
  // We also ignore Phi uses: if {result} is used in a Phi in the block in
  // which it is defined, this means that this block is a loop header, and
  // {result} back into it through the back edge. In this case, it's
  // normal to schedule {result} before the Phi that uses it.
  for (OpIndex use : turboshaft_uses(result.value())) {
    // We ignore TupleOp uses, since TupleOp don't lead to emitted machine
    // instructions and are just Turboshaft "meta operations".
    if (!Is<TupleOp>(use) && !IsDefined(use) &&
        block(schedule_, use) == current_block_ && !Is<PhiOp>(use)) {
      return;
    }
  }

  // Visiting the projection now. Note that this relies on the fact that
  // VisitProjection doesn't Emit something: if it did, then we could be
  // Emitting something after a Branch, which is invalid (Branch can only
  // be at the end of a block, and the end of a block must always be a
  // block terminator). (remember that we emit operation in reverse order,
  // so because we are doing TryPrepareScheduleFirstProjection before
  // actually emitting the Branch, it would be after in the final
  // instruction sequence, not before)
  VisitProjection(result.value());
}

void InstructionSelectorT::VisitDeoptimizeIf(OpIndex node) {
  const DeoptimizeIfOp& deopt = Cast<DeoptimizeIfOp>(node);

  TryPrepareScheduleFirstProjection(deopt.condition());

  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      deopt.negated ? kEqual : kNotEqual, deopt.parameters->reason(), node.id(),
      deopt.parameters->feedback(), deopt.frame_state());
  VisitWordCompareZero(node, deopt.condition(), &cont);
}

void InstructionSelectorT::VisitSelect(OpIndex node) {
  const SelectOp& select = Cast<SelectOp>(node);
  DCHECK_EQ(select.input_count, 3);
  FlagsContinuation cont = FlagsContinuation::ForSelect(
      kNotEqual, node, select.vtrue(), select.vfalse());
  VisitWordCompareZero(node, select.cond(), &cont);
}

void InstructionSelectorT::VisitTrapIf(OpIndex node) {
#if V8_ENABLE_WEBASSEMBLY
  const TrapIfOp& trap_if = Cast<TrapIfOp>(node);
  // FrameStates are only used for wasm traps inlined in JS. In that case the
  // trap node will be lowered (replaced) before instruction selection.
  // Therefore any TrapIf node has only one input.
  DCHECK_EQ(trap_if.input_count, 1);
  FlagsContinuation cont = FlagsContinuation::ForTrap(
      trap_if.negated ? kEqual : kNotEqual, trap_if.trap_id);
  VisitWordCompareZero(node, trap_if.condition(), &cont);
#else
  UNREACHABLE();
#endif
}

void InstructionSelectorT::EmitIdentity(OpIndex node) {
  const Operation& op = Get(node);
  MarkAsUsed(op.input(0));
  MarkAsDefined(node);
  SetRename(node, op.input(0));
}

void InstructionSelectorT::VisitDeoptimize(DeoptimizeReason reason,
                                           uint32_t node_id,
                                           FeedbackSource const& feedback,
                                           OpIndex frame_state) {
  InstructionOperandVector args(instruction_zone());
  AppendDeoptimizeArguments(&args, reason, node_id, feedback, frame_state);
  Emit(kArchDeoptimize, 0, nullptr, args.size(), &args.front(), 0, nullptr);
}

void InstructionSelectorT::VisitDebugBreak(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchDebugBreak, g.NoOutput());
}

void InstructionSelectorT::VisitUnreachable(OpIndex node) {
  OperandGenerator g(this);
  Emit(kArchDebugBreak, g.NoOutput());
}

void InstructionSelectorT::VisitStaticAssert(OpIndex node) {
  const StaticAssertOp& op = Cast<StaticAssertOp>(node);
  DCHECK_EQ(op.input_count, 1);
  OpIndex asserted = op.condition();
  UnparkedScopeIfNeeded scope(broker_);
  AllowHandleDereference allow_handle_dereference;
    StdoutStream os;
    os << Get(asserted);
    FATAL(
        "Expected Turbofan static assert to hold, but got non-true input:\n  "
        "%s",
        op.source);
}

void InstructionSelectorT::VisitComment(OpIndex node) {
  OperandGenerator g(this);
  const CommentOp& comment =
      this->turboshaft_graph()->Get(node).template Cast<CommentOp>();
  using ptrsize_int_t =
      std::conditional_t<kSystemPointerSize == 8, int64_t, int32_t>;
  InstructionOperand operand = sequence()->AddImmediate(
      Constant{reinterpret_cast<ptrsize_int_t>(comment.message)});
  Emit(kArchComment, 0, nullptr, 1, &operand);
}

void InstructionSelectorT::VisitRetain(OpIndex node) {
  const RetainOp& retain = Cast<RetainOp>(node);
  OperandGenerator g(this);
  DCHECK_EQ(retain.input_count, 1);
  Emit(kArchNop, g.NoOutput(), g.UseAny(retain.retained()));
}

void InstructionSelectorT::VisitControl(const Block* block) {
#ifdef DEBUG
  // SSA deconstruction requires targets of branches not to have phis.
  // Edge split form guarantees this property, but is more strict.
  if (auto successors =
          SuccessorBlocks(block->LastOperation(*turboshaft_graph()));
      successors.size() > 1) {
    for (Block* successor : successors) {
      if (successor->HasPhis(*turboshaft_graph())) {
        std::ostringstream str;
        str << "You might have specified merged variables for a label with "
            << "only one predecessor." << std::endl
            << "# Current Block: " << successor->index() << std::endl;
        FATAL("%s", str.str().c_str());
      }
    }
  }
#endif  // DEBUG
  const Operation& op = block->LastOperation(*schedule());
  OpIndex node = schedule()->Index(op);
  int instruction_end = static_cast<int>(instructions_.size());
  switch (op.opcode) {
    case Opcode::kGoto:
      VisitGoto(op.Cast<GotoOp>().destination);
      break;
    case Opcode::kReturn:
      VisitReturn(node);
      break;
    case Opcode::kTailCall:
      VisitTailCall(node);
      break;
    case Opcode::kDeoptimize: {
      const DeoptimizeOp& deoptimize = op.Cast<DeoptimizeOp>();
      VisitDeoptimize(deoptimize.parameters->reason(), node.id(),
                      deoptimize.parameters->feedback(),
                      deoptimize.frame_state());
      break;
    }
    case Opcode::kBranch: {
      const BranchOp& branch = op.Cast<BranchOp>();
      Block* tbranch = branch.if_true;
      Block* fbranch = branch.if_false;
      if (tbranch == fbranch) {
        VisitGoto(tbranch);
      } else {
        VisitBranch(node, tbranch, fbranch);
      }
      break;
    }
    case Opcode::kSwitch: {
      const SwitchOp& swtch = op.Cast<SwitchOp>();
      int32_t min_value = std::numeric_limits<int32_t>::max();
      int32_t max_value = std::numeric_limits<int32_t>::min();

      ZoneVector<CaseInfo> cases(swtch.cases.size(), zone());
      for (size_t i = 0; i < swtch.cases.size(); ++i) {
        const SwitchOp::Case& c = swtch.cases[i];
        cases[i] = CaseInfo{c.value, 0, c.destination};
        if (min_value > c.value) min_value = c.value;
        if (max_value < c.value) max_value = c.value;
      }
      SwitchInfo sw(std::move(cases), min_value, max_value, swtch.default_case);
      return VisitSwitch(node, sw);
    }
    case Opcode::kCheckException: {
      const CheckExceptionOp& check = op.Cast<CheckExceptionOp>();
      VisitCall(check.throwing_operation(), check.catch_block);
      VisitGoto(check.didnt_throw_block);
      return;
    }
    case Opcode::kUnreachable:
      return VisitUnreachable(node);
    case Opcode::kStaticAssert:
      return VisitStaticAssert(node);
    default: {
      const std::string op_string = op.ToString();
      PrintF("\033[31mNo ISEL support for: %s\033[m\n", op_string.c_str());
      FATAL("Unexpected operation #%d:%s", node.id(), op_string.c_str());
    }
  }

  if (trace_turbo_ == InstructionSelector::kEnableTraceTurboJson) {
    DCHECK(node.valid());
    int instruction_start = static_cast<int>(instructions_.size());
    instr_origins_[node.id()] = {instruction_start, instruction_end};
  }
}

void InstructionSelectorT::VisitNode(OpIndex node) {
  tick_counter_->TickAndMaybeEnterSafepoint();
  const Operation& op = this->Get(node);
  using Opcode = Opcode;
  using Rep = RegisterRepresentation;
  switch (op.opcode) {
    case Opcode::kBranch:
    case Opcode::kGoto:
    case Opcode::kReturn:
    case Opcode::kTailCall:
    case Opcode::kUnreachable:
    case Opcode::kDeoptimize:
    case Opcode::kSwitch:
    case Opcode::kCheckException:
      // Those are already handled in VisitControl.
      DCHECK(op.IsBlockTerminator());
      break;
    case Opcode::kParameter: {
      // Parameters should always be scheduled to the first block.
      DCHECK_EQ(this->rpo_number(this->block(schedule(), node)).ToInt(), 0);
      MachineType type =
          linkage()->GetParameterType(op.Cast<ParameterOp>().parameter_index);
      MarkAsRepresentation(type.representation(), node);
      return VisitParameter(node);
    }
    case Opcode::kChange: {
      const ChangeOp& change = op.Cast<ChangeOp>();
      MarkAsRepresentation(change.to.machine_representation(), node);
      switch (change.kind) {
        case ChangeOp::Kind::kFloatConversion:
          if (change.from == Rep::Float64()) {
            DCHECK_EQ(change.to, Rep::Float32());
            return VisitTruncateFloat64ToFloat32(node);
          } else {
            DCHECK_EQ(change.from, Rep::Float32());
            DCHECK_EQ(change.to, Rep::Float64());
            return VisitChangeFloat32ToFloat64(node);
          }
        case ChangeOp::Kind::kSignedFloatTruncateOverflowToMin:
        case ChangeOp::Kind::kUnsignedFloatTruncateOverflowToMin: {
          using A = ChangeOp::Assumption;
          bool is_signed =
              change.kind == ChangeOp::Kind::kSignedFloatTruncateOverflowToMin;
          switch (multi(change.from, change.to, is_signed, change.assumption)) {
            case multi(Rep::Float32(), Rep::Word32(), true, A::kNoOverflow):
            case multi(Rep::Float32(), Rep::Word32(), true, A::kNoAssumption):
              return VisitTruncateFloat32ToInt32(node);
            case multi(Rep::Float32(), Rep::Word32(), false, A::kNoOverflow):
            case multi(Rep::Float32(), Rep::Word32(), false, A::kNoAssumption):
              return VisitTruncateFloat32ToUint32(node);
            case multi(Rep::Float64(), Rep::Word32(), true, A::kReversible):
              return VisitChangeFloat64ToInt32(node);
            case multi(Rep::Float64(), Rep::Word32(), false, A::kReversible):
              return VisitChangeFloat64ToUint32(node);
            case multi(Rep::Float64(), Rep::Word32(), true, A::kNoOverflow):
              return VisitRoundFloat64ToInt32(node);
            case multi(Rep::Float64(), Rep::Word32(), false, A::kNoAssumption):
            case multi(Rep::Float64(), Rep::Word32(), false, A::kNoOverflow):
              return VisitTruncateFloat64ToUint32(node);
            case multi(Rep::Float64(), Rep::Word64(), true, A::kReversible):
              return VisitChangeFloat64ToInt64(node);
            case multi(Rep::Float64(), Rep::Word64(), false, A::kReversible):
              return VisitChangeFloat64ToUint64(node);
            case multi(Rep::Float64(), Rep::Word64(), true, A::kNoOverflow):
            case multi(Rep::Float64(), Rep::Word64(), true, A::kNoAssumption):
              return VisitTruncateFloat64ToInt64(node);
            default:
              // Invalid combination.
              UNREACHABLE();
          }

          UNREACHABLE();
        }
        case ChangeOp::Kind::kJSFloatTruncate:
          DCHECK_EQ(change.from, Rep::Float64());
          DCHECK_EQ(change.to, Rep::Word32());
          return VisitTruncateFloat64ToWord32(node);
        case ChangeOp::Kind::kJSFloat16TruncateWithBitcast:
          DCHECK_EQ(Rep::Float64(), change.from);
          DCHECK_EQ(Rep::Word32(), change.to);
          return VisitTruncateFloat64ToFloat16RawBits(node);
        case ChangeOp::Kind::kJSFloat16ChangeWithBitcast:
          DCHECK_EQ(Rep::Word32(), change.from);
          DCHECK_EQ(Rep::Float64(), change.to);
          return VisitChangeFloat16RawBitsToFloat64(node);
        case ChangeOp::Kind::kSignedToFloat:
          if (change.from == Rep::Word32()) {
            if (change.to == Rep::Float32()) {
              return VisitRoundInt32ToFloat32(node);
            } else {
              DCHECK_EQ(change.to, Rep::Float64());
              DCHECK_EQ(change.assumption, ChangeOp::Assumption::kNoAssumption);
              return VisitChangeInt32ToFloat64(node);
            }
          } else {
            DCHECK_EQ(change.from, Rep::Word64());
            if (change.to == Rep::Float32()) {
              return VisitRoundInt64ToFloat32(node);
            } else {
              DCHECK_EQ(change.to, Rep::Float64());
              if (change.assumption == ChangeOp::Assumption::kReversible) {
                return VisitChangeInt64ToFloat64(node);
              } else {
                return VisitRoundInt64ToFloat64(node);
              }
            }
          }
          UNREACHABLE();
        case ChangeOp::Kind::kUnsignedToFloat:
          switch (multi(change.from, change.to)) {
            case multi(Rep::Word32(), Rep::Float32()):
              return VisitRoundUint32ToFloat32(node);
            case multi(Rep::Word32(), Rep::Float64()):
              return VisitChangeUint32ToFloat64(node);
            case multi(Rep::Word64(), Rep::Float32()):
              return VisitRoundUint64ToFloat32(node);
            case multi(Rep::Word64(), Rep::Float64()):
              return VisitRoundUint64ToFloat64(node);
            default:
              UNREACHABLE();
          }
        case ChangeOp::Kind::kExtractHighHalf:
          DCHECK_EQ(change.from, Rep::Float64());
          DCHECK_EQ(change.to, Rep::Word32());
          return VisitFloat64ExtractHighWord32(node);
        case ChangeOp::Kind::kExtractLowHalf:
          DCHECK_EQ(change.from, Rep::Float64());
          DCHECK_EQ(change.to, Rep::Word32());
          return VisitFloat64ExtractLowWord32(node);
        case ChangeOp::Kind::kZeroExtend:
          DCHECK_EQ(change.from, Rep::Word32());
          DCHECK_EQ(change.to, Rep::Word64());
          return VisitChangeUint32ToUint64(node);
        case ChangeOp::Kind::kSignExtend:
          DCHECK_EQ(change.from, Rep::Word32());
          DCHECK_EQ(change.to, Rep::Word64());
          return VisitChangeInt32ToInt64(node);
        case ChangeOp::Kind::kTruncate:
          DCHECK_EQ(change.from, Rep::Word64());
          DCHECK_EQ(change.to, Rep::Word32());
          MarkAsWord32(node);
          return VisitTruncateInt64ToInt32(node);
        case ChangeOp::Kind::kBitcast:
          switch (multi(change.from, change.to)) {
            case multi(Rep::Word32(), Rep::Word64()):
              return VisitBitcastWord32ToWord64(node);
            case multi(Rep::Word32(), Rep::Float32()):
              return VisitBitcastInt32ToFloat32(node);
            case multi(Rep::Word64(), Rep::Float64()):
              return VisitBitcastInt64ToFloat64(node);
            case multi(Rep::Float32(), Rep::Word32()):
              return VisitBitcastFloat32ToInt32(node);
            case multi(Rep::Float64(), Rep::Word64()):
              return VisitBitcastFloat64ToInt64(node);
            default:
              UNREACHABLE();
          }
      }
      UNREACHABLE();
    }
    case Opcode::kTryChange: {
      const TryChangeOp& try_change = op.Cast<TryChangeOp>();
      MarkAsRepresentation(try_change.to.machine_representation(), node);
      DCHECK(try_change.kind ==
                 TryChangeOp::Kind::kSignedFloatTruncateOverflowUndefined ||
             try_change.kind ==
                 TryChangeOp::Kind::kUnsignedFloatTruncateOverflowUndefined);
      const bool is_signed =
          try_change.kind ==
          TryChangeOp::Kind::kSignedFloatTruncateOverflowUndefined;
      switch (multi(try_change.from, try_change.to, is_signed)) {
        case multi(Rep::Float64(), Rep::Word64(), true):
          return VisitTryTruncateFloat64ToInt64(node);
        case multi(Rep::Float64(), Rep::Word64(), false):
          return VisitTryTruncateFloat64ToUint64(node);
        case multi(Rep::Float64(), Rep::Word32(), true):
          return VisitTryTruncateFloat64ToInt32(node);
        case multi(Rep::Float64(), Rep::Word32(), false):
          return VisitTryTruncateFloat64ToUint32(node);
        case multi(Rep::Float32(), Rep::Word64(), true):
          return VisitTryTruncateFloat32ToInt64(node);
        case multi(Rep::Float32(), Rep::Word64(), false):
          return VisitTryTruncateFloat32ToUint64(node);
        default:
          UNREACHABLE();
      }
      UNREACHABLE();
    }
    case Opcode::kConstant: {
      const ConstantOp& constant = op.Cast<ConstantOp>();
      switch (constant.kind) {
        case ConstantOp::Kind::kWord32:
        case ConstantOp::Kind::kWord64:
        case ConstantOp::Kind::kSmi:
        case ConstantOp::Kind::kTaggedIndex:
        case ConstantOp::Kind::kExternal:
          break;
        case ConstantOp::Kind::kFloat32:
          MarkAsFloat32(node);
          break;
        case ConstantOp::Kind::kFloat64:
          MarkAsFloat64(node);
          break;
        case ConstantOp::Kind::kHeapObject:
        case ConstantOp::Kind::kTrustedHeapObject:
          MarkAsTagged(node);
          break;
        case ConstantOp::Kind::kCompressedHeapObject:
          MarkAsCompressed(node);
          break;
        case ConstantOp::Kind::kNumber:
          if (!IsSmiDouble(constant.number().get_scalar())) MarkAsTagged(node);
          break;
        case ConstantOp::Kind::kRelocatableWasmCall:
        case ConstantOp::Kind::kRelocatableWasmStubCall:
        case ConstantOp::Kind::kRelocatableWasmCanonicalSignatureId:
        case ConstantOp::Kind::kRelocatableWasmIndirectCallTarget:
          break;
      }
      VisitConstant(node);
      break;
    }
    case Opcode::kWordUnary: {
      const WordUnaryOp& unop = op.Cast<WordUnaryOp>();
      if (unop.rep == WordRepresentation::Word32()) {
        MarkAsWord32(node);
        switch (unop.kind) {
          case WordUnaryOp::Kind::kReverseBytes:
            return VisitWord32ReverseBytes(node);
          case WordUnaryOp::Kind::kCountLeadingZeros:
            return VisitWord32Clz(node);
          case WordUnaryOp::Kind::kCountTrailingZeros:
            return VisitWord32Ctz(node);
          case WordUnaryOp::Kind::kPopCount:
            return VisitWord32Popcnt(node);
          case WordUnaryOp::Kind::kSignExtend8:
            return VisitSignExtendWord8ToInt32(node);
          case WordUnaryOp::Kind::kSignExtend16:
            return VisitSignExtendWord16ToInt32(node);
        }
      } else {
        DCHECK_EQ(unop.rep, WordRepresentation::Word64());
        MarkAsWord64(node);
        switch (unop.kind) {
          case WordUnaryOp::Kind::kReverseBytes:
            return VisitWord64ReverseBytes(node);
          case WordUnaryOp::Kind::kCountLeadingZeros:
            return VisitWord64Clz(node);
          case WordUnaryOp::Kind::kCountTrailingZeros:
            return VisitWord64Ctz(node);
          case WordUnaryOp::Kind::kPopCount:
            return VisitWord64Popcnt(node);
          case WordUnaryOp::Kind::kSignExtend8:
            return VisitSignExtendWord8ToInt64(node);
          case WordUnaryOp::Kind::kSignExtend16:
            return VisitSignExtendWord16ToInt64(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kWordBinop: {
      const WordBinopOp& binop = op.Cast<WordBinopOp>();
      if (binop.rep == WordRepresentation::Word32()) {
        MarkAsWord32(node);
        switch (binop.kind) {
          case WordBinopOp::Kind::kAdd:
            return VisitInt32Add(node);
          case WordBinopOp::Kind::kMul:
            return VisitInt32Mul(node);
          case WordBinopOp::Kind::kSignedMulOverflownBits:
            return VisitInt32MulHigh(node);
          case WordBinopOp::Kind::kUnsignedMulOverflownBits:
            return VisitUint32MulHigh(node);
          case WordBinopOp::Kind::kBitwiseAnd:
            return VisitWord32And(node);
          case WordBinopOp::Kind::kBitwiseOr:
            return VisitWord32Or(node);
          case WordBinopOp::Kind::kBitwiseXor:
            return VisitWord32Xor(node);
          case WordBinopOp::Kind::kSub:
            return VisitInt32Sub(node);
          case WordBinopOp::Kind::kSignedDiv:
            return VisitInt32Div(node);
          case WordBinopOp::Kind::kUnsignedDiv:
            return VisitUint32Div(node);
          case WordBinopOp::Kind::kSignedMod:
            return VisitInt32Mod(node);
          case WordBinopOp::Kind::kUnsignedMod:
            return VisitUint32Mod(node);
        }
      } else {
        DCHECK_EQ(binop.rep, WordRepresentation::Word64());
        MarkAsWord64(node);
        switch (binop.kind) {
          case WordBinopOp::Kind::kAdd:
            return VisitInt64Add(node);
          case WordBinopOp::Kind::kMul:
            return VisitInt64Mul(node);
          case WordBinopOp::Kind::kSignedMulOverflownBits:
            return VisitInt64MulHigh(node);
          case WordBinopOp::Kind::kUnsignedMulOverflownBits:
            return VisitUint64MulHigh(node);
          case WordBinopOp::Kind::kBitwiseAnd:
            return VisitWord64And(node);
          case WordBinopOp::Kind::kBitwiseOr:
            return VisitWord64Or(node);
          case WordBinopOp::Kind::kBitwiseXor:
            return VisitWord64Xor(node);
          case WordBinopOp::Kind::kSub:
            return VisitInt64Sub(node);
          case WordBinopOp::Kind::kSignedDiv:
            return VisitInt64Div(node);
          case WordBinopOp::Kind::kUnsignedDiv:
            return VisitUint64Div(node);
          case WordBinopOp::Kind::kSignedMod:
            return VisitInt64Mod(node);
          case WordBinopOp::Kind::kUnsignedMod:
            return VisitUint64Mod(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kFloatUnary: {
      const auto& unop = op.Cast<FloatUnaryOp>();
      if (unop.rep == Rep::Float32()) {
        MarkAsFloat32(node);
        switch (unop.kind) {
          case FloatUnaryOp::Kind::kAbs:
            return VisitFloat32Abs(node);
          case FloatUnaryOp::Kind::kNegate:
            return VisitFloat32Neg(node);
          case FloatUnaryOp::Kind::kRoundDown:
            return VisitFloat32RoundDown(node);
          case FloatUnaryOp::Kind::kRoundUp:
            return VisitFloat32RoundUp(node);
          case FloatUnaryOp::Kind::kRoundToZero:
            return VisitFloat32RoundTruncate(node);
          case FloatUnaryOp::Kind::kRoundTiesEven:
            return VisitFloat32RoundTiesEven(node);
          case FloatUnaryOp::Kind::kSqrt:
            return VisitFloat32Sqrt(node);
          // Those operations are only supported on 64 bit.
          case FloatUnaryOp::Kind::kSilenceNaN:
          case FloatUnaryOp::Kind::kLog:
          case FloatUnaryOp::Kind::kLog2:
          case FloatUnaryOp::Kind::kLog10:
          case FloatUnaryOp::Kind::kLog1p:
          case FloatUnaryOp::Kind::kCbrt:
          case FloatUnaryOp::Kind::kExp:
          case FloatUnaryOp::Kind::kExpm1:
          case FloatUnaryOp::Kind::kSin:
          case FloatUnaryOp::Kind::kCos:
          case FloatUnaryOp::Kind::kSinh:
          case FloatUnaryOp::Kind::kCosh:
          case FloatUnaryOp::Kind::kAcos:
          case FloatUnaryOp::Kind::kAsin:
          case FloatUnaryOp::Kind::kAsinh:
          case FloatUnaryOp::Kind::kAcosh:
          case FloatUnaryOp::Kind::kTan:
          case FloatUnaryOp::Kind::kTanh:
          case FloatUnaryOp::Kind::kAtan:
          case FloatUnaryOp::Kind::kAtanh:
            UNREACHABLE();
        }
      } else {
        DCHECK_EQ(unop.rep, Rep::Float64());
        MarkAsFloat64(node);
        switch (unop.kind) {
          case FloatUnaryOp::Kind::kAbs:
            return VisitFloat64Abs(node);
          case FloatUnaryOp::Kind::kNegate:
            return VisitFloat64Neg(node);
          case FloatUnaryOp::Kind::kSilenceNaN:
            return VisitFloat64SilenceNaN(node);
          case FloatUnaryOp::Kind::kRoundDown:
            return VisitFloat64RoundDown(node);
          case FloatUnaryOp::Kind::kRoundUp:
            return VisitFloat64RoundUp(node);
          case FloatUnaryOp::Kind::kRoundToZero:
            return VisitFloat64RoundTruncate(node);
          case FloatUnaryOp::Kind::kRoundTiesEven:
            return VisitFloat64RoundTiesEven(node);
          case FloatUnaryOp::Kind::kLog:
            return VisitFloat64Log(node);
          case FloatUnaryOp::Kind::kLog2:
            return VisitFloat64Log2(node);
          case FloatUnaryOp::Kind::kLog10:
            return VisitFloat64Log10(node);
          case FloatUnaryOp::Kind::kLog1p:
            return VisitFloat64Log1p(node);
          case FloatUnaryOp::Kind::kSqrt:
            return VisitFloat64Sqrt(node);
          case FloatUnaryOp::Kind::kCbrt:
            return VisitFloat64Cbrt(node);
          case FloatUnaryOp::Kind::kExp:
            return VisitFloat64Exp(node);
          case FloatUnaryOp::Kind::kExpm1:
            return VisitFloat64Expm1(node);
          case FloatUnaryOp::Kind::kSin:
            return VisitFloat64Sin(node);
          case FloatUnaryOp::Kind::kCos:
            return VisitFloat64Cos(node);
          case FloatUnaryOp::Kind::kSinh:
            return VisitFloat64Sinh(node);
          case FloatUnaryOp::Kind::kCosh:
            return VisitFloat64Cosh(node);
          case FloatUnaryOp::Kind::kAcos:
            return VisitFloat64Acos(node);
          case FloatUnaryOp::Kind::kAsin:
            return VisitFloat64Asin(node);
          case FloatUnaryOp::Kind::kAsinh:
            return VisitFloat64Asinh(node);
          case FloatUnaryOp::Kind::kAcosh:
            return VisitFloat64Acosh(node);
          case FloatUnaryOp::Kind::kTan:
            return VisitFloat64Tan(node);
          case FloatUnaryOp::Kind::kTanh:
            return VisitFloat64Tanh(node);
          case FloatUnaryOp::Kind::kAtan:
            return VisitFloat64Atan(node);
          case FloatUnaryOp::Kind::kAtanh:
            return VisitFloat64Atanh(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kFloatBinop: {
      const auto& binop = op.Cast<FloatBinopOp>();
      if (binop.rep == Rep::Float32()) {
        MarkAsFloat32(node);
        switch (binop.kind) {
          case FloatBinopOp::Kind::kAdd:
            return VisitFloat32Add(node);
          case FloatBinopOp::Kind::kSub:
            return VisitFloat32Sub(node);
          case FloatBinopOp::Kind::kMul:
            return VisitFloat32Mul(node);
          case FloatBinopOp::Kind::kDiv:
            return VisitFloat32Div(node);
          case FloatBinopOp::Kind::kMin:
            return VisitFloat32Min(node);
          case FloatBinopOp::Kind::kMax:
            return VisitFloat32Max(node);
          case FloatBinopOp::Kind::kMod:
          case FloatBinopOp::Kind::kPower:
          case FloatBinopOp::Kind::kAtan2:
            UNREACHABLE();
        }
      } else {
        DCHECK_EQ(binop.rep, Rep::Float64());
        MarkAsFloat64(node);
        switch (binop.kind) {
          case FloatBinopOp::Kind::kAdd:
            return VisitFloat64Add(node);
          case FloatBinopOp::Kind::kSub:
            return VisitFloat64Sub(node);
          case FloatBinopOp::Kind::kMul:
            return VisitFloat64Mul(node);
          case FloatBinopOp::Kind::kDiv:
            return VisitFloat64Div(node);
          case FloatBinopOp::Kind::kMod:
            return VisitFloat64Mod(node);
          case FloatBinopOp::Kind::kMin:
            return VisitFloat64Min(node);
          case FloatBinopOp::Kind::kMax:
            return VisitFloat64Max(node);
          case FloatBinopOp::Kind::kPower:
            return VisitFloat64Pow(node);
          case FloatBinopOp::Kind::kAtan2:
            return VisitFloat64Atan2(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kOverflowCheckedBinop: {
      const auto& binop = op.Cast<OverflowCheckedBinopOp>();
      if (binop.rep == WordRepresentation::Word32()) {
        MarkAsWord32(node);
        switch (binop.kind) {
          case OverflowCheckedBinopOp::Kind::kSignedAdd:
            return VisitInt32AddWithOverflow(node);
          case OverflowCheckedBinopOp::Kind::kSignedMul:
            return VisitInt32MulWithOverflow(node);
          case OverflowCheckedBinopOp::Kind::kSignedSub:
            return VisitInt32SubWithOverflow(node);
        }
      } else {
        DCHECK_EQ(binop.rep, WordRepresentation::Word64());
        MarkAsWord64(node);
        switch (binop.kind) {
          case OverflowCheckedBinopOp::Kind::kSignedAdd:
            return VisitInt64AddWithOverflow(node);
          case OverflowCheckedBinopOp::Kind::kSignedMul:
            return VisitInt64MulWithOverflow(node);
          case OverflowCheckedBinopOp::Kind::kSignedSub:
            return VisitInt64SubWithOverflow(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kOverflowCheckedUnary: {
      const auto& unop = op.Cast<OverflowCheckedUnaryOp>();
      if (unop.rep == WordRepresentation::Word32()) {
        MarkAsWord32(node);
        switch (unop.kind) {
          case OverflowCheckedUnaryOp::Kind::kAbs:
            return VisitInt32AbsWithOverflow(node);
        }
      } else {
        DCHECK_EQ(unop.rep, WordRepresentation::Word64());
        MarkAsWord64(node);
        switch (unop.kind) {
          case OverflowCheckedUnaryOp::Kind::kAbs:
            return VisitInt64AbsWithOverflow(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kShift: {
      const auto& shift = op.Cast<ShiftOp>();
      if (shift.rep == RegisterRepresentation::Word32()) {
        MarkAsWord32(node);
        switch (shift.kind) {
          case ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros:
          case ShiftOp::Kind::kShiftRightArithmetic:
            return VisitWord32Sar(node);
          case ShiftOp::Kind::kShiftRightLogical:
            return VisitWord32Shr(node);
          case ShiftOp::Kind::kShiftLeft:
            return VisitWord32Shl(node);
          case ShiftOp::Kind::kRotateRight:
            return VisitWord32Ror(node);
          case ShiftOp::Kind::kRotateLeft:
            return VisitWord32Rol(node);
        }
      } else {
        DCHECK_EQ(shift.rep, RegisterRepresentation::Word64());
        MarkAsWord64(node);
        switch (shift.kind) {
          case ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros:
          case ShiftOp::Kind::kShiftRightArithmetic:
            return VisitWord64Sar(node);
          case ShiftOp::Kind::kShiftRightLogical:
            return VisitWord64Shr(node);
          case ShiftOp::Kind::kShiftLeft:
            return VisitWord64Shl(node);
          case ShiftOp::Kind::kRotateRight:
            return VisitWord64Ror(node);
          case ShiftOp::Kind::kRotateLeft:
            return VisitWord64Rol(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kCall:
      // Process the call at `DidntThrow`, when we know if exceptions are caught
      // or not.
      break;
    case Opcode::kDidntThrow:
      if (current_block_->begin() == node) {
        DCHECK_EQ(current_block_->PredecessorCount(), 1);
        DCHECK(current_block_->LastPredecessor()
                   ->LastOperation(*this->turboshaft_graph())
                   .Is<CheckExceptionOp>());
        // In this case, the Call has been generated at the `CheckException`
        // already.
      } else {
        VisitCall(op.Cast<DidntThrowOp>().throwing_operation());
      }
      EmitIdentity(node);
      break;
    case Opcode::kFrameConstant: {
      const auto& constant = op.Cast<FrameConstantOp>();
      using Kind = FrameConstantOp::Kind;
      OperandGenerator g(this);
      switch (constant.kind) {
        case Kind::kStackCheckOffset:
          Emit(kArchStackCheckOffset, g.DefineAsRegister(node));
          break;
        case Kind::kFramePointer:
          Emit(kArchFramePointer, g.DefineAsRegister(node));
          break;
        case Kind::kParentFramePointer:
          Emit(kArchParentFramePointer, g.DefineAsRegister(node));
          break;
      }
      break;
    }
    case Opcode::kStackPointerGreaterThan:
      return VisitStackPointerGreaterThan(node);
    case Opcode::kComparison: {
      const ComparisonOp& comparison = op.Cast<ComparisonOp>();
      using Kind = ComparisonOp::Kind;
      switch (multi(comparison.kind, comparison.rep)) {
        case multi(Kind::kEqual, Rep::Word32()):
          return VisitWord32Equal(node);
        case multi(Kind::kEqual, Rep::Word64()):
          return VisitWord64Equal(node);
        case multi(Kind::kEqual, Rep::Float32()):
          return VisitFloat32Equal(node);
        case multi(Kind::kEqual, Rep::Float64()):
          return VisitFloat64Equal(node);
        case multi(Kind::kEqual, Rep::Tagged()):
          if constexpr (Is64() && !COMPRESS_POINTERS_BOOL) {
            return VisitWord64Equal(node);
          }
          return VisitWord32Equal(node);
        case multi(Kind::kSignedLessThan, Rep::Word32()):
          return VisitInt32LessThan(node);
        case multi(Kind::kSignedLessThan, Rep::Word64()):
          return VisitInt64LessThan(node);
        case multi(Kind::kSignedLessThan, Rep::Float32()):
          return VisitFloat32LessThan(node);
        case multi(Kind::kSignedLessThan, Rep::Float64()):
          return VisitFloat64LessThan(node);
        case multi(Kind::kSignedLessThanOrEqual, Rep::Word32()):
          return VisitInt32LessThanOrEqual(node);
        case multi(Kind::kSignedLessThanOrEqual, Rep::Word64()):
          return VisitInt64LessThanOrEqual(node);
        case multi(Kind::kSignedLessThanOrEqual, Rep::Float32()):
          return VisitFloat32LessThanOrEqual(node);
        case multi(Kind::kSignedLessThanOrEqual, Rep::Float64()):
          return VisitFloat64LessThanOrEqual(node);
        case multi(Kind::kUnsignedLessThan, Rep::Word32()):
          return VisitUint32LessThan(node);
        case multi(Kind::kUnsignedLessThan, Rep::Word64()):
          return VisitUint64LessThan(node);
        case multi(Kind::kUnsignedLessThanOrEqual, Rep::Word32()):
          return VisitUint32LessThanOrEqual(node);
        case multi(Kind::kUnsignedLessThanOrEqual, Rep::Word64()):
          return VisitUint64LessThanOrEqual(node);
        default:
          UNREACHABLE();
      }
      UNREACHABLE();
    }
    case Opcode::kLoad: {
      const LoadOp& load = op.Cast<LoadOp>();
      MachineType loaded_type = load.machine_type();
      MarkAsRepresentation(loaded_type.representation(), node);
      if (load.kind.maybe_unaligned) {
        DCHECK(!load.kind.with_trap_handler);
        DCHECK(!load.kind.is_atomic);
        if (loaded_type.representation() == MachineRepresentation::kWord8 ||
            InstructionSelector::AlignmentRequirements()
                .IsUnalignedLoadSupported(loaded_type.representation())) {
          return VisitLoad(node);
        } else {
          return VisitUnalignedLoad(node);
        }
      } else if (load.kind.is_atomic) {
        if (load.result_rep == Rep::Word32()) {
          return VisitWord32AtomicLoad(node);
        } else if (load.result_rep == Rep::Word64()) {
          return VisitWord64AtomicLoad(node);
        } else if (load.result_rep == Rep::Tagged()) {
          return kTaggedSize == 4 ? VisitWord32AtomicLoad(node)
                                  : VisitWord64AtomicLoad(node);
        }
      } else if (load.kind.with_trap_handler) {
        DCHECK(!load.kind.maybe_unaligned);
        return VisitProtectedLoad(node);
      } else {
        return VisitLoad(node);
      }
      UNREACHABLE();
    }
    case Opcode::kStore: {
      const StoreOp& store = op.Cast<StoreOp>();
      MachineRepresentation rep =
          store.stored_rep.ToMachineType().representation();
      if (store.kind.maybe_unaligned) {
        DCHECK(!store.kind.with_trap_handler);
        DCHECK_EQ(store.write_barrier, WriteBarrierKind::kNoWriteBarrier);
        if (rep == MachineRepresentation::kWord8 ||
            InstructionSelector::AlignmentRequirements()
                .IsUnalignedStoreSupported(rep)) {
          return VisitStore(node);
        } else {
          return VisitUnalignedStore(node);
        }
      } else if (store.kind.is_atomic) {
        if (store.stored_rep.SizeInBytes() == 8) {
          return VisitWord64AtomicStore(node);
        } else {
          DCHECK_LE(store.stored_rep.SizeInBytes(), 4);
          return VisitWord32AtomicStore(node);
        }
      } else if (store.kind.with_trap_handler) {
        DCHECK(!store.kind.maybe_unaligned);
        return VisitProtectedStore(node);
      } else {
        return VisitStore(node);
      }
      UNREACHABLE();
    }
    case Opcode::kTaggedBitcast: {
      const TaggedBitcastOp& cast = op.Cast<TaggedBitcastOp>();
      switch (multi(cast.from, cast.to)) {
        case multi(Rep::Tagged(), Rep::Word32()):
          MarkAsWord32(node);
          if constexpr (Is64()) {
            DCHECK_EQ(cast.kind, TaggedBitcastOp::Kind::kSmi);
            DCHECK(SmiValuesAre31Bits());
            return VisitBitcastSmiToWord(node);
          } else {
            return VisitBitcastTaggedToWord(node);
          }
        case multi(Rep::Tagged(), Rep::Word64()):
          MarkAsWord64(node);
          return VisitBitcastTaggedToWord(node);
        case multi(Rep::Word32(), Rep::Tagged()):
        case multi(Rep::Word64(), Rep::Tagged()):
          if (cast.kind == TaggedBitcastOp::Kind::kSmi) {
            MarkAsRepresentation(MachineRepresentation::kTaggedSigned, node);
            return EmitIdentity(node);
          } else {
            MarkAsTagged(node);
            return VisitBitcastWordToTagged(node);
          }
        case multi(Rep::Compressed(), Rep::Word32()):
          MarkAsWord32(node);
          if (cast.kind == TaggedBitcastOp::Kind::kSmi) {
            return VisitBitcastSmiToWord(node);
          } else {
            return VisitBitcastTaggedToWord(node);
          }
        default:
          UNIMPLEMENTED();
      }
    }
    case Opcode::kPhi:
      MarkAsRepresentation(op.Cast<PhiOp>().rep, node);
      return VisitPhi(node);
    case Opcode::kProjection:
      return VisitProjection(node);
    case Opcode::kDeoptimizeIf:
      return VisitDeoptimizeIf(node);
#if V8_ENABLE_WEBASSEMBLY
    case Opcode::kTrapIf:
      return VisitTrapIf(node);
#endif  // V8_ENABLE_WEBASSEMBLY
    case Opcode::kCatchBlockBegin:
      MarkAsTagged(node);
      return VisitIfException(node);
    case Opcode::kRetain:
      return VisitRetain(node);
    case Opcode::kOsrValue:
      MarkAsTagged(node);
      return VisitOsrValue(node);
    case Opcode::kStackSlot:
      return VisitStackSlot(node);
    case Opcode::kFrameState:
      // FrameState is covered as part of calls.
      UNREACHABLE();
    case Opcode::kLoadRootRegister:
      return VisitLoadRootRegister(node);
    case Opcode::kAssumeMap:
      // AssumeMap is used as a hint for optimization phases but does not
      // produce any code.
      return;
    case Opcode::kDebugBreak:
      return VisitDebugBreak(node);
    case Opcode::kAbortCSADcheck:
      return VisitAbortCSADcheck(node);
    case Opcode::kSelect: {
      const SelectOp& select = op.Cast<SelectOp>();
      // If there is a Select, then it should only be one that is supported by
      // the machine, and it should be meant to be implementation with cmove.
      DCHECK_EQ(select.implem, SelectOp::Implementation::kCMove);
      MarkAsRepresentation(select.rep, node);
      return VisitSelect(node);
    }
    case Opcode::kWord32PairBinop: {
      const Word32PairBinopOp& binop = op.Cast<Word32PairBinopOp>();
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      switch (binop.kind) {
        case Word32PairBinopOp::Kind::kAdd:
          return VisitInt32PairAdd(node);
        case Word32PairBinopOp::Kind::kSub:
          return VisitInt32PairSub(node);
        case Word32PairBinopOp::Kind::kMul:
          return VisitInt32PairMul(node);
        case Word32PairBinopOp::Kind::kShiftLeft:
          return VisitWord32PairShl(node);
        case Word32PairBinopOp::Kind::kShiftRightLogical:
          return VisitWord32PairShr(node);
        case Word32PairBinopOp::Kind::kShiftRightArithmetic:
          return VisitWord32PairSar(node);
      }
      UNREACHABLE();
    }
    case Opcode::kAtomicWord32Pair: {
      const AtomicWord32PairOp& atomic_op = op.Cast<AtomicWord32PairOp>();
      if (atomic_op.kind != AtomicWord32PairOp::Kind::kStore) {
        MarkAsWord32(node);
        MarkPairProjectionsAsWord32(node);
      }
      switch (atomic_op.kind) {
        case AtomicWord32PairOp::Kind::kAdd:
          return VisitWord32AtomicPairAdd(node);
        case AtomicWord32PairOp::Kind::kAnd:
          return VisitWord32AtomicPairAnd(node);
        case AtomicWord32PairOp::Kind::kCompareExchange:
          return VisitWord32AtomicPairCompareExchange(node);
        case AtomicWord32PairOp::Kind::kExchange:
          return VisitWord32AtomicPairExchange(node);
        case AtomicWord32PairOp::Kind::kLoad:
          return VisitWord32AtomicPairLoad(node);
        case AtomicWord32PairOp::Kind::kOr:
          return VisitWord32AtomicPairOr(node);
        case AtomicWord32PairOp::Kind::kSub:
          return VisitWord32AtomicPairSub(node);
        case AtomicWord32PairOp::Kind::kXor:
          return VisitWord32AtomicPairXor(node);
        case AtomicWord32PairOp::Kind::kStore:
          return VisitWord32AtomicPairStore(node);
      }
    }
    case Opcode::kBitcastWord32PairToFloat64:
      return MarkAsFloat64(node), VisitBitcastWord32PairToFloat64(node);
    case Opcode::kAtomicRMW: {
      const AtomicRMWOp& atomic_op = op.Cast<AtomicRMWOp>();
      MarkAsRepresentation(atomic_op.memory_rep.ToRegisterRepresentation(),
                           node);
      if (atomic_op.in_out_rep == Rep::Word32()) {
        switch (atomic_op.bin_op) {
          case AtomicRMWOp::BinOp::kAdd:
            return VisitWord32AtomicAdd(node);
          case AtomicRMWOp::BinOp::kSub:
            return VisitWord32AtomicSub(node);
          case AtomicRMWOp::BinOp::kAnd:
            return VisitWord32AtomicAnd(node);
          case AtomicRMWOp::BinOp::kOr:
            return VisitWord32AtomicOr(node);
          case AtomicRMWOp::BinOp::kXor:
            return VisitWord32AtomicXor(node);
          case AtomicRMWOp::BinOp::kExchange:
            return VisitWord32AtomicExchange(node);
          case AtomicRMWOp::BinOp::kCompareExchange:
            return VisitWord32AtomicCompareExchange(node);
        }
      } else {
        DCHECK_EQ(atomic_op.in_out_rep, Rep::Word64());
        switch (atomic_op.bin_op) {
          case AtomicRMWOp::BinOp::kAdd:
            return VisitWord64AtomicAdd(node);
          case AtomicRMWOp::BinOp::kSub:
            return VisitWord64AtomicSub(node);
          case AtomicRMWOp::BinOp::kAnd:
            return VisitWord64AtomicAnd(node);
          case AtomicRMWOp::BinOp::kOr:
            return VisitWord64AtomicOr(node);
          case AtomicRMWOp::BinOp::kXor:
            return VisitWord64AtomicXor(node);
          case AtomicRMWOp::BinOp::kExchange:
            return VisitWord64AtomicExchange(node);
          case AtomicRMWOp::BinOp::kCompareExchange:
            return VisitWord64AtomicCompareExchange(node);
        }
      }
      UNREACHABLE();
    }
    case Opcode::kMemoryBarrier:
      return VisitMemoryBarrier(node);

    case Opcode::kComment:
      return VisitComment(node);

#ifdef V8_ENABLE_WEBASSEMBLY
    case Opcode::kSimd128Constant: {
      const Simd128ConstantOp& constant = op.Cast<Simd128ConstantOp>();
      MarkAsSimd128(node);
      if (constant.IsZero()) return VisitS128Zero(node);
      return VisitS128Const(node);
    }
    case Opcode::kSimd128Unary: {
      const Simd128UnaryOp& unary = op.Cast<Simd128UnaryOp>();
      MarkAsSimd128(node);
      switch (unary.kind) {
#define VISIT_SIMD_UNARY(kind)        \
  case Simd128UnaryOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_128_UNARY_OPCODE(VISIT_SIMD_UNARY)
#undef VISIT_SIMD_UNARY
      }
    }
    case Opcode::kSimd128Reduce: {
      const Simd128ReduceOp& reduce = op.Cast<Simd128ReduceOp>();
      MarkAsSimd128(node);
      switch (reduce.kind) {
        case Simd128ReduceOp::Kind::kI8x16AddReduce:
          return VisitI8x16AddReduce(node);
        case Simd128ReduceOp::Kind::kI16x8AddReduce:
          return VisitI16x8AddReduce(node);
        case Simd128ReduceOp::Kind::kI32x4AddReduce:
          return VisitI32x4AddReduce(node);
        case Simd128ReduceOp::Kind::kI64x2AddReduce:
          return VisitI64x2AddReduce(node);
        case Simd128ReduceOp::Kind::kF32x4AddReduce:
          return VisitF32x4AddReduce(node);
        case Simd128ReduceOp::Kind::kF64x2AddReduce:
          return VisitF64x2AddReduce(node);
      }
    }
    case Opcode::kSimd128Binop: {
      const Simd128BinopOp& binop = op.Cast<Simd128BinopOp>();
      MarkAsSimd128(node);
      switch (binop.kind) {
#define VISIT_SIMD_BINOP(kind)        \
  case Simd128BinopOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_128_BINARY_OPCODE(VISIT_SIMD_BINOP)
#undef VISIT_SIMD_BINOP
      }
    }
    case Opcode::kSimd128Shift: {
      const Simd128ShiftOp& shift = op.Cast<Simd128ShiftOp>();
      MarkAsSimd128(node);
      switch (shift.kind) {
#define VISIT_SIMD_SHIFT(kind)        \
  case Simd128ShiftOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_128_SHIFT_OPCODE(VISIT_SIMD_SHIFT)
#undef VISIT_SIMD_SHIFT
      }
    }
    case Opcode::kSimd128Test: {
      const Simd128TestOp& test = op.Cast<Simd128TestOp>();
      MarkAsWord32(node);
      switch (test.kind) {
#define VISIT_SIMD_TEST(kind)        \
  case Simd128TestOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_128_TEST_OPCODE(VISIT_SIMD_TEST)
#undef VISIT_SIMD_TEST
      }
    }
    case Opcode::kSimd128Splat: {
      const Simd128SplatOp& splat = op.Cast<Simd128SplatOp>();
      MarkAsSimd128(node);
      switch (splat.kind) {
#define VISIT_SIMD_SPLAT(kind)        \
  case Simd128SplatOp::Kind::k##kind: \
    return Visit##kind##Splat(node);
        FOREACH_SIMD_128_SPLAT_OPCODE(VISIT_SIMD_SPLAT)
#undef VISIT_SIMD_SPLAT
      }
    }
    case Opcode::kSimd128Shuffle: {
      MarkAsSimd128(node);
      const Simd128ShuffleOp& shuffle = op.Cast<Simd128ShuffleOp>();
      switch (shuffle.kind) {
        case Simd128ShuffleOp::Kind::kI8x2:
          return VisitI8x2Shuffle(node);
        case Simd128ShuffleOp::Kind::kI8x4:
          return VisitI8x4Shuffle(node);
        case Simd128ShuffleOp::Kind::kI8x8:
          return VisitI8x8Shuffle(node);
        case Simd128ShuffleOp::Kind::kI8x16:
          return VisitI8x16Shuffle(node);
      }
    }
    case Opcode::kSimd128ReplaceLane: {
      const Simd128ReplaceLaneOp& replace = op.Cast<Simd128ReplaceLaneOp>();
      MarkAsSimd128(node);
      switch (replace.kind) {
        case Simd128ReplaceLaneOp::Kind::kI8x16:
          return VisitI8x16ReplaceLane(node);
        case Simd128ReplaceLaneOp::Kind::kI16x8:
          return VisitI16x8ReplaceLane(node);
        case Simd128ReplaceLaneOp::Kind::kI32x4:
          return VisitI32x4ReplaceLane(node);
        case Simd128ReplaceLaneOp::Kind::kI64x2:
          return VisitI64x2ReplaceLane(node);
        case Simd128ReplaceLaneOp::Kind::kF16x8:
          return VisitF16x8ReplaceLane(node);
        case Simd128ReplaceLaneOp::Kind::kF32x4:
          return VisitF32x4ReplaceLane(node);
        case Simd128ReplaceLaneOp::Kind::kF64x2:
          return VisitF64x2ReplaceLane(node);
      }
    }
    case Opcode::kSimd128ExtractLane: {
      const Simd128ExtractLaneOp& extract = op.Cast<Simd128ExtractLaneOp>();
      switch (extract.kind) {
        case Simd128ExtractLaneOp::Kind::kI8x16S:
          MarkAsWord32(node);
          return VisitI8x16ExtractLaneS(node);
        case Simd128ExtractLaneOp::Kind::kI8x16U:
          MarkAsWord32(node);
          return VisitI8x16ExtractLaneU(node);
        case Simd128ExtractLaneOp::Kind::kI16x8S:
          MarkAsWord32(node);
          return VisitI16x8ExtractLaneS(node);
        case Simd128ExtractLaneOp::Kind::kI16x8U:
          MarkAsWord32(node);
          return VisitI16x8ExtractLaneU(node);
        case Simd128ExtractLaneOp::Kind::kI32x4:
          MarkAsWord32(node);
          return VisitI32x4ExtractLane(node);
        case Simd128ExtractLaneOp::Kind::kI64x2:
          MarkAsWord64(node);
          return VisitI64x2ExtractLane(node);
        case Simd128ExtractLaneOp::Kind::kF16x8:
          MarkAsFloat32(node);
          return VisitF16x8ExtractLane(node);
        case Simd128ExtractLaneOp::Kind::kF32x4:
          MarkAsFloat32(node);
          return VisitF32x4ExtractLane(node);
        case Simd128ExtractLaneOp::Kind::kF64x2:
          MarkAsFloat64(node);
          return VisitF64x2ExtractLane(node);
      }
    }
    case Opcode::kSimd128LoadTransform:
      MarkAsSimd128(node);
      return VisitLoadTransform(node);
    case Opcode::kSimd128LaneMemory: {
      const Simd128LaneMemoryOp& memory = op.Cast<Simd128LaneMemoryOp>();
      MarkAsSimd128(node);
      if (memory.mode == Simd128LaneMemoryOp::Mode::kLoad) {
        return VisitLoadLane(node);
      } else {
        DCHECK_EQ(memory.mode, Simd128LaneMemoryOp::Mode::kStore);
        return VisitStoreLane(node);
      }
    }
    case Opcode::kSimd128Ternary: {
      const Simd128TernaryOp& ternary = op.Cast<Simd128TernaryOp>();
      MarkAsSimd128(node);
      switch (ternary.kind) {
#define VISIT_SIMD_TERNARY(kind)        \
  case Simd128TernaryOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_128_TERNARY_OPCODE(VISIT_SIMD_TERNARY)
#undef VISIT_SIMD_TERNARY
      }
    }

#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
    case Opcode::kSimd128LoadPairDeinterleave: {
      OptionalOpIndex projection0 = FindProjection(node, 0);
      DCHECK(projection0.valid());
      MarkAsSimd128(projection0.value());
      OptionalOpIndex projection1 = FindProjection(node, 1);
      DCHECK(projection1.valid());
      MarkAsSimd128(projection1.value());
      return VisitSimd128LoadPairDeinterleave(node);
    }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

    // SIMD256
#if V8_ENABLE_WASM_SIMD256_REVEC
    case Opcode::kSimd256Constant: {
      const Simd256ConstantOp& constant = op.Cast<Simd256ConstantOp>();
      MarkAsSimd256(node);
      if (constant.IsZero()) return VisitS256Zero(node);
      return VisitS256Const(node);
    }
    case Opcode::kSimd256Extract128Lane: {
      MarkAsSimd128(node);
      return VisitExtractF128(node);
    }
    case Opcode::kSimd256LoadTransform: {
      MarkAsSimd256(node);
      return VisitSimd256LoadTransform(node);
    }
    case Opcode::kSimd256Unary: {
      const Simd256UnaryOp& unary = op.Cast<Simd256UnaryOp>();
      MarkAsSimd256(node);
      switch (unary.kind) {
#define VISIT_SIMD_256_UNARY(kind)    \
  case Simd256UnaryOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_256_UNARY_OPCODE(VISIT_SIMD_256_UNARY)
#undef VISIT_SIMD_256_UNARY
      }
    }
    case Opcode::kSimd256Binop: {
      const Simd256BinopOp& binop = op.Cast<Simd256BinopOp>();
      MarkAsSimd256(node);
      switch (binop.kind) {
#define VISIT_SIMD_BINOP(kind)        \
  case Simd256BinopOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_256_BINARY_OPCODE(VISIT_SIMD_BINOP)
#undef VISIT_SIMD_BINOP
      }
    }
    case Opcode::kSimd256Shift: {
      const Simd256ShiftOp& shift = op.Cast<Simd256ShiftOp>();
      MarkAsSimd256(node);
      switch (shift.kind) {
#define VISIT_SIMD_SHIFT(kind)        \
  case Simd256ShiftOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_256_SHIFT_OPCODE(VISIT_SIMD_SHIFT)
#undef VISIT_SIMD_SHIFT
      }
    }
    case Opcode::kSimd256Ternary: {
      const Simd256TernaryOp& ternary = op.Cast<Simd256TernaryOp>();
      MarkAsSimd256(node);
      switch (ternary.kind) {
#define VISIT_SIMD_256_TERNARY(kind)    \
  case Simd256TernaryOp::Kind::k##kind: \
    return Visit##kind(node);
        FOREACH_SIMD_256_TERNARY_OPCODE(VISIT_SIMD_256_TERNARY)
#undef VISIT_SIMD_256_UNARY
      }
    }
    case Opcode::kSimd256Splat: {
      const Simd256SplatOp& splat = op.Cast<Simd256SplatOp>();
      MarkAsSimd256(node);
      switch (splat.kind) {
#define VISIT_SIMD_SPLAT(kind)        \
  case Simd256SplatOp::Kind::k##kind: \
    return Visit##kind##Splat(node);
        FOREACH_SIMD_256_SPLAT_OPCODE(VISIT_SIMD_SPLAT)
#undef VISIT_SIMD_SPLAT
      }
    }
#ifdef V8_TARGET_ARCH_X64
    case Opcode::kSimd256Shufd: {
      MarkAsSimd256(node);
      return VisitSimd256Shufd(node);
    }
    case Opcode::kSimd256Shufps: {
      MarkAsSimd256(node);
      return VisitSimd256Shufps(node);
    }
    case Opcode::kSimd256Unpack: {
      MarkAsSimd256(node);
      return VisitSimd256Unpack(node);
    }
    case Opcode::kSimdPack128To256: {
      MarkAsSimd256(node);
      return VisitSimdPack128To256(node);
    }
#endif  // V8_TARGET_ARCH_X64
#endif  // V8_ENABLE_WASM_SIMD256_REVEC

    case Opcode::kLoadStackPointer:
      return VisitLoadStackPointer(node);

    case Opcode::kSetStackPointer:
      this->frame_->set_invalidates_sp();
      return VisitSetStackPointer(node);

#endif  // V8_ENABLE_WEBASSEMBLY
#define UNREACHABLE_CASE(op) case Opcode::k##op:
      TURBOSHAFT_JS_OPERATION_LIST(UNREACHABLE_CASE)
      TURBOSHAFT_SIMPLIFIED_OPERATION_LIST(UNREACHABLE_CASE)
      TURBOSHAFT_WASM_OPERATION_LIST(UNREACHABLE_CASE)
      TURBOSHAFT_OTHER_OPERATION_LIST(UNREACHABLE_CASE)
      UNREACHABLE_CASE(PendingLoopPhi)
      UNREACHABLE_CASE(Tuple)
      UNREACHABLE_CASE(Dead)
      UNREACHABLE();
#undef UNREACHABLE_CASE
  }
}

bool InstructionSelectorT::CanProduceSignalingNaN(Node* node) {
  // TODO(jarin) Improve the heuristic here.
  if (node->opcode() == IrOpcode::kFloat64Add ||
      node->opcode() == IrOpcode::kFloat64Sub ||
      node->opcode() == IrOpcode::kFloat64Mul) {
    return false;
  }
  return true;
}

#if V8_TARGET_ARCH_64_BIT
bool InstructionSelectorT::ZeroExtendsWord32ToWord64(OpIndex node,
                                                     int recursion_depth) {
  // To compute whether a Node sets its upper 32 bits to zero, there are three
  // cases.
  // 1. Phi node, with a computed result already available in phi_states_:
  //    Read the value from phi_states_.
  // 2. Phi node, with no result available in phi_states_ yet:
  //    Recursively check its inputs, and store the result in phi_states_.
  // 3. Anything else:
  //    Call the architecture-specific ZeroExtendsWord32ToWord64NoPhis.

  // Limit recursion depth to avoid the possibility of stack overflow on very
  // large functions.
  const int kMaxRecursionDepth = 100;

  if (const PhiOp* phi = TryCast<PhiOp>(node)) {
    if (recursion_depth == 0) {
      if (phi_states_.empty()) {
        // This vector is lazily allocated because the majority of compilations
        // never use it.
        phi_states_ = ZoneVector<Upper32BitsState>(
            node_count_, Upper32BitsState::kNotYetChecked, zone());
      }
    }

    Upper32BitsState current = phi_states_[node.id()];
    if (current != Upper32BitsState::kNotYetChecked) {
      return current == Upper32BitsState::kZero;
    }

    // If further recursion is prevented, we can't make any assumptions about
    // the output of this phi node.
    if (recursion_depth >= kMaxRecursionDepth) {
      return false;
    }

    // Optimistically mark the current node as zero-extended so that we skip it
    // if we recursively visit it again due to a cycle. If this optimistic guess
    // is wrong, it will be corrected in MarkNodeAsNotZeroExtended.
    phi_states_[node.id()] = Upper32BitsState::kZero;

    for (int i = 0; i < phi->input_count; ++i) {
      OpIndex input = phi->input(i);
      if (!ZeroExtendsWord32ToWord64(input, recursion_depth + 1)) {
        MarkNodeAsNotZeroExtended(node);
        return false;
      }
    }

    return true;
  }
  return ZeroExtendsWord32ToWord64NoPhis(node);
}

void InstructionSelectorT::MarkNodeAsNotZeroExtended(OpIndex node) {
  if (phi_states_[node.id()] == Upper32BitsState::kMayBeNonZero) return;
  phi_states_[node.id()] = Upper32BitsState::kMayBeNonZero;
  ZoneVector<OpIndex> worklist(zone_);
  worklist.push_back(node);
  while (!worklist.empty()) {
    node = worklist.back();
    worklist.pop_back();
    // We may have previously marked some uses of this node as zero-extended,
    // but that optimistic guess was proven incorrect.
    for (OpIndex use : turboshaft_uses(node)) {
      if (phi_states_[use.id()] == Upper32BitsState::kZero) {
        phi_states_[use.id()] = Upper32BitsState::kMayBeNonZero;
        worklist.push_back(use);
      }
    }
  }
}
#endif  // V8_TARGET_ARCH_64_BIT

namespace {

FrameStateDescriptor* GetFrameStateDescriptorInternal(
    Zone* zone, Graph* graph, const FrameStateOp& state) {
  const FrameStateInfo& state_info = state.data->frame_state_info;
  uint16_t parameters = state_info.parameter_count();
  uint16_t max_arguments = state_info.max_arguments();
  int locals = state_info.local_count();
  int stack = state_info.stack_count();

  FrameStateDescriptor* outer_state = nullptr;
  if (state.inlined) {
    outer_state = GetFrameStateDescriptorInternal(
        zone, graph,
        graph->Get(state.parent_frame_state()).template Cast<FrameStateOp>());
  }

#if V8_ENABLE_WEBASSEMBLY
  if (state_info.type() == FrameStateType::kJSToWasmBuiltinContinuation) {
    auto function_info = static_cast<const JSToWasmFrameStateFunctionInfo*>(
        state_info.function_info());
    return zone->New<JSToWasmFrameStateDescriptor>(
        zone, state_info.type(), state_info.bailout_id(),
        state_info.state_combine(), parameters, locals, stack,
        state_info.shared_info(), outer_state, function_info->signature());
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  return zone->New<FrameStateDescriptor>(
      zone, state_info.type(), state_info.bailout_id(),
      state_info.state_combine(), parameters, max_arguments, locals, stack,
      state_info.shared_info(), state_info.bytecode_array(), outer_state,
      state_info.function_info()->wasm_liftoff_frame_size(),
      state_info.function_info()->wasm_function_index());
}

}  // namespace

FrameStateDescriptor* InstructionSelectorT::GetFrameStateDescriptor(
    OpIndex node) {
  const FrameStateOp& state =
      this->turboshaft_graph()->Get(node).template Cast<FrameStateOp>();
  auto* desc = GetFrameStateDescriptorInternal(instruction_zone(),
                                               this->turboshaft_graph(), state);
  *max_unoptimized_frame_height_ =
      std::max(*max_unoptimized_frame_height_,
               desc->total_conservative_frame_size_in_bytes() +
                   (desc->max_arguments() * kSystemPointerSize));
  return desc;
}

#if V8_ENABLE_WEBASSEMBLY
// static
void InstructionSelectorT::SwapShuffleInputs(
    TurboshaftAdapter::SimdShuffleView& view) {
  view.SwapInputs();
}
#endif  // V8_ENABLE_WEBASSEMBLY

InstructionSelector InstructionSelector::ForTurboshaft(
    Zone* zone, size_t node_count, Linkage* linkage,
    InstructionSequence* sequence, Graph* graph, Frame* frame,
    EnableSwitchJumpTable enable_switch_jump_table, TickCounter* tick_counter,
    JSHeapBroker* broker, size_t* max_unoptimized_frame_height,
    size_t* max_pushed_argument_count, SourcePositionMode source_position_mode,
    Features features, EnableScheduling enable_scheduling,
    EnableRootsRelativeAddressing enable_roots_relative_addressing,
    EnableTraceTurboJson trace_turbo) {
  return InstructionSelector(
      nullptr,
      new InstructionSelectorT(
          zone, node_count, linkage, sequence, graph,
          &graph->source_positions(), frame, enable_switch_jump_table,
          tick_counter, broker, max_unoptimized_frame_height,
          max_pushed_argument_count, source_position_mode, features,
          enable_scheduling, enable_roots_relative_addressing, trace_turbo));
}

InstructionSelector::InstructionSelector(std::nullptr_t,
                                         InstructionSelectorT* turboshaft_impl)
    : turboshaft_impl_(turboshaft_impl) {}

InstructionSelector::~InstructionSelector() { delete turboshaft_impl_; }

#define DISPATCH_TO_IMPL(...) return turboshaft_impl_->__VA_ARGS__;

std::optional<BailoutReason> InstructionSelector::SelectInstructions() {
  DISPATCH_TO_IMPL(SelectInstructions())
}

bool InstructionSelector::IsSupported(CpuFeature feature) const {
  DISPATCH_TO_IMPL(IsSupported(feature))
}

const ZoneVector<std::pair<int, int>>& InstructionSelector::instr_origins()
    const {
  DISPATCH_TO_IMPL(instr_origins())
}

const std::map<NodeId, int> InstructionSelector::GetVirtualRegistersForTesting()
    const {
  DISPATCH_TO_IMPL(GetVirtualRegistersForTesting());
}

#undef DISPATCH_TO_IMPL
#undef VISIT_UNSUPPORTED_OP

}  // namespace compiler
}  // namespace internal
}  // namespace v8
