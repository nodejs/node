// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector.h"

#include <limits>

#include "src/base/adapters.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/schedule.h"
#include "src/compiler/state-values-utils.h"
#include "src/deoptimizer.h"

namespace v8 {
namespace internal {
namespace compiler {

InstructionSelector::InstructionSelector(
    Zone* zone, size_t node_count, Linkage* linkage,
    InstructionSequence* sequence, Schedule* schedule,
    SourcePositionTable* source_positions, Frame* frame,
    SourcePositionMode source_position_mode, Features features,
    EnableScheduling enable_scheduling,
    EnableSerialization enable_serialization)
    : zone_(zone),
      linkage_(linkage),
      sequence_(sequence),
      source_positions_(source_positions),
      source_position_mode_(source_position_mode),
      features_(features),
      schedule_(schedule),
      current_block_(nullptr),
      instructions_(zone),
      defined_(node_count, false, zone),
      used_(node_count, false, zone),
      effect_level_(node_count, 0, zone),
      virtual_registers_(node_count,
                         InstructionOperand::kInvalidVirtualRegister, zone),
      virtual_register_rename_(zone),
      scheduler_(nullptr),
      enable_scheduling_(enable_scheduling),
      enable_serialization_(enable_serialization),
      frame_(frame),
      instruction_selection_failed_(false) {
  instructions_.reserve(node_count);
}

bool InstructionSelector::SelectInstructions() {
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
    if (instruction_selection_failed()) return false;
  }

  // Schedule the selected instructions.
  if (UseInstructionScheduling()) {
    scheduler_ = new (zone()) InstructionScheduler(zone(), sequence());
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
    while (start-- > end) {
      UpdateRenames(instructions_[start]);
      AddInstruction(instructions_[start]);
    }
    EndBlock(RpoNumber::FromInt(block->rpo_number()));
  }
#if DEBUG
  sequence()->ValidateSSA();
#endif
  return true;
}

void InstructionSelector::StartBlock(RpoNumber rpo) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->StartBlock(rpo);
  } else {
    sequence()->StartBlock(rpo);
  }
}


void InstructionSelector::EndBlock(RpoNumber rpo) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->EndBlock(rpo);
  } else {
    sequence()->EndBlock(rpo);
  }
}


void InstructionSelector::AddInstruction(Instruction* instr) {
  if (UseInstructionScheduling()) {
    DCHECK_NOT_NULL(scheduler_);
    scheduler_->AddInstruction(instr);
  } else {
    sequence()->AddInstruction(instr);
  }
}

Instruction* InstructionSelector::Emit(InstructionCode opcode,
                                       InstructionOperand output,
                                       size_t temp_count,
                                       InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  return Emit(opcode, output_count, &output, 0, nullptr, temp_count, temps);
}


Instruction* InstructionSelector::Emit(InstructionCode opcode,
                                       InstructionOperand output,
                                       InstructionOperand a, size_t temp_count,
                                       InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  return Emit(opcode, output_count, &output, 1, &a, temp_count, temps);
}


Instruction* InstructionSelector::Emit(InstructionCode opcode,
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


Instruction* InstructionSelector::Emit(InstructionCode opcode,
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


Instruction* InstructionSelector::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}


Instruction* InstructionSelector::Emit(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, InstructionOperand c, InstructionOperand d,
    InstructionOperand e, size_t temp_count, InstructionOperand* temps) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b, c, d, e};
  size_t input_count = arraysize(inputs);
  return Emit(opcode, output_count, &output, input_count, inputs, temp_count,
              temps);
}


Instruction* InstructionSelector::Emit(
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


Instruction* InstructionSelector::Emit(
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


Instruction* InstructionSelector::Emit(Instruction* instr) {
  instructions_.push_back(instr);
  return instr;
}


bool InstructionSelector::CanCover(Node* user, Node* node) const {
  // 1. Both {user} and {node} must be in the same basic block.
  if (schedule()->block(node) != schedule()->block(user)) {
    return false;
  }
  // 2. Pure {node}s must be owned by the {user}.
  if (node->op()->HasProperty(Operator::kPure)) {
    return node->OwnedBy(user);
  }
  // 3. Impure {node}s must match the effect level of {user}.
  if (GetEffectLevel(node) != GetEffectLevel(user)) {
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

bool InstructionSelector::IsOnlyUserOfNodeInSameBlock(Node* user,
                                                      Node* node) const {
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

void InstructionSelector::UpdateRenames(Instruction* instruction) {
  for (size_t i = 0; i < instruction->InputCount(); i++) {
    TryRename(instruction->InputAt(i));
  }
}

void InstructionSelector::UpdateRenamesInPhi(PhiInstruction* phi) {
  for (size_t i = 0; i < phi->operands().size(); i++) {
    int vreg = phi->operands()[i];
    int renamed = GetRename(vreg);
    if (vreg != renamed) {
      phi->RenameInput(i, renamed);
    }
  }
}

int InstructionSelector::GetRename(int virtual_register) {
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

void InstructionSelector::TryRename(InstructionOperand* op) {
  if (!op->IsUnallocated()) return;
  int vreg = UnallocatedOperand::cast(op)->virtual_register();
  int rename = GetRename(vreg);
  if (rename != vreg) {
    UnallocatedOperand::cast(op)->set_virtual_register(rename);
  }
}

void InstructionSelector::SetRename(const Node* node, const Node* rename) {
  int vreg = GetVirtualRegister(node);
  if (static_cast<size_t>(vreg) >= virtual_register_rename_.size()) {
    int invalid = InstructionOperand::kInvalidVirtualRegister;
    virtual_register_rename_.resize(vreg + 1, invalid);
  }
  virtual_register_rename_[vreg] = GetVirtualRegister(rename);
}

int InstructionSelector::GetVirtualRegister(const Node* node) {
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


const std::map<NodeId, int> InstructionSelector::GetVirtualRegistersForTesting()
    const {
  std::map<NodeId, int> virtual_registers;
  for (size_t n = 0; n < virtual_registers_.size(); ++n) {
    if (virtual_registers_[n] != InstructionOperand::kInvalidVirtualRegister) {
      NodeId const id = static_cast<NodeId>(n);
      virtual_registers.insert(std::make_pair(id, virtual_registers_[n]));
    }
  }
  return virtual_registers;
}


bool InstructionSelector::IsDefined(Node* node) const {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, defined_.size());
  return defined_[id];
}


void InstructionSelector::MarkAsDefined(Node* node) {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, defined_.size());
  defined_[id] = true;
}


bool InstructionSelector::IsUsed(Node* node) const {
  DCHECK_NOT_NULL(node);
  // TODO(bmeurer): This is a terrible monster hack, but we have to make sure
  // that the Retain is actually emitted, otherwise the GC will mess up.
  if (node->opcode() == IrOpcode::kRetain) return true;
  if (!node->op()->HasProperty(Operator::kEliminatable)) return true;
  size_t const id = node->id();
  DCHECK_LT(id, used_.size());
  return used_[id];
}


void InstructionSelector::MarkAsUsed(Node* node) {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, used_.size());
  used_[id] = true;
}

int InstructionSelector::GetEffectLevel(Node* node) const {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, effect_level_.size());
  return effect_level_[id];
}

void InstructionSelector::SetEffectLevel(Node* node, int effect_level) {
  DCHECK_NOT_NULL(node);
  size_t const id = node->id();
  DCHECK_LT(id, effect_level_.size());
  effect_level_[id] = effect_level;
}

bool InstructionSelector::CanAddressRelativeToRootsRegister() const {
  return enable_serialization_ == kDisableSerialization &&
         CanUseRootsRegister();
}

bool InstructionSelector::CanUseRootsRegister() const {
  return linkage()->GetIncomingDescriptor()->flags() &
         CallDescriptor::kCanUseRoots;
}

void InstructionSelector::MarkAsRepresentation(MachineRepresentation rep,
                                               const InstructionOperand& op) {
  UnallocatedOperand unalloc = UnallocatedOperand::cast(op);
  sequence()->MarkAsRepresentation(rep, unalloc.virtual_register());
}


void InstructionSelector::MarkAsRepresentation(MachineRepresentation rep,
                                               Node* node) {
  sequence()->MarkAsRepresentation(rep, GetVirtualRegister(node));
}

namespace {

InstructionOperand OperandForDeopt(Isolate* isolate, OperandGenerator* g,
                                   Node* input, FrameStateInputKind kind,
                                   MachineRepresentation rep) {
  if (rep == MachineRepresentation::kNone) {
    return g->TempImmediate(FrameStateDescriptor::kImpossibleValue);
  }

  switch (input->opcode()) {
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kNumberConstant:
    case IrOpcode::kFloat32Constant:
    case IrOpcode::kFloat64Constant:
      return g->UseImmediate(input);
    case IrOpcode::kHeapConstant: {
      if (!CanBeTaggedPointer(rep)) {
        // If we have inconsistent static and dynamic types, e.g. if we
        // smi-check a string, we can get here with a heap object that
        // says it is a smi. In that case, we return an invalid instruction
        // operand, which will be interpreted as an optimized-out value.

        // TODO(jarin) Ideally, we should turn the current instruction
        // into an abort (we should never execute it).
        return InstructionOperand();
      }

      Handle<HeapObject> constant = OpParameter<Handle<HeapObject>>(input);
      Heap::RootListIndex root_index;
      if (isolate->heap()->IsRootHandle(constant, &root_index) &&
          root_index == Heap::kOptimizedOutRootIndex) {
        // For an optimized-out object we return an invalid instruction
        // operand, so that we take the fast path for optimized-out values.
        return InstructionOperand();
      }

      return g->UseImmediate(input);
    }
    case IrOpcode::kObjectState:
    case IrOpcode::kTypedObjectState:
      UNREACHABLE();
      break;
    default:
      switch (kind) {
        case FrameStateInputKind::kStackSlot:
          return g->UseUniqueSlot(input);
        case FrameStateInputKind::kAny:
          // Currently deopts "wrap" other operations, so the deopt's inputs
          // are potentially needed untill the end of the deoptimising code.
          return g->UseAnyAtEnd(input);
      }
  }
  UNREACHABLE();
  return InstructionOperand();
}

}  // namespace

class StateObjectDeduplicator {
 public:
  explicit StateObjectDeduplicator(Zone* zone) : objects_(zone) {}
  static const size_t kNotDuplicated = SIZE_MAX;

  size_t GetObjectId(Node* node) {
    for (size_t i = 0; i < objects_.size(); ++i) {
      if (objects_[i] == node) {
        return i;
      }
    }
    return kNotDuplicated;
  }

  size_t InsertObject(Node* node) {
    size_t id = objects_.size();
    objects_.push_back(node);
    return id;
  }

 private:
  ZoneVector<Node*> objects_;
};

// Returns the number of instruction operands added to inputs.
size_t InstructionSelector::AddOperandToStateValueDescriptor(
    StateValueList* values, InstructionOperandVector* inputs,
    OperandGenerator* g, StateObjectDeduplicator* deduplicator, Node* input,
    MachineType type, FrameStateInputKind kind, Zone* zone) {
  if (input == nullptr) {
    values->PushOptimizedOut();
    return 0;
  }

  switch (input->opcode()) {
    case IrOpcode::kObjectState: {
      UNREACHABLE();
      return 0;
    }
    case IrOpcode::kTypedObjectState: {
      size_t id = deduplicator->GetObjectId(input);
      if (id == StateObjectDeduplicator::kNotDuplicated) {
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
        // Crankshaft counts duplicate objects for the running id, so we have
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


// Returns the number of instruction operands added to inputs.
size_t InstructionSelector::AddInputsToFrameStateDescriptor(
    FrameStateDescriptor* descriptor, Node* state, OperandGenerator* g,
    StateObjectDeduplicator* deduplicator, InstructionOperandVector* inputs,
    FrameStateInputKind kind, Zone* zone) {
  DCHECK_EQ(IrOpcode::kFrameState, state->op()->opcode());

  size_t entries = 0;
  size_t initial_size = inputs->size();
  USE(initial_size);  // initial_size is only used for debug.

  if (descriptor->outer_state()) {
    entries += AddInputsToFrameStateDescriptor(
        descriptor->outer_state(), state->InputAt(kFrameStateOuterStateInput),
        g, deduplicator, inputs, kind, zone);
  }

  Node* parameters = state->InputAt(kFrameStateParametersInput);
  Node* locals = state->InputAt(kFrameStateLocalsInput);
  Node* stack = state->InputAt(kFrameStateStackInput);
  Node* context = state->InputAt(kFrameStateContextInput);
  Node* function = state->InputAt(kFrameStateFunctionInput);

  DCHECK_EQ(descriptor->parameters_count(),
            StateValuesAccess(parameters).size());
  DCHECK_EQ(descriptor->locals_count(), StateValuesAccess(locals).size());
  DCHECK_EQ(descriptor->stack_count(), StateValuesAccess(stack).size());

  StateValueList* values_descriptor = descriptor->GetStateValueDescriptors();

  DCHECK_EQ(values_descriptor->size(), 0u);
  values_descriptor->ReserveSize(
      descriptor->GetSize(OutputFrameStateCombine::Ignore()));

  entries += AddOperandToStateValueDescriptor(
      values_descriptor, inputs, g, deduplicator, function,
      MachineType::AnyTagged(), FrameStateInputKind::kStackSlot, zone);
  for (StateValuesAccess::TypedNode input_node :
       StateValuesAccess(parameters)) {
    entries += AddOperandToStateValueDescriptor(values_descriptor, inputs, g,
                                                deduplicator, input_node.node,
                                                input_node.type, kind, zone);
  }
  if (descriptor->HasContext()) {
    entries += AddOperandToStateValueDescriptor(
        values_descriptor, inputs, g, deduplicator, context,
        MachineType::AnyTagged(), FrameStateInputKind::kStackSlot, zone);
  }
  for (StateValuesAccess::TypedNode input_node : StateValuesAccess(locals)) {
    entries += AddOperandToStateValueDescriptor(values_descriptor, inputs, g,
                                                deduplicator, input_node.node,
                                                input_node.type, kind, zone);
  }
  for (StateValuesAccess::TypedNode input_node : StateValuesAccess(stack)) {
    entries += AddOperandToStateValueDescriptor(values_descriptor, inputs, g,
                                                deduplicator, input_node.node,
                                                input_node.type, kind, zone);
  }
  DCHECK_EQ(initial_size + entries, inputs->size());
  return entries;
}


// An internal helper class for generating the operands to calls.
// TODO(bmeurer): Get rid of the CallBuffer business and make
// InstructionSelector::VisitCall platform independent instead.
struct CallBuffer {
  CallBuffer(Zone* zone, const CallDescriptor* descriptor,
             FrameStateDescriptor* frame_state)
      : descriptor(descriptor),
        frame_state_descriptor(frame_state),
        output_nodes(zone),
        outputs(zone),
        instruction_args(zone),
        pushed_nodes(zone) {
    output_nodes.reserve(descriptor->ReturnCount());
    outputs.reserve(descriptor->ReturnCount());
    pushed_nodes.reserve(input_count());
    instruction_args.reserve(input_count() + frame_state_value_count());
  }


  const CallDescriptor* descriptor;
  FrameStateDescriptor* frame_state_descriptor;
  NodeVector output_nodes;
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
void InstructionSelector::InitializeCallBuffer(Node* call, CallBuffer* buffer,
                                               CallBufferFlags flags,
                                               int stack_param_delta) {
  OperandGenerator g(this);
  DCHECK_LE(call->op()->ValueOutputCount(),
            static_cast<int>(buffer->descriptor->ReturnCount()));
  DCHECK_EQ(
      call->op()->ValueInputCount(),
      static_cast<int>(buffer->input_count() + buffer->frame_state_count()));

  if (buffer->descriptor->ReturnCount() > 0) {
    // Collect the projections that represent multiple outputs from this call.
    if (buffer->descriptor->ReturnCount() == 1) {
      buffer->output_nodes.push_back(call);
    } else {
      buffer->output_nodes.resize(buffer->descriptor->ReturnCount(), nullptr);
      for (auto use : call->uses()) {
        if (use->opcode() != IrOpcode::kProjection) continue;
        size_t const index = ProjectionIndexOf(use->op());
        DCHECK_LT(index, buffer->output_nodes.size());
        DCHECK(!buffer->output_nodes[index]);
        buffer->output_nodes[index] = use;
      }
    }

    // Filter out the outputs that aren't live because no projection uses them.
    size_t outputs_needed_by_framestate =
        buffer->frame_state_descriptor == nullptr
            ? 0
            : buffer->frame_state_descriptor->state_combine()
                  .ConsumedOutputCount();
    for (size_t i = 0; i < buffer->output_nodes.size(); i++) {
      bool output_is_live = buffer->output_nodes[i] != nullptr ||
                            i < outputs_needed_by_framestate;
      if (output_is_live) {
        MachineRepresentation rep =
            buffer->descriptor->GetReturnType(static_cast<int>(i))
                .representation();
        LinkageLocation location =
            buffer->descriptor->GetReturnLocation(static_cast<int>(i));

        Node* output = buffer->output_nodes[i];
        InstructionOperand op = output == nullptr
                                    ? g.TempLocation(location)
                                    : g.DefineAsLocation(output, location);
        MarkAsRepresentation(rep, op);

        buffer->outputs.push_back(op);
      }
    }
  }

  // The first argument is always the callee code.
  Node* callee = call->InputAt(0);
  bool call_code_immediate = (flags & kCallCodeImmediate) != 0;
  bool call_address_immediate = (flags & kCallAddressImmediate) != 0;
  switch (buffer->descriptor->kind()) {
    case CallDescriptor::kCallCodeObject:
      buffer->instruction_args.push_back(
          (call_code_immediate && callee->opcode() == IrOpcode::kHeapConstant)
              ? g.UseImmediate(callee)
              : g.UseRegister(callee));
      break;
    case CallDescriptor::kCallAddress:
      buffer->instruction_args.push_back(
          (call_address_immediate &&
           callee->opcode() == IrOpcode::kExternalConstant)
              ? g.UseImmediate(callee)
              : g.UseRegister(callee));
      break;
    case CallDescriptor::kCallJSFunction:
      buffer->instruction_args.push_back(
          g.UseLocation(callee, buffer->descriptor->GetInputLocation(0)));
      break;
  }
  DCHECK_EQ(1u, buffer->instruction_args.size());

  // If the call needs a frame state, we insert the state information as
  // follows (n is the number of value inputs to the frame state):
  // arg 1               : deoptimization id.
  // arg 2 - arg (n + 1) : value inputs to the frame state.
  size_t frame_state_entries = 0;
  USE(frame_state_entries);  // frame_state_entries is only used for debug.
  if (buffer->frame_state_descriptor != nullptr) {
    Node* frame_state =
        call->InputAt(static_cast<int>(buffer->descriptor->InputCount()));

    // If it was a syntactic tail call we need to drop the current frame and
    // all the frames on top of it that are either an arguments adaptor frame
    // or a tail caller frame.
    if (buffer->descriptor->SupportsTailCalls()) {
      frame_state = NodeProperties::GetFrameStateInput(frame_state);
      buffer->frame_state_descriptor =
          buffer->frame_state_descriptor->outer_state();
      while (buffer->frame_state_descriptor != nullptr &&
             (buffer->frame_state_descriptor->type() ==
                  FrameStateType::kArgumentsAdaptor ||
              buffer->frame_state_descriptor->type() ==
                  FrameStateType::kTailCallerFunction)) {
        frame_state = NodeProperties::GetFrameStateInput(frame_state);
        buffer->frame_state_descriptor =
            buffer->frame_state_descriptor->outer_state();
      }
    }

    int const state_id = sequence()->AddDeoptimizationEntry(
        buffer->frame_state_descriptor, DeoptimizeReason::kNoReason);
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
  bool call_tail = (flags & kCallTail) != 0;
  for (size_t index = 0; index < input_count; ++iter, ++index) {
    DCHECK(iter != call->inputs().end());
    DCHECK((*iter)->op()->opcode() != IrOpcode::kFrameState);
    if (index == 0) continue;  // The first argument (callee) is already done.

    LinkageLocation location = buffer->descriptor->GetInputLocation(index);
    if (call_tail) {
      location = LinkageLocation::ConvertToTailCallerLocation(
          location, stack_param_delta);
    }
    InstructionOperand op = g.UseLocation(*iter, location);
    if (UnallocatedOperand::cast(op).HasFixedSlotPolicy() && !call_tail) {
      int stack_index = -UnallocatedOperand::cast(op).fixed_slot_index() - 1;
      if (static_cast<size_t>(stack_index) >= buffer->pushed_nodes.size()) {
        buffer->pushed_nodes.resize(stack_index + 1);
      }
      PushParameter parameter(*iter, buffer->descriptor->GetInputType(index));
      buffer->pushed_nodes[stack_index] = parameter;
      pushed_count++;
    } else {
      buffer->instruction_args.push_back(op);
    }
  }
  DCHECK_EQ(input_count, buffer->instruction_args.size() + pushed_count -
                             frame_state_entries);
  if (V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK && call_tail &&
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

bool InstructionSelector::IsSourcePositionUsed(Node* node) {
  return (source_position_mode_ == kAllSourcePositions ||
          node->opcode() == IrOpcode::kCall ||
          node->opcode() == IrOpcode::kTrapIf ||
          node->opcode() == IrOpcode::kTrapUnless);
}

void InstructionSelector::VisitBlock(BasicBlock* block) {
  DCHECK(!current_block_);
  current_block_ = block;
  auto current_num_instructions = [&] {
    DCHECK_GE(kMaxInt, instructions_.size());
    return static_cast<int>(instructions_.size());
  };
  int current_block_end = current_num_instructions();

  int effect_level = 0;
  for (Node* const node : *block) {
    if (node->opcode() == IrOpcode::kStore ||
        node->opcode() == IrOpcode::kUnalignedStore ||
        node->opcode() == IrOpcode::kCheckedStore ||
        node->opcode() == IrOpcode::kCall ||
        node->opcode() == IrOpcode::kProtectedLoad ||
        node->opcode() == IrOpcode::kProtectedStore) {
      ++effect_level;
    }
    SetEffectLevel(node, effect_level);
  }

  // We visit the control first, then the nodes in the block, so the block's
  // control input should be on the same effect level as the last node.
  if (block->control_input() != nullptr) {
    SetEffectLevel(block->control_input(), effect_level);
  }

  auto FinishEmittedInstructions = [&](Node* node, int instruction_start) {
    if (instruction_selection_failed()) return false;
    if (current_num_instructions() == instruction_start) return true;
    std::reverse(instructions_.begin() + instruction_start,
                 instructions_.end());
    if (!node) return true;
    SourcePosition source_position = source_positions_->GetSourcePosition(node);
    if (source_position.IsKnown() && IsSourcePositionUsed(node)) {
      sequence()->SetSourcePosition(instructions_[instruction_start],
                                    source_position);
    }
    return true;
  };

  // Generate code for the block control "top down", but schedule the code
  // "bottom up".
  VisitControl(block);
  if (!FinishEmittedInstructions(block->control_input(), current_block_end))
    return;

  // Visit code in reverse control flow order, because architecture-specific
  // matching may cover more than one node at a time.
  for (auto node : base::Reversed(*block)) {
    // Skip nodes that are unused or already defined.
    if (!IsUsed(node) || IsDefined(node)) continue;
    // Generate code for this node "top down", but schedule the code "bottom
    // up".
    int current_node_end = current_num_instructions();
    VisitNode(node);
    if (!FinishEmittedInstructions(node, current_node_end)) return;
  }

  // We're done with the block.
  InstructionBlock* instruction_block =
      sequence()->InstructionBlockAt(RpoNumber::FromInt(block->rpo_number()));
  instruction_block->set_code_start(static_cast<int>(instructions_.size()));
  instruction_block->set_code_end(current_block_end);

  current_block_ = nullptr;
}


void InstructionSelector::VisitControl(BasicBlock* block) {
#ifdef DEBUG
  // SSA deconstruction requires targets of branches not to have phis.
  // Edge split form guarantees this property, but is more strict.
  if (block->SuccessorCount() > 1) {
    for (BasicBlock* const successor : block->successors()) {
      for (Node* const node : *successor) {
        CHECK(!IrOpcode::IsPhiOpcode(node->opcode()));
      }
    }
  }
#endif

  Node* input = block->control_input();
  switch (block->control()) {
    case BasicBlock::kGoto:
      return VisitGoto(block->SuccessorAt(0));
    case BasicBlock::kCall: {
      DCHECK_EQ(IrOpcode::kCall, input->opcode());
      BasicBlock* success = block->SuccessorAt(0);
      BasicBlock* exception = block->SuccessorAt(1);
      return VisitCall(input, exception), VisitGoto(success);
    }
    case BasicBlock::kTailCall: {
      DCHECK_EQ(IrOpcode::kTailCall, input->opcode());
      return VisitTailCall(input);
    }
    case BasicBlock::kBranch: {
      DCHECK_EQ(IrOpcode::kBranch, input->opcode());
      BasicBlock* tbranch = block->SuccessorAt(0);
      BasicBlock* fbranch = block->SuccessorAt(1);
      if (tbranch == fbranch) return VisitGoto(tbranch);
      return VisitBranch(input, tbranch, fbranch);
    }
    case BasicBlock::kSwitch: {
      DCHECK_EQ(IrOpcode::kSwitch, input->opcode());
      SwitchInfo sw;
      // Last successor must be Default.
      sw.default_branch = block->successors().back();
      DCHECK_EQ(IrOpcode::kIfDefault, sw.default_branch->front()->opcode());
      // All other successors must be cases.
      sw.case_count = block->SuccessorCount() - 1;
      sw.case_branches = &block->successors().front();
      // Determine case values and their min/max.
      sw.case_values = zone()->NewArray<int32_t>(sw.case_count);
      sw.min_value = std::numeric_limits<int32_t>::max();
      sw.max_value = std::numeric_limits<int32_t>::min();
      for (size_t index = 0; index < sw.case_count; ++index) {
        BasicBlock* branch = sw.case_branches[index];
        int32_t value = OpParameter<int32_t>(branch->front()->op());
        sw.case_values[index] = value;
        if (sw.min_value > value) sw.min_value = value;
        if (sw.max_value < value) sw.max_value = value;
      }
      DCHECK_LE(sw.min_value, sw.max_value);
      // Note that {value_range} can be 0 if {min_value} is -2^31 and
      // {max_value}
      // is 2^31-1, so don't assume that it's non-zero below.
      sw.value_range = 1u + bit_cast<uint32_t>(sw.max_value) -
                       bit_cast<uint32_t>(sw.min_value);
      return VisitSwitch(input, sw);
    }
    case BasicBlock::kReturn: {
      DCHECK_EQ(IrOpcode::kReturn, input->opcode());
      return VisitReturn(input);
    }
    case BasicBlock::kDeoptimize: {
      DeoptimizeParameters p = DeoptimizeParametersOf(input->op());
      Node* value = input->InputAt(0);
      return VisitDeoptimize(p.kind(), p.reason(), value);
    }
    case BasicBlock::kThrow:
      DCHECK_EQ(IrOpcode::kThrow, input->opcode());
      return VisitThrow(input->InputAt(0));
    case BasicBlock::kNone: {
      // Exit block doesn't have control.
      DCHECK_NULL(input);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}

void InstructionSelector::MarkPairProjectionsAsWord32(Node* node) {
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  if (projection0) {
    MarkAsWord32(projection0);
  }
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    MarkAsWord32(projection1);
  }
}

void InstructionSelector::VisitNode(Node* node) {
  DCHECK_NOT_NULL(schedule()->block(node));  // should only use scheduled nodes.
  switch (node->opcode()) {
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
      return MarkAsReference(node), VisitIfException(node);
    case IrOpcode::kFinishRegion:
      return MarkAsReference(node), VisitFinishRegion(node);
    case IrOpcode::kParameter: {
      MachineType type =
          linkage()->GetParameterType(ParameterIndexOf(node->op()));
      MarkAsRepresentation(type.representation(), node);
      return VisitParameter(node);
    }
    case IrOpcode::kOsrValue:
      return MarkAsReference(node), VisitOsrValue(node);
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
    case IrOpcode::kExternalConstant:
    case IrOpcode::kRelocatableInt32Constant:
    case IrOpcode::kRelocatableInt64Constant:
      return VisitConstant(node);
    case IrOpcode::kFloat32Constant:
      return MarkAsFloat32(node), VisitConstant(node);
    case IrOpcode::kFloat64Constant:
      return MarkAsFloat64(node), VisitConstant(node);
    case IrOpcode::kHeapConstant:
      return MarkAsReference(node), VisitConstant(node);
    case IrOpcode::kNumberConstant: {
      double value = OpParameter<double>(node);
      if (!IsSmiDouble(value)) MarkAsReference(node);
      return VisitConstant(node);
    }
    case IrOpcode::kCall:
      return VisitCall(node);
    case IrOpcode::kDeoptimizeIf:
      return VisitDeoptimizeIf(node);
    case IrOpcode::kDeoptimizeUnless:
      return VisitDeoptimizeUnless(node);
    case IrOpcode::kTrapIf:
      return VisitTrapIf(node, static_cast<Runtime::FunctionId>(
                                   OpParameter<int32_t>(node->op())));
    case IrOpcode::kTrapUnless:
      return VisitTrapUnless(node, static_cast<Runtime::FunctionId>(
                                       OpParameter<int32_t>(node->op())));
    case IrOpcode::kFrameState:
    case IrOpcode::kStateValues:
    case IrOpcode::kObjectState:
      return;
    case IrOpcode::kDebugBreak:
      VisitDebugBreak(node);
      return;
    case IrOpcode::kComment:
      VisitComment(node);
      return;
    case IrOpcode::kRetain:
      VisitRetain(node);
      return;
    case IrOpcode::kLoad: {
      LoadRepresentation type = LoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitLoad(node);
    }
    case IrOpcode::kStore:
      return VisitStore(node);
    case IrOpcode::kProtectedStore:
      return VisitProtectedStore(node);
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
    case IrOpcode::kWord32Popcnt:
      return MarkAsWord32(node), VisitWord32Popcnt(node);
    case IrOpcode::kWord64Popcnt:
      return MarkAsWord32(node), VisitWord64Popcnt(node);
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
    case IrOpcode::kWord64Equal:
      return VisitWord64Equal(node);
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
      return MarkAsRepresentation(MachineType::PointerRepresentation(), node),
             VisitBitcastTaggedToWord(node);
    case IrOpcode::kBitcastWordToTagged:
      return MarkAsReference(node), VisitBitcastWordToTagged(node);
    case IrOpcode::kBitcastWordToTaggedSigned:
      return MarkAsRepresentation(MachineRepresentation::kTaggedSigned, node),
             EmitIdentity(node);
    case IrOpcode::kChangeFloat32ToFloat64:
      return MarkAsFloat64(node), VisitChangeFloat32ToFloat64(node);
    case IrOpcode::kChangeInt32ToFloat64:
      return MarkAsFloat64(node), VisitChangeInt32ToFloat64(node);
    case IrOpcode::kChangeUint32ToFloat64:
      return MarkAsFloat64(node), VisitChangeUint32ToFloat64(node);
    case IrOpcode::kChangeFloat64ToInt32:
      return MarkAsWord32(node), VisitChangeFloat64ToInt32(node);
    case IrOpcode::kChangeFloat64ToUint32:
      return MarkAsWord32(node), VisitChangeFloat64ToUint32(node);
    case IrOpcode::kFloat64SilenceNaN:
      MarkAsFloat64(node);
      if (CanProduceSignalingNaN(node->InputAt(0))) {
        return VisitFloat64SilenceNaN(node);
      } else {
        return EmitIdentity(node);
      }
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
    case IrOpcode::kLoadStackPointer:
      return VisitLoadStackPointer(node);
    case IrOpcode::kLoadFramePointer:
      return VisitLoadFramePointer(node);
    case IrOpcode::kLoadParentFramePointer:
      return VisitLoadParentFramePointer(node);
    case IrOpcode::kUnalignedLoad: {
      UnalignedLoadRepresentation type =
          UnalignedLoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitUnalignedLoad(node);
    }
    case IrOpcode::kUnalignedStore:
      return VisitUnalignedStore(node);
    case IrOpcode::kCheckedLoad: {
      MachineRepresentation rep =
          CheckedLoadRepresentationOf(node->op()).representation();
      MarkAsRepresentation(rep, node);
      return VisitCheckedLoad(node);
    }
    case IrOpcode::kCheckedStore:
      return VisitCheckedStore(node);
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
    case IrOpcode::kAtomicLoad: {
      LoadRepresentation type = LoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitAtomicLoad(node);
    }
    case IrOpcode::kAtomicStore:
      return VisitAtomicStore(node);
    case IrOpcode::kProtectedLoad: {
      LoadRepresentation type = LoadRepresentationOf(node->op());
      MarkAsRepresentation(type.representation(), node);
      return VisitProtectedLoad(node);
    }
    case IrOpcode::kUnsafePointerAdd:
      MarkAsRepresentation(MachineType::PointerRepresentation(), node);
      return VisitUnsafePointerAdd(node);
    case IrOpcode::kCreateFloat32x4:
      return MarkAsSimd128(node), VisitCreateFloat32x4(node);
    case IrOpcode::kFloat32x4ExtractLane:
      return MarkAsFloat32(node), VisitFloat32x4ExtractLane(node);
    case IrOpcode::kFloat32x4ReplaceLane:
      return MarkAsSimd128(node), VisitFloat32x4ReplaceLane(node);
    case IrOpcode::kFloat32x4FromInt32x4:
      return MarkAsSimd128(node), VisitFloat32x4FromInt32x4(node);
    case IrOpcode::kFloat32x4FromUint32x4:
      return MarkAsSimd128(node), VisitFloat32x4FromUint32x4(node);
    case IrOpcode::kFloat32x4Abs:
      return MarkAsSimd128(node), VisitFloat32x4Abs(node);
    case IrOpcode::kFloat32x4Neg:
      return MarkAsSimd128(node), VisitFloat32x4Neg(node);
    case IrOpcode::kFloat32x4Add:
      return MarkAsSimd128(node), VisitFloat32x4Add(node);
    case IrOpcode::kFloat32x4Sub:
      return MarkAsSimd128(node), VisitFloat32x4Sub(node);
    case IrOpcode::kFloat32x4Equal:
      return MarkAsSimd128(node), VisitFloat32x4Equal(node);
    case IrOpcode::kFloat32x4NotEqual:
      return MarkAsSimd128(node), VisitFloat32x4NotEqual(node);
    case IrOpcode::kCreateInt32x4:
      return MarkAsSimd128(node), VisitCreateInt32x4(node);
    case IrOpcode::kInt32x4ExtractLane:
      return MarkAsWord32(node), VisitInt32x4ExtractLane(node);
    case IrOpcode::kInt32x4ReplaceLane:
      return MarkAsSimd128(node), VisitInt32x4ReplaceLane(node);
    case IrOpcode::kInt32x4FromFloat32x4:
      return MarkAsSimd128(node), VisitInt32x4FromFloat32x4(node);
    case IrOpcode::kUint32x4FromFloat32x4:
      return MarkAsSimd128(node), VisitUint32x4FromFloat32x4(node);
    case IrOpcode::kInt32x4Add:
      return MarkAsSimd128(node), VisitInt32x4Add(node);
    case IrOpcode::kInt32x4Sub:
      return MarkAsSimd128(node), VisitInt32x4Sub(node);
    case IrOpcode::kInt32x4Equal:
      return MarkAsSimd128(node), VisitInt32x4Equal(node);
    case IrOpcode::kInt32x4NotEqual:
      return MarkAsSimd128(node), VisitInt32x4NotEqual(node);
    case IrOpcode::kSimd32x4Select:
      return MarkAsSimd128(node), VisitSimd32x4Select(node);
    default:
      V8_Fatal(__FILE__, __LINE__, "Unexpected operator #%d:%s @ node #%d",
               node->opcode(), node->op()->mnemonic(), node->id());
      break;
  }
}

void InstructionSelector::VisitLoadStackPointer(Node* node) {
  OperandGenerator g(this);
  Emit(kArchStackPointer, g.DefineAsRegister(node));
}

void InstructionSelector::VisitLoadFramePointer(Node* node) {
  OperandGenerator g(this);
  Emit(kArchFramePointer, g.DefineAsRegister(node));
}

void InstructionSelector::VisitLoadParentFramePointer(Node* node) {
  OperandGenerator g(this);
  Emit(kArchParentFramePointer, g.DefineAsRegister(node));
}

void InstructionSelector::VisitFloat64Acos(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Acos);
}

void InstructionSelector::VisitFloat64Acosh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Acosh);
}

void InstructionSelector::VisitFloat64Asin(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Asin);
}

void InstructionSelector::VisitFloat64Asinh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Asinh);
}

void InstructionSelector::VisitFloat64Atan(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Atan);
}

void InstructionSelector::VisitFloat64Atanh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Atanh);
}

void InstructionSelector::VisitFloat64Atan2(Node* node) {
  VisitFloat64Ieee754Binop(node, kIeee754Float64Atan2);
}

void InstructionSelector::VisitFloat64Cbrt(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cbrt);
}

void InstructionSelector::VisitFloat64Cos(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cos);
}

void InstructionSelector::VisitFloat64Cosh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Cosh);
}

void InstructionSelector::VisitFloat64Exp(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Exp);
}

void InstructionSelector::VisitFloat64Expm1(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Expm1);
}

void InstructionSelector::VisitFloat64Log(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log);
}

void InstructionSelector::VisitFloat64Log1p(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log1p);
}

void InstructionSelector::VisitFloat64Log2(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log2);
}

void InstructionSelector::VisitFloat64Log10(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Log10);
}

void InstructionSelector::VisitFloat64Pow(Node* node) {
  VisitFloat64Ieee754Binop(node, kIeee754Float64Pow);
}

void InstructionSelector::VisitFloat64Sin(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Sin);
}

void InstructionSelector::VisitFloat64Sinh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Sinh);
}

void InstructionSelector::VisitFloat64Tan(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Tan);
}

void InstructionSelector::VisitFloat64Tanh(Node* node) {
  VisitFloat64Ieee754Unop(node, kIeee754Float64Tanh);
}

void InstructionSelector::EmitTableSwitch(const SwitchInfo& sw,
                                          InstructionOperand& index_operand) {
  OperandGenerator g(this);
  size_t input_count = 2 + sw.value_range;
  auto* inputs = zone()->NewArray<InstructionOperand>(input_count);
  inputs[0] = index_operand;
  InstructionOperand default_operand = g.Label(sw.default_branch);
  std::fill(&inputs[1], &inputs[input_count], default_operand);
  for (size_t index = 0; index < sw.case_count; ++index) {
    size_t value = sw.case_values[index] - sw.min_value;
    BasicBlock* branch = sw.case_branches[index];
    DCHECK_LE(0u, value);
    DCHECK_LT(value + 2, input_count);
    inputs[value + 2] = g.Label(branch);
  }
  Emit(kArchTableSwitch, 0, nullptr, input_count, inputs, 0, nullptr);
}


void InstructionSelector::EmitLookupSwitch(const SwitchInfo& sw,
                                           InstructionOperand& value_operand) {
  OperandGenerator g(this);
  size_t input_count = 2 + sw.case_count * 2;
  auto* inputs = zone()->NewArray<InstructionOperand>(input_count);
  inputs[0] = value_operand;
  inputs[1] = g.Label(sw.default_branch);
  for (size_t index = 0; index < sw.case_count; ++index) {
    int32_t value = sw.case_values[index];
    BasicBlock* branch = sw.case_branches[index];
    inputs[index * 2 + 2 + 0] = g.TempImmediate(value);
    inputs[index * 2 + 2 + 1] = g.Label(branch);
  }
  Emit(kArchLookupSwitch, 0, nullptr, input_count, inputs, 0, nullptr);
}

void InstructionSelector::VisitStackSlot(Node* node) {
  int size = StackSlotSizeOf(node->op());
  int slot = frame_->AllocateSpillSlot(size);
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitBitcastTaggedToWord(Node* node) {
  EmitIdentity(node);
}

void InstructionSelector::VisitBitcastWordToTagged(Node* node) {
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(node->InputAt(0)));
}

// 32 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_32_BIT

void InstructionSelector::VisitWord64And(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Or(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Xor(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Shl(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Shr(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Sar(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Ror(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Clz(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Ctz(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64ReverseBits(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitWord64Popcnt(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitWord64Equal(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Add(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitInt64Sub(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitInt64Mul(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Div(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64LessThan(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitUint64Div(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitInt64Mod(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitUint64LessThan(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitUint64Mod(Node* node) { UNIMPLEMENTED(); }


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  UNIMPLEMENTED();
}


void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  UNIMPLEMENTED();
}
#endif  // V8_TARGET_ARCH_32_BIT

// 64 bit targets do not implement the following instructions.
#if V8_TARGET_ARCH_64_BIT
void InstructionSelector::VisitInt32PairAdd(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitInt32PairSub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitInt32PairMul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitWord32PairShl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitWord32PairShr(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitWord32PairSar(Node* node) { UNIMPLEMENTED(); }
#endif  // V8_TARGET_ARCH_64_BIT

#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM
void InstructionSelector::VisitCreateInt32x4(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitInt32x4ExtractLane(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitInt32x4ReplaceLane(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitInt32x4Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitInt32x4Sub(Node* node) { UNIMPLEMENTED(); }

#endif  // !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM

#if !V8_TARGET_ARCH_ARM
void InstructionSelector::VisitCreateFloat32x4(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitFloat32x4ExtractLane(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitFloat32x4ReplaceLane(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitFloat32x4FromInt32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitFloat32x4FromUint32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitFloat32x4Abs(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitFloat32x4Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitFloat32x4Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitFloat32x4Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitFloat32x4Equal(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitFloat32x4NotEqual(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitInt32x4FromFloat32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitUint32x4FromFloat32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitInt32x4Equal(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitInt32x4NotEqual(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitSimd32x4Select(Node* node) { UNIMPLEMENTED(); }
#endif  // !V8_TARGET_ARCH_ARM

void InstructionSelector::VisitFinishRegion(Node* node) { EmitIdentity(node); }

void InstructionSelector::VisitParameter(Node* node) {
  OperandGenerator g(this);
  int index = ParameterIndexOf(node->op());
  InstructionOperand op =
      linkage()->ParameterHasSecondaryLocation(index)
          ? g.DefineAsDualLocation(
                node, linkage()->GetParameterLocation(index),
                linkage()->GetParameterSecondaryLocation(index))
          : g.DefineAsLocation(node, linkage()->GetParameterLocation(index));

  Emit(kArchNop, op);
}

namespace {
LinkageLocation ExceptionLocation() {
  return LinkageLocation::ForRegister(kReturnRegister0.code(),
                                      MachineType::IntPtr());
}
}

void InstructionSelector::VisitIfException(Node* node) {
  OperandGenerator g(this);
  DCHECK_EQ(IrOpcode::kCall, node->InputAt(1)->opcode());
  Emit(kArchNop, g.DefineAsLocation(node, ExceptionLocation()));
}


void InstructionSelector::VisitOsrValue(Node* node) {
  OperandGenerator g(this);
  int index = OsrValueIndexOf(node->op());
  Emit(kArchNop,
       g.DefineAsLocation(node, linkage()->GetOsrValueLocation(index)));
}


void InstructionSelector::VisitPhi(Node* node) {
  const int input_count = node->op()->ValueInputCount();
  PhiInstruction* phi = new (instruction_zone())
      PhiInstruction(instruction_zone(), GetVirtualRegister(node),
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


void InstructionSelector::VisitProjection(Node* node) {
  OperandGenerator g(this);
  Node* value = node->InputAt(0);
  switch (value->opcode()) {
    case IrOpcode::kInt32AddWithOverflow:
    case IrOpcode::kInt32SubWithOverflow:
    case IrOpcode::kInt32MulWithOverflow:
    case IrOpcode::kInt64AddWithOverflow:
    case IrOpcode::kInt64SubWithOverflow:
    case IrOpcode::kTryTruncateFloat32ToInt64:
    case IrOpcode::kTryTruncateFloat64ToInt64:
    case IrOpcode::kTryTruncateFloat32ToUint64:
    case IrOpcode::kTryTruncateFloat64ToUint64:
    case IrOpcode::kInt32PairAdd:
    case IrOpcode::kInt32PairSub:
    case IrOpcode::kInt32PairMul:
    case IrOpcode::kWord32PairShl:
    case IrOpcode::kWord32PairShr:
    case IrOpcode::kWord32PairSar:
      if (ProjectionIndexOf(node->op()) == 0u) {
        Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
      } else {
        DCHECK(ProjectionIndexOf(node->op()) == 1u);
        MarkAsUsed(value);
      }
      break;
    default:
      break;
  }
}


void InstructionSelector::VisitConstant(Node* node) {
  // We must emit a NOP here because every live range needs a defining
  // instruction in the register allocator.
  OperandGenerator g(this);
  Emit(kArchNop, g.DefineAsConstant(node));
}


void InstructionSelector::VisitCall(Node* node, BasicBlock* handler) {
  OperandGenerator g(this);
  const CallDescriptor* descriptor = CallDescriptorOf(node->op());

  FrameStateDescriptor* frame_state_descriptor = nullptr;
  if (descriptor->NeedsFrameState()) {
    frame_state_descriptor = GetFrameStateDescriptor(
        node->InputAt(static_cast<int>(descriptor->InputCount())));
  }

  CallBuffer buffer(zone(), descriptor, frame_state_descriptor);

  // Compute InstructionOperands for inputs and outputs.
  // TODO(turbofan): on some architectures it's probably better to use
  // the code object in a register if there are multiple uses of it.
  // Improve constant pool and the heuristics in the register allocator
  // for where to emit constants.
  CallBufferFlags call_buffer_flags(kCallCodeImmediate | kCallAddressImmediate);
  InitializeCallBuffer(node, &buffer, call_buffer_flags);

  EmitPrepareArguments(&(buffer.pushed_nodes), descriptor, node);

  // Pass label of exception handler block.
  CallDescriptor::Flags flags = descriptor->flags();
  if (handler) {
    DCHECK_EQ(IrOpcode::kIfException, handler->front()->opcode());
    flags |= CallDescriptor::kHasExceptionHandler;
    buffer.instruction_args.push_back(g.Label(handler));
  }

  bool from_native_stack = linkage()->GetIncomingDescriptor()->UseNativeStack();
  bool to_native_stack = descriptor->UseNativeStack();
  if (from_native_stack != to_native_stack) {
    // (arm64 only) Mismatch in the use of stack pointers. One or the other
    // has to be restored manually by the code generator.
    flags |= to_native_stack ? CallDescriptor::kRestoreJSSP
                             : CallDescriptor::kRestoreCSP;
  }

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode = kArchNop;
  switch (descriptor->kind()) {
    case CallDescriptor::kCallAddress:
      opcode =
          kArchCallCFunction |
          MiscField::encode(static_cast<int>(descriptor->ParameterCount()));
      break;
    case CallDescriptor::kCallCodeObject:
      opcode = kArchCallCodeObject | MiscField::encode(flags);
      break;
    case CallDescriptor::kCallJSFunction:
      opcode = kArchCallJSFunction | MiscField::encode(flags);
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
}


void InstructionSelector::VisitTailCall(Node* node) {
  OperandGenerator g(this);
  CallDescriptor const* descriptor = CallDescriptorOf(node->op());
  DCHECK_NE(0, descriptor->flags() & CallDescriptor::kSupportsTailCalls);

  CallDescriptor* caller = linkage()->GetIncomingDescriptor();
  DCHECK(caller->CanTailCall(node));
  const CallDescriptor* callee = CallDescriptorOf(node->op());
  int stack_param_delta = callee->GetStackParameterDelta(caller);
  CallBuffer buffer(zone(), descriptor, nullptr);

  // Compute InstructionOperands for inputs and outputs.
  CallBufferFlags flags(kCallCodeImmediate | kCallTail);
  if (IsTailCallAddressImmediate()) {
    flags |= kCallAddressImmediate;
  }
  InitializeCallBuffer(node, &buffer, flags, stack_param_delta);

  // Select the appropriate opcode based on the call type.
  InstructionCode opcode;
  InstructionOperandVector temps(zone());
  if (linkage()->GetIncomingDescriptor()->IsJSFunctionCall()) {
    switch (descriptor->kind()) {
      case CallDescriptor::kCallCodeObject:
        opcode = kArchTailCallCodeObjectFromJSFunction;
        break;
      case CallDescriptor::kCallJSFunction:
        opcode = kArchTailCallJSFunctionFromJSFunction;
        break;
      default:
        UNREACHABLE();
        return;
    }
    int temps_count = GetTempsCountForTailCallFromJSFunction();
    for (int i = 0; i < temps_count; i++) {
      temps.push_back(g.TempRegister());
    }
  } else {
    switch (descriptor->kind()) {
      case CallDescriptor::kCallCodeObject:
        opcode = kArchTailCallCodeObject;
        break;
      case CallDescriptor::kCallAddress:
        opcode = kArchTailCallAddress;
        break;
      default:
        UNREACHABLE();
        return;
    }
  }
  opcode |= MiscField::encode(descriptor->flags());

  Emit(kArchPrepareTailCall, g.NoOutput());

  int first_unused_stack_slot =
      (V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0) +
      stack_param_delta;
  buffer.instruction_args.push_back(g.TempImmediate(first_unused_stack_slot));

  // Emit the tailcall instruction.
  Emit(opcode, 0, nullptr, buffer.instruction_args.size(),
       &buffer.instruction_args.front(), temps.size(),
       temps.empty() ? nullptr : &temps.front());
}


void InstructionSelector::VisitGoto(BasicBlock* target) {
  // jump to the next block.
  OperandGenerator g(this);
  Emit(kArchJmp, g.NoOutput(), g.Label(target));
}

void InstructionSelector::VisitReturn(Node* ret) {
  OperandGenerator g(this);
  const int input_count = linkage()->GetIncomingDescriptor()->ReturnCount() == 0
                              ? 1
                              : ret->op()->ValueInputCount();
  DCHECK_GE(input_count, 1);
  auto value_locations = zone()->NewArray<InstructionOperand>(input_count);
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

Instruction* InstructionSelector::EmitDeoptimize(InstructionCode opcode,
                                                 InstructionOperand output,
                                                 InstructionOperand a,
                                                 DeoptimizeReason reason,
                                                 Node* frame_state) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a};
  size_t input_count = arraysize(inputs);
  return EmitDeoptimize(opcode, output_count, &output, input_count, inputs,
                        reason, frame_state);
}

Instruction* InstructionSelector::EmitDeoptimize(
    InstructionCode opcode, InstructionOperand output, InstructionOperand a,
    InstructionOperand b, DeoptimizeReason reason, Node* frame_state) {
  size_t output_count = output.IsInvalid() ? 0 : 1;
  InstructionOperand inputs[] = {a, b};
  size_t input_count = arraysize(inputs);
  return EmitDeoptimize(opcode, output_count, &output, input_count, inputs,
                        reason, frame_state);
}

Instruction* InstructionSelector::EmitDeoptimize(
    InstructionCode opcode, size_t output_count, InstructionOperand* outputs,
    size_t input_count, InstructionOperand* inputs, DeoptimizeReason reason,
    Node* frame_state) {
  OperandGenerator g(this);
  FrameStateDescriptor* const descriptor = GetFrameStateDescriptor(frame_state);
  InstructionOperandVector args(instruction_zone());
  args.reserve(input_count + 1 + descriptor->GetTotalSize());
  for (size_t i = 0; i < input_count; ++i) {
    args.push_back(inputs[i]);
  }
  opcode |= MiscField::encode(static_cast<int>(input_count));
  int const state_id = sequence()->AddDeoptimizationEntry(descriptor, reason);
  args.push_back(g.TempImmediate(state_id));
  StateObjectDeduplicator deduplicator(instruction_zone());
  AddInputsToFrameStateDescriptor(descriptor, frame_state, &g, &deduplicator,
                                  &args, FrameStateInputKind::kAny,
                                  instruction_zone());
  return Emit(opcode, output_count, outputs, args.size(), &args.front(), 0,
              nullptr);
}

void InstructionSelector::EmitIdentity(Node* node) {
  OperandGenerator g(this);
  MarkAsUsed(node->InputAt(0));
  SetRename(node, node->InputAt(0));
}

void InstructionSelector::VisitDeoptimize(DeoptimizeKind kind,
                                          DeoptimizeReason reason,
                                          Node* value) {
  InstructionCode opcode = kArchDeoptimize;
  switch (kind) {
    case DeoptimizeKind::kEager:
      opcode |= MiscField::encode(Deoptimizer::EAGER);
      break;
    case DeoptimizeKind::kSoft:
      opcode |= MiscField::encode(Deoptimizer::SOFT);
      break;
  }
  EmitDeoptimize(opcode, 0, nullptr, 0, nullptr, reason, value);
}


void InstructionSelector::VisitThrow(Node* value) {
  OperandGenerator g(this);
  Emit(kArchThrowTerminator, g.NoOutput());
}

void InstructionSelector::VisitDebugBreak(Node* node) {
  OperandGenerator g(this);
  Emit(kArchDebugBreak, g.NoOutput());
}

void InstructionSelector::VisitComment(Node* node) {
  OperandGenerator g(this);
  InstructionOperand operand(g.UseImmediate(node));
  Emit(kArchComment, 0, nullptr, 1, &operand);
}

void InstructionSelector::VisitUnsafePointerAdd(Node* node) {
#if V8_TARGET_ARCH_64_BIT
  VisitInt64Add(node);
#else   // V8_TARGET_ARCH_64_BIT
  VisitInt32Add(node);
#endif  // V8_TARGET_ARCH_64_BIT
}

void InstructionSelector::VisitRetain(Node* node) {
  OperandGenerator g(this);
  Emit(kArchNop, g.NoOutput(), g.UseAny(node->InputAt(0)));
}

bool InstructionSelector::CanProduceSignalingNaN(Node* node) {
  // TODO(jarin) Improve the heuristic here.
  if (node->opcode() == IrOpcode::kFloat64Add ||
      node->opcode() == IrOpcode::kFloat64Sub ||
      node->opcode() == IrOpcode::kFloat64Mul) {
    return false;
  }
  return true;
}

FrameStateDescriptor* InstructionSelector::GetFrameStateDescriptor(
    Node* state) {
  DCHECK(state->opcode() == IrOpcode::kFrameState);
  DCHECK_EQ(kFrameStateInputCount, state->InputCount());
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(state);

  int parameters = static_cast<int>(
      StateValuesAccess(state->InputAt(kFrameStateParametersInput)).size());
  int locals = static_cast<int>(
      StateValuesAccess(state->InputAt(kFrameStateLocalsInput)).size());
  int stack = static_cast<int>(
      StateValuesAccess(state->InputAt(kFrameStateStackInput)).size());

  DCHECK_EQ(parameters, state_info.parameter_count());
  DCHECK_EQ(locals, state_info.local_count());

  FrameStateDescriptor* outer_state = nullptr;
  Node* outer_node = state->InputAt(kFrameStateOuterStateInput);
  if (outer_node->opcode() == IrOpcode::kFrameState) {
    outer_state = GetFrameStateDescriptor(outer_node);
  }

  return new (instruction_zone()) FrameStateDescriptor(
      instruction_zone(), state_info.type(), state_info.bailout_id(),
      state_info.state_combine(), parameters, locals, stack,
      state_info.shared_info(), outer_state);
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
