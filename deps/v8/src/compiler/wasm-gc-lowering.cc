// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-gc-lowering.h"

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/compiler/wasm-graph-assembler.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

WasmGCLowering::WasmGCLowering(Editor* editor, MachineGraph* mcgraph)
    : AdvancedReducer(editor),
      gasm_(mcgraph, mcgraph->zone()),
      dead_(mcgraph->Dead()),
      instance_node_(nullptr) {
  // Find and store the instance node.
  for (Node* start_use : mcgraph->graph()->start()->uses()) {
    if (start_use->opcode() == IrOpcode::kParameter &&
        ParameterIndexOf(start_use->op()) == 0) {
      instance_node_ = start_use;
      break;
    }
  }
  DCHECK_NOT_NULL(instance_node_);
}

Reduction WasmGCLowering::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kWasmTypeCheck:
      return ReduceWasmTypeCheck(node);
    case IrOpcode::kWasmTypeCast:
      return ReduceWasmTypeCast(node);
    case IrOpcode::kAssertNotNull:
      return ReduceAssertNotNull(node);
    case IrOpcode::kNull:
      return ReduceNull(node);
    case IrOpcode::kIsNull:
      return ReduceIsNull(node);
    case IrOpcode::kIsNotNull:
      return ReduceIsNotNull(node);
    case IrOpcode::kRttCanon:
      return ReduceRttCanon(node);
    case IrOpcode::kTypeGuard:
      return ReduceTypeGuard(node);
    case IrOpcode::kWasmExternInternalize:
      return ReduceWasmExternInternalize(node);
    case IrOpcode::kWasmExternExternalize:
      return ReduceWasmExternExternalize(node);
    default:
      return NoChange();
  }
}

Node* WasmGCLowering::RootNode(RootIndex index) {
  Node* isolate_root = gasm_.LoadImmutable(
      MachineType::Pointer(), instance_node_,
      WasmInstanceObject::kIsolateRootOffset - kHeapObjectTag);
  return gasm_.LoadImmutable(MachineType::Pointer(), isolate_root,
                             IsolateData::root_slot_offset(index));
}

Node* WasmGCLowering::Null() { return RootNode(RootIndex::kNullValue); }

// TODO(manoskouk): Use the Callbacks infrastructure from wasm-compiler.h to
// unify all check/cast implementations.
// TODO(manoskouk): Find a way to optimize branches on typechecks.
Reduction WasmGCLowering::ReduceWasmTypeCheck(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmTypeCheck);

  Node* object = node->InputAt(0);
  Node* rtt = node->InputAt(1);
  Node* effect_input = NodeProperties::GetEffectInput(node);
  Node* control_input = NodeProperties::GetControlInput(node);
  auto config = OpParameter<WasmTypeCheckConfig>(node->op());
  int rtt_depth = config.rtt_depth;
  bool object_can_be_null = config.object_can_be_null;

  gasm_.InitializeEffectControl(effect_input, control_input);

  auto end_label = gasm_.MakeLabel(MachineRepresentation::kWord32);

  if (object_can_be_null) {
    const int kResult = config.null_succeeds ? 1 : 0;
    gasm_.GotoIf(gasm_.TaggedEqual(object, Null()), &end_label,
                 BranchHint::kFalse, gasm_.Int32Constant(kResult));
  }

  // TODO(7748): In some cases the Smi check is redundant. If we had information
  // about the source type, we could skip it in those cases.
  gasm_.GotoIf(gasm_.IsI31(object), &end_label, gasm_.Int32Constant(0));

  Node* map = gasm_.LoadMap(object);

  // First, check if types happen to be equal. This has been shown to give large
  // speedups.
  gasm_.GotoIf(gasm_.TaggedEqual(map, rtt), &end_label, BranchHint::kTrue,
               gasm_.Int32Constant(1));

  Node* type_info = gasm_.LoadWasmTypeInfo(map);
  DCHECK_GE(rtt_depth, 0);
  // If the depth of the rtt is known to be less that the minimum supertype
  // array length, we can access the supertype without bounds-checking the
  // supertype array.
  if (static_cast<uint32_t>(rtt_depth) >= wasm::kMinimumSupertypeArraySize) {
    Node* supertypes_length =
        gasm_.BuildChangeSmiToIntPtr(gasm_.LoadImmutableFromObject(
            MachineType::TaggedSigned(), type_info,
            wasm::ObjectAccess::ToTagged(
                WasmTypeInfo::kSupertypesLengthOffset)));
    gasm_.GotoIfNot(
        gasm_.UintLessThan(gasm_.IntPtrConstant(rtt_depth), supertypes_length),
        &end_label, BranchHint::kTrue, gasm_.Int32Constant(0));
  }

  Node* maybe_match = gasm_.LoadImmutableFromObject(
      MachineType::TaggedPointer(), type_info,
      wasm::ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset +
                                   kTaggedSize * rtt_depth));

  gasm_.Goto(&end_label, gasm_.TaggedEqual(maybe_match, rtt));

  gasm_.Bind(&end_label);

  ReplaceWithValue(node, end_label.PhiAt(0), gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(end_label.PhiAt(0));  // Meaningless argument.
}

Reduction WasmGCLowering::ReduceWasmTypeCast(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmTypeCast);

  Node* object = node->InputAt(0);
  Node* rtt = node->InputAt(1);
  Node* effect_input = NodeProperties::GetEffectInput(node);
  Node* control_input = NodeProperties::GetControlInput(node);
  auto config = OpParameter<WasmTypeCheckConfig>(node->op());
  int rtt_depth = config.rtt_depth;
  bool object_can_be_null = config.object_can_be_null;

  gasm_.InitializeEffectControl(effect_input, control_input);

  auto end_label = gasm_.MakeLabel();

  if (object_can_be_null) {
    gasm_.GotoIf(gasm_.TaggedEqual(object, Null()), &end_label,
                 BranchHint::kFalse);
  }

  Node* map = gasm_.LoadMap(object);

  // First, check if types happen to be equal. This has been shown to give large
  // speedups.
  gasm_.GotoIf(gasm_.TaggedEqual(map, rtt), &end_label, BranchHint::kTrue);

  Node* type_info = gasm_.LoadWasmTypeInfo(map);
  DCHECK_GE(rtt_depth, 0);
  // If the depth of the rtt is known to be less that the minimum supertype
  // array length, we can access the supertype without bounds-checking the
  // supertype array.
  if (static_cast<uint32_t>(rtt_depth) >= wasm::kMinimumSupertypeArraySize) {
    Node* supertypes_length =
        gasm_.BuildChangeSmiToIntPtr(gasm_.LoadImmutableFromObject(
            MachineType::TaggedSigned(), type_info,
            wasm::ObjectAccess::ToTagged(
                WasmTypeInfo::kSupertypesLengthOffset)));
    gasm_.TrapUnless(
        gasm_.UintLessThan(gasm_.IntPtrConstant(rtt_depth), supertypes_length),
        TrapId::kTrapIllegalCast);
  }

  Node* maybe_match = gasm_.LoadImmutableFromObject(
      MachineType::TaggedPointer(), type_info,
      wasm::ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset +
                                   kTaggedSize * rtt_depth));

  gasm_.TrapUnless(gasm_.TaggedEqual(maybe_match, rtt),
                   TrapId::kTrapIllegalCast);
  gasm_.Goto(&end_label);

  gasm_.Bind(&end_label);

  ReplaceWithValue(node, object, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(object);
}

Reduction WasmGCLowering::ReduceAssertNotNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAssertNotNull);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* object = NodeProperties::GetValueInput(node, 0);
  gasm_.InitializeEffectControl(effect, control);
  gasm_.TrapIf(gasm_.TaggedEqual(object, Null()), TrapId::kTrapNullDereference);

  ReplaceWithValue(node, object, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(object);
}

Reduction WasmGCLowering::ReduceNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kNull);
  return Replace(Null());
}

Reduction WasmGCLowering::ReduceIsNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kIsNull);
  Node* object = NodeProperties::GetValueInput(node, 0);
  return Replace(gasm_.TaggedEqual(object, Null()));
}

Reduction WasmGCLowering::ReduceIsNotNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kIsNotNull);
  Node* object = NodeProperties::GetValueInput(node, 0);
  return Replace(gasm_.Word32Equal(gasm_.TaggedEqual(object, Null()),
                                   gasm_.Int32Constant(0)));
}

Reduction WasmGCLowering::ReduceRttCanon(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kRttCanon);
  int type_index = OpParameter<int>(node->op());
  Node* maps_list = gasm_.LoadImmutable(
      MachineType::TaggedPointer(), instance_node_,
      WasmInstanceObject::kManagedObjectMapsOffset - kHeapObjectTag);
  return Replace(gasm_.LoadImmutable(
      MachineType::TaggedPointer(), maps_list,
      wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(type_index)));
}

Reduction WasmGCLowering::ReduceTypeGuard(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kTypeGuard);
  Node* alias = NodeProperties::GetValueInput(node, 0);
  ReplaceWithValue(node, alias);
  node->Kill();
  return Replace(alias);
}

Reduction WasmGCLowering::ReduceWasmExternInternalize(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmExternInternalize);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* object = NodeProperties::GetValueInput(node, 0);
  gasm_.InitializeEffectControl(effect, control);
  auto end = gasm_.MakeLabel(MachineRepresentation::kTaggedPointer);

  if (!v8_flags.wasm_gc_js_interop) {
    Node* context = gasm_.LoadImmutable(
        MachineType::TaggedPointer(), instance_node_,
        WasmInstanceObject::kNativeContextOffset - kHeapObjectTag);
    Node* obj = gasm_.CallBuiltin(
        Builtin::kWasmGetOwnProperty, Operator::kEliminatable, object,
        RootNode(RootIndex::kwasm_wrapped_object_symbol), context);
    // Invalid object wrappers (i.e. any other JS object that doesn't have the
    // magic hidden property) will return {undefined}. Map that to {object}.
    Node* is_undefined =
        gasm_.TaggedEqual(obj, RootNode(RootIndex::kUndefinedValue));
    gasm_.GotoIf(is_undefined, &end, object);
    gasm_.Goto(&end, obj);
  } else {
    gasm_.Goto(&end, object);
  }
  gasm_.Bind(&end);
  Node* replacement = end.PhiAt(0);
  ReplaceWithValue(node, replacement, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(replacement);
}

Reduction WasmGCLowering::ReduceWasmExternExternalize(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmExternExternalize);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* object = NodeProperties::GetValueInput(node, 0);
  gasm_.InitializeEffectControl(effect, control);

  auto end = gasm_.MakeLabel(MachineRepresentation::kTaggedPointer);
  if (!v8_flags.wasm_gc_js_interop) {
    auto wrap = gasm_.MakeLabel();
    gasm_.GotoIf(gasm_.IsI31(object), &end, object);
    gasm_.GotoIf(gasm_.IsDataRefMap(gasm_.LoadMap(object)), &wrap);
    // This includes the case where {node == null}.
    gasm_.Goto(&end, object);

    gasm_.Bind(&wrap);
    Node* context = gasm_.LoadImmutable(
        MachineType::TaggedPointer(), instance_node_,
        WasmInstanceObject::kNativeContextOffset - kHeapObjectTag);
    Node* wrapped = gasm_.CallBuiltin(Builtin::kWasmAllocateObjectWrapper,
                                      Operator::kEliminatable, object, context);
    gasm_.Goto(&end, wrapped);
  } else {
    gasm_.Goto(&end, object);
  }

  gasm_.Bind(&end);
  Node* replacement = end.PhiAt(0);
  ReplaceWithValue(node, replacement, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(replacement);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
