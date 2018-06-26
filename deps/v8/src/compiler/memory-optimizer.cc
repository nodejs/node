// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/memory-optimizer.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

MemoryOptimizer::MemoryOptimizer(JSGraph* jsgraph, Zone* zone,
                                 PoisoningMitigationLevel poisoning_enabled,
                                 AllocationFolding allocation_folding)
    : jsgraph_(jsgraph),
      empty_state_(AllocationState::Empty(zone)),
      pending_(zone),
      tokens_(zone),
      zone_(zone),
      graph_assembler_(jsgraph, nullptr, nullptr, zone),
      poisoning_enabled_(poisoning_enabled),
      allocation_folding_(allocation_folding) {}

void MemoryOptimizer::Optimize() {
  EnqueueUses(graph()->start(), empty_state());
  while (!tokens_.empty()) {
    Token const token = tokens_.front();
    tokens_.pop();
    VisitNode(token.node, token.state);
  }
  DCHECK(pending_.empty());
  DCHECK(tokens_.empty());
}

MemoryOptimizer::AllocationGroup::AllocationGroup(Node* node,
                                                  PretenureFlag pretenure,
                                                  Zone* zone)
    : node_ids_(zone), pretenure_(pretenure), size_(nullptr) {
  node_ids_.insert(node->id());
}

MemoryOptimizer::AllocationGroup::AllocationGroup(Node* node,
                                                  PretenureFlag pretenure,
                                                  Node* size, Zone* zone)
    : node_ids_(zone), pretenure_(pretenure), size_(size) {
  node_ids_.insert(node->id());
}

void MemoryOptimizer::AllocationGroup::Add(Node* node) {
  node_ids_.insert(node->id());
}

bool MemoryOptimizer::AllocationGroup::Contains(Node* node) const {
  return node_ids_.find(node->id()) != node_ids_.end();
}

MemoryOptimizer::AllocationState::AllocationState()
    : group_(nullptr), size_(std::numeric_limits<int>::max()), top_(nullptr) {}

MemoryOptimizer::AllocationState::AllocationState(AllocationGroup* group)
    : group_(group), size_(std::numeric_limits<int>::max()), top_(nullptr) {}

MemoryOptimizer::AllocationState::AllocationState(AllocationGroup* group,
                                                  int size, Node* top)
    : group_(group), size_(size), top_(top) {}

bool MemoryOptimizer::AllocationState::IsNewSpaceAllocation() const {
  return group() && group()->IsNewSpaceAllocation();
}

void MemoryOptimizer::VisitNode(Node* node, AllocationState const* state) {
  DCHECK(!node->IsDead());
  DCHECK_LT(0, node->op()->EffectInputCount());
  switch (node->opcode()) {
    case IrOpcode::kAllocate:
      // Allocate nodes were purged from the graph in effect-control
      // linearization.
      UNREACHABLE();
    case IrOpcode::kAllocateRaw:
      return VisitAllocateRaw(node, state);
    case IrOpcode::kCall:
      return VisitCall(node, state);
    case IrOpcode::kCallWithCallerSavedRegisters:
      return VisitCallWithCallerSavedRegisters(node, state);
    case IrOpcode::kLoadElement:
      return VisitLoadElement(node, state);
    case IrOpcode::kLoadField:
      return VisitLoadField(node, state);
    case IrOpcode::kStoreElement:
      return VisitStoreElement(node, state);
    case IrOpcode::kStoreField:
      return VisitStoreField(node, state);
    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless:
    case IrOpcode::kIfException:
    case IrOpcode::kLoad:
    case IrOpcode::kProtectedLoad:
    case IrOpcode::kStore:
    case IrOpcode::kProtectedStore:
    case IrOpcode::kRetain:
    case IrOpcode::kUnsafePointerAdd:
    case IrOpcode::kDebugBreak:
    case IrOpcode::kUnreachable:
      return VisitOtherEffect(node, state);
    default:
      break;
  }
  DCHECK_EQ(0, node->op()->EffectOutputCount());
}

#define __ gasm()->

void MemoryOptimizer::VisitAllocateRaw(Node* node,
                                       AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kAllocateRaw, node->opcode());
  Node* value;
  Node* size = node->InputAt(0);
  Node* effect = node->InputAt(1);
  Node* control = node->InputAt(2);

  gasm()->Reset(effect, control);

  PretenureFlag pretenure = PretenureFlagOf(node->op());

  // Propagate tenuring from outer allocations to inner allocations, i.e.
  // when we allocate an object in old space and store a newly allocated
  // child object into the pretenured object, then the newly allocated
  // child object also should get pretenured to old space.
  if (pretenure == TENURED) {
    for (Edge const edge : node->use_edges()) {
      Node* const user = edge.from();
      if (user->opcode() == IrOpcode::kStoreField && edge.index() == 0) {
        Node* const child = user->InputAt(1);
        if (child->opcode() == IrOpcode::kAllocateRaw &&
            PretenureFlagOf(child->op()) == NOT_TENURED) {
          NodeProperties::ChangeOp(child, node->op());
          break;
        }
      }
    }
  } else {
    DCHECK_EQ(NOT_TENURED, pretenure);
    for (Edge const edge : node->use_edges()) {
      Node* const user = edge.from();
      if (user->opcode() == IrOpcode::kStoreField && edge.index() == 1) {
        Node* const parent = user->InputAt(0);
        if (parent->opcode() == IrOpcode::kAllocateRaw &&
            PretenureFlagOf(parent->op()) == TENURED) {
          pretenure = TENURED;
          break;
        }
      }
    }
  }

  // Determine the top/limit addresses.
  Node* top_address = __ ExternalConstant(
      pretenure == NOT_TENURED
          ? ExternalReference::new_space_allocation_top_address(isolate())
          : ExternalReference::old_space_allocation_top_address(isolate()));
  Node* limit_address = __ ExternalConstant(
      pretenure == NOT_TENURED
          ? ExternalReference::new_space_allocation_limit_address(isolate())
          : ExternalReference::old_space_allocation_limit_address(isolate()));

  // Check if we can fold this allocation into a previous allocation represented
  // by the incoming {state}.
  Int32Matcher m(size);
  if (m.HasValue() && m.Value() < kMaxRegularHeapObjectSize) {
    int32_t const object_size = m.Value();
    if (allocation_folding_ == AllocationFolding::kDoAllocationFolding &&
        state->size() <= kMaxRegularHeapObjectSize - object_size &&
        state->group()->pretenure() == pretenure) {
      // We can fold this Allocate {node} into the allocation {group}
      // represented by the given {state}. Compute the upper bound for
      // the new {state}.
      int32_t const state_size = state->size() + object_size;

      // Update the reservation check to the actual maximum upper bound.
      AllocationGroup* const group = state->group();
      if (OpParameter<int32_t>(group->size()->op()) < state_size) {
        NodeProperties::ChangeOp(group->size(),
                                 common()->Int32Constant(state_size));
      }

      // Update the allocation top with the new object allocation.
      // TODO(bmeurer): Defer writing back top as much as possible.
      Node* top = __ IntAdd(state->top(), __ IntPtrConstant(object_size));
      __ Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                   kNoWriteBarrier),
               top_address, __ IntPtrConstant(0), top);

      // Compute the effective inner allocated address.
      value = __ BitcastWordToTagged(
          __ IntAdd(state->top(), __ IntPtrConstant(kHeapObjectTag)));

      // Extend the allocation {group}.
      group->Add(value);
      state = AllocationState::Open(group, state_size, top, zone());
    } else {
      auto call_runtime = __ MakeDeferredLabel();
      auto done = __ MakeLabel(MachineType::PointerRepresentation());

      // Setup a mutable reservation size node; will be patched as we fold
      // additional allocations into this new group.
      Node* size = __ UniqueInt32Constant(object_size);

      // Load allocation top and limit.
      Node* top =
          __ Load(MachineType::Pointer(), top_address, __ IntPtrConstant(0));
      Node* limit =
          __ Load(MachineType::Pointer(), limit_address, __ IntPtrConstant(0));

      // Check if we need to collect garbage before we can start bump pointer
      // allocation (always done for folded allocations).
      Node* check = __ UintLessThan(
          __ IntAdd(top,
                    machine()->Is64() ? __ ChangeInt32ToInt64(size) : size),
          limit);

      __ GotoIfNot(check, &call_runtime);
      __ Goto(&done, top);

      __ Bind(&call_runtime);
      {
        Node* target =
            pretenure == NOT_TENURED ? __ AllocateInNewSpaceStubConstant()
                                     : __
                                       AllocateInOldSpaceStubConstant();
        if (!allocate_operator_.is_set()) {
          auto call_descriptor =
              Linkage::GetAllocateCallDescriptor(graph()->zone());
          allocate_operator_.set(common()->Call(call_descriptor));
        }
        Node* vfalse = __ Call(allocate_operator_.get(), target, size);
        vfalse = __ IntSub(vfalse, __ IntPtrConstant(kHeapObjectTag));
        __ Goto(&done, vfalse);
      }

      __ Bind(&done);

      // Compute the new top and write it back.
      top = __ IntAdd(done.PhiAt(0), __ IntPtrConstant(object_size));
      __ Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                   kNoWriteBarrier),
               top_address, __ IntPtrConstant(0), top);

      // Compute the initial object address.
      value = __ BitcastWordToTagged(
          __ IntAdd(done.PhiAt(0), __ IntPtrConstant(kHeapObjectTag)));

      // Start a new allocation group.
      AllocationGroup* group =
          new (zone()) AllocationGroup(value, pretenure, size, zone());
      state = AllocationState::Open(group, object_size, top, zone());
    }
  } else {
    auto call_runtime = __ MakeDeferredLabel();
    auto done = __ MakeLabel(MachineRepresentation::kTaggedPointer);

    // Load allocation top and limit.
    Node* top =
        __ Load(MachineType::Pointer(), top_address, __ IntPtrConstant(0));
    Node* limit =
        __ Load(MachineType::Pointer(), limit_address, __ IntPtrConstant(0));

    // Compute the new top.
    Node* new_top =
        __ IntAdd(top, machine()->Is64() ? __ ChangeInt32ToInt64(size) : size);

    // Check if we can do bump pointer allocation here.
    Node* check = __ UintLessThan(new_top, limit);
    __ GotoIfNot(check, &call_runtime);
    __ Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                 kNoWriteBarrier),
             top_address, __ IntPtrConstant(0), new_top);
    __ Goto(&done, __ BitcastWordToTagged(
                       __ IntAdd(top, __ IntPtrConstant(kHeapObjectTag))));

    __ Bind(&call_runtime);
    Node* target =
        pretenure == NOT_TENURED ? __ AllocateInNewSpaceStubConstant()
                                 : __
                                   AllocateInOldSpaceStubConstant();
    if (!allocate_operator_.is_set()) {
      auto call_descriptor =
          Linkage::GetAllocateCallDescriptor(graph()->zone());
      allocate_operator_.set(common()->Call(call_descriptor));
    }
    __ Goto(&done, __ Call(allocate_operator_.get(), target, size));

    __ Bind(&done);
    value = done.PhiAt(0);

    // Create an unfoldable allocation group.
    AllocationGroup* group =
        new (zone()) AllocationGroup(value, pretenure, zone());
    state = AllocationState::Closed(group, zone());
  }

  effect = __ ExtractCurrentEffect();
  control = __ ExtractCurrentControl();

  // Replace all effect uses of {node} with the {effect}, enqueue the
  // effect uses for further processing, and replace all value uses of
  // {node} with the {value}.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) {
      EnqueueUse(edge.from(), edge.index(), state);
      edge.UpdateTo(effect);
    } else if (NodeProperties::IsValueEdge(edge)) {
      edge.UpdateTo(value);
    } else {
      DCHECK(NodeProperties::IsControlEdge(edge));
      edge.UpdateTo(control);
    }
  }

  // Kill the {node} to make sure we don't leave dangling dead uses.
  node->Kill();
}

#undef __

void MemoryOptimizer::VisitCall(Node* node, AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kCall, node->opcode());
  // If the call can allocate, we start with a fresh state.
  if (!(CallDescriptorOf(node->op())->flags() & CallDescriptor::kNoAllocate)) {
    state = empty_state();
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitCallWithCallerSavedRegisters(
    Node* node, AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kCallWithCallerSavedRegisters, node->opcode());
  // If the call can allocate, we start with a fresh state.
  if (!(CallDescriptorOf(node->op())->flags() & CallDescriptor::kNoAllocate)) {
    state = empty_state();
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitLoadElement(Node* node,
                                       AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kLoadElement, node->opcode());
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* index = node->InputAt(1);
  node->ReplaceInput(1, ComputeIndex(access, index));
  if (poisoning_enabled_ == PoisoningMitigationLevel::kOn &&
      access.machine_type.representation() !=
          MachineRepresentation::kTaggedPointer) {
    NodeProperties::ChangeOp(node,
                             machine()->PoisonedLoad(access.machine_type));
  } else {
    NodeProperties::ChangeOp(node, machine()->Load(access.machine_type));
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitLoadField(Node* node, AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kLoadField, node->opcode());
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* offset = jsgraph()->IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph()->zone(), 1, offset);
  if (poisoning_enabled_ == PoisoningMitigationLevel::kOn &&
      access.machine_type.representation() !=
          MachineRepresentation::kTaggedPointer) {
    NodeProperties::ChangeOp(node,
                             machine()->PoisonedLoad(access.machine_type));
  } else {
    NodeProperties::ChangeOp(node, machine()->Load(access.machine_type));
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitStoreElement(Node* node,
                                        AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreElement, node->opcode());
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* object = node->InputAt(0);
  Node* index = node->InputAt(1);
  WriteBarrierKind write_barrier_kind =
      ComputeWriteBarrierKind(object, state, access.write_barrier_kind);
  node->ReplaceInput(1, ComputeIndex(access, index));
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitStoreField(Node* node,
                                      AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreField, node->opcode());
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* object = node->InputAt(0);
  WriteBarrierKind write_barrier_kind =
      ComputeWriteBarrierKind(object, state, access.write_barrier_kind);
  Node* offset = jsgraph()->IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph()->zone(), 1, offset);
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitOtherEffect(Node* node,
                                       AllocationState const* state) {
  EnqueueUses(node, state);
}

Node* MemoryOptimizer::ComputeIndex(ElementAccess const& access, Node* key) {
  Node* index;
  if (machine()->Is64()) {
    // On 64-bit platforms, we need to feed a Word64 index to the Load and
    // Store operators. Since LoadElement or StoreElement don't do any bounds
    // checking themselves, we can be sure that the {key} was already checked
    // and is in valid range, so we can do the further address computation on
    // Word64 below, which ideally allows us to fuse the address computation
    // with the actual memory access operation on Intel platforms.
    index = graph()->NewNode(machine()->ChangeUint32ToUint64(), key);
  } else {
    index = key;
  }
  int const element_size_shift =
      ElementSizeLog2Of(access.machine_type.representation());
  if (element_size_shift) {
    index = graph()->NewNode(machine()->WordShl(), index,
                             jsgraph()->IntPtrConstant(element_size_shift));
  }
  int const fixed_offset = access.header_size - access.tag();
  if (fixed_offset) {
    index = graph()->NewNode(machine()->IntAdd(), index,
                             jsgraph()->IntPtrConstant(fixed_offset));
  }
  return index;
}

WriteBarrierKind MemoryOptimizer::ComputeWriteBarrierKind(
    Node* object, AllocationState const* state,
    WriteBarrierKind write_barrier_kind) {
  if (state->IsNewSpaceAllocation() && state->group()->Contains(object)) {
    write_barrier_kind = kNoWriteBarrier;
  }
  return write_barrier_kind;
}

MemoryOptimizer::AllocationState const* MemoryOptimizer::MergeStates(
    AllocationStates const& states) {
  // Check if all states are the same; or at least if all allocation
  // states belong to the same allocation group.
  AllocationState const* state = states.front();
  AllocationGroup* group = state->group();
  for (size_t i = 1; i < states.size(); ++i) {
    if (states[i] != state) state = nullptr;
    if (states[i]->group() != group) group = nullptr;
  }
  if (state == nullptr) {
    if (group != nullptr) {
      // We cannot fold any more allocations into this group, but we can still
      // eliminate write barriers on stores to this group.
      // TODO(bmeurer): We could potentially just create a Phi here to merge
      // the various tops; but we need to pay special attention not to create
      // an unschedulable graph.
      state = AllocationState::Closed(group, zone());
    } else {
      // The states are from different allocation groups.
      state = empty_state();
    }
  }
  return state;
}

void MemoryOptimizer::EnqueueMerge(Node* node, int index,
                                   AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  int const input_count = node->InputCount() - 1;
  DCHECK_LT(0, input_count);
  Node* const control = node->InputAt(input_count);
  if (control->opcode() == IrOpcode::kLoop) {
    // For loops we always start with an empty state at the beginning.
    if (index == 0) EnqueueUses(node, empty_state());
  } else {
    DCHECK_EQ(IrOpcode::kMerge, control->opcode());
    // Check if we already know about this pending merge.
    NodeId const id = node->id();
    auto it = pending_.find(id);
    if (it == pending_.end()) {
      // Insert a new pending merge.
      it = pending_.insert(std::make_pair(id, AllocationStates(zone()))).first;
    }
    // Add the next input state.
    it->second.push_back(state);
    // Check if states for all inputs are available by now.
    if (it->second.size() == static_cast<size_t>(input_count)) {
      // All inputs to this effect merge are done, merge the states given all
      // input constraints, drop the pending merge and enqueue uses of the
      // EffectPhi {node}.
      state = MergeStates(it->second);
      EnqueueUses(node, state);
      pending_.erase(it);
    }
  }
}

void MemoryOptimizer::EnqueueUses(Node* node, AllocationState const* state) {
  for (Edge const edge : node->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) {
      EnqueueUse(edge.from(), edge.index(), state);
    }
  }
}

void MemoryOptimizer::EnqueueUse(Node* node, int index,
                                 AllocationState const* state) {
  if (node->opcode() == IrOpcode::kEffectPhi) {
    // An EffectPhi represents a merge of different effect chains, which
    // needs special handling depending on whether the merge is part of a
    // loop or just a normal control join.
    EnqueueMerge(node, index, state);
  } else {
    Token token = {node, state};
    tokens_.push(token);
  }
}

Graph* MemoryOptimizer::graph() const { return jsgraph()->graph(); }

Isolate* MemoryOptimizer::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* MemoryOptimizer::common() const {
  return jsgraph()->common();
}

MachineOperatorBuilder* MemoryOptimizer::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
