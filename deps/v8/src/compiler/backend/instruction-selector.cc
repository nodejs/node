// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-selector.h"

#include <limits>

#include "src/base/iterator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/tick-counter.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/schedule.h"
#include "src/compiler/state-values-utils.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/simd-shuffle.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {
namespace compiler {

Smi NumberConstantToSmi(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kNumberConstant);
  const double d = OpParameter<double>(node->op());
  Smi smi = Smi::FromInt(static_cast<int32_t>(d));
  CHECK_EQ(smi.value(), d);
  return smi;
}

template <typename Adapter>
InstructionSelectorT<Adapter>::InstructionSelectorT(
    Zone* zone, size_t node_count, Linkage* linkage,
    InstructionSequence* sequence, Schedule* schedule,
    SourcePositionTable* source_positions, Frame* frame,
    EnableSwitchJumpTable enable_switch_jump_table, TickCounter* tick_counter,
    JSHeapBroker* broker, size_t* max_unoptimized_frame_height,
    size_t* max_pushed_argument_count, SourcePositionMode source_position_mode,
    Features features, EnableScheduling enable_scheduling,
    EnableRootsRelativeAddressing enable_roots_relative_addressing,
    EnableTraceTurboJson trace_turbo)
    : zone_(zone),
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
      phi_states_(node_count, Upper32BitsState::kNotYetChecked, zone)
#endif
{
  DCHECK_EQ(*max_unoptimized_frame_height, 0);  // Caller-initialized.

  instructions_.reserve(node_count);
  continuation_inputs_.reserve(5);
  continuation_outputs_.reserve(2);

  if (trace_turbo_ == kEnableTraceTurboJson) {
    instr_origins_.assign(node_count, {-1, 0});
  }
}

template <typename Adapter>
base::Optional<BailoutReason>
InstructionSelectorT<Adapter>::SelectInstructions() {
  // Mark the inputs of all phis in loop headers as used.
  BasicBlockVector* blocks = schedule()->rpo_order();
  for (auto const block : *blocks) {
    if (!block->IsLoopHeader()) continue;
    DCHECK_LE(2u, block->PredecessorCount());
    for (Node* const phi : *block) {
      if (phi->opcode() != IrOpcode::kPhi) continue;

      // Mark all inputs as used.
      for (Node* const input : phi->inputs()) {
        MarkAsUsed(input);
      }
    }
  }

  // Visit each basic block in post order.
  for (auto i = blocks->rbegin(); i != blocks->rend(); ++i) {
    VisitBlock(*i);
    if (instruction_selection_failed())
      return BailoutReason::kCodeGenerationFailed;
  }

  // Schedule the selected instructions.
  if (UseInstructionScheduling()) {
    scheduler_ = zone()->template New<InstructionScheduler>(zone(), sequence());
  }

  for (auto const block : *blocks) {
    InstructionBlock* instruction_block =
        sequence()->InstructionBlockAt(RpoNumber::FromInt(block->rpo_number()));
    for (size_t i = 0; i < instruction_block->phis().size(); i++) {
      UpdateRenamesInPhi(instruction_block->PhiAt(i));
    }
    size_t end = instruction_block->code_end();
    size_t start = instruction_block->code_start();
    DCHECK_LE(end, start);
    StartBlock(RpoNumber::FromInt(block->rpo_number()));
    if (end != start) {
      while (start-- > end + 1) {
        UpdateRenames(instructions_[start]);
        AddInstruction(instructions_[start]);
      }
      UpdateRenames(instructions_[end]);
      AddTerminator(instructions_[end]);
    }
    EndBlock(RpoNumber::FromInt(block->rpo_number()));
  }
#if DEBUG
  sequence()->ValidateSSA();
#endif
  return base::nullopt;
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::StartBlock(RpoNumber rpo) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->StartBlock(rpo);
  } else {
    sequence()->StartBlock(rpo);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::EndBlock(RpoNumber rpo) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->EndBlock(rpo);
  } else {
    sequence()->EndBlock(rpo);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::AddTerminator(Instruction* instr) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->AddTerminator(instr);
  } else {
    sequence()->AddInstruction(instr);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::AddInstruction(Instruction* instr) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->AddInstruction(instr);
  } else {
    sequence()->AddInstruction(instr);
  }
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(InstructionCode opcode,
                                                 InstructionOperand output,
                                                 size_t temp_count,
                                                 InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  return Emit(opcode, output_count, &output, 0, nullptr, temp_count, temps);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(InstructionCode opcode,
                                                 InstructionOperand output,
                                                 InstructionOperand a,
                                                 size_t temp_count,
                                                 InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  return Emit(opcode, output_count, &output, 1, &a, temp_count, temps);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, size_t temp_count,
    InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    InstructionOperand e, size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d, e};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(
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

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(
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

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::Emit(Instruction* instr) {
  instructions_.push_back(instr);
  return instr;
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::CanCover(Node* user, Node* node) const {
  // 1. Both {user} and {node} must be in the same basic block.
  if (schedule()->block(node) != current_block_) {
    return false;
  }
  // 2. Pure {node}s must be owned by the {user}.
  if (node->op()->HasProperty(Operator::kPure)) {
    return node->OwnedBy(user);
  }
  // 3. Impure {node}s must match the effect level of {user}.
  if (GetEffectLevel(node) != current_effect_level_) {
    return false;
  }
  // 4. Only {node} must have value edges pointing to {user}.
  for (Edge const edge : node->use_edges()) {
    if (edge.from() != user && NodeProperties::IsValueEdge(edge)) {
      return false;
    }
  }
  return true;
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::IsOnlyUserOfNodeInSameBlock(
    Node* user, Node* node) const {
  BasicBlock* bb_user = schedule()->block(user);
  BasicBlock* bb_node = schedule()->block(node);
  if (bb_user != bb_node) return false;
  for (Edge const edge : node->use_edges()) {
    Node* from = edge.from();
    if ((from != user) && (schedule()->block(from) == bb_user)) {
      return false;
    }
  }
  return true;
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::UpdateRenames(Instruction* instruction) {
  for (size_t i = 0; i < instruction->InputCount(); i++) {
    TryRename(instruction->InputAt(i));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::UpdateRenamesInPhi(PhiInstruction* phi) {
  for (size_t i = 0; i < phi->operands().size(); i++) {
    int vreg = phi->operands()[i];
    int renamed = GetRename(vreg);
    if (vreg != renamed) {
      phi->RenameInput(i, renamed);
    }
  }
}

template <typename Adapter>
int InstructionSelectorT<Adapter>::GetRename(int virtual_register) {
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

template <typename Adapter>
void InstructionSelectorT<Adapter>::TryRename(InstructionOperand* op) {
  if (!op->IsUnallocated()) return;
  UnallocatedOperand* unalloc = UnallocatedOperand::cast(op);
  int vreg = unalloc->virtual_register();
  int rename = GetRename(vreg);
  if (rename != vreg) {
    *unalloc = UnallocatedOperand(*unalloc, rename);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::SetRename(const Node* node,
                                              const Node* rename) {
  int vreg = GetVirtualRegister(node);
  if (static_cast<size_t>(vreg) >= virtual_register_rename_.size()) {
    int invalid = InstructionOperand::kInvalidVirtualRegister;
    virtual_register_rename_.resize(vreg + 1, invalid);
  }
  virtual_register_rename_[vreg] = GetVirtualRegister(rename);
}

template <typename Adapter>
int InstructionSelectorT<Adapter>::GetVirtualRegister(const Node* node) {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, virtual_registers_.size());
  int virtual_register = virtual_registers_[id];
  if (virtual_register == InstructionOperand::kInvalidVirtualRegister) {
    virtual_register = sequence()->NextVirtualRegister();
    virtual_registers_[id] = virtual_register;
  }
  return virtual_register;
}

template <typename Adapter>
const std::map<NodeId, int>
InstructionSelectorT<Adapter>::GetVirtualRegistersForTesting() const {
  std::map<NodeId, int> virtual_registers;
  for (size_t n = 0; n < virtual_registers_.size(); ++n) {
    if (virtual_registers_[n] != InstructionOperand::kInvalidVirtualRegister) {
      NodeId const id = static_cast<NodeId>(n);
      virtual_registers.insert(std::make_pair(id, virtual_registers_[n]));
    }
  }
  return virtual_registers;
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::IsDefined(Node* node) const {
  DCHECK_NOT_NULL(node);
  return defined_.Contains(node->id());
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::MarkAsDefined(Node* node) {
  DCHECK_NOT_NULL(node);
  defined_.Add(node->id());
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::IsUsed(Node* node) const {
  DCHECK_NOT_NULL(node);
  // TODO(bmeurer): This is a terrible monster hack, but we have to make sure
  // that the Retain is actually emitted, otherwise the GC will mess up.
  if (node->opcode() == IrOpcode::kRetain) return true;
  if (!node->op()->HasProperty(Operator::kEliminatable)) return true;
  return used_.Contains(node->id());
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::MarkAsUsed(Node* node) {
  DCHECK_NOT_NULL(node);
  used_.Add(node->id());
}

template <typename Adapter>
int InstructionSelectorT<Adapter>::GetEffectLevel(Node* node) const {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, effect_level_.size());
  return effect_level_[id];
}

template <typename Adapter>
int InstructionSelectorT<Adapter>::GetEffectLevel(
    Node* node, FlagsContinuation* cont) const {
  return cont->IsBranch()
             ? GetEffectLevel(
                   cont->true_block()->PredecessorAt(0)->control_input())
             : GetEffectLevel(node);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::SetEffectLevel(Node* node,
                                                   int effect_level) {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, effect_level_.size());
  effect_level_[id] = effect_level;
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::CanAddressRelativeToRootsRegister(
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
      (enable_roots_relative_addressing_ == kEnableRootsRelativeAddressing);
  if (all_root_relative_offsets_are_constant) return true;

  // 3. IsAddressableThroughRootRegister: Is the target address guaranteed to
  //    have a fixed root-relative offset? If so, we can ignore 2.
  const bool this_root_relative_offset_is_constant =
      MacroAssemblerBase::IsAddressableThroughRootRegister(isolate(),
                                                           reference);
  return this_root_relative_offset_is_constant;
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::CanUseRootsRegister() const {
  return linkage()->GetIncomingDescriptor()->flags() &
         CallDescriptor::kCanUseRoots;
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::MarkAsRepresentation(
    MachineRepresentation rep, const InstructionOperand& op) {
  UnallocatedOperand unalloc = UnallocatedOperand::cast(op);
  sequence()->MarkAsRepresentation(rep, unalloc.virtual_register());
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::MarkAsRepresentation(
    MachineRepresentation rep, Node* node) {
  sequence()->MarkAsRepresentation(rep, GetVirtualRegister(node));
}

namespace {

template <typename Adapter>
InstructionOperand OperandForDeopt(Isolate* isolate,
                                   OperandGeneratorT<Adapter>* g, Node* input,
                                   FrameStateInputKind kind,
                                   MachineRepresentation rep) {
  if (rep == MachineRepresentation::kNone) {
    return g->TempImmediate(FrameStateDescriptor::kImpossibleValue);
  }

  switch (input->opcode()) {
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat32Constant:
    case IrOpcode::kFloat64Constant:
      return g->UseImmediate(input);
    case IrOpcode::kNumberConstant:
      if (rep == MachineRepresentation::kWord32) {
        Smi smi = NumberConstantToSmi(input);
        return g->UseImmediate(static_cast<int32_t>(smi.ptr()));
      } else {
        return g->UseImmediate(input);
      }
    case IrOpcode::kCompressedHeapConstant:
    case IrOpcode::kHeapConstant: {
      if (!CanBeTaggedOrCompressedPointer(rep)) {
        // If we have inconsistent static and dynamic types, e.g. if we
        // smi-check a string, we can get here with a heap object that
        // says it is a smi. In that case, we return an invalid instruction
        // operand, which will be interpreted as an optimized-out value.

        // TODO(jarin) Ideally, we should turn the current instruction
        // into an abort (we should never execute it).
        return InstructionOperand();
      }

      Handle<HeapObject> constant = HeapConstantOf(input->op());
      RootIndex root_index;
      if (isolate->roots_table().IsRootHandle(constant, &root_index) &&
          root_index == RootIndex::kOptimizedOut) {
        // For an optimized-out object we return an invalid instruction
        // operand, so that we take the fast path for optimized-out values.
        return InstructionOperand();
      }

      return g->UseImmediate(input);
    }
    case IrOpcode::kArgumentsElementsState:
    case IrOpcode::kArgumentsLengthState:
    case IrOpcode::kObjectState:
    case IrOpcode::kTypedObjectState:
      UNREACHABLE();
    default:
      switch (kind) {
        case FrameStateInputKind::kStackSlot:
          return g->UseUniqueSlot(input);
        case FrameStateInputKind::kAny:
          // Currently deopts "wrap" other operations, so the deopt's inputs
          // are potentially needed until the end of the deoptimising code.
          return g->UseAnyAtEnd(input);
      }
  }
  UNREACHABLE();
}

}  // namespace

class StateObjectDeduplicator {
 public:
  explicit StateObjectDeduplicator(Zone* zone) : objects_(zone) {}
  static const size_t kNotDuplicated = SIZE_MAX;

  size_t GetObjectId(Node* node) {
    DCHECK(node->opcode() == IrOpcode::kTypedObjectState ||
           node->opcode() == IrOpcode::kObjectId ||
           node->opcode() == IrOpcode::kArgumentsElementsState);
    for (size_t i = 0; i < objects_.size(); ++i) {
      if (objects_[i] == node) return i;
      // ObjectId nodes are the Turbofan way to express objects with the same
      // identity in the deopt info. So they should always be mapped to
      // previously appearing TypedObjectState nodes.
      if (HasObjectId(objects_[i]) && HasObjectId(node) &&
          ObjectIdOf(objects_[i]->op()) == ObjectIdOf(node->op())) {
        return i;
      }
    }
    DCHECK(node->opcode() == IrOpcode::kTypedObjectState ||
           node->opcode() == IrOpcode::kArgumentsElementsState);
    return kNotDuplicated;
  }

  size_t InsertObject(Node* node) {
    DCHECK(node->opcode() == IrOpcode::kTypedObjectState ||
           node->opcode() == IrOpcode::kObjectId ||
           node->opcode() == IrOpcode::kArgumentsElementsState);
    size_t id = objects_.size();
    objects_.push_back(node);
    return id;
  }

  size_t size() const { return objects_.size(); }

 private:
  static bool HasObjectId(Node* node) {
    return node->opcode() == IrOpcode::kTypedObjectState ||
           node->opcode() == IrOpcode::kObjectId;
  }

  ZoneVector<Node*> objects_;
};

// Returns the number of instruction operands added to inputs.
template <typename Adapter>
size_t InstructionSelectorT<Adapter>::AddOperandToStateValueDescriptor(
    StateValueList* values, InstructionOperandVector* inputs,
    OperandGenerator* g, StateObjectDeduplicator* deduplicator, Node* input,
    MachineType type, FrameStateInputKind kind, Zone* zone) {
  DCHECK_NOT_NULL(input);
  switch (input->opcode()) {
    case IrOpcode::kArgumentsElementsState: {
      values->PushArgumentsElements(ArgumentsStateTypeOf(input->op()));
      // The elements backing store of an arguments object participates in the
      // duplicate object counting, but can itself never appear duplicated.
      DCHECK_EQ(StateObjectDeduplicator::kNotDuplicated,
                deduplicator->GetObjectId(input));
      deduplicator->InsertObject(input);
      return 0;
    }
    case IrOpcode::kArgumentsLengthState: {
      values->PushArgumentsLength();
      return 0;
    }
    case IrOpcode::kObjectState:
      UNREACHABLE();
    case IrOpcode::kTypedObjectState:
    case IrOpcode::kObjectId: {
      size_t id = deduplicator->GetObjectId(input);
      if (id == StateObjectDeduplicator::kNotDuplicated) {
        DCHECK_EQ(IrOpcode::kTypedObjectState, input->opcode());
        size_t entries = 0;
        id = deduplicator->InsertObject(input);
        StateValueList* nested = values->PushRecursiveField(zone, id);
        int const input_count = input->op()->ValueInputCount();
        ZoneVector<MachineType> const* types = MachineTypesOf(input->op());
        for (int i = 0; i < input_count; ++i) {
          entries += AddOperandToStateValueDescriptor(
              nested, inputs, g, deduplicator, input->InputAt(i), types->at(i),
              kind, zone);
        }
        return entries;
      } else {
        // Deoptimizer counts duplicate objects for the running id, so we have
        // to push the input again.
        deduplicator->InsertObject(input);
        values->PushDuplicate(id);
        return 0;
      }
    }
    default: {
      InstructionOperand op =
          OperandForDeopt(isolate(), g, input, kind, type.representation());
      if (op.kind() == InstructionOperand::INVALID) {
        // Invalid operand means the value is impossible or optimized-out.
        values->PushOptimizedOut();
        return 0;
      } else {
        inputs->push_back(op);
        values->PushPlain(type);
        return 1;
      }
    }
  }
}

template <typename Adapter>
struct InstructionSelectorT<Adapter>::CachedStateValues : public ZoneObject {
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

template <typename Adapter>
class InstructionSelectorT<Adapter>::CachedStateValuesBuilder {
 public:
  explicit CachedStateValuesBuilder(StateValueList* values,
                                    InstructionOperandVector* inputs,
                                    StateObjectDeduplicator* deduplicator)
      : values_(values),
        inputs_(inputs),
        deduplicator_(deduplicator),
        values_start_(values->size()),
        nested_start_(values->nested_count()),
        inputs_start_(inputs->size()),
        deduplicator_start_(deduplicator->size()) {}

  // We can only build a CachedStateValues for a StateValue if it didn't update
  // any of the ids in the deduplicator.
  bool CanCache() const { return deduplicator_->size() == deduplicator_start_; }

  InstructionSelectorT<Adapter>::CachedStateValues* Build(Zone* zone) {
    DCHECK(CanCache());
    DCHECK(values_->nested_count() == nested_start_);
    return zone->New<InstructionSelectorT<Adapter>::CachedStateValues>(
        zone, values_, values_start_, inputs_, inputs_start_);
  }

 private:
  StateValueList* values_;
  InstructionOperandVector* inputs_;
  StateObjectDeduplicator* deduplicator_;
  size_t values_start_;
  size_t nested_start_;
  size_t inputs_start_;
  size_t deduplicator_start_;
};

template <typename Adapter>
size_t InstructionSelectorT<Adapter>::AddInputsToFrameStateDescriptor(
    StateValueList* values, InstructionOperandVector* inputs,
    OperandGenerator* g, StateObjectDeduplicator* deduplicator, Node* node,
    FrameStateInputKind kind, Zone* zone) {
  // StateValues are often shared across different nodes, and processing them is
  // expensive, so cache the result of processing a StateValue so that we can
  // quickly copy the result if we see it again.
  FrameStateInput key(node, kind);
  auto cache_entry = state_values_cache_.find(key);
  if (cache_entry != state_values_cache_.end()) {
    // Entry found in cache, emit cached version.
    return cache_entry->second->Emit(inputs, values);
  } else {
    // Not found in cache, generate and then store in cache if possible.
    size_t entries = 0;
    CachedStateValuesBuilder cache_builder(values, inputs, deduplicator);
    StateValuesAccess::iterator it = StateValuesAccess(node).begin();
    // Take advantage of sparse nature of StateValuesAccess to skip over
    // multiple empty nodes at once pushing repeated OptimizedOuts all in one
    // go.
    while (!it.done()) {
      values->PushOptimizedOut(it.AdvanceTillNotEmpty());
      if (it.done()) break;
      StateValuesAccess::TypedNode input_node = *it;
      entries += AddOperandToStateValueDescriptor(values, inputs, g,
                                                  deduplicator, input_node.node,
                                                  input_node.type, kind, zone);
      ++it;
    }
    if (cache_builder.CanCache()) {
      // Use this->zone() to build the cache entry in the instruction selector's
      // zone rather than the more long-lived instruction zone.
      state_values_cache_.emplace(key, cache_builder.Build(this->zone()));
    }
    return entries;
  }
}

// Returns the number of instruction operands added to inputs.
template <typename Adapter>
size_t InstructionSelectorT<Adapter>::AddInputsToFrameStateDescriptor(
    FrameStateDescriptor* descriptor, FrameState state, OperandGenerator* g,
    StateObjectDeduplicator* deduplicator, InstructionOperandVector* inputs,
    FrameStateInputKind kind, Zone* zone) {
  size_t entries = 0;
  size_t initial_size = inputs->size();
  USE(initial_size);  // initial_size is only used for debug.

  if (descriptor->outer_state()) {
    entries += AddInputsToFrameStateDescriptor(
        descriptor->outer_state(), FrameState{state.outer_frame_state()}, g,
        deduplicator, inputs, kind, zone);
  }

  Node* parameters = state.parameters();
  Node* locals = state.locals();
  Node* stack = state.stack();
  Node* context = state.context();
  Node* function = state.function();

  DCHECK_EQ(descriptor->parameters_count(),
            StateValuesAccess(parameters).size());
  DCHECK_EQ(descriptor->locals_count(), StateValuesAccess(locals).size());
  DCHECK_EQ(descriptor->stack_count(), StateValuesAccess(stack).size());

  StateValueList* values_descriptor = descriptor->GetStateValueDescriptors();

  DCHECK_EQ(values_descriptor->size(), 0u);
  values_descriptor->ReserveSize(descriptor->GetSize());

  DCHECK_NOT_NULL(function);
  entries += AddOperandToStateValueDescriptor(
      values_descriptor, inputs, g, deduplicator, function,
      MachineType::AnyTagged(), FrameStateInputKind::kStackSlot, zone);

  entries += AddInputsToFrameStateDescriptor(
      values_descriptor, inputs, g, deduplicator, parameters, kind, zone);

  if (descriptor->HasContext()) {
    DCHECK_NOT_NULL(context);
    entries += AddOperandToStateValueDescriptor(
        values_descriptor, inputs, g, deduplicator, context,
        MachineType::AnyTagged(), FrameStateInputKind::kStackSlot, zone);
  }

  entries += AddInputsToFrameStateDescriptor(values_descriptor, inputs, g,
                                             deduplicator, locals, kind, zone);
  entries += AddInputsToFrameStateDescriptor(values_descriptor, inputs, g,
                                             deduplicator, stack, kind, zone);
  DCHECK_EQ(initial_size + entries, inputs->size());
  return entries;
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::EmitWithContinuation(
    InstructionCode opcode, InstructionOperand a, FlagsContinuation* cont) {
  return EmitWithContinuation(opcode, 0, nullptr, 1, &a, cont);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::EmitWithContinuation(
    InstructionCode opcode, InstructionOperand a, InstructionOperand b,
    FlagsContinuation* cont) {
  InstructionOperand inputs[] = {a, b};
  return EmitWithContinuation(opcode, 0, nullptr, arraysize(inputs), inputs,
                              cont);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::EmitWithContinuation(
    InstructionCode opcode, InstructionOperand a, InstructionOperand b,
    InstructionOperand c, FlagsContinuation* cont) {
  InstructionOperand inputs[] = {a, b, c};
  return EmitWithContinuation(opcode, 0, nullptr, arraysize(inputs), inputs,
                              cont);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::EmitWithContinuation(
    InstructionCode opcode, size_t output_count, InstructionOperand* outputs,
    size_t input_count, InstructionOperand* inputs, FlagsContinuation* cont) {
  return EmitWithContinuation(opcode, output_count, outputs, input_count,
                              inputs, 0, nullptr, cont);
}

template <typename Adapter>
Instruction* InstructionSelectorT<Adapter>::EmitWithContinuation(
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

  if (cont->IsBranch()) {
    continuation_inputs_.push_back(g.Label(cont->true_block()));
    continuation_inputs_.push_back(g.Label(cont->false_block()));
  } else if (cont->IsDeoptimize()) {
    int immediate_args_count = 0;
    opcode |= DeoptImmedArgsCountField::encode(immediate_args_count) |
              DeoptFrameStateOffsetField::encode(static_cast<int>(input_count));
    AppendDeoptimizeArguments(&continuation_inputs_, cont->reason(),
                              cont->node_id(), cont->feedback(),
                              FrameState{cont->frame_state()});
  } else if (cont->IsSet()) {
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

template <typename Adapter>
void InstructionSelectorT<Adapter>::AppendDeoptimizeArguments(
    InstructionOperandVector* args, DeoptimizeReason reason, NodeId node_id,
    FeedbackSource const& feedback, FrameState frame_state,
    DeoptimizeKind kind) {
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
struct CallBuffer {
  CallBuffer(Zone* zone, const CallDescriptor* call_descriptor,
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
  ZoneVector<PushParameter> output_nodes;
  InstructionOperandVector outputs;
  InstructionOperandVector instruction_args;
  ZoneVector<PushParameter> pushed_nodes;

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
template <typename Adapter>
void InstructionSelectorT<Adapter>::InitializeCallBuffer(
    Node* call, CallBuffer* buffer, CallBufferFlags flags,
    int stack_param_delta) {
  OperandGenerator g(this);
  size_t ret_count = buffer->descriptor->ReturnCount();
  bool is_tail_call = (flags & kCallTail) != 0;
  DCHECK_LE(call->op()->ValueOutputCount(), ret_count);
  DCHECK_EQ(
      call->op()->ValueInputCount(),
      static_cast<int>(buffer->input_count() + buffer->frame_state_count()));

  if (ret_count > 0) {
    // Collect the projections that represent multiple outputs from this call.
    if (ret_count == 1) {
      PushParameter result = {call, buffer->descriptor->GetReturnLocation(0)};
      buffer->output_nodes.push_back(result);
    } else {
      buffer->output_nodes.resize(ret_count);
      for (size_t i = 0; i < ret_count; ++i) {
        LinkageLocation location = buffer->descriptor->GetReturnLocation(i);
        buffer->output_nodes[i] = PushParameter(nullptr, location);
      }
      for (Edge const edge : call->use_edges()) {
        if (!NodeProperties::IsValueEdge(edge)) continue;
        Node* node = edge.from();
        DCHECK_EQ(IrOpcode::kProjection, node->opcode());
        size_t const index = ProjectionIndexOf(node->op());

        DCHECK_LT(index, buffer->output_nodes.size());
        DCHECK(!buffer->output_nodes[index].node);
        buffer->output_nodes[index].node = node;
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
      bool output_is_live = buffer->output_nodes[i].node != nullptr ||
                            i < outputs_needed_by_framestate;
      if (output_is_live) {
        LinkageLocation location = buffer->output_nodes[i].location;
        MachineRepresentation rep = location.GetType().representation();

        Node* output = buffer->output_nodes[i].node;
        InstructionOperand op = output == nullptr
                                    ? g.TempLocation(location)
                                    : g.DefineAsLocation(output, location);
        MarkAsRepresentation(rep, op);

        if (!UnallocatedOperand::cast(op).HasFixedSlotPolicy()) {
          buffer->outputs.push_back(op);
          buffer->output_nodes[i].node = nullptr;
        }
      }
    }
  }

  // The first argument is always the callee code.
  Node* callee = call->InputAt(0);
  bool call_code_immediate = (flags & kCallCodeImmediate) != 0;
  bool call_address_immediate = (flags & kCallAddressImmediate) != 0;
  bool call_use_fixed_target_reg = (flags & kCallFixedTargetRegister) != 0;
  switch (buffer->descriptor->kind()) {
    case CallDescriptor::kCallCodeObject:
      buffer->instruction_args.push_back(
          (call_code_immediate && callee->opcode() == IrOpcode::kHeapConstant)
              ? g.UseImmediate(callee)
              : call_use_fixed_target_reg
                    ? g.UseFixed(callee, kJavaScriptCallCodeStartRegister)
                    : g.UseRegister(callee));
      break;
    case CallDescriptor::kCallAddress:
      buffer->instruction_args.push_back(
          (call_address_immediate &&
           callee->opcode() == IrOpcode::kExternalConstant)
              ? g.UseImmediate(callee)
              : call_use_fixed_target_reg
                    ? g.UseFixed(callee, kJavaScriptCallCodeStartRegister)
                    : g.UseRegister(callee));
      break;
#if V8_ENABLE_WEBASSEMBLY
    case CallDescriptor::kCallWasmCapiFunction:
    case CallDescriptor::kCallWasmFunction:
    case CallDescriptor::kCallWasmImportWrapper:
      buffer->instruction_args.push_back(
          (call_address_immediate &&
           (callee->opcode() == IrOpcode::kRelocatableInt64Constant ||
            callee->opcode() == IrOpcode::kRelocatableInt32Constant))
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
    FrameState frame_state{
        call->InputAt(static_cast<int>(buffer->descriptor->InputCount()))};

    // If it was a syntactic tail call we need to drop the current frame and
    // all the frames on top of it that are either inlined extra arguments
    // or a tail caller frame.
    if (is_tail_call) {
      frame_state = FrameState{NodeProperties::GetFrameStateInput(frame_state)};
      buffer->frame_state_descriptor =
          buffer->frame_state_descriptor->outer_state();
      while (buffer->frame_state_descriptor != nullptr &&
             buffer->frame_state_descriptor->type() ==
                 FrameStateType::kInlinedExtraArguments) {
        frame_state =
            FrameState{NodeProperties::GetFrameStateInput(frame_state)};
        buffer->frame_state_descriptor =
            buffer->frame_state_descriptor->outer_state();
      }
    }

    int const state_id = sequence()->AddDeoptimizationEntry(
        buffer->frame_state_descriptor, DeoptimizeKind::kLazy,
        DeoptimizeReason::kUnknown, call->id(), FeedbackSource());
    buffer->instruction_args.push_back(g.TempImmediate(state_id));

    StateObjectDeduplicator deduplicator(instruction_zone());

    frame_state_entries =
        1 + AddInputsToFrameStateDescriptor(
                buffer->frame_state_descriptor, frame_state, &g, &deduplicator,
                &buffer->instruction_args, FrameStateInputKind::kStackSlot,
                instruction_zone());

    DCHECK_EQ(1 + frame_state_entries, buffer->instruction_args.size());
  }

  size_t input_count = static_cast<size_t>(buffer->input_count());

  // Split the arguments into pushed_nodes and instruction_args. Pushed
  // arguments require an explicit push instruction before the call and do
  // not appear as arguments to the call. Everything else ends up
  // as an InstructionOperand argument to the call.
  auto iter(call->inputs().begin());
  size_t pushed_count = 0;
  for (size_t index = 0; index < input_count; ++iter, ++index) {
    DCHECK(iter != call->inputs().end());
    DCHECK_NE(IrOpcode::kFrameState, (*iter)->op()->opcode());
    if (index == 0) continue;  // The first argument (callee) is already done.

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

template <typename Adapter>
bool InstructionSelectorT<Adapter>::IsSourcePositionUsed(Node* node) {
  return (source_position_mode_ == kAllSourcePositions ||
          node->opcode() == IrOpcode::kCall ||
          node->opcode() == IrOpcode::kTrapIf ||
          node->opcode() == IrOpcode::kTrapUnless ||
          node->opcode() == IrOpcode::kProtectedLoad ||
          node->opcode() == IrOpcode::kProtectedStore ||
          node->opcode() == IrOpcode::kLoadTrapOnNull ||
          node->opcode() == IrOpcode::kStoreTrapOnNull);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBlock(BasicBlock* block) {
  DCHECK(!current_block_);
  current_block_ = block;
  auto current_num_instructions = [&] {
    DCHECK_GE(kMaxInt, instructions_.size());
    return static_cast<int>(instructions_.size());
  };
  int current_block_end = current_num_instructions();

  int effect_level = 0;
  for (Node* const node : *block) {
    SetEffectLevel(node, effect_level);
    current_effect_level_ = effect_level;
    if (node->opcode() == IrOpcode::kStore ||
        node->opcode() == IrOpcode::kUnalignedStore ||
        node->opcode() == IrOpcode::kCall ||
        node->opcode() == IrOpcode::kProtectedStore ||
        node->opcode() == IrOpcode::kStoreTrapOnNull ||
#define ADD_EFFECT_FOR_ATOMIC_OP(Opcode) \
  node->opcode() == IrOpcode::k##Opcode ||
        MACHINE_ATOMIC_OP_LIST(ADD_EFFECT_FOR_ATOMIC_OP)
#undef ADD_EFFECT_FOR_ATOMIC_OP
                node->opcode() == IrOpcode::kMemoryBarrier) {
      ++effect_level;
    }
  }

  // We visit the control first, then the nodes in the block, so the block's
  // control input should be on the same effect level as the last node.
  if (block->control_input() != nullptr) {
    SetEffectLevel(block->control_input(), effect_level);
    current_effect_level_ = effect_level;
  }

  auto FinishEmittedInstructions = [&](Node* node, int instruction_start) {
    if (instruction_selection_failed()) return false;
    if (current_num_instructions() == instruction_start) return true;
    std::reverse(instructions_.begin() + instruction_start,
                 instructions_.end());
    if (!node) return true;
    if (!source_positions_) return true;
    SourcePosition source_position = source_positions_->GetSourcePosition(node);
    if (source_position.IsKnown() && IsSourcePositionUsed(node)) {
      sequence()->SetSourcePosition(instructions_.back(), source_position);
    }
    return true;
  };

  // Generate code for the block control "top down", but schedule the code
  // "bottom up".
  VisitControl(block);
  if (!FinishEmittedInstructions(block->control_input(), current_block_end)) {
    return;
  }

  // Visit code in reverse control flow order, because architecture-specific
  // matching may cover more than one node at a time.
  for (auto node : base::Reversed(*block)) {
    int current_node_end = current_num_instructions();
    // Skip nodes that are unused or already defined.
    if (IsUsed(node) && !IsDefined(node)) {
      // Generate code for this node "top down", but schedule the code "bottom
      // up".
      VisitNode(node);
      if (!FinishEmittedInstructions(node, current_node_end)) return;
    }
    if (trace_turbo_ == kEnableTraceTurboJson) {
      instr_origins_[node->id()] = {current_num_instructions(),
                                    current_node_end};
    }
  }

  // We're done with the block.
  InstructionBlock* instruction_block =
      sequence()->InstructionBlockAt(RpoNumber::FromInt(block->rpo_number()));
  if (current_num_instructions() == current_block_end) {
    // Avoid empty block: insert a {kArchNop} instruction.
    Emit(Instruction::New(sequence()->zone(), kArchNop));
  }
  instruction_block->set_code_start(current_num_instructions());
  instruction_block->set_code_end(current_block_end);
  current_block_ = nullptr;
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitControl(BasicBlock* block) {
#ifdef DEBUG
  // SSA deconstruction requires targets of branches not to have phis.
  // Edge split form guarantees this property, but is more strict.
  if (block->SuccessorCount() > 1) {
    for (BasicBlock* const successor : block->successors()) {
      for (Node* const node : *successor) {
        if (IrOpcode::IsPhiOpcode(node->opcode())) {
          std::ostringstream str;
          str << "You might have specified merged variables for a label with "
              << "only one predecessor." << std::endl
              << "# Current Block: " << *successor << std::endl
              << "#          Node: " << *node;
          FATAL("%s", str.str().c_str());
        }
      }
    }
  }
#endif

  Node* input = block->control_input();
  int instruction_end = static_cast<int>(instructions_.size());
  switch (block->control()) {
    case BasicBlock::kGoto:
      VisitGoto(block->SuccessorAt(0));
      break;
    case BasicBlock::kCall: {
      DCHECK_EQ(IrOpcode::kCall, input->opcode());
      BasicBlock* success = block->SuccessorAt(0);
      BasicBlock* exception = block->SuccessorAt(1);
      VisitCall(input, exception);
      VisitGoto(success);
      break;
    }
    case BasicBlock::kTailCall: {
      DCHECK_EQ(IrOpcode::kTailCall, input->opcode());
      VisitTailCall(input);
      break;
    }
    case BasicBlock::kBranch: {
      DCHECK_EQ(IrOpcode::kBranch, input->opcode());
      // TODO(nicohartmann@): Once all branches have explicitly specified
      // semantics, we should allow only BranchSemantics::kMachine here.
      DCHECK_NE(BranchSemantics::kJS,
                BranchParametersOf(input->op()).semantics());
      BasicBlock* tbranch = block->SuccessorAt(0);
      BasicBlock* fbranch = block->SuccessorAt(1);
      if (tbranch == fbranch) {
        VisitGoto(tbranch);
      } else {
        VisitBranch(input, tbranch, fbranch);
      }
      break;
    }
    case BasicBlock::kSwitch: {
      DCHECK_EQ(IrOpcode::kSwitch, input->opcode());
      // Last successor must be {IfDefault}.
      BasicBlock* default_branch = block->successors().back();
      DCHECK_EQ(IrOpcode::kIfDefault, default_branch->front()->opcode());
      // All other successors must be {IfValue}s.
      int32_t min_value = std::numeric_limits<int32_t>::max();
      int32_t max_value = std::numeric_limits<int32_t>::min();
      size_t case_count = block->SuccessorCount() - 1;
      ZoneVector<CaseInfo> cases(case_count, zone());
      for (size_t i = 0; i < case_count; ++i) {
        BasicBlock* branch = block->SuccessorAt(i);
        const IfValueParameters& p = IfValueParametersOf(branch->front()->op());
        cases[i] = CaseInfo{p.value(), p.comparison_order(), branch};
        if (min_value > p.value()) min_value = p.value();
        if (max_value < p.value()) max_value = p.value();
      }
      SwitchInfo sw(cases, min_value, max_value, default_branch);
      VisitSwitch(input, sw);
      break;
    }
    case BasicBlock::kReturn: {
      DCHECK_EQ(IrOpcode::kReturn, input->opcode());
      VisitReturn(input);
      break;
    }
    case BasicBlock::kDeoptimize: {
      DeoptimizeParameters p = DeoptimizeParametersOf(input->op());
      FrameState value{input->InputAt(0)};
      VisitDeoptimize(p.reason(), input->id(), p.feedback(), value);
      break;
    }
    case BasicBlock::kThrow:
      DCHECK_EQ(IrOpcode::kThrow, input->opcode());
      VisitThrow(input);
      break;
    case BasicBlock::kNone: {
      // Exit block doesn't have control.
      DCHECK_NULL(input);
      break;
    }
    default:
      UNREACHABLE();
  }
  if (trace_turbo_ == kEnableTraceTurboJson && input) {
    int instruction_start = static_cast<int>(instructions_.size());
    instr_origins_[input->id()] = {instruction_start, instruction_end};
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::MarkPairProjectionsAsWord32(Node* node) {
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  if (projection0) {
    MarkAsWord32(projection0);
  }
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    MarkAsWord32(projection1);
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitNode(Node* node) {
  tick_counter_->TickAndMaybeEnterSafepoint();
  DCHECK_NOT_NULL(schedule()->block(node));  // should only use scheduled nodes.
  switch (node->opcode()) {
    case IrOpcode::kTraceInstruction:
#if V8_TARGET_ARCH_X64
      return VisitTraceInstruction(node);
#else
      return;
#endif
    case IrOpcode::kStart:
    case IrOpcode::kLoop:
    case IrOpcode::kEnd:
    case IrOpcode::kBranch:
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
    case IrOpcode::kIfSuccess:
    case IrOpcode::kSwitch:
    case IrOpcode::kIfValue:
    case IrOpcode::kIfDefault:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kMerge:
    case IrOpcode::kTerminate:
    case IrOpcode::kBeginRegion:
      // No code needed for these graph artifacts.
      return;
    case IrOpcode::kIfException:
      return MarkAsTagged(node), VisitIfException(node);
    case IrOpcode::kFinishRegion:
      return MarkAsTagged(node), VisitFinishRegion(node);
    case IrOpcode::kParameter: {
      // Parameters should always be scheduled to the first block.
      DCHECK_EQ(schedule()->block(node)->rpo_number(), 0);
      MachineType type =
          linkage()->GetParameterType(ParameterIndexOf(node->op()));
      MarkAsRepresentation(type.representation(), node);
      return VisitParameter(node);
    }
    case IrOpcode::kOsrValue:
      return MarkAsTagged(node), VisitOsrValue(node);
    case IrOpcode::kPhi: {
      MachineRepresentation rep = PhiRepresentationOf(node->op());
      if (rep == MachineRepresentation::kNone) return;
      MarkAsRepresentation(rep, node);
      return VisitPhi(node);
    }
    case IrOpcode::kProjection:
      return VisitProjection(node);
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kTaggedIndexConstant:
    case IrOpcode::kExternalConstant:
    case IrOpcode::kRelocatableInt32Constant:
    case IrOpcode::kRelocatableInt64Constant:
      return VisitConstant(node);
    case IrOpcode::kFloat32Constant:
      return MarkAsFloat32(node), VisitConstant(node);
    case IrOpcode::kFloat64Constant:
      return MarkAsFloat64(node), VisitConstant(node);
    case IrOpcode::kHeapConstant:
      return MarkAsTagged(node), VisitConstant(node);
    case IrOpcode::kCompressedHeapConstant:
      return MarkAsCompressed(node), VisitConstant(node);
    case IrOpcode::kNumberConstant: {
      double value = OpParameter<double>(node->op());
      if (!IsSmiDouble(value)) MarkAsTagged(node);
      return VisitConstant(node);
    }
    case IrOpcode::kCall:
      return VisitCall(node);
    case IrOpcode::kDeoptimizeIf:
      return VisitDeoptimizeIf(node);
    case IrOpcode::kDeoptimizeUnless:
      return VisitDeoptimizeUnless(node);
    case IrOpcode::kTrapIf:
      return VisitTrapIf(node, TrapIdOf(node->op()));
    case IrOpcode::kTrapUnless:
      return VisitTrapUnless(node, TrapIdOf(node->op()));
    case IrOpcode::kFrameState:
    case IrOpcode::kStateValues:
    case IrOpcode::kObjectState:
      return;
    case IrOpcode::kAbortCSADcheck:
      VisitAbortCSADcheck(node);
      return;
    case IrOpcode::kDebugBreak:
      VisitDebugBreak(node);
      return;
    case IrOpcode::kUnreachable:
      VisitUnreachable(node);
      return;
    case IrOpcode::kStaticAssert:
      VisitStaticAssert(node);
      return;
    case IrOpcode::kDeadValue:
      VisitDeadValue(node);
      return;
    case IrOpcode::kComment:
      VisitComment(node);
      return;
    case IrOpcode::kRetain:
      VisitRetain(node);
      return;
    case IrOpcode::kLoad:
    case IrOpcode::kLoadImmutable: {
      LoadRepresentation type = LoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitLoad(node);
    }
    case IrOpcode::kLoadTransform: {
      LoadTransformParameters params = LoadTransformParametersOf(node->op());
      if (params.transformation == LoadTransformation::kS256Load32Splat ||
          params.transformation == LoadTransformation::kS256Load64Splat) {
        MarkAsRepresentation(MachineRepresentation::kSimd256, node);
      } else {
        MarkAsRepresentation(MachineRepresentation::kSimd128, node);
      }
      return VisitLoadTransform(node);
    }
    case IrOpcode::kLoadLane: {
      MarkAsRepresentation(MachineRepresentation::kSimd128, node);
      return VisitLoadLane(node);
    }
    case IrOpcode::kStore:
      return VisitStore(node);
    case IrOpcode::kStorePair:
      return VisitStorePair(node);
    case IrOpcode::kProtectedStore:
    case IrOpcode::kStoreTrapOnNull:
      return VisitProtectedStore(node);
    case IrOpcode::kStoreLane: {
      MarkAsRepresentation(MachineRepresentation::kSimd128, node);
      return VisitStoreLane(node);
    }
    case IrOpcode::kWord32And:
      return MarkAsWord32(node), VisitWord32And(node);
    case IrOpcode::kWord32Or:
      return MarkAsWord32(node), VisitWord32Or(node);
    case IrOpcode::kWord32Xor:
      return MarkAsWord32(node), VisitWord32Xor(node);
    case IrOpcode::kWord32Shl:
      return MarkAsWord32(node), VisitWord32Shl(node);
    case IrOpcode::kWord32Shr:
      return MarkAsWord32(node), VisitWord32Shr(node);
    case IrOpcode::kWord32Sar:
      return MarkAsWord32(node), VisitWord32Sar(node);
    case IrOpcode::kWord32Rol:
      return MarkAsWord32(node), VisitWord32Rol(node);
    case IrOpcode::kWord32Ror:
      return MarkAsWord32(node), VisitWord32Ror(node);
    case IrOpcode::kWord32Equal:
      return VisitWord32Equal(node);
    case IrOpcode::kWord32Clz:
      return MarkAsWord32(node), VisitWord32Clz(node);
    case IrOpcode::kWord32Ctz:
      return MarkAsWord32(node), VisitWord32Ctz(node);
    case IrOpcode::kWord32ReverseBits:
      return MarkAsWord32(node), VisitWord32ReverseBits(node);
    case IrOpcode::kWord32ReverseBytes:
      return MarkAsWord32(node), VisitWord32ReverseBytes(node);
    case IrOpcode::kInt32AbsWithOverflow:
      return MarkAsWord32(node), VisitInt32AbsWithOverflow(node);
    case IrOpcode::kWord32Popcnt:
      return MarkAsWord32(node), VisitWord32Popcnt(node);
    case IrOpcode::kWord64Popcnt:
      return MarkAsWord32(node), VisitWord64Popcnt(node);
    case IrOpcode::kWord32Select:
      return MarkAsWord32(node), VisitSelect(node);
    case IrOpcode::kWord64And:
      return MarkAsWord64(node), VisitWord64And(node);
    case IrOpcode::kWord64Or:
      return MarkAsWord64(node), VisitWord64Or(node);
    case IrOpcode::kWord64Xor:
      return MarkAsWord64(node), VisitWord64Xor(node);
    case IrOpcode::kWord64Shl:
      return MarkAsWord64(node), VisitWord64Shl(node);
    case IrOpcode::kWord64Shr:
      return MarkAsWord64(node), VisitWord64Shr(node);
    case IrOpcode::kWord64Sar:
      return MarkAsWord64(node), VisitWord64Sar(node);
    case IrOpcode::kWord64Rol:
      return MarkAsWord64(node), VisitWord64Rol(node);
    case IrOpcode::kWord64Ror:
      return MarkAsWord64(node), VisitWord64Ror(node);
    case IrOpcode::kWord64Clz:
      return MarkAsWord64(node), VisitWord64Clz(node);
    case IrOpcode::kWord64Ctz:
      return MarkAsWord64(node), VisitWord64Ctz(node);
    case IrOpcode::kWord64ReverseBits:
      return MarkAsWord64(node), VisitWord64ReverseBits(node);
    case IrOpcode::kWord64ReverseBytes:
      return MarkAsWord64(node), VisitWord64ReverseBytes(node);
    case IrOpcode::kSimd128ReverseBytes:
      return MarkAsSimd128(node), VisitSimd128ReverseBytes(node);
    case IrOpcode::kInt64AbsWithOverflow:
      return MarkAsWord64(node), VisitInt64AbsWithOverflow(node);
    case IrOpcode::kWord64Equal:
      return VisitWord64Equal(node);
    case IrOpcode::kWord64Select:
      return MarkAsWord64(node), VisitSelect(node);
    case IrOpcode::kInt32Add:
      return MarkAsWord32(node), VisitInt32Add(node);
    case IrOpcode::kInt32AddWithOverflow:
      return MarkAsWord32(node), VisitInt32AddWithOverflow(node);
    case IrOpcode::kInt32Sub:
      return MarkAsWord32(node), VisitInt32Sub(node);
    case IrOpcode::kInt32SubWithOverflow:
      return VisitInt32SubWithOverflow(node);
    case IrOpcode::kInt32Mul:
      return MarkAsWord32(node), VisitInt32Mul(node);
    case IrOpcode::kInt32MulWithOverflow:
      return MarkAsWord32(node), VisitInt32MulWithOverflow(node);
    case IrOpcode::kInt32MulHigh:
      return VisitInt32MulHigh(node);
    case IrOpcode::kInt64MulHigh:
      return VisitInt64MulHigh(node);
    case IrOpcode::kInt32Div:
      return MarkAsWord32(node), VisitInt32Div(node);
    case IrOpcode::kInt32Mod:
      return MarkAsWord32(node), VisitInt32Mod(node);
    case IrOpcode::kInt32LessThan:
      return VisitInt32LessThan(node);
    case IrOpcode::kInt32LessThanOrEqual:
      return VisitInt32LessThanOrEqual(node);
    case IrOpcode::kUint32Div:
      return MarkAsWord32(node), VisitUint32Div(node);
    case IrOpcode::kUint32LessThan:
      return VisitUint32LessThan(node);
    case IrOpcode::kUint32LessThanOrEqual:
      return VisitUint32LessThanOrEqual(node);
    case IrOpcode::kUint32Mod:
      return MarkAsWord32(node), VisitUint32Mod(node);
    case IrOpcode::kUint32MulHigh:
      return VisitUint32MulHigh(node);
    case IrOpcode::kUint64MulHigh:
      return VisitUint64MulHigh(node);
    case IrOpcode::kInt64Add:
      return MarkAsWord64(node), VisitInt64Add(node);
    case IrOpcode::kInt64AddWithOverflow:
      return MarkAsWord64(node), VisitInt64AddWithOverflow(node);
    case IrOpcode::kInt64Sub:
      return MarkAsWord64(node), VisitInt64Sub(node);
    case IrOpcode::kInt64SubWithOverflow:
      return MarkAsWord64(node), VisitInt64SubWithOverflow(node);
    case IrOpcode::kInt64Mul:
      return MarkAsWord64(node), VisitInt64Mul(node);
    case IrOpcode::kInt64MulWithOverflow:
      return MarkAsWord64(node), VisitInt64MulWithOverflow(node);
    case IrOpcode::kInt64Div:
      return MarkAsWord64(node), VisitInt64Div(node);
    case IrOpcode::kInt64Mod:
      return MarkAsWord64(node), VisitInt64Mod(node);
    case IrOpcode::kInt64LessThan:
      return VisitInt64LessThan(node);
    case IrOpcode::kInt64LessThanOrEqual:
      return VisitInt64LessThanOrEqual(node);
    case IrOpcode::kUint64Div:
      return MarkAsWord64(node), VisitUint64Div(node);
    case IrOpcode::kUint64LessThan:
      return VisitUint64LessThan(node);
    case IrOpcode::kUint64LessThanOrEqual:
      return VisitUint64LessThanOrEqual(node);
    case IrOpcode::kUint64Mod:
      return MarkAsWord64(node), VisitUint64Mod(node);
    case IrOpcode::kBitcastTaggedToWord:
    case IrOpcode::kBitcastTaggedToWordForTagAndSmiBits:
      return MarkAsRepresentation(MachineType::PointerRepresentation(), node),
             VisitBitcastTaggedToWord(node);
    case IrOpcode::kBitcastWordToTagged:
      return MarkAsTagged(node), VisitBitcastWordToTagged(node);
    case IrOpcode::kBitcastWordToTaggedSigned:
      return MarkAsRepresentation(MachineRepresentation::kTaggedSigned, node),
             EmitIdentity(node);
    case IrOpcode::kChangeFloat32ToFloat64:
      return MarkAsFloat64(node), VisitChangeFloat32ToFloat64(node);
    case IrOpcode::kChangeInt32ToFloat64:
      return MarkAsFloat64(node), VisitChangeInt32ToFloat64(node);
    case IrOpcode::kChangeInt64ToFloat64:
      return MarkAsFloat64(node), VisitChangeInt64ToFloat64(node);
    case IrOpcode::kChangeUint32ToFloat64:
      return MarkAsFloat64(node), VisitChangeUint32ToFloat64(node);
    case IrOpcode::kChangeFloat64ToInt32:
      return MarkAsWord32(node), VisitChangeFloat64ToInt32(node);
    case IrOpcode::kChangeFloat64ToInt64:
      return MarkAsWord64(node), VisitChangeFloat64ToInt64(node);
    case IrOpcode::kChangeFloat64ToUint32:
      return MarkAsWord32(node), VisitChangeFloat64ToUint32(node);
    case IrOpcode::kChangeFloat64ToUint64:
      return MarkAsWord64(node), VisitChangeFloat64ToUint64(node);
    case IrOpcode::kFloat64SilenceNaN:
      MarkAsFloat64(node);
      if (CanProduceSignalingNaN(node->InputAt(0))) {
        return VisitFloat64SilenceNaN(node);
      } else {
        return EmitIdentity(node);
      }
    case IrOpcode::kTruncateFloat64ToInt64:
      return MarkAsWord64(node), VisitTruncateFloat64ToInt64(node);
    case IrOpcode::kTruncateFloat64ToUint32:
      return MarkAsWord32(node), VisitTruncateFloat64ToUint32(node);
    case IrOpcode::kTruncateFloat32ToInt32:
      return MarkAsWord32(node), VisitTruncateFloat32ToInt32(node);
    case IrOpcode::kTruncateFloat32ToUint32:
      return MarkAsWord32(node), VisitTruncateFloat32ToUint32(node);
    case IrOpcode::kTryTruncateFloat32ToInt64:
      return MarkAsWord64(node), VisitTryTruncateFloat32ToInt64(node);
    case IrOpcode::kTryTruncateFloat64ToInt64:
      return MarkAsWord64(node), VisitTryTruncateFloat64ToInt64(node);
    case IrOpcode::kTryTruncateFloat32ToUint64:
      return MarkAsWord64(node), VisitTryTruncateFloat32ToUint64(node);
    case IrOpcode::kTryTruncateFloat64ToUint64:
      return MarkAsWord64(node), VisitTryTruncateFloat64ToUint64(node);
    case IrOpcode::kTryTruncateFloat64ToInt32:
      return MarkAsWord32(node), VisitTryTruncateFloat64ToInt32(node);
    case IrOpcode::kTryTruncateFloat64ToUint32:
      return MarkAsWord32(node), VisitTryTruncateFloat64ToUint32(node);
    case IrOpcode::kBitcastWord32ToWord64:
      return MarkAsWord64(node), VisitBitcastWord32ToWord64(node);
    case IrOpcode::kChangeInt32ToInt64:
      return MarkAsWord64(node), VisitChangeInt32ToInt64(node);
    case IrOpcode::kChangeUint32ToUint64:
      return MarkAsWord64(node), VisitChangeUint32ToUint64(node);
    case IrOpcode::kTruncateFloat64ToFloat32:
      return MarkAsFloat32(node), VisitTruncateFloat64ToFloat32(node);
    case IrOpcode::kTruncateFloat64ToWord32:
      return MarkAsWord32(node), VisitTruncateFloat64ToWord32(node);
    case IrOpcode::kTruncateInt64ToInt32:
      return MarkAsWord32(node), VisitTruncateInt64ToInt32(node);
    case IrOpcode::kRoundFloat64ToInt32:
      return MarkAsWord32(node), VisitRoundFloat64ToInt32(node);
    case IrOpcode::kRoundInt64ToFloat32:
      return MarkAsFloat32(node), VisitRoundInt64ToFloat32(node);
    case IrOpcode::kRoundInt32ToFloat32:
      return MarkAsFloat32(node), VisitRoundInt32ToFloat32(node);
    case IrOpcode::kRoundInt64ToFloat64:
      return MarkAsFloat64(node), VisitRoundInt64ToFloat64(node);
    case IrOpcode::kBitcastFloat32ToInt32:
      return MarkAsWord32(node), VisitBitcastFloat32ToInt32(node);
    case IrOpcode::kRoundUint32ToFloat32:
      return MarkAsFloat32(node), VisitRoundUint32ToFloat32(node);
    case IrOpcode::kRoundUint64ToFloat32:
      return MarkAsFloat64(node), VisitRoundUint64ToFloat32(node);
    case IrOpcode::kRoundUint64ToFloat64:
      return MarkAsFloat64(node), VisitRoundUint64ToFloat64(node);
    case IrOpcode::kBitcastFloat64ToInt64:
      return MarkAsWord64(node), VisitBitcastFloat64ToInt64(node);
    case IrOpcode::kBitcastInt32ToFloat32:
      return MarkAsFloat32(node), VisitBitcastInt32ToFloat32(node);
    case IrOpcode::kBitcastInt64ToFloat64:
      return MarkAsFloat64(node), VisitBitcastInt64ToFloat64(node);
    case IrOpcode::kFloat32Add:
      return MarkAsFloat32(node), VisitFloat32Add(node);
    case IrOpcode::kFloat32Sub:
      return MarkAsFloat32(node), VisitFloat32Sub(node);
    case IrOpcode::kFloat32Neg:
      return MarkAsFloat32(node), VisitFloat32Neg(node);
    case IrOpcode::kFloat32Mul:
      return MarkAsFloat32(node), VisitFloat32Mul(node);
    case IrOpcode::kFloat32Div:
      return MarkAsFloat32(node), VisitFloat32Div(node);
    case IrOpcode::kFloat32Abs:
      return MarkAsFloat32(node), VisitFloat32Abs(node);
    case IrOpcode::kFloat32Sqrt:
      return MarkAsFloat32(node), VisitFloat32Sqrt(node);
    case IrOpcode::kFloat32Equal:
      return VisitFloat32Equal(node);
    case IrOpcode::kFloat32LessThan:
      return VisitFloat32LessThan(node);
    case IrOpcode::kFloat32LessThanOrEqual:
      return VisitFloat32LessThanOrEqual(node);
    case IrOpcode::kFloat32Max:
      return MarkAsFloat32(node), VisitFloat32Max(node);
    case IrOpcode::kFloat32Min:
      return MarkAsFloat32(node), VisitFloat32Min(node);
    case IrOpcode::kFloat32Select:
      return MarkAsFloat32(node), VisitSelect(node);
    case IrOpcode::kFloat64Add:
      return MarkAsFloat64(node), VisitFloat64Add(node);
    case IrOpcode::kFloat64Sub:
      return MarkAsFloat64(node), VisitFloat64Sub(node);
    case IrOpcode::kFloat64Neg:
      return MarkAsFloat64(node), VisitFloat64Neg(node);
    case IrOpcode::kFloat64Mul:
      return MarkAsFloat64(node), VisitFloat64Mul(node);
    case IrOpcode::kFloat64Div:
      return MarkAsFloat64(node), VisitFloat64Div(node);
    case IrOpcode::kFloat64Mod:
      return MarkAsFloat64(node), VisitFloat64Mod(node);
    case IrOpcode::kFloat64Min:
      return MarkAsFloat64(node), VisitFloat64Min(node);
    case IrOpcode::kFloat64Max:
      return MarkAsFloat64(node), VisitFloat64Max(node);
    case IrOpcode::kFloat64Abs:
      return MarkAsFloat64(node), VisitFloat64Abs(node);
    case IrOpcode::kFloat64Acos:
      return MarkAsFloat64(node), VisitFloat64Acos(node);
    case IrOpcode::kFloat64Acosh:
      return MarkAsFloat64(node), VisitFloat64Acosh(node);
    case IrOpcode::kFloat64Asin:
      return MarkAsFloat64(node), VisitFloat64Asin(node);
    case IrOpcode::kFloat64Asinh:
      return MarkAsFloat64(node), VisitFloat64Asinh(node);
    case IrOpcode::kFloat64Atan:
      return MarkAsFloat64(node), VisitFloat64Atan(node);
    case IrOpcode::kFloat64Atanh:
      return MarkAsFloat64(node), VisitFloat64Atanh(node);
    case IrOpcode::kFloat64Atan2:
      return MarkAsFloat64(node), VisitFloat64Atan2(node);
    case IrOpcode::kFloat64Cbrt:
      return MarkAsFloat64(node), VisitFloat64Cbrt(node);
    case IrOpcode::kFloat64Cos:
      return MarkAsFloat64(node), VisitFloat64Cos(node);
    case IrOpcode::kFloat64Cosh:
      return MarkAsFloat64(node), VisitFloat64Cosh(node);
    case IrOpcode::kFloat64Exp:
      return MarkAsFloat64(node), VisitFloat64Exp(node);
    case IrOpcode::kFloat64Expm1:
      return MarkAsFloat64(node), VisitFloat64Expm1(node);
    case IrOpcode::kFloat64Log:
      return MarkAsFloat64(node), VisitFloat64Log(node);
    case IrOpcode::kFloat64Log1p:
      return MarkAsFloat64(node), VisitFloat64Log1p(node);
    case IrOpcode::kFloat64Log10:
      return MarkAsFloat64(node), VisitFloat64Log10(node);
    case IrOpcode::kFloat64Log2:
      return MarkAsFloat64(node), VisitFloat64Log2(node);
    case IrOpcode::kFloat64Pow:
      return MarkAsFloat64(node), VisitFloat64Pow(node);
    case IrOpcode::kFloat64Sin:
      return MarkAsFloat64(node), VisitFloat64Sin(node);
    case IrOpcode::kFloat64Sinh:
      return MarkAsFloat64(node), VisitFloat64Sinh(node);
    case IrOpcode::kFloat64Sqrt:
      return MarkAsFloat64(node), VisitFloat64Sqrt(node);
    case IrOpcode::kFloat64Tan:
      return MarkAsFloat64(node), VisitFloat64Tan(node);
    case IrOpcode::kFloat64Tanh:
      return MarkAsFloat64(node), VisitFloat64Tanh(node);
    case IrOpcode::kFloat64Equal:
      return VisitFloat64Equal(node);
    case IrOpcode::kFloat64LessThan:
      return VisitFloat64LessThan(node);
    case IrOpcode::kFloat64LessThanOrEqual:
      return VisitFloat64LessThanOrEqual(node);
    case IrOpcode::kFloat64Select:
      return MarkAsFloat64(node), VisitSelect(node);
    case IrOpcode::kFloat32RoundDown:
      return MarkAsFloat32(node), VisitFloat32RoundDown(node);
    case IrOpcode::kFloat64RoundDown:
      return MarkAsFloat64(node), VisitFloat64RoundDown(node);
    case IrOpcode::kFloat32RoundUp:
      return MarkAsFloat32(node), VisitFloat32RoundUp(node);
    case IrOpcode::kFloat64RoundUp:
      return MarkAsFloat64(node), VisitFloat64RoundUp(node);
    case IrOpcode::kFloat32RoundTruncate:
      return MarkAsFloat32(node), VisitFloat32RoundTruncate(node);
    case IrOpcode::kFloat64RoundTruncate:
      return MarkAsFloat64(node), VisitFloat64RoundTruncate(node);
    case IrOpcode::kFloat64RoundTiesAway:
      return MarkAsFloat64(node), VisitFloat64RoundTiesAway(node);
    case IrOpcode::kFloat32RoundTiesEven:
      return MarkAsFloat32(node), VisitFloat32RoundTiesEven(node);
    case IrOpcode::kFloat64RoundTiesEven:
      return MarkAsFloat64(node), VisitFloat64RoundTiesEven(node);
    case IrOpcode::kFloat64ExtractLowWord32:
      return MarkAsWord32(node), VisitFloat64ExtractLowWord32(node);
    case IrOpcode::kFloat64ExtractHighWord32:
      return MarkAsWord32(node), VisitFloat64ExtractHighWord32(node);
    case IrOpcode::kFloat64InsertLowWord32:
      return MarkAsFloat64(node), VisitFloat64InsertLowWord32(node);
    case IrOpcode::kFloat64InsertHighWord32:
      return MarkAsFloat64(node), VisitFloat64InsertHighWord32(node);
    case IrOpcode::kStackSlot:
      return VisitStackSlot(node);
    case IrOpcode::kStackPointerGreaterThan:
      return VisitStackPointerGreaterThan(node);
    case IrOpcode::kLoadStackCheckOffset:
      return VisitLoadStackCheckOffset(node);
    case IrOpcode::kLoadFramePointer:
      return VisitLoadFramePointer(node);
    case IrOpcode::kLoadParentFramePointer:
      return VisitLoadParentFramePointer(node);
    case IrOpcode::kLoadRootRegister:
      return VisitLoadRootRegister(node);
    case IrOpcode::kUnalignedLoad: {
      LoadRepresentation type = LoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitUnalignedLoad(node);
    }
    case IrOpcode::kUnalignedStore:
      return VisitUnalignedStore(node);
    case IrOpcode::kInt32PairAdd:
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitInt32PairAdd(node);
    case IrOpcode::kInt32PairSub:
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitInt32PairSub(node);
    case IrOpcode::kInt32PairMul:
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitInt32PairMul(node);
    case IrOpcode::kWord32PairShl:
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitWord32PairShl(node);
    case IrOpcode::kWord32PairShr:
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitWord32PairShr(node);
    case IrOpcode::kWord32PairSar:
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitWord32PairSar(node);
    case IrOpcode::kMemoryBarrier:
      return VisitMemoryBarrier(node);
    case IrOpcode::kWord32AtomicLoad: {
      AtomicLoadParameters params = AtomicLoadParametersOf(node->op());
      LoadRepresentation type = params.representation();
      MarkAsRepresentation(type.representation(), node);
      return VisitWord32AtomicLoad(node);
    }
    case IrOpcode::kWord64AtomicLoad: {
      AtomicLoadParameters params = AtomicLoadParametersOf(node->op());
      LoadRepresentation type = params.representation();
      MarkAsRepresentation(type.representation(), node);
      return VisitWord64AtomicLoad(node);
    }
    case IrOpcode::kWord32AtomicStore:
      return VisitWord32AtomicStore(node);
    case IrOpcode::kWord64AtomicStore:
      return VisitWord64AtomicStore(node);
    case IrOpcode::kWord32AtomicPairStore:
      return VisitWord32AtomicPairStore(node);
    case IrOpcode::kWord32AtomicPairLoad: {
      MarkAsWord32(node);
      MarkPairProjectionsAsWord32(node);
      return VisitWord32AtomicPairLoad(node);
    }
#define ATOMIC_CASE(name, rep)                         \
  case IrOpcode::k##rep##Atomic##name: {               \
    MachineType type = AtomicOpType(node->op());       \
    MarkAsRepresentation(type.representation(), node); \
    return Visit##rep##Atomic##name(node);             \
  }
      ATOMIC_CASE(Add, Word32)
      ATOMIC_CASE(Add, Word64)
      ATOMIC_CASE(Sub, Word32)
      ATOMIC_CASE(Sub, Word64)
      ATOMIC_CASE(And, Word32)
      ATOMIC_CASE(And, Word64)
      ATOMIC_CASE(Or, Word32)
      ATOMIC_CASE(Or, Word64)
      ATOMIC_CASE(Xor, Word32)
      ATOMIC_CASE(Xor, Word64)
      ATOMIC_CASE(Exchange, Word32)
      ATOMIC_CASE(Exchange, Word64)
      ATOMIC_CASE(CompareExchange, Word32)
      ATOMIC_CASE(CompareExchange, Word64)
#undef ATOMIC_CASE
#define ATOMIC_CASE(name)                     \
  case IrOpcode::kWord32AtomicPair##name: {   \
    MarkAsWord32(node);                       \
    MarkPairProjectionsAsWord32(node);        \
    return VisitWord32AtomicPair##name(node); \
  }
      ATOMIC_CASE(Add)
      ATOMIC_CASE(Sub)
      ATOMIC_CASE(And)
      ATOMIC_CASE(Or)
      ATOMIC_CASE(Xor)
      ATOMIC_CASE(Exchange)
      ATOMIC_CASE(CompareExchange)
#undef ATOMIC_CASE
    case IrOpcode::kProtectedLoad:
    case IrOpcode::kLoadTrapOnNull: {
      LoadRepresentation type = LoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitProtectedLoad(node);
    }
    case IrOpcode::kSignExtendWord8ToInt32:
      return MarkAsWord32(node), VisitSignExtendWord8ToInt32(node);
    case IrOpcode::kSignExtendWord16ToInt32:
      return MarkAsWord32(node), VisitSignExtendWord16ToInt32(node);
    case IrOpcode::kSignExtendWord8ToInt64:
      return MarkAsWord64(node), VisitSignExtendWord8ToInt64(node);
    case IrOpcode::kSignExtendWord16ToInt64:
      return MarkAsWord64(node), VisitSignExtendWord16ToInt64(node);
    case IrOpcode::kSignExtendWord32ToInt64:
      return MarkAsWord64(node), VisitSignExtendWord32ToInt64(node);
    case IrOpcode::kF64x2Splat:
      return MarkAsSimd128(node), VisitF64x2Splat(node);
    case IrOpcode::kF64x2ExtractLane:
      return MarkAsFloat64(node), VisitF64x2ExtractLane(node);
    case IrOpcode::kF64x2ReplaceLane:
      return MarkAsSimd128(node), VisitF64x2ReplaceLane(node);
    case IrOpcode::kF64x2Abs:
      return MarkAsSimd128(node), VisitF64x2Abs(node);
    case IrOpcode::kF64x2Neg:
      return MarkAsSimd128(node), VisitF64x2Neg(node);
    case IrOpcode::kF64x2Sqrt:
      return MarkAsSimd128(node), VisitF64x2Sqrt(node);
    case IrOpcode::kF64x2Add:
      return MarkAsSimd128(node), VisitF64x2Add(node);
    case IrOpcode::kF64x2Sub:
      return MarkAsSimd128(node), VisitF64x2Sub(node);
    case IrOpcode::kF64x2Mul:
      return MarkAsSimd128(node), VisitF64x2Mul(node);
    case IrOpcode::kF64x2Div:
      return MarkAsSimd128(node), VisitF64x2Div(node);
    case IrOpcode::kF64x2Min:
      return MarkAsSimd128(node), VisitF64x2Min(node);
    case IrOpcode::kF64x2Max:
      return MarkAsSimd128(node), VisitF64x2Max(node);
    case IrOpcode::kF64x2Eq:
      return MarkAsSimd128(node), VisitF64x2Eq(node);
    case IrOpcode::kF64x2Ne:
      return MarkAsSimd128(node), VisitF64x2Ne(node);
    case IrOpcode::kF64x2Lt:
      return MarkAsSimd128(node), VisitF64x2Lt(node);
    case IrOpcode::kF64x2Le:
      return MarkAsSimd128(node), VisitF64x2Le(node);
    case IrOpcode::kF64x2Qfma:
      return MarkAsSimd128(node), VisitF64x2Qfma(node);
    case IrOpcode::kF64x2Qfms:
      return MarkAsSimd128(node), VisitF64x2Qfms(node);
    case IrOpcode::kF64x2Pmin:
      return MarkAsSimd128(node), VisitF64x2Pmin(node);
    case IrOpcode::kF64x2Pmax:
      return MarkAsSimd128(node), VisitF64x2Pmax(node);
    case IrOpcode::kF64x2Ceil:
      return MarkAsSimd128(node), VisitF64x2Ceil(node);
    case IrOpcode::kF64x2Floor:
      return MarkAsSimd128(node), VisitF64x2Floor(node);
    case IrOpcode::kF64x2Trunc:
      return MarkAsSimd128(node), VisitF64x2Trunc(node);
    case IrOpcode::kF64x2NearestInt:
      return MarkAsSimd128(node), VisitF64x2NearestInt(node);
    case IrOpcode::kF64x2ConvertLowI32x4S:
      return MarkAsSimd128(node), VisitF64x2ConvertLowI32x4S(node);
    case IrOpcode::kF64x2ConvertLowI32x4U:
      return MarkAsSimd128(node), VisitF64x2ConvertLowI32x4U(node);
    case IrOpcode::kF64x2PromoteLowF32x4:
      return MarkAsSimd128(node), VisitF64x2PromoteLowF32x4(node);
    case IrOpcode::kF32x4Splat:
      return MarkAsSimd128(node), VisitF32x4Splat(node);
    case IrOpcode::kF32x4ExtractLane:
      return MarkAsFloat32(node), VisitF32x4ExtractLane(node);
    case IrOpcode::kF32x4ReplaceLane:
      return MarkAsSimd128(node), VisitF32x4ReplaceLane(node);
    case IrOpcode::kF32x4SConvertI32x4:
      return MarkAsSimd128(node), VisitF32x4SConvertI32x4(node);
    case IrOpcode::kF32x4UConvertI32x4:
      return MarkAsSimd128(node), VisitF32x4UConvertI32x4(node);
    case IrOpcode::kF32x4Abs:
      return MarkAsSimd128(node), VisitF32x4Abs(node);
    case IrOpcode::kF32x4Neg:
      return MarkAsSimd128(node), VisitF32x4Neg(node);
    case IrOpcode::kF32x4Sqrt:
      return MarkAsSimd128(node), VisitF32x4Sqrt(node);
    case IrOpcode::kF32x4Add:
      return MarkAsSimd128(node), VisitF32x4Add(node);
    case IrOpcode::kF32x4Sub:
      return MarkAsSimd128(node), VisitF32x4Sub(node);
    case IrOpcode::kF32x4Mul:
      return MarkAsSimd128(node), VisitF32x4Mul(node);
    case IrOpcode::kF32x4Div:
      return MarkAsSimd128(node), VisitF32x4Div(node);
    case IrOpcode::kF32x4Min:
      return MarkAsSimd128(node), VisitF32x4Min(node);
    case IrOpcode::kF32x4Max:
      return MarkAsSimd128(node), VisitF32x4Max(node);
    case IrOpcode::kF32x4Eq:
      return MarkAsSimd128(node), VisitF32x4Eq(node);
    case IrOpcode::kF32x4Ne:
      return MarkAsSimd128(node), VisitF32x4Ne(node);
    case IrOpcode::kF32x4Lt:
      return MarkAsSimd128(node), VisitF32x4Lt(node);
    case IrOpcode::kF32x4Le:
      return MarkAsSimd128(node), VisitF32x4Le(node);
    case IrOpcode::kF32x4Qfma:
      return MarkAsSimd128(node), VisitF32x4Qfma(node);
    case IrOpcode::kF32x4Qfms:
      return MarkAsSimd128(node), VisitF32x4Qfms(node);
    case IrOpcode::kF32x4Pmin:
      return MarkAsSimd128(node), VisitF32x4Pmin(node);
    case IrOpcode::kF32x4Pmax:
      return MarkAsSimd128(node), VisitF32x4Pmax(node);
    case IrOpcode::kF32x4Ceil:
      return MarkAsSimd128(node), VisitF32x4Ceil(node);
    case IrOpcode::kF32x4Floor:
      return MarkAsSimd128(node), VisitF32x4Floor(node);
    case IrOpcode::kF32x4Trunc:
      return MarkAsSimd128(node), VisitF32x4Trunc(node);
    case IrOpcode::kF32x4NearestInt:
      return MarkAsSimd128(node), VisitF32x4NearestInt(node);
    case IrOpcode::kF32x4DemoteF64x2Zero:
      return MarkAsSimd128(node), VisitF32x4DemoteF64x2Zero(node);
    case IrOpcode::kI64x2Splat:
      return MarkAsSimd128(node), VisitI64x2Splat(node);
    case IrOpcode::kI64x2SplatI32Pair:
      return MarkAsSimd128(node), VisitI64x2SplatI32Pair(node);
    case IrOpcode::kI64x2ExtractLane:
      return MarkAsWord64(node), VisitI64x2ExtractLane(node);
    case IrOpcode::kI64x2ReplaceLane:
      return MarkAsSimd128(node), VisitI64x2ReplaceLane(node);
    case IrOpcode::kI64x2ReplaceLaneI32Pair:
      return MarkAsSimd128(node), VisitI64x2ReplaceLaneI32Pair(node);
    case IrOpcode::kI64x2Abs:
      return MarkAsSimd128(node), VisitI64x2Abs(node);
    case IrOpcode::kI64x2Neg:
      return MarkAsSimd128(node), VisitI64x2Neg(node);
    case IrOpcode::kI64x2SConvertI32x4Low:
      return MarkAsSimd128(node), VisitI64x2SConvertI32x4Low(node);
    case IrOpcode::kI64x2SConvertI32x4High:
      return MarkAsSimd128(node), VisitI64x2SConvertI32x4High(node);
    case IrOpcode::kI64x2UConvertI32x4Low:
      return MarkAsSimd128(node), VisitI64x2UConvertI32x4Low(node);
    case IrOpcode::kI64x2UConvertI32x4High:
      return MarkAsSimd128(node), VisitI64x2UConvertI32x4High(node);
    case IrOpcode::kI64x2BitMask:
      return MarkAsWord32(node), VisitI64x2BitMask(node);
    case IrOpcode::kI64x2Shl:
      return MarkAsSimd128(node), VisitI64x2Shl(node);
    case IrOpcode::kI64x2ShrS:
      return MarkAsSimd128(node), VisitI64x2ShrS(node);
    case IrOpcode::kI64x2Add:
      return MarkAsSimd128(node), VisitI64x2Add(node);
    case IrOpcode::kI64x2Sub:
      return MarkAsSimd128(node), VisitI64x2Sub(node);
    case IrOpcode::kI64x2Mul:
      return MarkAsSimd128(node), VisitI64x2Mul(node);
    case IrOpcode::kI64x2Eq:
      return MarkAsSimd128(node), VisitI64x2Eq(node);
    case IrOpcode::kI64x2Ne:
      return MarkAsSimd128(node), VisitI64x2Ne(node);
    case IrOpcode::kI64x2GtS:
      return MarkAsSimd128(node), VisitI64x2GtS(node);
    case IrOpcode::kI64x2GeS:
      return MarkAsSimd128(node), VisitI64x2GeS(node);
    case IrOpcode::kI64x2ShrU:
      return MarkAsSimd128(node), VisitI64x2ShrU(node);
    case IrOpcode::kI64x2ExtMulLowI32x4S:
      return MarkAsSimd128(node), VisitI64x2ExtMulLowI32x4S(node);
    case IrOpcode::kI64x2ExtMulHighI32x4S:
      return MarkAsSimd128(node), VisitI64x2ExtMulHighI32x4S(node);
    case IrOpcode::kI64x2ExtMulLowI32x4U:
      return MarkAsSimd128(node), VisitI64x2ExtMulLowI32x4U(node);
    case IrOpcode::kI64x2ExtMulHighI32x4U:
      return MarkAsSimd128(node), VisitI64x2ExtMulHighI32x4U(node);
    case IrOpcode::kI32x4Splat:
      return MarkAsSimd128(node), VisitI32x4Splat(node);
    case IrOpcode::kI32x4ExtractLane:
      return MarkAsWord32(node), VisitI32x4ExtractLane(node);
    case IrOpcode::kI32x4ReplaceLane:
      return MarkAsSimd128(node), VisitI32x4ReplaceLane(node);
    case IrOpcode::kI32x4SConvertF32x4:
      return MarkAsSimd128(node), VisitI32x4SConvertF32x4(node);
    case IrOpcode::kI32x4SConvertI16x8Low:
      return MarkAsSimd128(node), VisitI32x4SConvertI16x8Low(node);
    case IrOpcode::kI32x4SConvertI16x8High:
      return MarkAsSimd128(node), VisitI32x4SConvertI16x8High(node);
    case IrOpcode::kI32x4Neg:
      return MarkAsSimd128(node), VisitI32x4Neg(node);
    case IrOpcode::kI32x4Shl:
      return MarkAsSimd128(node), VisitI32x4Shl(node);
    case IrOpcode::kI32x4ShrS:
      return MarkAsSimd128(node), VisitI32x4ShrS(node);
    case IrOpcode::kI32x4Add:
      return MarkAsSimd128(node), VisitI32x4Add(node);
    case IrOpcode::kI32x4Sub:
      return MarkAsSimd128(node), VisitI32x4Sub(node);
    case IrOpcode::kI32x4Mul:
      return MarkAsSimd128(node), VisitI32x4Mul(node);
    case IrOpcode::kI32x4MinS:
      return MarkAsSimd128(node), VisitI32x4MinS(node);
    case IrOpcode::kI32x4MaxS:
      return MarkAsSimd128(node), VisitI32x4MaxS(node);
    case IrOpcode::kI32x4Eq:
      return MarkAsSimd128(node), VisitI32x4Eq(node);
    case IrOpcode::kI32x4Ne:
      return MarkAsSimd128(node), VisitI32x4Ne(node);
    case IrOpcode::kI32x4GtS:
      return MarkAsSimd128(node), VisitI32x4GtS(node);
    case IrOpcode::kI32x4GeS:
      return MarkAsSimd128(node), VisitI32x4GeS(node);
    case IrOpcode::kI32x4UConvertF32x4:
      return MarkAsSimd128(node), VisitI32x4UConvertF32x4(node);
    case IrOpcode::kI32x4UConvertI16x8Low:
      return MarkAsSimd128(node), VisitI32x4UConvertI16x8Low(node);
    case IrOpcode::kI32x4UConvertI16x8High:
      return MarkAsSimd128(node), VisitI32x4UConvertI16x8High(node);
    case IrOpcode::kI32x4ShrU:
      return MarkAsSimd128(node), VisitI32x4ShrU(node);
    case IrOpcode::kI32x4MinU:
      return MarkAsSimd128(node), VisitI32x4MinU(node);
    case IrOpcode::kI32x4MaxU:
      return MarkAsSimd128(node), VisitI32x4MaxU(node);
    case IrOpcode::kI32x4GtU:
      return MarkAsSimd128(node), VisitI32x4GtU(node);
    case IrOpcode::kI32x4GeU:
      return MarkAsSimd128(node), VisitI32x4GeU(node);
    case IrOpcode::kI32x4Abs:
      return MarkAsSimd128(node), VisitI32x4Abs(node);
    case IrOpcode::kI32x4BitMask:
      return MarkAsWord32(node), VisitI32x4BitMask(node);
    case IrOpcode::kI32x4DotI16x8S:
      return MarkAsSimd128(node), VisitI32x4DotI16x8S(node);
    case IrOpcode::kI32x4ExtMulLowI16x8S:
      return MarkAsSimd128(node), VisitI32x4ExtMulLowI16x8S(node);
    case IrOpcode::kI32x4ExtMulHighI16x8S:
      return MarkAsSimd128(node), VisitI32x4ExtMulHighI16x8S(node);
    case IrOpcode::kI32x4ExtMulLowI16x8U:
      return MarkAsSimd128(node), VisitI32x4ExtMulLowI16x8U(node);
    case IrOpcode::kI32x4ExtMulHighI16x8U:
      return MarkAsSimd128(node), VisitI32x4ExtMulHighI16x8U(node);
    case IrOpcode::kI32x4ExtAddPairwiseI16x8S:
      return MarkAsSimd128(node), VisitI32x4ExtAddPairwiseI16x8S(node);
    case IrOpcode::kI32x4ExtAddPairwiseI16x8U:
      return MarkAsSimd128(node), VisitI32x4ExtAddPairwiseI16x8U(node);
    case IrOpcode::kI32x4TruncSatF64x2SZero:
      return MarkAsSimd128(node), VisitI32x4TruncSatF64x2SZero(node);
    case IrOpcode::kI32x4TruncSatF64x2UZero:
      return MarkAsSimd128(node), VisitI32x4TruncSatF64x2UZero(node);
    case IrOpcode::kI16x8Splat:
      return MarkAsSimd128(node), VisitI16x8Splat(node);
    case IrOpcode::kI16x8ExtractLaneU:
      return MarkAsWord32(node), VisitI16x8ExtractLaneU(node);
    case IrOpcode::kI16x8ExtractLaneS:
      return MarkAsWord32(node), VisitI16x8ExtractLaneS(node);
    case IrOpcode::kI16x8ReplaceLane:
      return MarkAsSimd128(node), VisitI16x8ReplaceLane(node);
    case IrOpcode::kI16x8SConvertI8x16Low:
      return MarkAsSimd128(node), VisitI16x8SConvertI8x16Low(node);
    case IrOpcode::kI16x8SConvertI8x16High:
      return MarkAsSimd128(node), VisitI16x8SConvertI8x16High(node);
    case IrOpcode::kI16x8Neg:
      return MarkAsSimd128(node), VisitI16x8Neg(node);
    case IrOpcode::kI16x8Shl:
      return MarkAsSimd128(node), VisitI16x8Shl(node);
    case IrOpcode::kI16x8ShrS:
      return MarkAsSimd128(node), VisitI16x8ShrS(node);
    case IrOpcode::kI16x8SConvertI32x4:
      return MarkAsSimd128(node), VisitI16x8SConvertI32x4(node);
    case IrOpcode::kI16x8Add:
      return MarkAsSimd128(node), VisitI16x8Add(node);
    case IrOpcode::kI16x8AddSatS:
      return MarkAsSimd128(node), VisitI16x8AddSatS(node);
    case IrOpcode::kI16x8Sub:
      return MarkAsSimd128(node), VisitI16x8Sub(node);
    case IrOpcode::kI16x8SubSatS:
      return MarkAsSimd128(node), VisitI16x8SubSatS(node);
    case IrOpcode::kI16x8Mul:
      return MarkAsSimd128(node), VisitI16x8Mul(node);
    case IrOpcode::kI16x8MinS:
      return MarkAsSimd128(node), VisitI16x8MinS(node);
    case IrOpcode::kI16x8MaxS:
      return MarkAsSimd128(node), VisitI16x8MaxS(node);
    case IrOpcode::kI16x8Eq:
      return MarkAsSimd128(node), VisitI16x8Eq(node);
    case IrOpcode::kI16x8Ne:
      return MarkAsSimd128(node), VisitI16x8Ne(node);
    case IrOpcode::kI16x8GtS:
      return MarkAsSimd128(node), VisitI16x8GtS(node);
    case IrOpcode::kI16x8GeS:
      return MarkAsSimd128(node), VisitI16x8GeS(node);
    case IrOpcode::kI16x8UConvertI8x16Low:
      return MarkAsSimd128(node), VisitI16x8UConvertI8x16Low(node);
    case IrOpcode::kI16x8UConvertI8x16High:
      return MarkAsSimd128(node), VisitI16x8UConvertI8x16High(node);
    case IrOpcode::kI16x8ShrU:
      return MarkAsSimd128(node), VisitI16x8ShrU(node);
    case IrOpcode::kI16x8UConvertI32x4:
      return MarkAsSimd128(node), VisitI16x8UConvertI32x4(node);
    case IrOpcode::kI16x8AddSatU:
      return MarkAsSimd128(node), VisitI16x8AddSatU(node);
    case IrOpcode::kI16x8SubSatU:
      return MarkAsSimd128(node), VisitI16x8SubSatU(node);
    case IrOpcode::kI16x8MinU:
      return MarkAsSimd128(node), VisitI16x8MinU(node);
    case IrOpcode::kI16x8MaxU:
      return MarkAsSimd128(node), VisitI16x8MaxU(node);
    case IrOpcode::kI16x8GtU:
      return MarkAsSimd128(node), VisitI16x8GtU(node);
    case IrOpcode::kI16x8GeU:
      return MarkAsSimd128(node), VisitI16x8GeU(node);
    case IrOpcode::kI16x8RoundingAverageU:
      return MarkAsSimd128(node), VisitI16x8RoundingAverageU(node);
    case IrOpcode::kI16x8Q15MulRSatS:
      return MarkAsSimd128(node), VisitI16x8Q15MulRSatS(node);
    case IrOpcode::kI16x8Abs:
      return MarkAsSimd128(node), VisitI16x8Abs(node);
    case IrOpcode::kI16x8BitMask:
      return MarkAsWord32(node), VisitI16x8BitMask(node);
    case IrOpcode::kI16x8ExtMulLowI8x16S:
      return MarkAsSimd128(node), VisitI16x8ExtMulLowI8x16S(node);
    case IrOpcode::kI16x8ExtMulHighI8x16S:
      return MarkAsSimd128(node), VisitI16x8ExtMulHighI8x16S(node);
    case IrOpcode::kI16x8ExtMulLowI8x16U:
      return MarkAsSimd128(node), VisitI16x8ExtMulLowI8x16U(node);
    case IrOpcode::kI16x8ExtMulHighI8x16U:
      return MarkAsSimd128(node), VisitI16x8ExtMulHighI8x16U(node);
    case IrOpcode::kI16x8ExtAddPairwiseI8x16S:
      return MarkAsSimd128(node), VisitI16x8ExtAddPairwiseI8x16S(node);
    case IrOpcode::kI16x8ExtAddPairwiseI8x16U:
      return MarkAsSimd128(node), VisitI16x8ExtAddPairwiseI8x16U(node);
    case IrOpcode::kI8x16Splat:
      return MarkAsSimd128(node), VisitI8x16Splat(node);
    case IrOpcode::kI8x16ExtractLaneU:
      return MarkAsWord32(node), VisitI8x16ExtractLaneU(node);
    case IrOpcode::kI8x16ExtractLaneS:
      return MarkAsWord32(node), VisitI8x16ExtractLaneS(node);
    case IrOpcode::kI8x16ReplaceLane:
      return MarkAsSimd128(node), VisitI8x16ReplaceLane(node);
    case IrOpcode::kI8x16Neg:
      return MarkAsSimd128(node), VisitI8x16Neg(node);
    case IrOpcode::kI8x16Shl:
      return MarkAsSimd128(node), VisitI8x16Shl(node);
    case IrOpcode::kI8x16ShrS:
      return MarkAsSimd128(node), VisitI8x16ShrS(node);
    case IrOpcode::kI8x16SConvertI16x8:
      return MarkAsSimd128(node), VisitI8x16SConvertI16x8(node);
    case IrOpcode::kI8x16Add:
      return MarkAsSimd128(node), VisitI8x16Add(node);
    case IrOpcode::kI8x16AddSatS:
      return MarkAsSimd128(node), VisitI8x16AddSatS(node);
    case IrOpcode::kI8x16Sub:
      return MarkAsSimd128(node), VisitI8x16Sub(node);
    case IrOpcode::kI8x16SubSatS:
      return MarkAsSimd128(node), VisitI8x16SubSatS(node);
    case IrOpcode::kI8x16MinS:
      return MarkAsSimd128(node), VisitI8x16MinS(node);
    case IrOpcode::kI8x16MaxS:
      return MarkAsSimd128(node), VisitI8x16MaxS(node);
    case IrOpcode::kI8x16Eq:
      return MarkAsSimd128(node), VisitI8x16Eq(node);
    case IrOpcode::kI8x16Ne:
      return MarkAsSimd128(node), VisitI8x16Ne(node);
    case IrOpcode::kI8x16GtS:
      return MarkAsSimd128(node), VisitI8x16GtS(node);
    case IrOpcode::kI8x16GeS:
      return MarkAsSimd128(node), VisitI8x16GeS(node);
    case IrOpcode::kI8x16ShrU:
      return MarkAsSimd128(node), VisitI8x16ShrU(node);
    case IrOpcode::kI8x16UConvertI16x8:
      return MarkAsSimd128(node), VisitI8x16UConvertI16x8(node);
    case IrOpcode::kI8x16AddSatU:
      return MarkAsSimd128(node), VisitI8x16AddSatU(node);
    case IrOpcode::kI8x16SubSatU:
      return MarkAsSimd128(node), VisitI8x16SubSatU(node);
    case IrOpcode::kI8x16MinU:
      return MarkAsSimd128(node), VisitI8x16MinU(node);
    case IrOpcode::kI8x16MaxU:
      return MarkAsSimd128(node), VisitI8x16MaxU(node);
    case IrOpcode::kI8x16GtU:
      return MarkAsSimd128(node), VisitI8x16GtU(node);
    case IrOpcode::kI8x16GeU:
      return MarkAsSimd128(node), VisitI8x16GeU(node);
    case IrOpcode::kI8x16RoundingAverageU:
      return MarkAsSimd128(node), VisitI8x16RoundingAverageU(node);
    case IrOpcode::kI8x16Popcnt:
      return MarkAsSimd128(node), VisitI8x16Popcnt(node);
    case IrOpcode::kI8x16Abs:
      return MarkAsSimd128(node), VisitI8x16Abs(node);
    case IrOpcode::kI8x16BitMask:
      return MarkAsWord32(node), VisitI8x16BitMask(node);
    case IrOpcode::kS128Const:
      return MarkAsSimd128(node), VisitS128Const(node);
    case IrOpcode::kS128Zero:
      return MarkAsSimd128(node), VisitS128Zero(node);
    case IrOpcode::kS128And:
      return MarkAsSimd128(node), VisitS128And(node);
    case IrOpcode::kS128Or:
      return MarkAsSimd128(node), VisitS128Or(node);
    case IrOpcode::kS128Xor:
      return MarkAsSimd128(node), VisitS128Xor(node);
    case IrOpcode::kS128Not:
      return MarkAsSimd128(node), VisitS128Not(node);
    case IrOpcode::kS128Select:
      return MarkAsSimd128(node), VisitS128Select(node);
    case IrOpcode::kS128AndNot:
      return MarkAsSimd128(node), VisitS128AndNot(node);
    case IrOpcode::kI8x16Swizzle:
      return MarkAsSimd128(node), VisitI8x16Swizzle(node);
    case IrOpcode::kI8x16Shuffle:
      return MarkAsSimd128(node), VisitI8x16Shuffle(node);
    case IrOpcode::kV128AnyTrue:
      return MarkAsWord32(node), VisitV128AnyTrue(node);
    case IrOpcode::kI64x2AllTrue:
      return MarkAsWord32(node), VisitI64x2AllTrue(node);
    case IrOpcode::kI32x4AllTrue:
      return MarkAsWord32(node), VisitI32x4AllTrue(node);
    case IrOpcode::kI16x8AllTrue:
      return MarkAsWord32(node), VisitI16x8AllTrue(node);
    case IrOpcode::kI8x16AllTrue:
      return MarkAsWord32(node), VisitI8x16AllTrue(node);
    case IrOpcode::kI8x16RelaxedLaneSelect:
      return MarkAsSimd128(node), VisitI8x16RelaxedLaneSelect(node);
    case IrOpcode::kI16x8RelaxedLaneSelect:
      return MarkAsSimd128(node), VisitI16x8RelaxedLaneSelect(node);
    case IrOpcode::kI32x4RelaxedLaneSelect:
      return MarkAsSimd128(node), VisitI32x4RelaxedLaneSelect(node);
    case IrOpcode::kI64x2RelaxedLaneSelect:
      return MarkAsSimd128(node), VisitI64x2RelaxedLaneSelect(node);
    case IrOpcode::kF32x4RelaxedMin:
      return MarkAsSimd128(node), VisitF32x4RelaxedMin(node);
    case IrOpcode::kF32x4RelaxedMax:
      return MarkAsSimd128(node), VisitF32x4RelaxedMax(node);
    case IrOpcode::kF64x2RelaxedMin:
      return MarkAsSimd128(node), VisitF64x2RelaxedMin(node);
    case IrOpcode::kF64x2RelaxedMax:
      return MarkAsSimd128(node), VisitF64x2RelaxedMax(node);
    case IrOpcode::kI32x4RelaxedTruncF64x2SZero:
      return MarkAsSimd128(node), VisitI32x4RelaxedTruncF64x2SZero(node);
    case IrOpcode::kI32x4RelaxedTruncF64x2UZero:
      return MarkAsSimd128(node), VisitI32x4RelaxedTruncF64x2UZero(node);
    case IrOpcode::kI32x4RelaxedTruncF32x4S:
      return MarkAsSimd128(node), VisitI32x4RelaxedTruncF32x4S(node);
    case IrOpcode::kI32x4RelaxedTruncF32x4U:
      return MarkAsSimd128(node), VisitI32x4RelaxedTruncF32x4U(node);
    case IrOpcode::kI16x8RelaxedQ15MulRS:
      return MarkAsSimd128(node), VisitI16x8RelaxedQ15MulRS(node);
    case IrOpcode::kI16x8DotI8x16I7x16S:
      return MarkAsSimd128(node), VisitI16x8DotI8x16I7x16S(node);
    case IrOpcode::kI32x4DotI8x16I7x16AddS:
      return MarkAsSimd128(node), VisitI32x4DotI8x16I7x16AddS(node);

      // SIMD256
#if V8_TARGET_ARCH_X64
    case IrOpcode::kF64x4Min:
      return MarkAsSimd256(node), VisitF64x4Min(node);
    case IrOpcode::kF64x4Max:
      return MarkAsSimd256(node), VisitF64x4Max(node);
    case IrOpcode::kF64x4Add:
      return MarkAsSimd256(node), VisitF64x4Add(node);
    case IrOpcode::kF32x8Add:
      return MarkAsSimd256(node), VisitF32x8Add(node);
    case IrOpcode::kI64x4Add:
      return MarkAsSimd256(node), VisitI64x4Add(node);
    case IrOpcode::kI32x8Add:
      return MarkAsSimd256(node), VisitI32x8Add(node);
    case IrOpcode::kI16x16Add:
      return MarkAsSimd256(node), VisitI16x16Add(node);
    case IrOpcode::kI8x32Add:
      return MarkAsSimd256(node), VisitI8x32Add(node);
    case IrOpcode::kF64x4Sub:
      return MarkAsSimd256(node), VisitF64x4Sub(node);
    case IrOpcode::kF32x8Sub:
      return MarkAsSimd256(node), VisitF32x8Sub(node);
    case IrOpcode::kF32x8Min:
      return MarkAsSimd256(node), VisitF32x8Min(node);
    case IrOpcode::kF32x8Max:
      return MarkAsSimd256(node), VisitF32x8Max(node);
    case IrOpcode::kI64x4Ne:
      return MarkAsSimd256(node), VisitI64x4Ne(node);
    case IrOpcode::kI32x8Ne:
      return MarkAsSimd256(node), VisitI32x8Ne(node);
    case IrOpcode::kI32x8GtU:
      return MarkAsSimd256(node), VisitI32x8GtU(node);
    case IrOpcode::kI32x8GeS:
      return MarkAsSimd256(node), VisitI32x8GeS(node);
    case IrOpcode::kI32x8GeU:
      return MarkAsSimd256(node), VisitI32x8GeU(node);
    case IrOpcode::kI16x16Ne:
      return MarkAsSimd256(node), VisitI16x16Ne(node);
    case IrOpcode::kI16x16GtU:
      return MarkAsSimd256(node), VisitI16x16GtU(node);
    case IrOpcode::kI16x16GeS:
      return MarkAsSimd256(node), VisitI16x16GeS(node);
    case IrOpcode::kI16x16GeU:
      return MarkAsSimd256(node), VisitI16x16GeU(node);
    case IrOpcode::kI8x32Ne:
      return MarkAsSimd256(node), VisitI8x32Ne(node);
    case IrOpcode::kI8x32GtU:
      return MarkAsSimd256(node), VisitI8x32GtU(node);
    case IrOpcode::kI8x32GeS:
      return MarkAsSimd256(node), VisitI8x32GeS(node);
    case IrOpcode::kI8x32GeU:
      return MarkAsSimd256(node), VisitI8x32GeU(node);
    case IrOpcode::kI64x4Sub:
      return MarkAsSimd256(node), VisitI64x4Sub(node);
    case IrOpcode::kI32x8Sub:
      return MarkAsSimd256(node), VisitI32x8Sub(node);
    case IrOpcode::kI16x16Sub:
      return MarkAsSimd256(node), VisitI16x16Sub(node);
    case IrOpcode::kI8x32Sub:
      return MarkAsSimd256(node), VisitI8x32Sub(node);
    case IrOpcode::kF64x4Mul:
      return MarkAsSimd256(node), VisitF64x4Mul(node);
    case IrOpcode::kF32x8Mul:
      return MarkAsSimd256(node), VisitF32x8Mul(node);
    case IrOpcode::kI64x4Mul:
      return MarkAsSimd256(node), VisitI64x4Mul(node);
    case IrOpcode::kI32x8Mul:
      return MarkAsSimd256(node), VisitI32x8Mul(node);
    case IrOpcode::kI16x16Mul:
      return MarkAsSimd256(node), VisitI16x16Mul(node);
    case IrOpcode::kF32x8Div:
      return MarkAsSimd256(node), VisitF32x8Div(node);
    case IrOpcode::kF64x4Div:
      return MarkAsSimd256(node), VisitF64x4Div(node);
    case IrOpcode::kI16x16AddSatS:
      return MarkAsSimd256(node), VisitI16x16AddSatS(node);
    case IrOpcode::kI8x32AddSatS:
      return MarkAsSimd256(node), VisitI8x32AddSatS(node);
    case IrOpcode::kI16x16AddSatU:
      return MarkAsSimd256(node), VisitI16x16AddSatU(node);
    case IrOpcode::kI8x32AddSatU:
      return MarkAsSimd256(node), VisitI8x32AddSatU(node);
    case IrOpcode::kI16x16SubSatS:
      return MarkAsSimd256(node), VisitI16x16SubSatS(node);
    case IrOpcode::kI8x32SubSatS:
      return MarkAsSimd256(node), VisitI8x32SubSatS(node);
    case IrOpcode::kI16x16SubSatU:
      return MarkAsSimd256(node), VisitI16x16SubSatU(node);
    case IrOpcode::kI8x32SubSatU:
      return MarkAsSimd256(node), VisitI8x32SubSatU(node);
    case IrOpcode::kF64x4ConvertI32x4S:
      return MarkAsSimd256(node), VisitF64x4ConvertI32x4S(node);
    case IrOpcode::kF32x8SConvertI32x8:
      return MarkAsSimd256(node), VisitF32x8SConvertI32x8(node);
    case IrOpcode::kF32x4DemoteF64x4:
      return MarkAsSimd256(node), VisitF32x4DemoteF64x4(node);
    case IrOpcode::kI64x4SConvertI32x4:
      return MarkAsSimd256(node), VisitI64x4SConvertI32x4(node);
    case IrOpcode::kI64x4UConvertI32x4:
      return MarkAsSimd256(node), VisitI64x4UConvertI32x4(node);
    case IrOpcode::kI32x8SConvertI16x8:
      return MarkAsSimd256(node), VisitI32x8SConvertI16x8(node);
    case IrOpcode::kI32x8UConvertI16x8:
      return MarkAsSimd256(node), VisitI32x8UConvertI16x8(node);
    case IrOpcode::kI16x16SConvertI8x16:
      return MarkAsSimd256(node), VisitI16x16SConvertI8x16(node);
    case IrOpcode::kI16x16UConvertI8x16:
      return MarkAsSimd256(node), VisitI16x16UConvertI8x16(node);
    case IrOpcode::kI16x16SConvertI32x8:
      return MarkAsSimd256(node), VisitI16x16SConvertI32x8(node);
    case IrOpcode::kI16x16UConvertI32x8:
      return MarkAsSimd256(node), VisitI16x16UConvertI32x8(node);
    case IrOpcode::kI8x32SConvertI16x16:
      return MarkAsSimd256(node), VisitI8x32SConvertI16x16(node);
    case IrOpcode::kI8x32UConvertI16x16:
      return MarkAsSimd256(node), VisitI8x32UConvertI16x16(node);
    case IrOpcode::kF32x8Abs:
      return MarkAsSimd256(node), VisitF32x8Abs(node);
    case IrOpcode::kF32x8Neg:
      return MarkAsSimd256(node), VisitF32x8Neg(node);
    case IrOpcode::kF32x8Sqrt:
      return MarkAsSimd256(node), VisitF32x8Sqrt(node);
    case IrOpcode::kF64x4Sqrt:
      return MarkAsSimd256(node), VisitF64x4Sqrt(node);
    case IrOpcode::kI32x8Abs:
      return MarkAsSimd256(node), VisitI32x8Abs(node);
    case IrOpcode::kI32x8Neg:
      return MarkAsSimd256(node), VisitI32x8Neg(node);
    case IrOpcode::kI16x16Abs:
      return MarkAsSimd256(node), VisitI16x16Abs(node);
    case IrOpcode::kI16x16Neg:
      return MarkAsSimd256(node), VisitI16x16Neg(node);
    case IrOpcode::kI8x32Abs:
      return MarkAsSimd256(node), VisitI8x32Abs(node);
    case IrOpcode::kI8x32Neg:
      return MarkAsSimd256(node), VisitI8x32Neg(node);
    case IrOpcode::kI64x4Shl:
      return MarkAsSimd256(node), VisitI64x4Shl(node);
    case IrOpcode::kI64x4ShrU:
      return MarkAsSimd256(node), VisitI64x4ShrU(node);
    case IrOpcode::kI32x8Shl:
      return MarkAsSimd256(node), VisitI32x8Shl(node);
    case IrOpcode::kI32x8ShrS:
      return MarkAsSimd256(node), VisitI32x8ShrS(node);
    case IrOpcode::kI32x8ShrU:
      return MarkAsSimd256(node), VisitI32x8ShrU(node);
    case IrOpcode::kI16x16Shl:
      return MarkAsSimd256(node), VisitI16x16Shl(node);
    case IrOpcode::kI16x16ShrS:
      return MarkAsSimd256(node), VisitI16x16ShrS(node);
    case IrOpcode::kI16x16ShrU:
      return MarkAsSimd256(node), VisitI16x16ShrU(node);
    case IrOpcode::kI32x8DotI16x16S:
      return MarkAsSimd256(node), VisitI32x8DotI16x16S(node);
    case IrOpcode::kI16x16RoundingAverageU:
      return MarkAsSimd256(node), VisitI16x16RoundingAverageU(node);
    case IrOpcode::kI8x32RoundingAverageU:
      return MarkAsSimd256(node), VisitI8x32RoundingAverageU(node);
    case IrOpcode::kS256Zero:
      return MarkAsSimd256(node), VisitS256Zero(node);
    case IrOpcode::kS256And:
      return MarkAsSimd256(node), VisitS256And(node);
    case IrOpcode::kS256Or:
      return MarkAsSimd256(node), VisitS256Or(node);
    case IrOpcode::kS256Xor:
      return MarkAsSimd256(node), VisitS256Xor(node);
    case IrOpcode::kS256Not:
      return MarkAsSimd256(node), VisitS256Not(node);
    case IrOpcode::kS256Select:
      return MarkAsSimd256(node), VisitS256Select(node);
    case IrOpcode::kS256AndNot:
      return MarkAsSimd256(node), VisitS256AndNot(node);
    case IrOpcode::kF32x8Eq:
      return MarkAsSimd256(node), VisitF32x8Eq(node);
    case IrOpcode::kF64x4Eq:
      return MarkAsSimd256(node), VisitF64x4Eq(node);
    case IrOpcode::kI64x4Eq:
      return MarkAsSimd256(node), VisitI64x4Eq(node);
    case IrOpcode::kI32x8Eq:
      return MarkAsSimd256(node), VisitI32x8Eq(node);
    case IrOpcode::kI16x16Eq:
      return MarkAsSimd256(node), VisitI16x16Eq(node);
    case IrOpcode::kI8x32Eq:
      return MarkAsSimd256(node), VisitI8x32Eq(node);
    case IrOpcode::kF32x8Ne:
      return MarkAsSimd256(node), VisitF32x8Ne(node);
    case IrOpcode::kF64x4Ne:
      return MarkAsSimd256(node), VisitF64x4Ne(node);
    case IrOpcode::kI64x4GtS:
      return MarkAsSimd256(node), VisitI64x4GtS(node);
    case IrOpcode::kI32x8GtS:
      return MarkAsSimd256(node), VisitI32x8GtS(node);
    case IrOpcode::kI16x16GtS:
      return MarkAsSimd256(node), VisitI16x16GtS(node);
    case IrOpcode::kI8x32GtS:
      return MarkAsSimd256(node), VisitI8x32GtS(node);
    case IrOpcode::kF64x4Lt:
      return MarkAsSimd256(node), VisitF64x4Lt(node);
    case IrOpcode::kF32x8Lt:
      return MarkAsSimd256(node), VisitF32x8Lt(node);
    case IrOpcode::kF64x4Le:
      return MarkAsSimd256(node), VisitF64x4Le(node);
    case IrOpcode::kF32x8Le:
      return MarkAsSimd256(node), VisitF32x8Le(node);
    case IrOpcode::kI32x8MinS:
      return MarkAsSimd256(node), VisitI32x8MinS(node);
    case IrOpcode::kI16x16MinS:
      return MarkAsSimd256(node), VisitI16x16MinS(node);
    case IrOpcode::kI8x32MinS:
      return MarkAsSimd256(node), VisitI8x32MinS(node);
    case IrOpcode::kI32x8MinU:
      return MarkAsSimd256(node), VisitI32x8MinU(node);
    case IrOpcode::kI16x16MinU:
      return MarkAsSimd256(node), VisitI16x16MinU(node);
    case IrOpcode::kI8x32MinU:
      return MarkAsSimd256(node), VisitI8x32MinU(node);
    case IrOpcode::kI32x8MaxS:
      return MarkAsSimd256(node), VisitI32x8MaxS(node);
    case IrOpcode::kI16x16MaxS:
      return MarkAsSimd256(node), VisitI16x16MaxS(node);
    case IrOpcode::kI8x32MaxS:
      return MarkAsSimd256(node), VisitI8x32MaxS(node);
    case IrOpcode::kI32x8MaxU:
      return MarkAsSimd256(node), VisitI32x8MaxU(node);
    case IrOpcode::kI16x16MaxU:
      return MarkAsSimd256(node), VisitI16x16MaxU(node);
    case IrOpcode::kI8x32MaxU:
      return MarkAsSimd256(node), VisitI8x32MaxU(node);
    case IrOpcode::kI64x4Splat:
      return MarkAsSimd256(node), VisitI64x4Splat(node);
    case IrOpcode::kI32x8Splat:
      return MarkAsSimd256(node), VisitI32x8Splat(node);
    case IrOpcode::kI16x16Splat:
      return MarkAsSimd256(node), VisitI16x16Splat(node);
    case IrOpcode::kI8x32Splat:
      return MarkAsSimd256(node), VisitI8x32Splat(node);
    case IrOpcode::kI64x4ExtMulI32x4S:
      return MarkAsSimd256(node), VisitI64x4ExtMulI32x4S(node);
    case IrOpcode::kI64x4ExtMulI32x4U:
      return MarkAsSimd256(node), VisitI64x4ExtMulI32x4U(node);
    case IrOpcode::kI32x8ExtMulI16x8S:
      return MarkAsSimd256(node), VisitI32x8ExtMulI16x8S(node);
    case IrOpcode::kI32x8ExtMulI16x8U:
      return MarkAsSimd256(node), VisitI32x8ExtMulI16x8U(node);
    case IrOpcode::kI16x16ExtMulI8x16S:
      return MarkAsSimd256(node), VisitI16x16ExtMulI8x16S(node);
    case IrOpcode::kI16x16ExtMulI8x16U:
      return MarkAsSimd256(node), VisitI16x16ExtMulI8x16U(node);
    case IrOpcode::kI32x8ExtAddPairwiseI16x16S:
      return MarkAsSimd256(node), VisitI32x8ExtAddPairwiseI16x16S(node);
    case IrOpcode::kI32x8ExtAddPairwiseI16x16U:
      return MarkAsSimd256(node), VisitI32x8ExtAddPairwiseI16x16U(node);
    case IrOpcode::kI16x16ExtAddPairwiseI8x32S:
      return MarkAsSimd256(node), VisitI16x16ExtAddPairwiseI8x32S(node);
    case IrOpcode::kI16x16ExtAddPairwiseI8x32U:
      return MarkAsSimd256(node), VisitI16x16ExtAddPairwiseI8x32U(node);
#endif  //  V8_TARGET_ARCH_X64
    default:
      FATAL("Unexpected operator #%d:%s @ node #%d", node->opcode(),
            node->op()->mnemonic(), node->id());
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitStackPointerGreaterThan(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kStackPointerGreaterThanCondition, node);
  VisitStackPointerGreaterThan(node, &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitLoadStackCheckOffset(Node* node) {
  OperandGenerator g(this);
  Emit(kArchStackCheckOffset, g.DefineAsRegister(node));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitLoadFramePointer(Node* node) {
  OperandGenerator g(this);
  Emit(kArchFramePointer, g.DefineAsRegister(node));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitLoadParentFramePointer(Node* node) {
  OperandGenerator g(this);
  Emit(kArchParentFramePointer, g.DefineAsRegister(node));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitLoadRootRegister(Node* node) {
  // Do nothing. Following loads/stores from this operator will use kMode_Root
  // to load/store from an offset of the root register.
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Acos(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Acos);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Acosh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Acosh);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Asin(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Asin);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Asinh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Asinh);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Atan(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Atan);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Atanh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Atanh);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Atan2(Node* node) {
  VisitFloat64Ieee754Binop(node, kIeee754Float64Atan2);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Cbrt(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cbrt);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Cos(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cos);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Cosh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cosh);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Exp(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Exp);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Expm1(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Expm1);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Log(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Log1p(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log1p);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Log2(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log2);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Log10(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log10);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Pow(Node* node) {
  VisitFloat64Ieee754Binop(node, kIeee754Float64Pow);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Sin(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Sin);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Sinh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Sinh);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Tan(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Tan);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFloat64Tanh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Tanh);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::EmitTableSwitch(
    const SwitchInfo& sw, InstructionOperand const& index_operand) {
  OperandGenerator g(this);
  size_t input_count = 2 + sw.value_range();
  DCHECK_LE(sw.value_range(), std::numeric_limits<size_t>::max() - 2);
  auto* inputs = zone()->template NewArray<InstructionOperand>(input_count);
  inputs[0] = index_operand;
  InstructionOperand default_operand = g.Label(sw.default_branch());
  std::fill(&inputs[1], &inputs[input_count], default_operand);
  for (const CaseInfo& c : sw.CasesUnsorted()) {
    size_t value = c.value - sw.min_value();
    DCHECK_LE(0u, value);
    DCHECK_LT(value + 2, input_count);
    inputs[value + 2] = g.Label(c.branch);
  }
  Emit(kArchTableSwitch, 0, nullptr, input_count, inputs, 0, nullptr);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::EmitBinarySearchSwitch(
    const SwitchInfo& sw, InstructionOperand const& value_operand) {
  OperandGenerator g(this);
  size_t input_count = 2 + sw.case_count() * 2;
  DCHECK_LE(sw.case_count(), (std::numeric_limits<size_t>::max() - 2) / 2);
  auto* inputs = zone()->template NewArray<InstructionOperand>(input_count);
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

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastTaggedToWord(Node* node) {
  EmitIdentity(node);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastWordToTagged(Node* node) {
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(node->InputAt(0)));
}

// 32 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_32_BIT

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64And(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Or(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Xor(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Shl(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Shr(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Sar(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Rol(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Ror(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Clz(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Ctz(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64ReverseBits(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Popcnt(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64Equal(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64Add(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64AddWithOverflow(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64Sub(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64SubWithOverflow(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64Mul(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64MulHigh(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint64MulHigh(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64MulWithOverflow(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64Div(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64LessThan(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64LessThanOrEqual(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint64Div(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt64Mod(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint64LessThan(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint64LessThanOrEqual(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUint64Mod(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastWord32ToWord64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeInt32ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeInt64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeUint32ToUint64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeFloat64ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitChangeFloat64ToUint64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTruncateFloat64ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTryTruncateFloat32ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTryTruncateFloat64ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTryTruncateFloat32ToUint64(
    Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTryTruncateFloat64ToUint64(
    Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTryTruncateFloat64ToInt32(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTryTruncateFloat64ToUint32(
    Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTruncateInt64ToInt32(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRoundInt64ToFloat32(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRoundInt64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRoundUint64ToFloat32(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRoundUint64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastFloat64ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBitcastInt64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitSignExtendWord8ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitSignExtendWord16ToInt64(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitSignExtendWord32ToInt64(Node* node) {
  UNIMPLEMENTED();
}
#endif  // V8_TARGET_ARCH_32_BIT

// 64 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_64_BIT
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32PairAdd(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32PairSub(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitInt32PairMul(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32PairShl(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32PairShr(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32PairSar(Node* node) {
  UNIMPLEMENTED();
}
#endif  // V8_TARGET_ARCH_64_BIT

#if !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_RISCV32
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairLoad(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairStore(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairAdd(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairSub(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairAnd(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairOr(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairXor(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairExchange(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord32AtomicPairCompareExchange(
    Node* node) {
  UNIMPLEMENTED();
}
#endif  // !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM
        // && !V8_TARGET_ARCH_RISCV32

#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_MIPS64 && \
    !V8_TARGET_ARCH_S390 && !V8_TARGET_ARCH_PPC64 &&                          \
    !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_LOONG64
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicLoad(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicStore(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicAdd(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicSub(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicAnd(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicOr(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicXor(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicExchange(Node* node) {
  UNIMPLEMENTED();
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitWord64AtomicCompareExchange(
    Node* node) {
  UNIMPLEMENTED();
}
#endif  // !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_PPC64
        // !V8_TARGET_ARCH_MIPS64 && !V8_TARGET_ARCH_S390 &&
        // !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_LOONG64

#if !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_RISCV32
// This is only needed on 32-bit to split the 64-bit value into two operands.
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2SplatI32Pair(Node* node) {
  UNIMPLEMENTED();
}
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2ReplaceLaneI32Pair(Node* node) {
  UNIMPLEMENTED();
}
#endif  // !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM &&
        // !V8_TARGET_ARCH_RISCV32

#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64
#if !V8_TARGET_ARCH_ARM64
#if !V8_TARGET_ARCH_MIPS64 && !V8_TARGET_ARCH_LOONG64 && \
    !V8_TARGET_ARCH_RISCV32 && !V8_TARGET_ARCH_RISCV64
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2Splat(Node* node) {
  UNIMPLEMENTED();
}
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2ExtractLane(Node* node) {
  UNIMPLEMENTED();
}
template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitI64x2ReplaceLane(Node* node) {
  UNIMPLEMENTED();
}
#endif  // !V8_TARGET_ARCH_MIPS64 && !V8_TARGET_ARCH_LOONG64 &&
        // !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_RISCV32
#endif  // !V8_TARGET_ARCH_ARM64
#endif  // !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitFinishRegion(Node* node) {
  EmitIdentity(node);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitParameter(Node* node) {
  OperandGenerator g(this);
  int index = ParameterIndexOf(node->op());

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

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitIfException(Node* node) {
  OperandGenerator g(this);
  DCHECK_EQ(IrOpcode::kCall, node->InputAt(1)->opcode());
  Emit(kArchNop, g.DefineAsLocation(node, ExceptionLocation()));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitOsrValue(Node* node) {
  OperandGenerator g(this);
  int index = OsrValueIndexOf(node->op());
  Emit(kArchNop,
       g.DefineAsLocation(node, linkage()->GetOsrValueLocation(index)));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitPhi(Node* node) {
  const int input_count = node->op()->ValueInputCount();
  DCHECK_EQ(input_count, current_block_->PredecessorCount());
  PhiInstruction* phi = instruction_zone()->template New<PhiInstruction>(
      instruction_zone(), GetVirtualRegister(node),
      static_cast<size_t>(input_count));
  sequence()
      ->InstructionBlockAt(RpoNumber::FromInt(current_block_->rpo_number()))
      ->AddPhi(phi);
  for (int i = 0; i < input_count; ++i) {
    Node* const input = node->InputAt(i);
    MarkAsUsed(input);
    phi->SetInput(static_cast<size_t>(i), GetVirtualRegister(input));
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitProjection(Node* node) {
  OperandGenerator g(this);
  Node* value = node->InputAt(0);
  switch (value->opcode()) {
    case IrOpcode::kInt32AddWithOverflow:
    case IrOpcode::kInt32SubWithOverflow:
    case IrOpcode::kInt32MulWithOverflow:
    case IrOpcode::kInt64AddWithOverflow:
    case IrOpcode::kInt64SubWithOverflow:
    case IrOpcode::kInt64MulWithOverflow:
    case IrOpcode::kTryTruncateFloat32ToInt64:
    case IrOpcode::kTryTruncateFloat64ToInt64:
    case IrOpcode::kTryTruncateFloat32ToUint64:
    case IrOpcode::kTryTruncateFloat64ToUint64:
    case IrOpcode::kTryTruncateFloat64ToInt32:
    case IrOpcode::kTryTruncateFloat64ToUint32:
    case IrOpcode::kInt32PairAdd:
    case IrOpcode::kInt32PairSub:
    case IrOpcode::kInt32PairMul:
    case IrOpcode::kWord32PairShl:
    case IrOpcode::kWord32PairShr:
    case IrOpcode::kWord32PairSar:
    case IrOpcode::kInt32AbsWithOverflow:
    case IrOpcode::kInt64AbsWithOverflow:
      if (ProjectionIndexOf(node->op()) == 0u) {
        EmitIdentity(node);
      } else {
        DCHECK_EQ(1u, ProjectionIndexOf(node->op()));
        MarkAsUsed(value);
      }
      break;
    default:
      break;
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitConstant(Node* node) {
  // We must emit a NOP here because every live range needs a defining
  // instruction in the register allocator.
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineAsConstant(node));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::UpdateMaxPushedArgumentCount(size_t count) {
  *max_pushed_argument_count_ = std::max(count, *max_pushed_argument_count_);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitCall(Node* node, BasicBlock* handler) {
  OperandGenerator g(this);
  auto call_descriptor = CallDescriptorOf(node->op());
  SaveFPRegsMode mode = call_descriptor->NeedsCallerSavedFPRegisters()
                            ? SaveFPRegsMode::kSave
                            : SaveFPRegsMode::kIgnore;

  if (call_descriptor->NeedsCallerSavedRegisters()) {
    Emit(kArchSaveCallerRegisters | MiscField::encode(static_cast<int>(mode)),
         g.NoOutput());
  }

  FrameStateDescriptor* frame_state_descriptor = nullptr;
  if (call_descriptor->NeedsFrameState()) {
    frame_state_descriptor = GetFrameStateDescriptor(FrameState{
        node->InputAt(static_cast<int>(call_descriptor->InputCount()))});
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
  InitializeCallBuffer(node, &buffer, call_buffer_flags);

  EmitPrepareArguments(&buffer.pushed_nodes, call_descriptor, node);
  UpdateMaxPushedArgumentCount(buffer.pushed_nodes.size());

  // Pass label of exception handler block.
  if (handler) {
    DCHECK_EQ(IrOpcode::kIfException, handler->front()->opcode());
    flags |= CallDescriptor::kHasExceptionHandler;
    buffer.instruction_args.push_back(g.Label(handler));
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
      opcode = kArchCallCFunction | ParamField::encode(gp_param_count) |
               FPParamField::encode(fp_param_count);
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
      opcode = EncodeCallDescriptorFlags(kArchCallWasmFunction, flags);
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

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTailCall(Node* node) {
  OperandGenerator g(this);

  auto caller = linkage()->GetIncomingDescriptor();
  auto callee = CallDescriptorOf(node->op());
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
  InitializeCallBuffer(node, &buffer, flags, stack_param_delta);
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
      opcode = kArchTailCallWasm;
      break;
#endif  // V8_ENABLE_WEBASSEMBLY
    default:
      UNREACHABLE();
  }
  opcode = EncodeCallDescriptorFlags(opcode, callee->flags());

  Emit(kArchPrepareTailCall, g.NoOutput());

  // Add an immediate operand that represents the offset to the first slot that
  // is unused with respect to the stack pointer that has been updated for the
  // tail call instruction. Backends that pad arguments can write the padding
  // value at this offset from the stack.
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

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitGoto(BasicBlock* target) {
  // jump to the next block.
  OperandGenerator g(this);
  Emit(kArchJmp, g.NoOutput(), g.Label(target));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitReturn(Node* ret) {
  OperandGenerator g(this);
  const int input_count = linkage()->GetIncomingDescriptor()->ReturnCount() == 0
                              ? 1
                              : ret->op()->ValueInputCount();
  DCHECK_GE(input_count, 1);
  auto value_locations =
      zone()->template NewArray<InstructionOperand>(input_count);
  Node* pop_count = ret->InputAt(0);
  value_locations[0] = (pop_count->opcode() == IrOpcode::kInt32Constant ||
                        pop_count->opcode() == IrOpcode::kInt64Constant)
                           ? g.UseImmediate(pop_count)
                           : g.UseRegister(pop_count);
  for (int i = 1; i < input_count; ++i) {
    value_locations[i] =
        g.UseLocation(ret->InputAt(i), linkage()->GetReturnLocation(i - 1));
  }
  Emit(kArchRet, 0, nullptr, input_count, value_locations);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitBranch(Node* branch,
                                                BasicBlock* tbranch,
                                                BasicBlock* fbranch) {
  TryPrepareScheduleFirstProjection(branch->InputAt(0));

  FlagsContinuation cont =
      FlagsContinuation::ForBranch(kNotEqual, tbranch, fbranch);
  VisitWordCompareZero(branch, branch->InputAt(0), &cont);
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
// VisitDeoptimizeIf/VisitDeoptimizeUnless/VisitBranch and detects if the 1st
// projection could be scheduled now, and, if so, defines it.
template <typename Adapter>
void InstructionSelectorT<Adapter>::TryPrepareScheduleFirstProjection(
    Node* const maybe_projection) {
  if (maybe_projection->opcode() != IrOpcode::kProjection) {
    // The DeoptimizeIf/DeoptimizeUnless/Branch condition is not a projection.
    return;
  }

  if (ProjectionIndexOf(maybe_projection->op()) != 1u) {
    // The DeoptimizeIf/DeoptimizeUnless/Branch isn't on the Projection[1] (ie,
    // not on the overflow bit of a BinopOverflow).
    return;
  }

  Node* const node = maybe_projection->InputAt(0);
  if (schedule_->block(node) != current_block_) {
    // The projection input is not in the current block, so it shouldn't be
    // emitted now, so we don't need to eagerly schedule its Projection[0].
    return;
  }

  switch (node->opcode()) {
    case IrOpcode::kInt32AddWithOverflow:
    case IrOpcode::kInt32SubWithOverflow:
    case IrOpcode::kInt32MulWithOverflow:
    case IrOpcode::kInt64AddWithOverflow:
    case IrOpcode::kInt64SubWithOverflow:
    case IrOpcode::kInt64MulWithOverflow: {
      Node* result = NodeProperties::FindProjection(node, 0);
      if (result == nullptr || IsDefined(result)) {
        // No Projection(0), or it's already defined.
        return;
      }

      if (schedule_->block(result) != current_block_) {
        // {result} wasn't planned to be scheduled in {current_block_}. To avoid
        // adding checks to see if it can still be scheduled now, we just bail
        // out.
        return;
      }

      // Checking if all uses of {result} that are in the current block have
      // already been Defined.
      // We also ignore Phi uses: if {result} is used in a Phi in the block in
      // which it is defined, this means that this block is a loop header, and
      // {result} back into it through the back edge. In this case, it's normal
      // to schedule {result} before the Phi that uses it.
      for (Node* use : result->uses()) {
        if (!IsDefined(use) && schedule_->block(use) == current_block_ &&
            use->opcode() != IrOpcode::kPhi) {
          // {use} is in the current block but is not defined yet. It's possible
          // that it's not actually used, but the IsUsed(x) predicate is not
          // valid until we have visited `x`, so we overaproximate and assume
          // that {use} is itself used.
          return;
        }
      }

      // Visiting the projection now. Note that this relies on the fact that
      // VisitProjection doesn't Emit something: if it did, then we could be
      // Emitting something after a Branch, which is invalid (Branch can only be
      // at the end of a block, and the end of a block must always be a block
      // terminator). (remember that we emit operation in reverse order, so
      // because we are doing TryPrepareScheduleFirstProjection before actually
      // emitting the Branch, it would be after in the final instruction
      // sequence, not before)
      VisitProjection(result);
      return;
    }

    default:
      return;
  }
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitDeoptimizeIf(Node* node) {
  TryPrepareScheduleFirstProjection(node->InputAt(0));

  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      kNotEqual, p.reason(), node->id(), p.feedback(),
      FrameState{node->InputAt(1)});
  VisitWordCompareZero(node, node->InputAt(0), &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitDeoptimizeUnless(Node* node) {
  TryPrepareScheduleFirstProjection(node->InputAt(0));

  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  FlagsContinuation cont = FlagsContinuation::ForDeoptimize(
      kEqual, p.reason(), node->id(), p.feedback(),
      FrameState{node->InputAt(1)});
  VisitWordCompareZero(node, node->InputAt(0), &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitSelect(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSelect(kNotEqual, node,
                                   node->InputAt(1), node->InputAt(2));
  VisitWordCompareZero(node, node->InputAt(0), &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTrapIf(Node* node, TrapId trap_id) {
  // FrameStates are only used for wasm traps inlined in JS. In that case the
  // trap node will be lowered (replaced) before instruction selection.
  // Therefore any TrapIf node has only one input.
  DCHECK_EQ(node->op()->ValueInputCount(), 1);
  FlagsContinuation cont = FlagsContinuation::ForTrap(kNotEqual, trap_id);
  VisitWordCompareZero(node, node->InputAt(0), &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitTrapUnless(Node* node,
                                                    TrapId trap_id) {
  // FrameStates are only used for wasm traps inlined in JS. In that case the
  // trap node will be lowered (replaced) before instruction selection.
  // Therefore any TrapUnless node has only one input.
  DCHECK_EQ(node->op()->ValueInputCount(), 1);
  FlagsContinuation cont = FlagsContinuation::ForTrap(kEqual, trap_id);
  VisitWordCompareZero(node, node->InputAt(0), &cont);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::EmitIdentity(Node* node) {
  MarkAsUsed(node->InputAt(0));
  MarkAsDefined(node);
  SetRename(node, node->InputAt(0));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitDeoptimize(
    DeoptimizeReason reason, NodeId node_id, FeedbackSource const& feedback,
    FrameState frame_state) {
  InstructionOperandVector args(instruction_zone());
  AppendDeoptimizeArguments(&args, reason, node_id, feedback, frame_state);
  Emit(kArchDeoptimize, 0, nullptr, args.size(), &args.front(), 0, nullptr);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitThrow(Node* node) {
  OperandGenerator g(this);
  Emit(kArchThrowTerminator, g.NoOutput());
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitDebugBreak(Node* node) {
  OperandGenerator g(this);
  Emit(kArchDebugBreak, g.NoOutput());
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitUnreachable(Node* node) {
  OperandGenerator g(this);
  Emit(kArchDebugBreak, g.NoOutput());
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitStaticAssert(Node* node) {
  Node* asserted = node->InputAt(0);
  UnparkedScopeIfNeeded scope(broker_);
  AllowHandleDereference allow_handle_dereference;
  asserted->Print(4);
  FATAL(
      "Expected Turbofan static assert to hold, but got non-true input:\n  %s",
      StaticAssertSourceOf(node->op()));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitDeadValue(Node* node) {
  OperandGenerator g(this);
  MarkAsRepresentation(DeadValueRepresentationOf(node->op()), node);
  Emit(kArchDebugBreak, g.DefineAsConstant(node));
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitComment(Node* node) {
  OperandGenerator g(this);
  InstructionOperand operand(g.UseImmediate(node));
  Emit(kArchComment, 0, nullptr, 1, &operand);
}

template <typename Adapter>
void InstructionSelectorT<Adapter>::VisitRetain(Node* node) {
  OperandGenerator g(this);
  Emit(kArchNop, g.NoOutput(), g.UseAny(node->InputAt(0)));
}

template <typename Adapter>
bool InstructionSelectorT<Adapter>::CanProduceSignalingNaN(Node* node) {
  // TODO(jarin) Improve the heuristic here.
  if (node->opcode() == IrOpcode::kFloat64Add ||
      node->opcode() == IrOpcode::kFloat64Sub ||
      node->opcode() == IrOpcode::kFloat64Mul) {
    return false;
  }
  return true;
}

#if V8_TARGET_ARCH_64_BIT
template <typename Adapter>
bool InstructionSelectorT<Adapter>::ZeroExtendsWord32ToWord64(
    Node* node, int recursion_depth) {
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

  if (node->opcode() == IrOpcode::kPhi) {
    Upper32BitsState current = phi_states_[node->id()];
    if (current != Upper32BitsState::kNotYetChecked) {
      return current == Upper32BitsState::kUpperBitsGuaranteedZero;
    }

    // If further recursion is prevented, we can't make any assumptions about
    // the output of this phi node.
    if (recursion_depth >= kMaxRecursionDepth) {
      return false;
    }

    // Mark the current node so that we skip it if we recursively visit it
    // again. Or, said differently, we compute a largest fixed-point so we can
    // be optimistic when we hit cycles.
    phi_states_[node->id()] = Upper32BitsState::kUpperBitsGuaranteedZero;

    int input_count = node->op()->ValueInputCount();
    for (int i = 0; i < input_count; ++i) {
      Node* input = NodeProperties::GetValueInput(node, i);
      if (!ZeroExtendsWord32ToWord64(input, recursion_depth + 1)) {
        phi_states_[node->id()] = Upper32BitsState::kNoGuarantee;
        return false;
      }
    }

    return true;
  }
  return ZeroExtendsWord32ToWord64NoPhis(node);
}
#endif  // V8_TARGET_ARCH_64_BIT

namespace {

FrameStateDescriptor* GetFrameStateDescriptorInternal(Zone* zone,
                                                      FrameState state) {
  DCHECK_EQ(IrOpcode::kFrameState, state->opcode());
  DCHECK_EQ(FrameState::kFrameStateInputCount, state->InputCount());
  const FrameStateInfo& state_info = FrameStateInfoOf(state->op());
  int parameters = state_info.parameter_count();
  int locals = state_info.local_count();
  int stack = state_info.stack_count();

  FrameStateDescriptor* outer_state = nullptr;
  if (state.outer_frame_state()->opcode() == IrOpcode::kFrameState) {
    outer_state = GetFrameStateDescriptorInternal(
        zone, FrameState{state.outer_frame_state()});
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
      state_info.state_combine(), parameters, locals, stack,
      state_info.shared_info(), outer_state);
}

}  // namespace

template <typename Adapter>
FrameStateDescriptor* InstructionSelectorT<Adapter>::GetFrameStateDescriptor(
    FrameState state) {
  auto* desc = GetFrameStateDescriptorInternal(instruction_zone(), state);
  *max_unoptimized_frame_height_ =
      std::max(*max_unoptimized_frame_height_,
               desc->total_conservative_frame_size_in_bytes());
  return desc;
}

#if V8_ENABLE_WEBASSEMBLY
template <typename Adapter>
void InstructionSelectorT<Adapter>::CanonicalizeShuffle(Node* node,
                                                        uint8_t* shuffle,
                                                        bool* is_swizzle) {
  // Get raw shuffle indices.
  memcpy(shuffle, S128ImmediateParameterOf(node->op()).data(), kSimd128Size);
  bool needs_swap;
  bool inputs_equal = GetVirtualRegister(node->InputAt(0)) ==
                      GetVirtualRegister(node->InputAt(1));
  wasm::SimdShuffle::CanonicalizeShuffle(inputs_equal, shuffle, &needs_swap,
                                         is_swizzle);
  if (needs_swap) {
    SwapShuffleInputs(node);
  }
  // Duplicate the first input; for some shuffles on some architectures, it's
  // easiest to implement a swizzle as a shuffle so it might be used.
  if (*is_swizzle) {
    node->ReplaceInput(1, node->InputAt(0));
  }
}

// static
template <typename Adapter>
void InstructionSelectorT<Adapter>::SwapShuffleInputs(Node* node) {
  Node* input0 = node->InputAt(0);
  Node* input1 = node->InputAt(1);
  node->ReplaceInput(0, input1);
  node->ReplaceInput(1, input0);
}
#endif  // V8_ENABLE_WEBASSEMBLY

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    InstructionSelectorT<TurbofanAdapter>;
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    InstructionSelectorT<TurboshaftAdapter>;

}  // namespace compiler
}  // namespace internal
}  // namespace v8
