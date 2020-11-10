// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/csa-load-elimination.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction CsaLoadElimination::Reduce(Node* node) {
  if (FLAG_trace_turbo_load_elimination) {
    if (node->op()->EffectInputCount() > 0) {
      PrintF(" visit #%d:%s", node->id(), node->op()->mnemonic());
      if (node->op()->ValueInputCount() > 0) {
        PrintF("(");
        for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
          if (i > 0) PrintF(", ");
          Node* const value = NodeProperties::GetValueInput(node, i);
          PrintF("#%d:%s", value->id(), value->op()->mnemonic());
        }
        PrintF(")");
      }
      PrintF("\n");
      for (int i = 0; i < node->op()->EffectInputCount(); ++i) {
        Node* const effect = NodeProperties::GetEffectInput(node, i);
        if (AbstractState const* const state = node_states_.Get(effect)) {
          PrintF("  state[%i]: #%d:%s\n", i, effect->id(),
                 effect->op()->mnemonic());
          state->Print();
        } else {
          PrintF("  no state[%i]: #%d:%s\n", i, effect->id(),
                 effect->op()->mnemonic());
        }
      }
    }
  }
  switch (node->opcode()) {
    case IrOpcode::kLoadFromObject:
      return ReduceLoadFromObject(node, ObjectAccessOf(node->op()));
    case IrOpcode::kStoreToObject:
      return ReduceStoreToObject(node, ObjectAccessOf(node->op()));
    case IrOpcode::kDebugBreak:
    case IrOpcode::kAbortCSAAssert:
      // Avoid changing optimizations in the presence of debug instructions.
      return PropagateInputState(node);
    case IrOpcode::kCall:
      return ReduceCall(node);
    case IrOpcode::kEffectPhi:
      return ReduceEffectPhi(node);
    case IrOpcode::kDead:
      break;
    case IrOpcode::kStart:
      return ReduceStart(node);
    default:
      return ReduceOtherNode(node);
  }
  return NoChange();
}

namespace CsaLoadEliminationHelpers {

bool IsCompatible(MachineRepresentation r1, MachineRepresentation r2) {
  if (r1 == r2) return true;
  return IsAnyTagged(r1) && IsAnyTagged(r2);
}

bool ObjectMayAlias(Node* a, Node* b) {
  if (a != b) {
    if (b->opcode() == IrOpcode::kAllocate) {
      std::swap(a, b);
    }
    if (a->opcode() == IrOpcode::kAllocate) {
      switch (b->opcode()) {
        case IrOpcode::kAllocate:
        case IrOpcode::kHeapConstant:
        case IrOpcode::kParameter:
          return false;
        default:
          break;
      }
    }
  }
  return true;
}

bool OffsetMayAlias(Node* offset1, MachineRepresentation repr1, Node* offset2,
                    MachineRepresentation repr2) {
  IntPtrMatcher matcher1(offset1);
  IntPtrMatcher matcher2(offset2);
  // If either of the offsets is variable, accesses may alias
  if (!matcher1.HasValue() || !matcher2.HasValue()) {
    return true;
  }
  // Otherwise, we return whether accesses overlap
  intptr_t start1 = matcher1.Value();
  intptr_t end1 = start1 + ElementSizeInBytes(repr1);
  intptr_t start2 = matcher2.Value();
  intptr_t end2 = start2 + ElementSizeInBytes(repr2);
  return !(end1 <= start2 || end2 <= start1);
}

}  // namespace CsaLoadEliminationHelpers

namespace Helpers = CsaLoadEliminationHelpers;

void CsaLoadElimination::AbstractState::Merge(AbstractState const* that,
                                              Zone* zone) {
  FieldInfo empty_info;
  for (std::pair<Field, FieldInfo> entry : field_infos_) {
    if (that->field_infos_.Get(entry.first) != entry.second) {
      field_infos_.Set(entry.first, empty_info);
    }
  }
}

CsaLoadElimination::AbstractState const*
CsaLoadElimination::AbstractState::KillField(Node* kill_object,
                                             Node* kill_offset,
                                             MachineRepresentation kill_repr,
                                             Zone* zone) const {
  FieldInfo empty_info;
  AbstractState* that = new (zone) AbstractState(*this);
  for (std::pair<Field, FieldInfo> entry : that->field_infos_) {
    Field field = entry.first;
    MachineRepresentation field_repr = entry.second.representation;
    if (Helpers::OffsetMayAlias(kill_offset, kill_repr, field.second,
                                field_repr) &&
        Helpers::ObjectMayAlias(kill_object, field.first)) {
      that->field_infos_.Set(field, empty_info);
    }
  }
  return that;
}

CsaLoadElimination::AbstractState const*
CsaLoadElimination::AbstractState::AddField(Node* object, Node* offset,
                                            CsaLoadElimination::FieldInfo info,
                                            Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  that->field_infos_.Set({object, offset}, info);
  return that;
}

CsaLoadElimination::FieldInfo CsaLoadElimination::AbstractState::Lookup(
    Node* object, Node* offset) const {
  if (object->IsDead()) {
    return {};
  }
  return field_infos_.Get({object, offset});
}

void CsaLoadElimination::AbstractState::Print() const {
  for (std::pair<Field, FieldInfo> entry : field_infos_) {
    Field field = entry.first;
    Node* object = field.first;
    Node* offset = field.second;
    FieldInfo info = entry.second;
    PrintF("    #%d+#%d:%s -> #%d:%s [repr=%s]\n", object->id(), offset->id(),
           object->op()->mnemonic(), info.value->id(),
           info.value->op()->mnemonic(),
           MachineReprToString(info.representation));
  }
}

Reduction CsaLoadElimination::ReduceLoadFromObject(Node* node,
                                                   ObjectAccess const& access) {
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* offset = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  MachineRepresentation representation = access.machine_type.representation();
  FieldInfo lookup_result = state->Lookup(object, offset);
  if (!lookup_result.IsEmpty()) {
    // Make sure we don't reuse values that were recorded with a different
    // representation or resurrect dead {replacement} nodes.
    Node* replacement = lookup_result.value;
    if (Helpers::IsCompatible(representation, lookup_result.representation) &&
        !replacement->IsDead()) {
      ReplaceWithValue(node, replacement, effect);
      return Replace(replacement);
    }
  }
  FieldInfo info(node, representation);
  state = state->AddField(object, offset, info, zone());

  return UpdateState(node, state);
}

Reduction CsaLoadElimination::ReduceStoreToObject(Node* node,
                                                  ObjectAccess const& access) {
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* offset = NodeProperties::GetValueInput(node, 1);
  Node* value = NodeProperties::GetValueInput(node, 2);
  Node* effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  FieldInfo info(value, access.machine_type.representation());
  state = state->KillField(object, offset, info.representation, zone());
  state = state->AddField(object, offset, info, zone());

  return UpdateState(node, state);
}

Reduction CsaLoadElimination::ReduceEffectPhi(Node* node) {
  Node* const effect0 = NodeProperties::GetEffectInput(node, 0);
  Node* const control = NodeProperties::GetControlInput(node);
  AbstractState const* state0 = node_states_.Get(effect0);
  if (state0 == nullptr) return NoChange();
  if (control->opcode() == IrOpcode::kLoop) {
    // Here we rely on having only reducible loops:
    // The loop entry edge always dominates the header, so we can just take
    // the state from the first input, and compute the loop state based on it.
    AbstractState const* state = ComputeLoopState(node, state0);
    return UpdateState(node, state);
  }
  DCHECK_EQ(IrOpcode::kMerge, control->opcode());

  // Shortcut for the case when we do not know anything about some input.
  int const input_count = node->op()->EffectInputCount();
  for (int i = 1; i < input_count; ++i) {
    Node* const effect = NodeProperties::GetEffectInput(node, i);
    if (node_states_.Get(effect) == nullptr) return NoChange();
  }

  // Make a copy of the first input's state and merge with the state
  // from other inputs.
  AbstractState* state = new (zone()) AbstractState(*state0);
  for (int i = 1; i < input_count; ++i) {
    Node* const input = NodeProperties::GetEffectInput(node, i);
    state->Merge(node_states_.Get(input), zone());
  }
  return UpdateState(node, state);
}

Reduction CsaLoadElimination::ReduceStart(Node* node) {
  return UpdateState(node, empty_state());
}

Reduction CsaLoadElimination::ReduceCall(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  ExternalReferenceMatcher m(value);
  if (m.Is(ExternalReference::check_object_type())) {
    return PropagateInputState(node);
  }
  return ReduceOtherNode(node);
}

Reduction CsaLoadElimination::ReduceOtherNode(Node* node) {
  if (node->op()->EffectInputCount() == 1) {
    if (node->op()->EffectOutputCount() == 1) {
      Node* const effect = NodeProperties::GetEffectInput(node);
      AbstractState const* state = node_states_.Get(effect);
      // If we do not know anything about the predecessor, do not propagate
      // just yet because we will have to recompute anyway once we compute
      // the predecessor.
      if (state == nullptr) return NoChange();
      // Check if this {node} has some uncontrolled side effects.
      if (!node->op()->HasProperty(Operator::kNoWrite)) {
        state = empty_state();
      }
      return UpdateState(node, state);
    } else {
      return NoChange();
    }
  }
  DCHECK_EQ(0, node->op()->EffectInputCount());
  DCHECK_EQ(0, node->op()->EffectOutputCount());
  return NoChange();
}

Reduction CsaLoadElimination::UpdateState(Node* node,
                                          AbstractState const* state) {
  AbstractState const* original = node_states_.Get(node);
  // Only signal that the {node} has Changed, if the information about {state}
  // has changed wrt. the {original}.
  if (state != original) {
    if (original == nullptr || !state->Equals(original)) {
      node_states_.Set(node, state);
      return Changed(node);
    }
  }
  return NoChange();
}

Reduction CsaLoadElimination::PropagateInputState(Node* node) {
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  return UpdateState(node, state);
}

CsaLoadElimination::AbstractState const* CsaLoadElimination::ComputeLoopState(
    Node* node, AbstractState const* state) const {
  DCHECK_EQ(node->opcode(), IrOpcode::kEffectPhi);
  Node* const control = NodeProperties::GetControlInput(node);
  ZoneQueue<Node*> queue(zone());
  ZoneSet<Node*> visited(zone());
  visited.insert(node);
  for (int i = 1; i < control->InputCount(); ++i) {
    queue.push(node->InputAt(i));
  }
  while (!queue.empty()) {
    Node* const current = queue.front();
    queue.pop();
    if (visited.insert(current).second) {
      if (!current->op()->HasProperty(Operator::kNoWrite)) {
        return empty_state();
      }
      for (int i = 0; i < current->op()->EffectInputCount(); ++i) {
        queue.push(NodeProperties::GetEffectInput(current, i));
      }
    }
  }
  return state;
}

CommonOperatorBuilder* CsaLoadElimination::common() const {
  return jsgraph()->common();
}

Graph* CsaLoadElimination::graph() const { return jsgraph()->graph(); }

Isolate* CsaLoadElimination::isolate() const { return jsgraph()->isolate(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
