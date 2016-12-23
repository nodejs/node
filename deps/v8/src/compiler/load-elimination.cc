// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/load-elimination.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

enum Aliasing { kNoAlias, kMayAlias, kMustAlias };

Aliasing QueryAlias(Node* a, Node* b) {
  if (a == b) return kMustAlias;
  if (!NodeProperties::GetType(a)->Maybe(NodeProperties::GetType(b))) {
    return kNoAlias;
  }
  switch (b->opcode()) {
    case IrOpcode::kAllocate: {
      switch (a->opcode()) {
        case IrOpcode::kAllocate:
        case IrOpcode::kHeapConstant:
        case IrOpcode::kParameter:
          return kNoAlias;
        default:
          break;
      }
      break;
    }
    case IrOpcode::kFinishRegion:
      return QueryAlias(a, b->InputAt(0));
    default:
      break;
  }
  switch (a->opcode()) {
    case IrOpcode::kAllocate: {
      switch (b->opcode()) {
        case IrOpcode::kHeapConstant:
        case IrOpcode::kParameter:
          return kNoAlias;
        default:
          break;
      }
      break;
    }
    case IrOpcode::kFinishRegion:
      return QueryAlias(a->InputAt(0), b);
    default:
      break;
  }
  return kMayAlias;
}

bool MayAlias(Node* a, Node* b) { return QueryAlias(a, b) != kNoAlias; }

bool MustAlias(Node* a, Node* b) { return QueryAlias(a, b) == kMustAlias; }

}  // namespace

Reduction LoadElimination::Reduce(Node* node) {
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
    case IrOpcode::kArrayBufferWasNeutered:
      return ReduceArrayBufferWasNeutered(node);
    case IrOpcode::kCheckMaps:
      return ReduceCheckMaps(node);
    case IrOpcode::kEnsureWritableFastElements:
      return ReduceEnsureWritableFastElements(node);
    case IrOpcode::kMaybeGrowFastElements:
      return ReduceMaybeGrowFastElements(node);
    case IrOpcode::kTransitionElementsKind:
      return ReduceTransitionElementsKind(node);
    case IrOpcode::kLoadField:
      return ReduceLoadField(node);
    case IrOpcode::kStoreField:
      return ReduceStoreField(node);
    case IrOpcode::kLoadElement:
      return ReduceLoadElement(node);
    case IrOpcode::kStoreElement:
      return ReduceStoreElement(node);
    case IrOpcode::kStoreTypedElement:
      return ReduceStoreTypedElement(node);
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

namespace {

bool IsCompatibleCheck(Node const* a, Node const* b) {
  if (a->op() != b->op()) return false;
  for (int i = a->op()->ValueInputCount(); --i >= 0;) {
    if (!MustAlias(a->InputAt(i), b->InputAt(i))) return false;
  }
  return true;
}

}  // namespace

Node* LoadElimination::AbstractChecks::Lookup(Node* node) const {
  for (Node* const check : nodes_) {
    if (check && IsCompatibleCheck(check, node)) {
      return check;
    }
  }
  return nullptr;
}

bool LoadElimination::AbstractChecks::Equals(AbstractChecks const* that) const {
  if (this == that) return true;
  for (size_t i = 0; i < arraysize(nodes_); ++i) {
    if (Node* this_node = this->nodes_[i]) {
      for (size_t j = 0;; ++j) {
        if (j == arraysize(nodes_)) return false;
        if (that->nodes_[j] == this_node) break;
      }
    }
  }
  for (size_t i = 0; i < arraysize(nodes_); ++i) {
    if (Node* that_node = that->nodes_[i]) {
      for (size_t j = 0;; ++j) {
        if (j == arraysize(nodes_)) return false;
        if (this->nodes_[j] == that_node) break;
      }
    }
  }
  return true;
}

LoadElimination::AbstractChecks const* LoadElimination::AbstractChecks::Merge(
    AbstractChecks const* that, Zone* zone) const {
  if (this->Equals(that)) return this;
  AbstractChecks* copy = new (zone) AbstractChecks(zone);
  for (Node* const this_node : this->nodes_) {
    if (this_node == nullptr) continue;
    for (Node* const that_node : that->nodes_) {
      if (this_node == that_node) {
        copy->nodes_[copy->next_index_++] = this_node;
        break;
      }
    }
  }
  copy->next_index_ %= arraysize(nodes_);
  return copy;
}

void LoadElimination::AbstractChecks::Print() const {
  for (Node* const node : nodes_) {
    if (node != nullptr) {
      PrintF("    #%d:%s\n", node->id(), node->op()->mnemonic());
    }
  }
}

Node* LoadElimination::AbstractElements::Lookup(Node* object,
                                                Node* index) const {
  for (Element const element : elements_) {
    if (element.object == nullptr) continue;
    DCHECK_NOT_NULL(element.index);
    DCHECK_NOT_NULL(element.value);
    if (MustAlias(object, element.object) && MustAlias(index, element.index)) {
      return element.value;
    }
  }
  return nullptr;
}

LoadElimination::AbstractElements const*
LoadElimination::AbstractElements::Kill(Node* object, Node* index,
                                        Zone* zone) const {
  for (Element const element : this->elements_) {
    if (element.object == nullptr) continue;
    if (MayAlias(object, element.object)) {
      AbstractElements* that = new (zone) AbstractElements(zone);
      for (Element const element : this->elements_) {
        if (element.object == nullptr) continue;
        DCHECK_NOT_NULL(element.index);
        DCHECK_NOT_NULL(element.value);
        if (!MayAlias(object, element.object) ||
            !NodeProperties::GetType(index)->Maybe(
                NodeProperties::GetType(element.index))) {
          that->elements_[that->next_index_++] = element;
        }
      }
      that->next_index_ %= arraysize(elements_);
      return that;
    }
  }
  return this;
}

bool LoadElimination::AbstractElements::Equals(
    AbstractElements const* that) const {
  if (this == that) return true;
  for (size_t i = 0; i < arraysize(elements_); ++i) {
    Element this_element = this->elements_[i];
    if (this_element.object == nullptr) continue;
    for (size_t j = 0;; ++j) {
      if (j == arraysize(elements_)) return false;
      Element that_element = that->elements_[j];
      if (this_element.object == that_element.object &&
          this_element.index == that_element.index &&
          this_element.value == that_element.value) {
        break;
      }
    }
  }
  for (size_t i = 0; i < arraysize(elements_); ++i) {
    Element that_element = that->elements_[i];
    if (that_element.object == nullptr) continue;
    for (size_t j = 0;; ++j) {
      if (j == arraysize(elements_)) return false;
      Element this_element = this->elements_[j];
      if (that_element.object == this_element.object &&
          that_element.index == this_element.index &&
          that_element.value == this_element.value) {
        break;
      }
    }
  }
  return true;
}

LoadElimination::AbstractElements const*
LoadElimination::AbstractElements::Merge(AbstractElements const* that,
                                         Zone* zone) const {
  if (this->Equals(that)) return this;
  AbstractElements* copy = new (zone) AbstractElements(zone);
  for (Element const this_element : this->elements_) {
    if (this_element.object == nullptr) continue;
    for (Element const that_element : that->elements_) {
      if (this_element.object == that_element.object &&
          this_element.index == that_element.index &&
          this_element.value == that_element.value) {
        copy->elements_[copy->next_index_++] = this_element;
        break;
      }
    }
  }
  copy->next_index_ %= arraysize(elements_);
  return copy;
}

void LoadElimination::AbstractElements::Print() const {
  for (Element const& element : elements_) {
    if (element.object) {
      PrintF("    #%d:%s @ #%d:%s -> #%d:%s\n", element.object->id(),
             element.object->op()->mnemonic(), element.index->id(),
             element.index->op()->mnemonic(), element.value->id(),
             element.value->op()->mnemonic());
    }
  }
}

Node* LoadElimination::AbstractField::Lookup(Node* object) const {
  for (auto pair : info_for_node_) {
    if (MustAlias(object, pair.first)) return pair.second;
  }
  return nullptr;
}

LoadElimination::AbstractField const* LoadElimination::AbstractField::Kill(
    Node* object, Zone* zone) const {
  for (auto pair : this->info_for_node_) {
    if (MayAlias(object, pair.first)) {
      AbstractField* that = new (zone) AbstractField(zone);
      for (auto pair : this->info_for_node_) {
        if (!MayAlias(object, pair.first)) that->info_for_node_.insert(pair);
      }
      return that;
    }
  }
  return this;
}

void LoadElimination::AbstractField::Print() const {
  for (auto pair : info_for_node_) {
    PrintF("    #%d:%s -> #%d:%s\n", pair.first->id(),
           pair.first->op()->mnemonic(), pair.second->id(),
           pair.second->op()->mnemonic());
  }
}

bool LoadElimination::AbstractState::Equals(AbstractState const* that) const {
  if (this->checks_) {
    if (!that->checks_ || !that->checks_->Equals(this->checks_)) {
      return false;
    }
  } else if (that->checks_) {
    return false;
  }
  if (this->elements_) {
    if (!that->elements_ || !that->elements_->Equals(this->elements_)) {
      return false;
    }
  } else if (that->elements_) {
    return false;
  }
  for (size_t i = 0u; i < arraysize(fields_); ++i) {
    AbstractField const* this_field = this->fields_[i];
    AbstractField const* that_field = that->fields_[i];
    if (this_field) {
      if (!that_field || !that_field->Equals(this_field)) return false;
    } else if (that_field) {
      return false;
    }
  }
  return true;
}

void LoadElimination::AbstractState::Merge(AbstractState const* that,
                                           Zone* zone) {
  // Merge the information we have about the checks.
  if (this->checks_) {
    this->checks_ =
        that->checks_ ? that->checks_->Merge(this->checks_, zone) : nullptr;
  }

  // Merge the information we have about the elements.
  if (this->elements_) {
    this->elements_ = that->elements_
                          ? that->elements_->Merge(this->elements_, zone)
                          : nullptr;
  }

  // Merge the information we have about the fields.
  for (size_t i = 0; i < arraysize(fields_); ++i) {
    if (this->fields_[i]) {
      if (that->fields_[i]) {
        this->fields_[i] = this->fields_[i]->Merge(that->fields_[i], zone);
      } else {
        this->fields_[i] = nullptr;
      }
    }
  }
}

Node* LoadElimination::AbstractState::LookupCheck(Node* node) const {
  return this->checks_ ? this->checks_->Lookup(node) : nullptr;
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::AddCheck(
    Node* node, Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  if (that->checks_) {
    that->checks_ = that->checks_->Extend(node, zone);
  } else {
    that->checks_ = new (zone) AbstractChecks(node, zone);
  }
  return that;
}

Node* LoadElimination::AbstractState::LookupElement(Node* object,
                                                    Node* index) const {
  if (this->elements_) {
    return this->elements_->Lookup(object, index);
  }
  return nullptr;
}

LoadElimination::AbstractState const*
LoadElimination::AbstractState::AddElement(Node* object, Node* index,
                                           Node* value, Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  if (that->elements_) {
    that->elements_ = that->elements_->Extend(object, index, value, zone);
  } else {
    that->elements_ = new (zone) AbstractElements(object, index, value, zone);
  }
  return that;
}

LoadElimination::AbstractState const*
LoadElimination::AbstractState::KillElement(Node* object, Node* index,
                                            Zone* zone) const {
  if (this->elements_) {
    AbstractElements const* that_elements =
        this->elements_->Kill(object, index, zone);
    if (this->elements_ != that_elements) {
      AbstractState* that = new (zone) AbstractState(*this);
      that->elements_ = that_elements;
      return that;
    }
  }
  return this;
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::AddField(
    Node* object, size_t index, Node* value, Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  if (that->fields_[index]) {
    that->fields_[index] = that->fields_[index]->Extend(object, value, zone);
  } else {
    that->fields_[index] = new (zone) AbstractField(object, value, zone);
  }
  return that;
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::KillField(
    Node* object, size_t index, Zone* zone) const {
  if (AbstractField const* this_field = this->fields_[index]) {
    this_field = this_field->Kill(object, zone);
    if (this->fields_[index] != this_field) {
      AbstractState* that = new (zone) AbstractState(*this);
      that->fields_[index] = this_field;
      return that;
    }
  }
  return this;
}

Node* LoadElimination::AbstractState::LookupField(Node* object,
                                                  size_t index) const {
  if (AbstractField const* this_field = this->fields_[index]) {
    return this_field->Lookup(object);
  }
  return nullptr;
}

void LoadElimination::AbstractState::Print() const {
  if (checks_) {
    PrintF("   checks:\n");
    checks_->Print();
  }
  if (elements_) {
    PrintF("   elements:\n");
    elements_->Print();
  }
  for (size_t i = 0; i < arraysize(fields_); ++i) {
    if (AbstractField const* const field = fields_[i]) {
      PrintF("   field %zu:\n", i);
      field->Print();
    }
  }
}

LoadElimination::AbstractState const*
LoadElimination::AbstractStateForEffectNodes::Get(Node* node) const {
  size_t const id = node->id();
  if (id < info_for_node_.size()) return info_for_node_[id];
  return nullptr;
}

void LoadElimination::AbstractStateForEffectNodes::Set(
    Node* node, AbstractState const* state) {
  size_t const id = node->id();
  if (id >= info_for_node_.size()) info_for_node_.resize(id + 1, nullptr);
  info_for_node_[id] = state;
}

Reduction LoadElimination::ReduceArrayBufferWasNeutered(Node* node) {
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  if (Node* const check = state->LookupCheck(node)) {
    ReplaceWithValue(node, check, effect);
    return Replace(check);
  }
  state = state->AddCheck(node, zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceCheckMaps(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  int const map_input_count = node->op()->ValueInputCount() - 1;
  if (Node* const object_map =
          state->LookupField(object, FieldIndexOf(HeapObject::kMapOffset))) {
    for (int i = 0; i < map_input_count; ++i) {
      Node* map = NodeProperties::GetValueInput(node, 1 + i);
      if (map == object_map) return Replace(effect);
    }
  }
  if (map_input_count == 1) {
    Node* const map0 = NodeProperties::GetValueInput(node, 1);
    state = state->AddField(object, FieldIndexOf(HeapObject::kMapOffset), map0,
                            zone());
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceEnsureWritableFastElements(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const elements = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  Node* fixed_array_map = jsgraph()->FixedArrayMapConstant();
  if (Node* const elements_map =
          state->LookupField(elements, FieldIndexOf(HeapObject::kMapOffset))) {
    // Check if the {elements} already have the fixed array map.
    if (elements_map == fixed_array_map) {
      ReplaceWithValue(node, elements, effect);
      return Replace(elements);
    }
  }
  // We know that the resulting elements have the fixed array map.
  state = state->AddField(node, FieldIndexOf(HeapObject::kMapOffset),
                          fixed_array_map, zone());
  // Kill the previous elements on {object}.
  state =
      state->KillField(object, FieldIndexOf(JSObject::kElementsOffset), zone());
  // Add the new elements on {object}.
  state = state->AddField(object, FieldIndexOf(JSObject::kElementsOffset), node,
                          zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceMaybeGrowFastElements(Node* node) {
  GrowFastElementsFlags flags = GrowFastElementsFlagsOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  if (flags & GrowFastElementsFlag::kDoubleElements) {
    // We know that the resulting elements have the fixed double array map.
    Node* fixed_double_array_map = jsgraph()->FixedDoubleArrayMapConstant();
    state = state->AddField(node, FieldIndexOf(HeapObject::kMapOffset),
                            fixed_double_array_map, zone());
  } else {
    // We know that the resulting elements have the fixed array map.
    Node* fixed_array_map = jsgraph()->FixedArrayMapConstant();
    state = state->AddField(node, FieldIndexOf(HeapObject::kMapOffset),
                            fixed_array_map, zone());
  }
  if (flags & GrowFastElementsFlag::kArrayObject) {
    // Kill the previous Array::length on {object}.
    state =
        state->KillField(object, FieldIndexOf(JSArray::kLengthOffset), zone());
  }
  // Kill the previous elements on {object}.
  state =
      state->KillField(object, FieldIndexOf(JSObject::kElementsOffset), zone());
  // Add the new elements on {object}.
  state = state->AddField(object, FieldIndexOf(JSObject::kElementsOffset), node,
                          zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceTransitionElementsKind(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const source_map = NodeProperties::GetValueInput(node, 1);
  Node* const target_map = NodeProperties::GetValueInput(node, 2);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  if (Node* const object_map =
          state->LookupField(object, FieldIndexOf(HeapObject::kMapOffset))) {
    if (target_map == object_map) {
      // The {object} already has the {target_map}, so this TransitionElements
      // {node} is fully redundant (independent of what {source_map} is).
      return Replace(effect);
    }
    state =
        state->KillField(object, FieldIndexOf(HeapObject::kMapOffset), zone());
    if (source_map == object_map) {
      state = state->AddField(object, FieldIndexOf(HeapObject::kMapOffset),
                              target_map, zone());
    }
  } else {
    state =
        state->KillField(object, FieldIndexOf(HeapObject::kMapOffset), zone());
  }
  ElementsTransition transition = ElementsTransitionOf(node->op());
  switch (transition) {
    case ElementsTransition::kFastTransition:
      break;
    case ElementsTransition::kSlowTransition:
      // Kill the elements as well.
      state = state->KillField(object, FieldIndexOf(JSObject::kElementsOffset),
                               zone());
      break;
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceLoadField(Node* node) {
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  int field_index = FieldIndexOf(access);
  if (field_index >= 0) {
    if (Node* replacement = state->LookupField(object, field_index)) {
      // Make sure we don't resurrect dead {replacement} nodes.
      if (!replacement->IsDead()) {
        // We might need to guard the {replacement} if the type of the
        // {node} is more precise than the type of the {replacement}.
        Type* const node_type = NodeProperties::GetType(node);
        if (!NodeProperties::GetType(replacement)->Is(node_type)) {
          replacement = graph()->NewNode(common()->TypeGuard(node_type),
                                         replacement, control);
        }
        ReplaceWithValue(node, replacement, effect);
        return Replace(replacement);
      }
    }
    state = state->AddField(object, field_index, node, zone());
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceStoreField(Node* node) {
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const new_value = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  int field_index = FieldIndexOf(access);
  if (field_index >= 0) {
    Node* const old_value = state->LookupField(object, field_index);
    if (old_value == new_value) {
      // This store is fully redundant.
      return Replace(effect);
    }
    // Kill all potentially aliasing fields and record the new value.
    state = state->KillField(object, field_index, zone());
    state = state->AddField(object, field_index, new_value, zone());
  } else {
    // Unsupported StoreField operator.
    state = empty_state();
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceLoadElement(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  if (Node* replacement = state->LookupElement(object, index)) {
    // Make sure we don't resurrect dead {replacement} nodes.
    if (!replacement->IsDead()) {
      // We might need to guard the {replacement} if the type of the
      // {node} is more precise than the type of the {replacement}.
      Type* const node_type = NodeProperties::GetType(node);
      if (!NodeProperties::GetType(replacement)->Is(node_type)) {
        replacement = graph()->NewNode(common()->TypeGuard(node_type),
                                       replacement, control);
      }
      ReplaceWithValue(node, replacement, effect);
      return Replace(replacement);
    }
  }
  state = state->AddElement(object, index, node, zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceStoreElement(Node* node) {
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const new_value = NodeProperties::GetValueInput(node, 2);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  Node* const old_value = state->LookupElement(object, index);
  if (old_value == new_value) {
    // This store is fully redundant.
    return Replace(effect);
  }
  // Kill all potentially aliasing elements.
  state = state->KillElement(object, index, zone());
  // Only record the new value if the store doesn't have an implicit truncation.
  switch (access.machine_type.representation()) {
    case MachineRepresentation::kNone:
    case MachineRepresentation::kBit:
      UNREACHABLE();
      break;
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kFloat32:
      // TODO(turbofan): Add support for doing the truncations.
      break;
    case MachineRepresentation::kFloat64:
    case MachineRepresentation::kSimd128:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      state = state->AddElement(object, index, new_value, zone());
      break;
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceStoreTypedElement(Node* node) {
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceEffectPhi(Node* node) {
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

Reduction LoadElimination::ReduceStart(Node* node) {
  return UpdateState(node, empty_state());
}

Reduction LoadElimination::ReduceOtherNode(Node* node) {
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
      // Effect terminators should be handled specially.
      return NoChange();
    }
  }
  DCHECK_EQ(0, node->op()->EffectInputCount());
  DCHECK_EQ(0, node->op()->EffectOutputCount());
  return NoChange();
}

Reduction LoadElimination::UpdateState(Node* node, AbstractState const* state) {
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

LoadElimination::AbstractState const* LoadElimination::ComputeLoopState(
    Node* node, AbstractState const* state) const {
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
    if (visited.find(current) == visited.end()) {
      visited.insert(current);
      if (!current->op()->HasProperty(Operator::kNoWrite)) {
        switch (current->opcode()) {
          case IrOpcode::kEnsureWritableFastElements: {
            Node* const object = NodeProperties::GetValueInput(current, 0);
            state = state->KillField(
                object, FieldIndexOf(JSObject::kElementsOffset), zone());
            break;
          }
          case IrOpcode::kMaybeGrowFastElements: {
            GrowFastElementsFlags flags =
                GrowFastElementsFlagsOf(current->op());
            Node* const object = NodeProperties::GetValueInput(current, 0);
            state = state->KillField(
                object, FieldIndexOf(JSObject::kElementsOffset), zone());
            if (flags & GrowFastElementsFlag::kArrayObject) {
              state = state->KillField(
                  object, FieldIndexOf(JSArray::kLengthOffset), zone());
            }
            break;
          }
          case IrOpcode::kTransitionElementsKind: {
            Node* const object = NodeProperties::GetValueInput(current, 0);
            state = state->KillField(
                object, FieldIndexOf(HeapObject::kMapOffset), zone());
            state = state->KillField(
                object, FieldIndexOf(JSObject::kElementsOffset), zone());
            break;
          }
          case IrOpcode::kStoreField: {
            FieldAccess const& access = FieldAccessOf(current->op());
            Node* const object = NodeProperties::GetValueInput(current, 0);
            int field_index = FieldIndexOf(access);
            if (field_index < 0) return empty_state();
            state = state->KillField(object, field_index, zone());
            break;
          }
          case IrOpcode::kStoreElement: {
            Node* const object = NodeProperties::GetValueInput(current, 0);
            Node* const index = NodeProperties::GetValueInput(current, 1);
            state = state->KillElement(object, index, zone());
            break;
          }
          case IrOpcode::kStoreBuffer:
          case IrOpcode::kStoreTypedElement: {
            // Doesn't affect anything we track with the state currently.
            break;
          }
          default:
            return empty_state();
        }
      }
      for (int i = 0; i < current->op()->EffectInputCount(); ++i) {
        queue.push(NodeProperties::GetEffectInput(current, i));
      }
    }
  }
  return state;
}

// static
int LoadElimination::FieldIndexOf(int offset) {
  DCHECK_EQ(0, offset % kPointerSize);
  int field_index = offset / kPointerSize;
  if (field_index >= static_cast<int>(kMaxTrackedFields)) return -1;
  return field_index;
}

// static
int LoadElimination::FieldIndexOf(FieldAccess const& access) {
  MachineRepresentation rep = access.machine_type.representation();
  switch (rep) {
    case MachineRepresentation::kNone:
    case MachineRepresentation::kBit:
      UNREACHABLE();
      break;
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord64:
      if (rep != MachineType::PointerRepresentation()) {
        return -1;  // We currently only track pointer size fields.
      }
      break;
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kFloat32:
      return -1;  // Currently untracked.
    case MachineRepresentation::kFloat64:
    case MachineRepresentation::kSimd128:
      return -1;  // Currently untracked.
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      // TODO(bmeurer): Check that we never do overlapping load/stores of
      // individual parts of Float64/Simd128 values.
      break;
  }
  DCHECK_EQ(kTaggedBase, access.base_is_tagged);
  return FieldIndexOf(access.offset);
}

CommonOperatorBuilder* LoadElimination::common() const {
  return jsgraph()->common();
}

Graph* LoadElimination::graph() const { return jsgraph()->graph(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
