// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/memory-optimizer.h"

#include "src/codegen/interface-descriptors.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

MemoryOptimizer::MemoryOptimizer(JSGraph* jsgraph, Zone* zone,
                                 PoisoningMitigationLevel poisoning_level,
                                 AllocationFolding allocation_folding,
                                 const char* function_debug_name)
    : jsgraph_(jsgraph),
      empty_state_(AllocationState::Empty(zone)),
      pending_(zone),
      tokens_(zone),
      zone_(zone),
      graph_assembler_(jsgraph, nullptr, nullptr, zone),
      poisoning_level_(poisoning_level),
      allocation_folding_(allocation_folding),
      function_debug_name_(function_debug_name) {}

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
                                                  AllocationType allocation,
                                                  Zone* zone)
    : node_ids_(zone), allocation_(allocation), size_(nullptr) {
  node_ids_.insert(node->id());
}

MemoryOptimizer::AllocationGroup::AllocationGroup(Node* node,
                                                  AllocationType allocation,
                                                  Node* size, Zone* zone)
    : node_ids_(zone), allocation_(allocation), size_(size) {
  node_ids_.insert(node->id());
}

void MemoryOptimizer::AllocationGroup::Add(Node* node) {
  node_ids_.insert(node->id());
}

bool MemoryOptimizer::AllocationGroup::Contains(Node* node) const {
  // Additions should stay within the same allocated object, so it's safe to
  // ignore them.
  while (node_ids_.find(node->id()) == node_ids_.end()) {
    switch (node->opcode()) {
      case IrOpcode::kBitcastTaggedToWord:
      case IrOpcode::kBitcastWordToTagged:
      case IrOpcode::kInt32Add:
      case IrOpcode::kInt64Add:
        node = NodeProperties::GetValueInput(node, 0);
        break;
      default:
        return false;
    }
  }
  return true;
}

MemoryOptimizer::AllocationState::AllocationState()
    : group_(nullptr), size_(std::numeric_limits<int>::max()), top_(nullptr) {}

MemoryOptimizer::AllocationState::AllocationState(AllocationGroup* group)
    : group_(group), size_(std::numeric_limits<int>::max()), top_(nullptr) {}

MemoryOptimizer::AllocationState::AllocationState(AllocationGroup* group,
                                                  intptr_t size, Node* top)
    : group_(group), size_(size), top_(top) {}

bool MemoryOptimizer::AllocationState::IsYoungGenerationAllocation() const {
  return group() && group()->IsYoungGenerationAllocation();
}

namespace {

bool CanAllocate(const Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kBitcastTaggedToWord:
    case IrOpcode::kBitcastWordToTagged:
    case IrOpcode::kComment:
    case IrOpcode::kDebugAbort:
    case IrOpcode::kDebugBreak:
    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kIfException:
    case IrOpcode::kLoad:
    case IrOpcode::kLoadElement:
    case IrOpcode::kLoadField:
    case IrOpcode::kPoisonedLoad:
    case IrOpcode::kProtectedLoad:
    case IrOpcode::kProtectedStore:
    case IrOpcode::kRetain:
    // TODO(tebbi): Store nodes might do a bump-pointer allocation.
    //              We should introduce a special bump-pointer store node to
    //              differentiate that.
    case IrOpcode::kStore:
    case IrOpcode::kStoreElement:
    case IrOpcode::kStoreField:
    case IrOpcode::kTaggedPoisonOnSpeculation:
    case IrOpcode::kUnalignedLoad:
    case IrOpcode::kUnalignedStore:
    case IrOpcode::kUnsafePointerAdd:
    case IrOpcode::kUnreachable:
    case IrOpcode::kStaticAssert:
    case IrOpcode::kWord32AtomicAdd:
    case IrOpcode::kWord32AtomicAnd:
    case IrOpcode::kWord32AtomicCompareExchange:
    case IrOpcode::kWord32AtomicExchange:
    case IrOpcode::kWord32AtomicLoad:
    case IrOpcode::kWord32AtomicOr:
    case IrOpcode::kWord32AtomicPairAdd:
    case IrOpcode::kWord32AtomicPairAnd:
    case IrOpcode::kWord32AtomicPairCompareExchange:
    case IrOpcode::kWord32AtomicPairExchange:
    case IrOpcode::kWord32AtomicPairLoad:
    case IrOpcode::kWord32AtomicPairOr:
    case IrOpcode::kWord32AtomicPairStore:
    case IrOpcode::kWord32AtomicPairSub:
    case IrOpcode::kWord32AtomicPairXor:
    case IrOpcode::kWord32AtomicStore:
    case IrOpcode::kWord32AtomicSub:
    case IrOpcode::kWord32AtomicXor:
    case IrOpcode::kWord32PoisonOnSpeculation:
    case IrOpcode::kWord64AtomicAdd:
    case IrOpcode::kWord64AtomicAnd:
    case IrOpcode::kWord64AtomicCompareExchange:
    case IrOpcode::kWord64AtomicExchange:
    case IrOpcode::kWord64AtomicLoad:
    case IrOpcode::kWord64AtomicOr:
    case IrOpcode::kWord64AtomicStore:
    case IrOpcode::kWord64AtomicSub:
    case IrOpcode::kWord64AtomicXor:
    case IrOpcode::kWord64PoisonOnSpeculation:
      return false;

    case IrOpcode::kCall:
    case IrOpcode::kCallWithCallerSavedRegisters:
      return !(CallDescriptorOf(node->op())->flags() &
               CallDescriptor::kNoAllocate);
    default:
      break;
  }
  return true;
}

Node* SearchAllocatingNode(Node* start, Node* limit, Zone* temp_zone) {
  ZoneQueue<Node*> queue(temp_zone);
  ZoneSet<Node*> visited(temp_zone);
  visited.insert(limit);
  queue.push(start);

  while (!queue.empty()) {
    Node* const current = queue.front();
    queue.pop();
    if (visited.find(current) == visited.end()) {
      visited.insert(current);

      if (CanAllocate(current)) {
        return current;
      }

      for (int i = 0; i < current->op()->EffectInputCount(); ++i) {
        queue.push(NodeProperties::GetEffectInput(current, i));
      }
    }
  }
  return nullptr;
}

bool CanLoopAllocate(Node* loop_effect_phi, Zone* temp_zone) {
  Node* const control = NodeProperties::GetControlInput(loop_effect_phi);
  // Start the effect chain walk from the loop back edges.
  for (int i = 1; i < control->InputCount(); ++i) {
    if (SearchAllocatingNode(loop_effect_phi->InputAt(i), loop_effect_phi,
                             temp_zone) != nullptr) {
      return true;
    }
  }
  return false;
}

Node* EffectPhiForPhi(Node* phi) {
  Node* control = NodeProperties::GetControlInput(phi);
  for (Node* use : control->uses()) {
    if (use->opcode() == IrOpcode::kEffectPhi) {
      return use;
    }
  }
  return nullptr;
}

}  // namespace

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
    case IrOpcode::kLoadFromObject:
      return VisitLoadFromObject(node, state);
    case IrOpcode::kLoadElement:
      return VisitLoadElement(node, state);
    case IrOpcode::kLoadField:
      return VisitLoadField(node, state);
    case IrOpcode::kStoreToObject:
      return VisitStoreToObject(node, state);
    case IrOpcode::kStoreElement:
      return VisitStoreElement(node, state);
    case IrOpcode::kStoreField:
      return VisitStoreField(node, state);
    case IrOpcode::kStore:
      return VisitStore(node, state);
    default:
      if (!CanAllocate(node)) {
        // These operations cannot trigger GC.
        return VisitOtherEffect(node, state);
      }
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

  const AllocateParameters& allocation = AllocateParametersOf(node->op());
  AllocationType allocation_type = allocation.allocation_type();

  // Propagate tenuring from outer allocations to inner allocations, i.e.
  // when we allocate an object in old space and store a newly allocated
  // child object into the pretenured object, then the newly allocated
  // child object also should get pretenured to old space.
  if (allocation_type == AllocationType::kOld) {
    for (Edge const edge : node->use_edges()) {
      Node* const user = edge.from();
      if (user->opcode() == IrOpcode::kStoreField && edge.index() == 0) {
        Node* const child = user->InputAt(1);
        if (child->opcode() == IrOpcode::kAllocateRaw &&
            AllocationTypeOf(child->op()) == AllocationType::kYoung) {
          NodeProperties::ChangeOp(child, node->op());
          break;
        }
      }
    }
  } else {
    DCHECK_EQ(AllocationType::kYoung, allocation_type);
    for (Edge const edge : node->use_edges()) {
      Node* const user = edge.from();
      if (user->opcode() == IrOpcode::kStoreField && edge.index() == 1) {
        Node* const parent = user->InputAt(0);
        if (parent->opcode() == IrOpcode::kAllocateRaw &&
            AllocationTypeOf(parent->op()) == AllocationType::kOld) {
          allocation_type = AllocationType::kOld;
          break;
        }
      }
    }
  }

  // Determine the top/limit addresses.
  Node* top_address = __ ExternalConstant(
      allocation_type == AllocationType::kYoung
          ? ExternalReference::new_space_allocation_top_address(isolate())
          : ExternalReference::old_space_allocation_top_address(isolate()));
  Node* limit_address = __ ExternalConstant(
      allocation_type == AllocationType::kYoung
          ? ExternalReference::new_space_allocation_limit_address(isolate())
          : ExternalReference::old_space_allocation_limit_address(isolate()));

  // Check if we can fold this allocation into a previous allocation represented
  // by the incoming {state}.
  IntPtrMatcher m(size);
  if (m.IsInRange(0, kMaxRegularHeapObjectSize) && FLAG_inline_new) {
    intptr_t const object_size = m.Value();
    if (allocation_folding_ == AllocationFolding::kDoAllocationFolding &&
        state->size() <= kMaxRegularHeapObjectSize - object_size &&
        state->group()->allocation() == allocation_type) {
      // We can fold this Allocate {node} into the allocation {group}
      // represented by the given {state}. Compute the upper bound for
      // the new {state}.
      intptr_t const state_size = state->size() + object_size;

      // Update the reservation check to the actual maximum upper bound.
      AllocationGroup* const group = state->group();
      if (machine()->Is64()) {
        if (OpParameter<int64_t>(group->size()->op()) < state_size) {
          NodeProperties::ChangeOp(group->size(),
                                   common()->Int64Constant(state_size));
        }
      } else {
        if (OpParameter<int32_t>(group->size()->op()) < state_size) {
          NodeProperties::ChangeOp(
              group->size(),
              common()->Int32Constant(static_cast<int32_t>(state_size)));
        }
      }

      // Update the allocation top with the new object allocation.
      // TODO(bmeurer): Defer writing back top as much as possible.
      Node* top = __ IntAdd(state->top(), size);
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
      Node* size = __ UniqueIntPtrConstant(object_size);

      // Load allocation top and limit.
      Node* top =
          __ Load(MachineType::Pointer(), top_address, __ IntPtrConstant(0));
      Node* limit =
          __ Load(MachineType::Pointer(), limit_address, __ IntPtrConstant(0));

      // Check if we need to collect garbage before we can start bump pointer
      // allocation (always done for folded allocations).
      Node* check = __ UintLessThan(__ IntAdd(top, size), limit);

      __ GotoIfNot(check, &call_runtime);
      __ Goto(&done, top);

      __ Bind(&call_runtime);
      {
        Node* target = allocation_type == AllocationType::kYoung
                           ? __
                             AllocateInYoungGenerationStubConstant()
                           : __
                             AllocateInOldGenerationStubConstant();
        if (!allocate_operator_.is_set()) {
          auto descriptor = AllocateDescriptor{};
          auto call_descriptor = Linkage::GetStubCallDescriptor(
              graph()->zone(), descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kCanUseRoots, Operator::kNoThrow);
          allocate_operator_.set(common()->Call(call_descriptor));
        }
        Node* vfalse = __ BitcastTaggedToWord(
            __ Call(allocate_operator_.get(), target, size));
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
          new (zone()) AllocationGroup(value, allocation_type, size, zone());
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
    Node* new_top = __ IntAdd(top, size);

    // Check if we can do bump pointer allocation here.
    Node* check = __ UintLessThan(new_top, limit);
    __ GotoIfNot(check, &call_runtime);
    if (allocation.allow_large_objects() == AllowLargeObjects::kTrue) {
      __ GotoIfNot(
          __ UintLessThan(size, __ IntPtrConstant(kMaxRegularHeapObjectSize)),
          &call_runtime);
    }
    __ Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                 kNoWriteBarrier),
             top_address, __ IntPtrConstant(0), new_top);
    __ Goto(&done, __ BitcastWordToTagged(
                       __ IntAdd(top, __ IntPtrConstant(kHeapObjectTag))));

    __ Bind(&call_runtime);
    Node* target = allocation_type == AllocationType::kYoung
                       ? __
                         AllocateInYoungGenerationStubConstant()
                       : __
                         AllocateInOldGenerationStubConstant();
    if (!allocate_operator_.is_set()) {
      auto descriptor = AllocateDescriptor{};
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          graph()->zone(), descriptor, descriptor.GetStackParameterCount(),
          CallDescriptor::kCanUseRoots, Operator::kNoThrow);
      allocate_operator_.set(common()->Call(call_descriptor));
    }
    __ Goto(&done, __ Call(allocate_operator_.get(), target, size));

    __ Bind(&done);
    value = done.PhiAt(0);

    // Create an unfoldable allocation group.
    AllocationGroup* group =
        new (zone()) AllocationGroup(value, allocation_type, zone());
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

void MemoryOptimizer::VisitLoadFromObject(Node* node,
                                          AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kLoadFromObject, node->opcode());
  ObjectAccess const& access = ObjectAccessOf(node->op());
  Node* offset = node->InputAt(1);
  node->ReplaceInput(1, __ IntSub(offset, __ IntPtrConstant(kHeapObjectTag)));
  NodeProperties::ChangeOp(node, machine()->Load(access.machine_type));
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitStoreToObject(Node* node,
                                         AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreToObject, node->opcode());
  ObjectAccess const& access = ObjectAccessOf(node->op());
  Node* object = node->InputAt(0);
  Node* offset = node->InputAt(1);
  Node* value = node->InputAt(2);
  node->ReplaceInput(1, __ IntSub(offset, __ IntPtrConstant(kHeapObjectTag)));
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  EnqueueUses(node, state);
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
  MachineType type = access.machine_type;
  if (NeedsPoisoning(access.load_sensitivity) &&
      type.representation() != MachineRepresentation::kTaggedPointer &&
      type.representation() != MachineRepresentation::kCompressedPointer) {
    NodeProperties::ChangeOp(node, machine()->PoisonedLoad(type));
  } else {
    NodeProperties::ChangeOp(node, machine()->Load(type));
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitLoadField(Node* node, AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kLoadField, node->opcode());
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* offset = jsgraph()->IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph()->zone(), 1, offset);
  MachineType type = access.machine_type;
  if (NeedsPoisoning(access.load_sensitivity) &&
      type.representation() != MachineRepresentation::kTaggedPointer &&
      type.representation() != MachineRepresentation::kCompressedPointer) {
    NodeProperties::ChangeOp(node, machine()->PoisonedLoad(type));
  } else {
    NodeProperties::ChangeOp(node, machine()->Load(type));
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitStoreElement(Node* node,
                                        AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreElement, node->opcode());
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* object = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
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
  Node* value = node->InputAt(1);
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  Node* offset = jsgraph()->IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph()->zone(), 1, offset);
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitStore(Node* node, AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStore, node->opcode());
  StoreRepresentation representation = StoreRepresentationOf(node->op());
  Node* object = node->InputAt(0);
  Node* value = node->InputAt(2);
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, representation.write_barrier_kind());
  if (write_barrier_kind != representation.write_barrier_kind()) {
    NodeProperties::ChangeOp(
        node, machine()->Store(StoreRepresentation(
                  representation.representation(), write_barrier_kind)));
  }
  EnqueueUses(node, state);
}

void MemoryOptimizer::VisitOtherEffect(Node* node,
                                       AllocationState const* state) {
  EnqueueUses(node, state);
}

Node* MemoryOptimizer::ComputeIndex(ElementAccess const& access, Node* index) {
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

namespace {

bool ValueNeedsWriteBarrier(Node* value, Isolate* isolate) {
  while (true) {
    switch (value->opcode()) {
      case IrOpcode::kBitcastWordToTaggedSigned:
      case IrOpcode::kChangeTaggedSignedToCompressedSigned:
      case IrOpcode::kChangeTaggedToCompressedSigned:
        return false;
      case IrOpcode::kChangeTaggedPointerToCompressedPointer:
      case IrOpcode::kChangeTaggedToCompressed:
        value = NodeProperties::GetValueInput(value, 0);
        continue;
      case IrOpcode::kHeapConstant: {
        RootIndex root_index;
        if (isolate->roots_table().IsRootHandle(HeapConstantOf(value->op()),
                                                &root_index) &&
            RootsTable::IsImmortalImmovable(root_index)) {
          return false;
        }
        break;
      }
      default:
        break;
    }
    return true;
  }
}

void WriteBarrierAssertFailed(Node* node, Node* object, const char* name,
                              Zone* temp_zone) {
  std::stringstream str;
  str << "MemoryOptimizer could not remove write barrier for node #"
      << node->id() << "\n";
  str << "  Run mksnapshot with --csa-trap-on-node=" << name << ","
      << node->id() << " to break in CSA code.\n";
  Node* object_position = object;
  if (object_position->opcode() == IrOpcode::kPhi) {
    object_position = EffectPhiForPhi(object_position);
  }
  Node* allocating_node = nullptr;
  if (object_position && object_position->op()->EffectOutputCount() > 0) {
    allocating_node = SearchAllocatingNode(node, object_position, temp_zone);
  }
  if (allocating_node) {
    str << "\n  There is a potentially allocating node in between:\n";
    str << "    " << *allocating_node << "\n";
    str << "  Run mksnapshot with --csa-trap-on-node=" << name << ","
        << allocating_node->id() << " to break there.\n";
    if (allocating_node->opcode() == IrOpcode::kCall) {
      str << "  If this is a never-allocating runtime call, you can add an "
             "exception to Runtime::MayAllocate.\n";
    }
  } else {
    str << "\n  It seems the store happened to something different than a "
           "direct "
           "allocation:\n";
    str << "    " << *object << "\n";
    str << "  Run mksnapshot with --csa-trap-on-node=" << name << ","
        << object->id() << " to break there.\n";
  }
  FATAL("%s", str.str().c_str());
}

}  // namespace

WriteBarrierKind MemoryOptimizer::ComputeWriteBarrierKind(
    Node* node, Node* object, Node* value, AllocationState const* state,
    WriteBarrierKind write_barrier_kind) {
  if (state->IsYoungGenerationAllocation() &&
      state->group()->Contains(object)) {
    write_barrier_kind = kNoWriteBarrier;
  }
  if (!ValueNeedsWriteBarrier(value, isolate())) {
    write_barrier_kind = kNoWriteBarrier;
  }
  if (write_barrier_kind == WriteBarrierKind::kAssertNoWriteBarrier) {
    WriteBarrierAssertFailed(node, object, function_debug_name_, zone());
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
    if (index == 0) {
      if (CanLoopAllocate(node, zone())) {
        // If the loop can allocate,  we start with an empty state at the
        // beginning.
        EnqueueUses(node, empty_state());
      } else {
        // If the loop cannot allocate, we can just propagate the state from
        // before the loop.
        EnqueueUses(node, state);
      }
    } else {
      // Do not revisit backedges.
    }
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

bool MemoryOptimizer::NeedsPoisoning(LoadSensitivity load_sensitivity) const {
  // Safe loads do not need poisoning.
  if (load_sensitivity == LoadSensitivity::kSafe) return false;

  switch (poisoning_level_) {
    case PoisoningMitigationLevel::kDontPoison:
      return false;
    case PoisoningMitigationLevel::kPoisonAll:
      return true;
    case PoisoningMitigationLevel::kPoisonCriticalOnly:
      return load_sensitivity == LoadSensitivity::kCritical;
  }
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
