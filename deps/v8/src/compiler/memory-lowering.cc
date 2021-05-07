// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/memory-lowering.h"

#include "src/codegen/interface-descriptors.h"
#include "src/common/external-pointer.h"
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

// An allocation group represents a set of allocations that have been folded
// together.
class MemoryLowering::AllocationGroup final : public ZoneObject {
 public:
  AllocationGroup(Node* node, AllocationType allocation, Zone* zone);
  AllocationGroup(Node* node, AllocationType allocation, Node* size,
                  Zone* zone);
  ~AllocationGroup() = default;

  void Add(Node* object);
  bool Contains(Node* object) const;
  bool IsYoungGenerationAllocation() const {
    return allocation() == AllocationType::kYoung;
  }

  AllocationType allocation() const { return allocation_; }
  Node* size() const { return size_; }

 private:
  ZoneSet<NodeId> node_ids_;
  AllocationType const allocation_;
  Node* const size_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationGroup);
};

MemoryLowering::MemoryLowering(JSGraph* jsgraph, Zone* zone,
                               JSGraphAssembler* graph_assembler,
                               PoisoningMitigationLevel poisoning_level,
                               AllocationFolding allocation_folding,
                               WriteBarrierAssertFailedCallback callback,
                               const char* function_debug_name)
    : isolate_(jsgraph->isolate()),
      zone_(zone),
      graph_(jsgraph->graph()),
      common_(jsgraph->common()),
      machine_(jsgraph->machine()),
      graph_assembler_(graph_assembler),
      allocation_folding_(allocation_folding),
      poisoning_level_(poisoning_level),
      write_barrier_assert_failed_(callback),
      function_debug_name_(function_debug_name) {}

Zone* MemoryLowering::graph_zone() const { return graph()->zone(); }

Reduction MemoryLowering::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kAllocate:
      // Allocate nodes were purged from the graph in effect-control
      // linearization.
      UNREACHABLE();
    case IrOpcode::kAllocateRaw:
      return ReduceAllocateRaw(node);
    case IrOpcode::kLoadFromObject:
      return ReduceLoadFromObject(node);
    case IrOpcode::kLoadElement:
      return ReduceLoadElement(node);
    case IrOpcode::kLoadField:
      return ReduceLoadField(node);
    case IrOpcode::kStoreToObject:
      return ReduceStoreToObject(node);
    case IrOpcode::kStoreElement:
      return ReduceStoreElement(node);
    case IrOpcode::kStoreField:
      return ReduceStoreField(node);
    case IrOpcode::kStore:
      return ReduceStore(node);
    default:
      return NoChange();
  }
}

#define __ gasm()->

Reduction MemoryLowering::ReduceAllocateRaw(
    Node* node, AllocationType allocation_type,
    AllowLargeObjects allow_large_objects, AllocationState const** state_ptr) {
  DCHECK_EQ(IrOpcode::kAllocateRaw, node->opcode());
  DCHECK_IMPLIES(allocation_folding_ == AllocationFolding::kDoAllocationFolding,
                 state_ptr != nullptr);
  // Code objects may have a maximum size smaller than kMaxHeapObjectSize due to
  // guard pages. If we need to support allocating code here we would need to
  // call MemoryChunkLayout::MaxRegularCodeObjectSize() at runtime.
  DCHECK_NE(allocation_type, AllocationType::kCode);
  Node* value;
  Node* size = node->InputAt(0);
  Node* effect = node->InputAt(1);
  Node* control = node->InputAt(2);

  gasm()->InitializeEffectControl(effect, control);

  Node* allocate_builtin;
  if (allocation_type == AllocationType::kYoung) {
    if (allow_large_objects == AllowLargeObjects::kTrue) {
      allocate_builtin = __ AllocateInYoungGenerationStubConstant();
    } else {
      allocate_builtin = __ AllocateRegularInYoungGenerationStubConstant();
    }
  } else {
    if (allow_large_objects == AllowLargeObjects::kTrue) {
      allocate_builtin = __ AllocateInOldGenerationStubConstant();
    } else {
      allocate_builtin = __ AllocateRegularInOldGenerationStubConstant();
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
  if (m.IsInRange(0, kMaxRegularHeapObjectSize) && FLAG_inline_new &&
      allocation_folding_ == AllocationFolding::kDoAllocationFolding) {
    intptr_t const object_size = m.ResolvedValue();
    AllocationState const* state = *state_ptr;
    if (state->size() <= kMaxRegularHeapObjectSize - object_size &&
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
      effect = gasm()->effect();
      control = gasm()->control();

      // Extend the allocation {group}.
      group->Add(value);
      *state_ptr =
          AllocationState::Open(group, state_size, top, effect, zone());
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
        if (!allocate_operator_.is_set()) {
          auto descriptor = AllocateDescriptor{};
          auto call_descriptor = Linkage::GetStubCallDescriptor(
              graph_zone(), descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kCanUseRoots, Operator::kNoThrow);
          allocate_operator_.set(common()->Call(call_descriptor));
        }
        Node* vfalse = __ BitcastTaggedToWord(
            __ Call(allocate_operator_.get(), allocate_builtin, size));
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
      effect = gasm()->effect();
      control = gasm()->control();

      // Start a new allocation group.
      AllocationGroup* group =
          zone()->New<AllocationGroup>(value, allocation_type, size, zone());
      *state_ptr =
          AllocationState::Open(group, object_size, top, effect, zone());
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
    if (allow_large_objects == AllowLargeObjects::kTrue) {
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
    if (!allocate_operator_.is_set()) {
      auto descriptor = AllocateDescriptor{};
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          graph_zone(), descriptor, descriptor.GetStackParameterCount(),
          CallDescriptor::kCanUseRoots, Operator::kNoThrow);
      allocate_operator_.set(common()->Call(call_descriptor));
    }
    __ Goto(&done, __ Call(allocate_operator_.get(), allocate_builtin, size));

    __ Bind(&done);
    value = done.PhiAt(0);
    effect = gasm()->effect();
    control = gasm()->control();

    if (state_ptr) {
      // Create an unfoldable allocation group.
      AllocationGroup* group =
          zone()->New<AllocationGroup>(value, allocation_type, zone());
      *state_ptr = AllocationState::Closed(group, effect, zone());
    }
  }

  return Replace(value);
}

Reduction MemoryLowering::ReduceLoadFromObject(Node* node) {
  DCHECK_EQ(IrOpcode::kLoadFromObject, node->opcode());
  ObjectAccess const& access = ObjectAccessOf(node->op());
  NodeProperties::ChangeOp(node, machine()->Load(access.machine_type));
  return Changed(node);
}

Reduction MemoryLowering::ReduceLoadElement(Node* node) {
  DCHECK_EQ(IrOpcode::kLoadElement, node->opcode());
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* index = node->InputAt(1);
  node->ReplaceInput(1, ComputeIndex(access, index));
  MachineType type = access.machine_type;
  if (NeedsPoisoning(access.load_sensitivity)) {
    NodeProperties::ChangeOp(node, machine()->PoisonedLoad(type));
  } else {
    NodeProperties::ChangeOp(node, machine()->Load(type));
  }
  return Changed(node);
}

Node* MemoryLowering::DecodeExternalPointer(
    Node* node, ExternalPointerTag external_pointer_tag) {
#ifdef V8_HEAP_SANDBOX
  DCHECK(V8_HEAP_SANDBOX_BOOL);
  DCHECK(node->opcode() == IrOpcode::kLoad ||
         node->opcode() == IrOpcode::kPoisonedLoad);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  __ InitializeEffectControl(effect, control);

  // Clone the load node and put it here.
  // TODO(turbofan): consider adding GraphAssembler::Clone() suitable for
  // cloning nodes from arbitrary locaions in effect/control chains.
  Node* index = __ AddNode(graph()->CloneNode(node));

  // Uncomment this to generate a breakpoint for debugging purposes.
  // __ DebugBreak();

  // Decode loaded external pointer.
  STATIC_ASSERT(kExternalPointerSize == kSystemPointerSize);
  Node* external_pointer_table_address = __ ExternalConstant(
      ExternalReference::external_pointer_table_address(isolate()));
  Node* table = __ Load(MachineType::Pointer(), external_pointer_table_address,
                        Internals::kExternalPointerTableBufferOffset);
  // TODO(v8:10391, saelo): bounds check if table is not caged
  Node* offset = __ Int32Mul(index, __ Int32Constant(8));
  Node* decoded_ptr =
      __ Load(MachineType::Pointer(), table, __ ChangeUint32ToUint64(offset));
  if (external_pointer_tag != 0) {
    Node* tag = __ IntPtrConstant(external_pointer_tag);
    decoded_ptr = __ WordXor(decoded_ptr, tag);
  }
  return decoded_ptr;
#else
  return node;
#endif  // V8_HEAP_SANDBOX
}

Reduction MemoryLowering::ReduceLoadField(Node* node) {
  DCHECK_EQ(IrOpcode::kLoadField, node->opcode());
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* offset = __ IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph_zone(), 1, offset);
  MachineType type = access.machine_type;
  if (V8_HEAP_SANDBOX_BOOL &&
      access.type.Is(Type::SandboxedExternalPointer())) {
    // External pointer table indices are 32bit numbers
    type = MachineType::Uint32();
  }
  if (NeedsPoisoning(access.load_sensitivity)) {
    NodeProperties::ChangeOp(node, machine()->PoisonedLoad(type));
  } else {
    NodeProperties::ChangeOp(node, machine()->Load(type));
  }
  if (V8_HEAP_SANDBOX_BOOL &&
      access.type.Is(Type::SandboxedExternalPointer())) {
#ifdef V8_HEAP_SANDBOX
    ExternalPointerTag tag = access.external_pointer_tag;
#else
    ExternalPointerTag tag = kExternalPointerNullTag;
#endif
    node = DecodeExternalPointer(node, tag);
    return Replace(node);
  } else {
    DCHECK(!access.type.Is(Type::SandboxedExternalPointer()));
  }
  return Changed(node);
}

Reduction MemoryLowering::ReduceStoreToObject(Node* node,
                                              AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreToObject, node->opcode());
  ObjectAccess const& access = ObjectAccessOf(node->op());
  Node* object = node->InputAt(0);
  Node* value = node->InputAt(2);
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  return Changed(node);
}

Reduction MemoryLowering::ReduceStoreElement(Node* node,
                                             AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreElement, node->opcode());
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* object = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  node->ReplaceInput(1, ComputeIndex(access, index));
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  return Changed(node);
}

Reduction MemoryLowering::ReduceStoreField(Node* node,
                                           AllocationState const* state) {
  DCHECK_EQ(IrOpcode::kStoreField, node->opcode());
  FieldAccess const& access = FieldAccessOf(node->op());
  // External pointer must never be stored by optimized code.
  DCHECK_IMPLIES(V8_HEAP_SANDBOX_BOOL,
                 !access.type.Is(Type::ExternalPointer()) &&
                     !access.type.Is(Type::SandboxedExternalPointer()));
  Node* object = node->InputAt(0);
  Node* value = node->InputAt(1);
  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  Node* offset = __ IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph_zone(), 1, offset);
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(
                access.machine_type.representation(), write_barrier_kind)));
  return Changed(node);
}

Reduction MemoryLowering::ReduceStore(Node* node,
                                      AllocationState const* state) {
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
    return Changed(node);
  }
  return NoChange();
}

Node* MemoryLowering::ComputeIndex(ElementAccess const& access, Node* index) {
  int const element_size_shift =
      ElementSizeLog2Of(access.machine_type.representation());
  if (element_size_shift) {
    index = __ WordShl(index, __ IntPtrConstant(element_size_shift));
  }
  int const fixed_offset = access.header_size - access.tag();
  if (fixed_offset) {
    index = __ IntAdd(index, __ IntPtrConstant(fixed_offset));
  }
  return index;
}

#undef __

namespace {

bool ValueNeedsWriteBarrier(Node* value, Isolate* isolate) {
  while (true) {
    switch (value->opcode()) {
      case IrOpcode::kBitcastWordToTaggedSigned:
        return false;
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

}  // namespace

Reduction MemoryLowering::ReduceAllocateRaw(Node* node) {
  DCHECK_EQ(IrOpcode::kAllocateRaw, node->opcode());
  const AllocateParameters& allocation = AllocateParametersOf(node->op());
  return ReduceAllocateRaw(node, allocation.allocation_type(),
                           allocation.allow_large_objects(), nullptr);
}

WriteBarrierKind MemoryLowering::ComputeWriteBarrierKind(
    Node* node, Node* object, Node* value, AllocationState const* state,
    WriteBarrierKind write_barrier_kind) {
  if (state && state->IsYoungGenerationAllocation() &&
      state->group()->Contains(object)) {
    write_barrier_kind = kNoWriteBarrier;
  }
  if (!ValueNeedsWriteBarrier(value, isolate())) {
    write_barrier_kind = kNoWriteBarrier;
  }
  if (write_barrier_kind == WriteBarrierKind::kAssertNoWriteBarrier) {
    write_barrier_assert_failed_(node, object, function_debug_name_, zone());
  }
  return write_barrier_kind;
}

bool MemoryLowering::NeedsPoisoning(LoadSensitivity load_sensitivity) const {
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

MemoryLowering::AllocationGroup::AllocationGroup(Node* node,
                                                 AllocationType allocation,
                                                 Zone* zone)
    : node_ids_(zone), allocation_(allocation), size_(nullptr) {
  node_ids_.insert(node->id());
}

MemoryLowering::AllocationGroup::AllocationGroup(Node* node,
                                                 AllocationType allocation,
                                                 Node* size, Zone* zone)
    : node_ids_(zone), allocation_(allocation), size_(size) {
  node_ids_.insert(node->id());
}

void MemoryLowering::AllocationGroup::Add(Node* node) {
  node_ids_.insert(node->id());
}

bool MemoryLowering::AllocationGroup::Contains(Node* node) const {
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

MemoryLowering::AllocationState::AllocationState()
    : group_(nullptr),
      size_(std::numeric_limits<int>::max()),
      top_(nullptr),
      effect_(nullptr) {}

MemoryLowering::AllocationState::AllocationState(AllocationGroup* group,
                                                 Node* effect)
    : group_(group),
      size_(std::numeric_limits<int>::max()),
      top_(nullptr),
      effect_(effect) {}

MemoryLowering::AllocationState::AllocationState(AllocationGroup* group,
                                                 intptr_t size, Node* top,
                                                 Node* effect)
    : group_(group), size_(size), top_(top), effect_(effect) {}

bool MemoryLowering::AllocationState::IsYoungGenerationAllocation() const {
  return group() && group()->IsYoungGenerationAllocation();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
