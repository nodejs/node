// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/memory-lowering.h"

#include "src/codegen/interface-descriptors-inl.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "src/roots/roots-inl.h"
#include "src/sandbox/external-pointer.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#endif
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

  static inline AllocationType CheckAllocationType(AllocationType allocation) {
    // For non-generational heap, all young allocations are redirected to old
    // space.
    if (FLAG_single_generation && allocation == AllocationType::kYoung) {
      return AllocationType::kOld;
    }
    return allocation;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationGroup);
};

MemoryLowering::MemoryLowering(JSGraph* jsgraph, Zone* zone,
                               JSGraphAssembler* graph_assembler,
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
    case IrOpcode::kLoadImmutableFromObject:
      return ReduceLoadFromObject(node);
    case IrOpcode::kLoadElement:
      return ReduceLoadElement(node);
    case IrOpcode::kLoadField:
      return ReduceLoadField(node);
    case IrOpcode::kStoreToObject:
    case IrOpcode::kInitializeImmutableInObject:
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

void MemoryLowering::EnsureAllocateOperator() {
  if (allocate_operator_.is_set()) return;

  auto descriptor = AllocateDescriptor{};
  StubCallMode mode = isolate_ != nullptr ? StubCallMode::kCallCodeObject
                                          : StubCallMode::kCallBuiltinPointer;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph_zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kCanUseRoots, Operator::kNoThrow, mode);
  allocate_operator_.set(common()->Call(call_descriptor));
}

#if V8_ENABLE_WEBASSEMBLY
Node* MemoryLowering::GetWasmInstanceNode() {
  if (wasm_instance_node_.is_set()) return wasm_instance_node_.get();
  for (Node* use : graph()->start()->uses()) {
    if (use->opcode() == IrOpcode::kParameter &&
        ParameterIndexOf(use->op()) == wasm::kWasmInstanceParameterIndex) {
      wasm_instance_node_.set(use);
      return use;
    }
  }
  UNREACHABLE();  // The instance node must have been created before.
}
#endif  // V8_ENABLE_WEBASSEMBLY

#define __ gasm()->

Reduction MemoryLowering::ReduceAllocateRaw(
    Node* node, AllocationType allocation_type,
    AllowLargeObjects allow_large_objects, AllocationState const** state_ptr) {
  DCHECK_EQ(IrOpcode::kAllocateRaw, node->opcode());
  DCHECK_IMPLIES(allocation_folding_ == AllocationFolding::kDoAllocationFolding,
                 state_ptr != nullptr);
  if (FLAG_single_generation && allocation_type == AllocationType::kYoung) {
    allocation_type = AllocationType::kOld;
  }
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
  if (isolate_ != nullptr) {
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
  } else {
    // This lowering is used by Wasm, where we compile isolate-independent
    // code. Builtin calls simply encode the target builtin ID, which will
    // be patched to the builtin's address later.
#if V8_ENABLE_WEBASSEMBLY
    Builtin builtin;
    if (allocation_type == AllocationType::kYoung) {
      if (allow_large_objects == AllowLargeObjects::kTrue) {
        builtin = Builtin::kAllocateInYoungGeneration;
      } else {
        builtin = Builtin::kAllocateRegularInYoungGeneration;
      }
    } else {
      if (allow_large_objects == AllowLargeObjects::kTrue) {
        builtin = Builtin::kAllocateInOldGeneration;
      } else {
        builtin = Builtin::kAllocateRegularInOldGeneration;
      }
    }
    static_assert(std::is_same<Smi, BuiltinPtr>(), "BuiltinPtr must be Smi");
    allocate_builtin =
        graph()->NewNode(common()->NumberConstant(static_cast<int>(builtin)));
#else
    UNREACHABLE();
#endif
  }

  // Determine the top/limit addresses.
  Node* top_address;
  Node* limit_address;
  if (isolate_ != nullptr) {
    top_address = __ ExternalConstant(
        allocation_type == AllocationType::kYoung
            ? ExternalReference::new_space_allocation_top_address(isolate())
            : ExternalReference::old_space_allocation_top_address(isolate()));
    limit_address = __ ExternalConstant(
        allocation_type == AllocationType::kYoung
            ? ExternalReference::new_space_allocation_limit_address(isolate())
            : ExternalReference::old_space_allocation_limit_address(isolate()));
  } else {
    // Wasm mode: producing isolate-independent code, loading the isolate
    // address at runtime.
#if V8_ENABLE_WEBASSEMBLY
    Node* instance_node = GetWasmInstanceNode();
    int top_address_offset =
        allocation_type == AllocationType::kYoung
            ? WasmInstanceObject::kNewAllocationTopAddressOffset
            : WasmInstanceObject::kOldAllocationTopAddressOffset;
    int limit_address_offset =
        allocation_type == AllocationType::kYoung
            ? WasmInstanceObject::kNewAllocationLimitAddressOffset
            : WasmInstanceObject::kOldAllocationLimitAddressOffset;
    top_address =
        __ Load(MachineType::Pointer(), instance_node,
                __ IntPtrConstant(top_address_offset - kHeapObjectTag));
    limit_address =
        __ Load(MachineType::Pointer(), instance_node,
                __ IntPtrConstant(limit_address_offset - kHeapObjectTag));
#else
    UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
  }

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
      Node* reservation_size = __ UniqueIntPtrConstant(object_size);

      // Load allocation top and limit.
      Node* top =
          __ Load(MachineType::Pointer(), top_address, __ IntPtrConstant(0));
      Node* limit =
          __ Load(MachineType::Pointer(), limit_address, __ IntPtrConstant(0));

      // Check if we need to collect garbage before we can start bump pointer
      // allocation (always done for folded allocations).
      Node* check = __ UintLessThan(__ IntAdd(top, reservation_size), limit);

      __ GotoIfNot(check, &call_runtime);
      __ Goto(&done, top);

      __ Bind(&call_runtime);
      {
        EnsureAllocateOperator();
        Node* vfalse = __ BitcastTaggedToWord(__ Call(
            allocate_operator_.get(), allocate_builtin, reservation_size));
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
      AllocationGroup* group = zone()->New<AllocationGroup>(
          value, allocation_type, reservation_size, zone());
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
    EnsureAllocateOperator();
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
  DCHECK(node->opcode() == IrOpcode::kLoadFromObject ||
         node->opcode() == IrOpcode::kLoadImmutableFromObject);
  ObjectAccess const& access = ObjectAccessOf(node->op());

  MachineType machine_type = access.machine_type;

  if (machine_type.IsMapWord()) {
    CHECK_EQ(machine_type.semantic(), MachineSemantic::kAny);
    return ReduceLoadMap(node);
  }

  MachineRepresentation rep = machine_type.representation();
  const Operator* load_op =
      ElementSizeInBytes(rep) > kTaggedSize &&
              !machine()->UnalignedLoadSupported(machine_type.representation())
          ? machine()->UnalignedLoad(machine_type)
          : machine()->Load(machine_type);
  NodeProperties::ChangeOp(node, load_op);
  return Changed(node);
}

Reduction MemoryLowering::ReduceLoadElement(Node* node) {
  DCHECK_EQ(IrOpcode::kLoadElement, node->opcode());
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* index = node->InputAt(1);
  node->ReplaceInput(1, ComputeIndex(access, index));
  MachineType type = access.machine_type;
  DCHECK(!type.IsMapWord());
  NodeProperties::ChangeOp(node, machine()->Load(type));
  return Changed(node);
}

Node* MemoryLowering::DecodeExternalPointer(
    Node* node, ExternalPointerTag external_pointer_tag) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  DCHECK(V8_SANDBOXED_EXTERNAL_POINTERS_BOOL);
  DCHECK(node->opcode() == IrOpcode::kLoad);
  DCHECK_EQ(kExternalPointerSize, kUInt32Size);
  DCHECK_NE(kExternalPointerNullTag, external_pointer_tag);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  __ InitializeEffectControl(effect, control);

  // Clone the load node and put it here.
  // TODO(turbofan): consider adding GraphAssembler::Clone() suitable for
  // cloning nodes from arbitrary locaions in effect/control chains.
  STATIC_ASSERT(kExternalPointerIndexShift > kSystemPointerSizeLog2);
  Node* shifted_index = __ AddNode(graph()->CloneNode(node));
  Node* shift_amount =
      __ Int32Constant(kExternalPointerIndexShift - kSystemPointerSizeLog2);
  Node* offset = __ Word32Shr(shifted_index, shift_amount);

  // Uncomment this to generate a breakpoint for debugging purposes.
  // __ DebugBreak();

  // Decode loaded external pointer.
  //
  // Here we access the external pointer table through an ExternalReference.
  // Alternatively, we could also hardcode the address of the table since it is
  // never reallocated. However, in that case we must be able to guarantee that
  // the generated code is never executed under a different Isolate, as that
  // would allow access to external objects from different Isolates. It also
  // would break if the code is serialized/deserialized at some point.
  Node* table_address = __ ExternalConstant(
      ExternalReference::external_pointer_table_address(isolate()));
  Node* table = __ Load(MachineType::Pointer(), table_address,
                        Internals::kExternalPointerTableBufferOffset);
  Node* decoded_ptr =
      __ Load(MachineType::Pointer(), table, __ ChangeUint32ToUint64(offset));
  Node* tag = __ IntPtrConstant(~external_pointer_tag);
  decoded_ptr = __ WordAnd(decoded_ptr, tag);
  return decoded_ptr;
#else
  return node;
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
}

Reduction MemoryLowering::ReduceLoadMap(Node* node) {
#ifdef V8_MAP_PACKING
  NodeProperties::ChangeOp(node, machine()->Load(MachineType::AnyTagged()));

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  __ InitializeEffectControl(effect, control);

  node = __ AddNode(graph()->CloneNode(node));
  return Replace(__ UnpackMapWord(node));
#else
  NodeProperties::ChangeOp(node, machine()->Load(MachineType::TaggedPointer()));
  return Changed(node);
#endif
}

Reduction MemoryLowering::ReduceLoadField(Node* node) {
  DCHECK_EQ(IrOpcode::kLoadField, node->opcode());
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* offset = __ IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph_zone(), 1, offset);
  MachineType type = access.machine_type;
  if (V8_SANDBOXED_EXTERNAL_POINTERS_BOOL &&
      access.type.Is(Type::ExternalPointer())) {
    // External pointer table indices are stored as 32-bit numbers
    type = MachineType::Uint32();
  }

  if (type.IsMapWord()) {
    DCHECK(!access.type.Is(Type::ExternalPointer()));
    return ReduceLoadMap(node);
  }

  NodeProperties::ChangeOp(node, machine()->Load(type));

#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  if (access.type.Is(Type::ExternalPointer())) {
    ExternalPointerTag tag = access.external_pointer_tag;
    DCHECK_NE(kExternalPointerNullTag, tag);
    node = DecodeExternalPointer(node, tag);
    return Replace(node);
  }
#endif

  return Changed(node);
}

Reduction MemoryLowering::ReduceStoreToObject(Node* node,
                                              AllocationState const* state) {
  DCHECK(node->opcode() == IrOpcode::kStoreToObject ||
         node->opcode() == IrOpcode::kInitializeImmutableInObject);
  ObjectAccess const& access = ObjectAccessOf(node->op());
  Node* object = node->InputAt(0);
  Node* value = node->InputAt(2);

  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  DCHECK(!access.machine_type.IsMapWord());
  MachineRepresentation rep = access.machine_type.representation();
  StoreRepresentation store_rep(rep, write_barrier_kind);
  const Operator* store_op = ElementSizeInBytes(rep) > kTaggedSize &&
                                     !machine()->UnalignedStoreSupported(rep)
                                 ? machine()->UnalignedStore(rep)
                                 : machine()->Store(store_rep);
  NodeProperties::ChangeOp(node, store_op);
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
  DCHECK_IMPLIES(V8_SANDBOXED_EXTERNAL_POINTERS_BOOL,
                 !access.type.Is(Type::ExternalPointer()));
  // SandboxedPointers are not currently stored by optimized code.
  DCHECK(!access.type.Is(Type::SandboxedPointer()));
  MachineType machine_type = access.machine_type;
  Node* object = node->InputAt(0);
  Node* value = node->InputAt(1);

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  __ InitializeEffectControl(effect, control);

  WriteBarrierKind write_barrier_kind = ComputeWriteBarrierKind(
      node, object, value, state, access.write_barrier_kind);
  Node* offset = __ IntPtrConstant(access.offset - access.tag());
  node->InsertInput(graph_zone(), 1, offset);

  if (machine_type.IsMapWord()) {
    machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
    Node* mapword = __ PackMapWord(TNode<Map>::UncheckedCast(value));
    node->ReplaceInput(2, mapword);
#endif
  }
  NodeProperties::ChangeOp(
      node, machine()->Store(StoreRepresentation(machine_type.representation(),
                                                 write_barrier_kind)));
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
  if (FLAG_disable_write_barriers) {
    write_barrier_kind = kNoWriteBarrier;
  }
  if (write_barrier_kind == WriteBarrierKind::kAssertNoWriteBarrier) {
    write_barrier_assert_failed_(node, object, function_debug_name_, zone());
  }
  return write_barrier_kind;
}

MemoryLowering::AllocationGroup::AllocationGroup(Node* node,
                                                 AllocationType allocation,
                                                 Zone* zone)
    : node_ids_(zone),
      allocation_(CheckAllocationType(allocation)),
      size_(nullptr) {
  node_ids_.insert(node->id());
}

MemoryLowering::AllocationGroup::AllocationGroup(Node* node,
                                                 AllocationType allocation,
                                                 Node* size, Zone* zone)
    : node_ids_(zone),
      allocation_(CheckAllocationType(allocation)),
      size_(size) {
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
