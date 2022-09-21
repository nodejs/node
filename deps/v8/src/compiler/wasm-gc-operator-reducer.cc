// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-gc-operator-reducer.h"

#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace compiler {

WasmGCOperatorReducer::WasmGCOperatorReducer(Editor* editor, Zone* temp_zone_,
                                             MachineGraph* mcgraph,
                                             const wasm::WasmModule* module)
    : AdvancedReducerWithControlPathState(editor, temp_zone_, mcgraph->graph()),
      mcgraph_(mcgraph),
      gasm_(mcgraph, mcgraph->zone()),
      module_(module) {}

Reduction WasmGCOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kStart:
      return ReduceStart(node);
    case IrOpcode::kAssertNotNull:
      return ReduceAssertNotNull(node);
    case IrOpcode::kIsNull:
    case IrOpcode::kIsNotNull:
      return ReduceCheckNull(node);
    case IrOpcode::kWasmTypeCheck:
      return ReduceWasmTypeCheck(node);
    case IrOpcode::kWasmTypeCast:
      return ReduceWasmTypeCast(node);
    case IrOpcode::kMerge:
      return ReduceMerge(node);
    case IrOpcode::kIfTrue:
      return ReduceIf(node, true);
    case IrOpcode::kIfFalse:
      return ReduceIf(node, false);
    case IrOpcode::kLoop:
      return TakeStatesFromFirstControl(node);
    default:
      if (node->op()->ControlOutputCount() > 0) {
        DCHECK_EQ(1, node->op()->ControlInputCount());
        return TakeStatesFromFirstControl(node);
      } else {
        return NoChange();
      }
  }
}

namespace {
bool InDeadBranch(Node* node) {
  return node->opcode() == IrOpcode::kDead ||
         NodeProperties::GetType(node).AsWasm().type.is_bottom();
}

Node* GetAlias(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kWasmTypeCheck:
    case IrOpcode::kTypeGuard:
    case IrOpcode::kAssertNotNull:
      return NodeProperties::GetValueInput(node, 0);
    default:
      return nullptr;
  }
}

}  // namespace

Node* WasmGCOperatorReducer::SetType(Node* node, wasm::ValueType type) {
  NodeProperties::SetType(node, Type::Wasm(type, module_, graph()->zone()));
  return node;
}

Reduction WasmGCOperatorReducer::UpdateNodeAndAliasesTypes(
    Node* state_owner, ControlPathTypes parent_state, Node* node,
    wasm::TypeInModule type, bool in_new_block) {
  ControlPathTypes previous_knowledge = GetState(state_owner);
  if (!previous_knowledge.IsEmpty()) {
    NodeWithType current_info = previous_knowledge.LookupState(node);
    if (current_info.IsSet() && current_info.type == type) return NoChange();
  }
  Node* current = node;
  ControlPathTypes current_state = parent_state;
  while (current != nullptr) {
    UpdateStates(state_owner, current_state, current, {current, type},
                 in_new_block);
    current = GetAlias(current);
    current_state = GetState(state_owner);
    in_new_block = false;
  }
  return Changed(state_owner);
}

Reduction WasmGCOperatorReducer::ReduceStart(Node* node) {
  return UpdateStates(node, ControlPathTypes(zone()));
}

wasm::TypeInModule WasmGCOperatorReducer::ObjectTypeFromContext(Node* object,
                                                                Node* control) {
  if (InDeadBranch(object)) return {};
  if (!IsReduced(control)) return {};
  wasm::TypeInModule type_from_node = NodeProperties::GetType(object).AsWasm();
  ControlPathTypes state = GetState(control);
  NodeWithType type_from_state = state.LookupState(object);
  // We manually resolve TypeGuard aliases in the state.
  while (object->opcode() == IrOpcode::kTypeGuard && !type_from_state.IsSet()) {
    object = NodeProperties::GetValueInput(object, 0);
    type_from_state = state.LookupState(object);
  }
  return type_from_state.IsSet()
             ? wasm::Intersection(type_from_node, type_from_state.type)
             : type_from_node;
}

Reduction WasmGCOperatorReducer::ReduceIf(Node* node, bool condition) {
  DCHECK(node->opcode() == IrOpcode::kIfTrue ||
         node->opcode() == IrOpcode::kIfFalse);
  Node* branch = NodeProperties::GetControlInput(node);
  DCHECK_EQ(branch->opcode(), IrOpcode::kBranch);
  if (!IsReduced(branch)) return NoChange();
  ControlPathTypes parent_state = GetState(branch);
  Node* condition_node = NodeProperties::GetValueInput(branch, 0);
  switch (condition_node->opcode()) {
    case IrOpcode::kWasmTypeCheck: {
      if (!condition) break;
      Node* object = NodeProperties::GetValueInput(condition_node, 0);
      wasm::TypeInModule object_type = ObjectTypeFromContext(object, branch);
      if (object_type.type.is_bottom()) return NoChange();

      Node* rtt = NodeProperties::GetValueInput(condition_node, 1);
      wasm::ValueType rtt_type = wasm::ValueType::RefNull(
          NodeProperties::GetType(rtt).AsWasm().type.ref_index());

      // TODO(manoskouk): Think about {module_} below if we have cross-module
      // inlining.
      wasm::TypeInModule new_type =
          wasm::Intersection(object_type, {rtt_type, module_});
      return UpdateNodeAndAliasesTypes(node, parent_state, object, new_type,
                                       true);
    }
    case IrOpcode::kIsNull:
    case IrOpcode::kIsNotNull: {
      Node* object = NodeProperties::GetValueInput(condition_node, 0);
      Node* control = NodeProperties::GetControlInput(condition_node);
      wasm::TypeInModule object_type = ObjectTypeFromContext(object, control);
      if (object_type.type.is_bottom()) return NoChange();
      // If the checked value is null, narrow the type to the corresponding
      // null type, otherwise to a non-null reference.
      bool is_null =
          condition == (condition_node->opcode() == IrOpcode::kIsNull);
      object_type.type = is_null ? wasm::ToNullSentinel(object_type)
                                 : object_type.type.AsNonNull();
      return UpdateNodeAndAliasesTypes(node, parent_state, object, object_type,
                                       true);
    }
    default:
      break;
  }
  return TakeStatesFromFirstControl(node);
}

Reduction WasmGCOperatorReducer::ReduceMerge(Node* node) {
  // Shortcut for the case when we do not know anything about some
  // input.
  Node::Inputs inputs = node->inputs();
  for (Node* input : inputs) {
    if (!IsReduced(input)) return NoChange();
  }

  auto input_it = inputs.begin();

  DCHECK_GT(inputs.count(), 0);

  ControlPathTypes types = GetState(*input_it);
  ++input_it;

  auto input_end = inputs.end();
  for (; input_it != input_end; ++input_it) {
    // Change the current type block list to a longest common prefix of this
    // state list and the other list. (The common prefix should correspond to
    // the state of the common dominator.)
    // TODO(manoskouk): Consider computing intersections for some types.
    types.ResetToCommonAncestor(GetState(*input_it));
  }
  return UpdateStates(node, types);
}

Reduction WasmGCOperatorReducer::ReduceAssertNotNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAssertNotNull);
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* control = NodeProperties::GetControlInput(node);

  wasm::TypeInModule object_type = ObjectTypeFromContext(object, control);
  if (object_type.type.is_bottom()) return NoChange();

  // Optimize the check away if the argument is known to be non-null.
  if (object_type.type.is_non_nullable()) {
    ReplaceWithValue(node, object);
    node->Kill();
    return Replace(object);
  }

  object_type.type = object_type.type.AsNonNull();

  return UpdateNodeAndAliasesTypes(node, GetState(control), node, object_type,
                                   false);
}

Reduction WasmGCOperatorReducer::ReduceCheckNull(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kIsNull ||
         node->opcode() == IrOpcode::kIsNotNull);
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* control = NodeProperties::GetControlInput(node);

  wasm::TypeInModule object_type = ObjectTypeFromContext(object, control);
  if (object_type.type.is_bottom()) return NoChange();

  // Optimize the check away if the argument is known to be non-null.
  if (object_type.type.is_non_nullable()) {
    ReplaceWithValue(
        node, gasm_.Int32Constant(node->opcode() == IrOpcode::kIsNull ? 0 : 1));
    node->Kill();
    return Replace(object);  // Irrelevant replacement.
  }

  // Optimize the check away if the argument is known to be null.
  if (object->opcode() == IrOpcode::kNull) {
    ReplaceWithValue(
        node, gasm_.Int32Constant(node->opcode() == IrOpcode::kIsNull ? 1 : 0));
    node->Kill();
    return Replace(object);  // Irrelevant replacement.
  }

  return NoChange();
}

Reduction WasmGCOperatorReducer::ReduceWasmTypeCast(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmTypeCast);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* rtt = NodeProperties::GetValueInput(node, 1);

  wasm::TypeInModule object_type = ObjectTypeFromContext(object, control);
  if (object_type.type.is_bottom()) return NoChange();
  if (InDeadBranch(rtt)) return NoChange();
  wasm::TypeInModule rtt_type = NodeProperties::GetType(rtt).AsWasm();

  if (wasm::IsHeapSubtypeOf(object_type.type.heap_type(),
                            wasm::HeapType(rtt_type.type.ref_index()),
                            object_type.module, rtt_type.module)) {
    // Type cast will always succeed. Remove it.
    ReplaceWithValue(node, object);
    node->Kill();
    return Replace(object);
  }

  if (wasm::HeapTypesUnrelated(object_type.type.heap_type(),
                               wasm::HeapType(rtt_type.type.ref_index()),
                               object_type.module, rtt_type.module)) {
    gasm_.InitializeEffectControl(effect, control);
    // A cast between unrelated types can only succeed if the argument is null.
    // Otherwise, it always fails.
    Node* non_trapping_condition = object_type.type.is_nullable()
                                       ? gasm_.IsNull(object)
                                       : gasm_.Int32Constant(0);
    gasm_.TrapUnless(SetType(non_trapping_condition, wasm::kWasmI32),
                     TrapId::kTrapIllegalCast);
    Node* null_node = SetType(gasm_.Null(), wasm::ToNullSentinel(object_type));
    ReplaceWithValue(node, null_node, gasm_.effect(), gasm_.control());
    node->Kill();
    return Replace(null_node);
  }

  // Remove the null check from the cast if able.
  if (!object_type.type.is_nullable()) {
    uint8_t rtt_depth = OpParameter<WasmTypeCheckConfig>(node->op()).rtt_depth;
    NodeProperties::ChangeOp(
        node, gasm_.simplified()->WasmTypeCast({false,  // object_can_be_null
                                                rtt_depth}));
  }

  // TODO(manoskouk): Think about {module_} below if we have cross-module
  // inlining.
  wasm::TypeInModule new_type = wasm::Intersection(
      object_type,
      {wasm::ValueType::RefNull(rtt_type.type.ref_index()), module_});

  return UpdateNodeAndAliasesTypes(node, GetState(control), node, new_type,
                                   false);
}

Reduction WasmGCOperatorReducer::ReduceWasmTypeCheck(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmTypeCheck);
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* rtt = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  wasm::TypeInModule object_type = ObjectTypeFromContext(object, control);
  if (object_type.type.is_bottom()) return NoChange();
  if (InDeadBranch(rtt)) return NoChange();
  wasm::TypeInModule rtt_type = NodeProperties::GetType(rtt).AsWasm();

  if (wasm::IsHeapSubtypeOf(object_type.type.heap_type(),
                            wasm::HeapType(rtt_type.type.ref_index()),
                            object_type.module, rtt_type.module)) {
    // Type cast will fail only on null.
    gasm_.InitializeEffectControl(effect, control);
    Node* condition =
        SetType(object_type.type.is_nullable() ? gasm_.IsNotNull(object)
                                               : gasm_.Int32Constant(1),
                wasm::kWasmI32);
    ReplaceWithValue(node, condition);
    node->Kill();
    return Replace(condition);
  }

  if (wasm::HeapTypesUnrelated(object_type.type.heap_type(),
                               wasm::HeapType(rtt_type.type.ref_index()),
                               object_type.module, rtt_type.module)) {
    Node* condition = SetType(gasm_.Int32Constant(0), wasm::kWasmI32);
    ReplaceWithValue(node, condition);
    node->Kill();
    return Replace(condition);
  }

  // Remove the null check from the typecheck if able.
  if (!object_type.type.is_nullable()) {
    uint8_t rtt_depth = OpParameter<WasmTypeCheckConfig>(node->op()).rtt_depth;
    NodeProperties::ChangeOp(
        node, gasm_.simplified()->WasmTypeCheck({false,  // object_can_be_null
                                                 rtt_depth}));
  }

  return TakeStatesFromFirstControl(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
