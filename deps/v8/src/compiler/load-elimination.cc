// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/load-elimination.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/heap/factory.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool IsRename(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kCheckHeapObject:
    case IrOpcode::kFinishRegion:
    case IrOpcode::kTypeGuard:
      return true;
    default:
      return false;
  }
}

Node* ResolveRenames(Node* node) {
  while (IsRename(node)) {
    node = node->InputAt(0);
  }
  return node;
}

bool MayAlias(Node* a, Node* b) {
  if (a != b) {
    if (!NodeProperties::GetType(a).Maybe(NodeProperties::GetType(b))) {
      return false;
    } else if (IsRename(b)) {
      return MayAlias(a, b->InputAt(0));
    } else if (IsRename(a)) {
      return MayAlias(a->InputAt(0), b);
    } else if (b->opcode() == IrOpcode::kAllocate) {
      switch (a->opcode()) {
        case IrOpcode::kAllocate:
        case IrOpcode::kHeapConstant:
        case IrOpcode::kParameter:
          return false;
        default:
          break;
      }
    } else if (a->opcode() == IrOpcode::kAllocate) {
      switch (b->opcode()) {
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

bool MustAlias(Node* a, Node* b) {
  return ResolveRenames(a) == ResolveRenames(b);
}

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
    case IrOpcode::kMapGuard:
      return ReduceMapGuard(node);
    case IrOpcode::kCheckMaps:
      return ReduceCheckMaps(node);
    case IrOpcode::kCompareMaps:
      return ReduceCompareMaps(node);
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
    case IrOpcode::kTransitionAndStoreElement:
      return ReduceTransitionAndStoreElement(node);
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

bool IsCompatible(MachineRepresentation r1, MachineRepresentation r2) {
  if (r1 == r2) return true;
  return IsAnyTagged(r1) && IsAnyTagged(r2);
}

}  // namespace

Node* LoadElimination::AbstractElements::Lookup(
    Node* object, Node* index, MachineRepresentation representation) const {
  for (Element const element : elements_) {
    if (element.object == nullptr) continue;
    DCHECK_NOT_NULL(element.index);
    DCHECK_NOT_NULL(element.value);
    if (MustAlias(object, element.object) && MustAlias(index, element.index) &&
        IsCompatible(representation, element.representation)) {
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
            !NodeProperties::GetType(index).Maybe(
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
    if (pair.first->IsDead()) continue;
    if (MustAlias(object, pair.first)) return pair.second.value;
  }
  return nullptr;
}

namespace {

bool MayAlias(MaybeHandle<Name> x, MaybeHandle<Name> y) {
  if (!x.address()) return true;
  if (!y.address()) return true;
  if (x.address() != y.address()) return false;
  return true;
}

}  // namespace

class LoadElimination::AliasStateInfo {
 public:
  AliasStateInfo(const AbstractState* state, Node* object, Handle<Map> map)
      : state_(state), object_(object), map_(map) {}
  AliasStateInfo(const AbstractState* state, Node* object)
      : state_(state), object_(object) {}

  bool MayAlias(Node* other) const;

 private:
  const AbstractState* state_;
  Node* object_;
  MaybeHandle<Map> map_;
};

LoadElimination::AbstractField const* LoadElimination::AbstractField::Kill(
    const AliasStateInfo& alias_info, MaybeHandle<Name> name,
    Zone* zone) const {
  for (auto pair : this->info_for_node_) {
    if (pair.first->IsDead()) continue;
    if (alias_info.MayAlias(pair.first)) {
      AbstractField* that = new (zone) AbstractField(zone);
      for (auto pair : this->info_for_node_) {
        if (!alias_info.MayAlias(pair.first) ||
            !MayAlias(name, pair.second.name)) {
          that->info_for_node_.insert(pair);
        }
      }
      return that;
    }
  }
  return this;
}

void LoadElimination::AbstractField::Print() const {
  for (auto pair : info_for_node_) {
    PrintF("    #%d:%s -> #%d:%s\n", pair.first->id(),
           pair.first->op()->mnemonic(), pair.second.value->id(),
           pair.second.value->op()->mnemonic());
  }
}

LoadElimination::AbstractMaps::AbstractMaps(Zone* zone)
    : info_for_node_(zone) {}

LoadElimination::AbstractMaps::AbstractMaps(Node* object,
                                            ZoneHandleSet<Map> maps, Zone* zone)
    : info_for_node_(zone) {
  object = ResolveRenames(object);
  info_for_node_.insert(std::make_pair(object, maps));
}

bool LoadElimination::AbstractMaps::Lookup(
    Node* object, ZoneHandleSet<Map>* object_maps) const {
  auto it = info_for_node_.find(ResolveRenames(object));
  if (it == info_for_node_.end()) return false;
  *object_maps = it->second;
  return true;
}

LoadElimination::AbstractMaps const* LoadElimination::AbstractMaps::Kill(
    const AliasStateInfo& alias_info, Zone* zone) const {
  for (auto pair : this->info_for_node_) {
    if (alias_info.MayAlias(pair.first)) {
      AbstractMaps* that = new (zone) AbstractMaps(zone);
      for (auto pair : this->info_for_node_) {
        if (!alias_info.MayAlias(pair.first)) that->info_for_node_.insert(pair);
      }
      return that;
    }
  }
  return this;
}

LoadElimination::AbstractMaps const* LoadElimination::AbstractMaps::Merge(
    AbstractMaps const* that, Zone* zone) const {
  if (this->Equals(that)) return this;
  AbstractMaps* copy = new (zone) AbstractMaps(zone);
  for (auto this_it : this->info_for_node_) {
    Node* this_object = this_it.first;
    ZoneHandleSet<Map> this_maps = this_it.second;
    auto that_it = that->info_for_node_.find(this_object);
    if (that_it != that->info_for_node_.end() && that_it->second == this_maps) {
      copy->info_for_node_.insert(this_it);
    }
  }
  return copy;
}

LoadElimination::AbstractMaps const* LoadElimination::AbstractMaps::Extend(
    Node* object, ZoneHandleSet<Map> maps, Zone* zone) const {
  AbstractMaps* that = new (zone) AbstractMaps(zone);
  that->info_for_node_ = this->info_for_node_;
  object = ResolveRenames(object);
  that->info_for_node_[object] = maps;
  return that;
}

void LoadElimination::AbstractMaps::Print() const {
  AllowHandleDereference allow_handle_dereference;
  StdoutStream os;
  for (auto pair : info_for_node_) {
    os << "    #" << pair.first->id() << ":" << pair.first->op()->mnemonic()
       << std::endl;
    ZoneHandleSet<Map> const& maps = pair.second;
    for (size_t i = 0; i < maps.size(); ++i) {
      os << "     - " << Brief(*maps[i]) << std::endl;
    }
  }
}

bool LoadElimination::AbstractState::Equals(AbstractState const* that) const {
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
  if (this->maps_) {
    if (!that->maps_ || !that->maps_->Equals(this->maps_)) {
      return false;
    }
  } else if (that->maps_) {
    return false;
  }
  return true;
}

void LoadElimination::AbstractState::Merge(AbstractState const* that,
                                           Zone* zone) {
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

  // Merge the information we have about the maps.
  if (this->maps_) {
    this->maps_ = that->maps_ ? that->maps_->Merge(this->maps_, zone) : nullptr;
  }
}

bool LoadElimination::AbstractState::LookupMaps(
    Node* object, ZoneHandleSet<Map>* object_map) const {
  return this->maps_ && this->maps_->Lookup(object, object_map);
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::SetMaps(
    Node* object, ZoneHandleSet<Map> maps, Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  if (that->maps_) {
    that->maps_ = that->maps_->Extend(object, maps, zone);
  } else {
    that->maps_ = new (zone) AbstractMaps(object, maps, zone);
  }
  return that;
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::KillMaps(
    const AliasStateInfo& alias_info, Zone* zone) const {
  if (this->maps_) {
    AbstractMaps const* that_maps = this->maps_->Kill(alias_info, zone);
    if (this->maps_ != that_maps) {
      AbstractState* that = new (zone) AbstractState(*this);
      that->maps_ = that_maps;
      return that;
    }
  }
  return this;
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::KillMaps(
    Node* object, Zone* zone) const {
  AliasStateInfo alias_info(this, object);
  return KillMaps(alias_info, zone);
}

Node* LoadElimination::AbstractState::LookupElement(
    Node* object, Node* index, MachineRepresentation representation) const {
  if (this->elements_) {
    return this->elements_->Lookup(object, index, representation);
  }
  return nullptr;
}

LoadElimination::AbstractState const*
LoadElimination::AbstractState::AddElement(Node* object, Node* index,
                                           Node* value,
                                           MachineRepresentation representation,
                                           Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  if (that->elements_) {
    that->elements_ =
        that->elements_->Extend(object, index, value, representation, zone);
  } else {
    that->elements_ =
        new (zone) AbstractElements(object, index, value, representation, zone);
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
    Node* object, size_t index, Node* value, MaybeHandle<Name> name,
    Zone* zone) const {
  AbstractState* that = new (zone) AbstractState(*this);
  if (that->fields_[index]) {
    that->fields_[index] =
        that->fields_[index]->Extend(object, value, name, zone);
  } else {
    that->fields_[index] = new (zone) AbstractField(object, value, name, zone);
  }
  return that;
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::KillField(
    Node* object, size_t index, MaybeHandle<Name> name, Zone* zone) const {
  AliasStateInfo alias_info(this, object);
  return KillField(alias_info, index, name, zone);
}

LoadElimination::AbstractState const* LoadElimination::AbstractState::KillField(
    const AliasStateInfo& alias_info, size_t index, MaybeHandle<Name> name,
    Zone* zone) const {
  if (AbstractField const* this_field = this->fields_[index]) {
    this_field = this_field->Kill(alias_info, name, zone);
    if (this->fields_[index] != this_field) {
      AbstractState* that = new (zone) AbstractState(*this);
      that->fields_[index] = this_field;
      return that;
    }
  }
  return this;
}

LoadElimination::AbstractState const*
LoadElimination::AbstractState::KillFields(Node* object, MaybeHandle<Name> name,
                                           Zone* zone) const {
  AliasStateInfo alias_info(this, object);
  for (size_t i = 0;; ++i) {
    if (i == arraysize(fields_)) return this;
    if (AbstractField const* this_field = this->fields_[i]) {
      AbstractField const* that_field =
          this_field->Kill(alias_info, name, zone);
      if (that_field != this_field) {
        AbstractState* that = new (zone) AbstractState(*this);
        that->fields_[i] = that_field;
        while (++i < arraysize(fields_)) {
          if (this->fields_[i] != nullptr) {
            that->fields_[i] = this->fields_[i]->Kill(alias_info, name, zone);
          }
        }
        return that;
      }
    }
  }
}

Node* LoadElimination::AbstractState::LookupField(Node* object,
                                                  size_t index) const {
  if (AbstractField const* this_field = this->fields_[index]) {
    return this_field->Lookup(object);
  }
  return nullptr;
}

bool LoadElimination::AliasStateInfo::MayAlias(Node* other) const {
  // If {object} is being initialized right here (indicated by {object} being
  // an Allocate node instead of a FinishRegion node), we know that {other}
  // can only alias with {object} if they refer to exactly the same node.
  if (object_->opcode() == IrOpcode::kAllocate) {
    return object_ == other;
  }
  // Decide aliasing based on the node kinds.
  if (!compiler::MayAlias(object_, other)) {
    return false;
  }
  // Decide aliasing based on maps (if available).
  Handle<Map> map;
  if (map_.ToHandle(&map)) {
    ZoneHandleSet<Map> other_maps;
    if (state_->LookupMaps(other, &other_maps) && other_maps.size() == 1) {
      if (map.address() != other_maps.at(0).address()) {
        return false;
      }
    }
  }
  return true;
}

void LoadElimination::AbstractState::Print() const {
  if (maps_) {
    PrintF("   maps:\n");
    maps_->Print();
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

Reduction LoadElimination::ReduceMapGuard(Node* node) {
  ZoneHandleSet<Map> const maps = MapGuardMapsOf(node->op()).maps();
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  ZoneHandleSet<Map> object_maps;
  if (state->LookupMaps(object, &object_maps)) {
    if (maps.contains(object_maps)) return Replace(effect);
    // TODO(turbofan): Compute the intersection.
  }
  state = state->SetMaps(object, maps, zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceCheckMaps(Node* node) {
  ZoneHandleSet<Map> const maps = CheckMapsParametersOf(node->op()).maps();
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  ZoneHandleSet<Map> object_maps;
  if (state->LookupMaps(object, &object_maps)) {
    if (maps.contains(object_maps)) return Replace(effect);
    // TODO(turbofan): Compute the intersection.
  }
  state = state->SetMaps(object, maps, zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceCompareMaps(Node* node) {
  ZoneHandleSet<Map> const maps = CompareMapsParametersOf(node->op()).maps();
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  ZoneHandleSet<Map> object_maps;
  if (state->LookupMaps(object, &object_maps)) {
    if (maps.contains(object_maps)) {
      Node* value = jsgraph()->TrueConstant();
      ReplaceWithValue(node, value, effect);
      return Replace(value);
    }
    // TODO(turbofan): Compute the intersection.
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceEnsureWritableFastElements(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const elements = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
    // Check if the {elements} already have the fixed array map.
  ZoneHandleSet<Map> elements_maps;
  ZoneHandleSet<Map> fixed_array_maps(factory()->fixed_array_map());
  if (state->LookupMaps(elements, &elements_maps) &&
      fixed_array_maps.contains(elements_maps)) {
    ReplaceWithValue(node, elements, effect);
    return Replace(elements);
  }
  // We know that the resulting elements have the fixed array map.
  state = state->SetMaps(node, fixed_array_maps, zone());
  // Kill the previous elements on {object}.
  state = state->KillField(object, FieldIndexOf(JSObject::kElementsOffset),
                           MaybeHandle<Name>(), zone());
  // Add the new elements on {object}.
  state = state->AddField(object, FieldIndexOf(JSObject::kElementsOffset), node,
                          MaybeHandle<Name>(), zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceMaybeGrowFastElements(Node* node) {
  GrowFastElementsParameters params = GrowFastElementsParametersOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  if (params.mode() == GrowFastElementsMode::kDoubleElements) {
    // We know that the resulting elements have the fixed double array map.
    state = state->SetMaps(
        node, ZoneHandleSet<Map>(factory()->fixed_double_array_map()), zone());
  } else {
    // We know that the resulting elements have the fixed array map or the COW
    // version thereof (if we didn't grow and it was already COW before).
    ZoneHandleSet<Map> fixed_array_maps(factory()->fixed_array_map());
    fixed_array_maps.insert(factory()->fixed_cow_array_map(), zone());
    state = state->SetMaps(node, fixed_array_maps, zone());
  }
  // Kill the previous elements on {object}.
  state = state->KillField(object, FieldIndexOf(JSObject::kElementsOffset),
                           MaybeHandle<Name>(), zone());
  // Add the new elements on {object}.
  state = state->AddField(object, FieldIndexOf(JSObject::kElementsOffset), node,
                          MaybeHandle<Name>(), zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceTransitionElementsKind(Node* node) {
  ElementsTransition transition = ElementsTransitionOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Handle<Map> source_map(transition.source());
  Handle<Map> target_map(transition.target());
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  switch (transition.mode()) {
    case ElementsTransition::kFastTransition:
      break;
    case ElementsTransition::kSlowTransition:
      // Kill the elements as well.
      AliasStateInfo alias_info(state, object, source_map);
      state =
          state->KillField(alias_info, FieldIndexOf(JSObject::kElementsOffset),
                           MaybeHandle<Name>(), zone());
      break;
  }
  ZoneHandleSet<Map> object_maps;
  if (state->LookupMaps(object, &object_maps)) {
    if (ZoneHandleSet<Map>(target_map).contains(object_maps)) {
      // The {object} already has the {target_map}, so this TransitionElements
      // {node} is fully redundant (independent of what {source_map} is).
      return Replace(effect);
    }
    if (object_maps.contains(ZoneHandleSet<Map>(source_map))) {
      object_maps.remove(source_map, zone());
      object_maps.insert(target_map, zone());
      AliasStateInfo alias_info(state, object, source_map);
      state = state->KillMaps(alias_info, zone());
      state = state->SetMaps(object, object_maps, zone());
    }
  } else {
    AliasStateInfo alias_info(state, object, source_map);
    state = state->KillMaps(alias_info, zone());
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceTransitionAndStoreElement(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Handle<Map> double_map(DoubleMapParameterOf(node->op()));
  Handle<Map> fast_map(FastMapParameterOf(node->op()));
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  // We need to add the double and fast maps to the set of possible maps for
  // this object, because we don't know which of those we'll transition to.
  // Additionally, we should kill all alias information.
  ZoneHandleSet<Map> object_maps;
  if (state->LookupMaps(object, &object_maps)) {
    object_maps.insert(double_map, zone());
    object_maps.insert(fast_map, zone());
    state = state->KillMaps(object, zone());
    state = state->SetMaps(object, object_maps, zone());
  }
  // Kill the elements as well.
  state = state->KillField(object, FieldIndexOf(JSObject::kElementsOffset),
                           MaybeHandle<Name>(), zone());
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceLoadField(Node* node) {
  FieldAccess const& access = FieldAccessOf(node->op());
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  if (access.offset == HeapObject::kMapOffset &&
      access.base_is_tagged == kTaggedBase) {
    DCHECK(IsAnyTagged(access.machine_type.representation()));
    ZoneHandleSet<Map> object_maps;
    if (state->LookupMaps(object, &object_maps) && object_maps.size() == 1) {
      Node* value = jsgraph()->HeapConstant(object_maps[0]);
      NodeProperties::SetType(value, Type::OtherInternal());
      ReplaceWithValue(node, value, effect);
      return Replace(value);
    }
  } else {
    int field_index = FieldIndexOf(access);
    if (field_index >= 0) {
      if (Node* replacement = state->LookupField(object, field_index)) {
        // Make sure we don't resurrect dead {replacement} nodes.
        if (!replacement->IsDead()) {
          // Introduce a TypeGuard if the type of the {replacement} node is not
          // a subtype of the original {node}'s type.
          if (!NodeProperties::GetType(replacement)
                   .Is(NodeProperties::GetType(node))) {
            Type replacement_type = Type::Intersect(
                NodeProperties::GetType(node),
                NodeProperties::GetType(replacement), graph()->zone());
            replacement = effect =
                graph()->NewNode(common()->TypeGuard(replacement_type),
                                 replacement, effect, control);
            NodeProperties::SetType(replacement, replacement_type);
          }
          ReplaceWithValue(node, replacement, effect);
          return Replace(replacement);
        }
      }
      state = state->AddField(object, field_index, node, access.name, zone());
    }
  }
  Handle<Map> field_map;
  if (access.map.ToHandle(&field_map)) {
    state = state->SetMaps(node, ZoneHandleSet<Map>(field_map), zone());
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
  if (access.offset == HeapObject::kMapOffset &&
      access.base_is_tagged == kTaggedBase) {
    DCHECK(IsAnyTagged(access.machine_type.representation()));
    // Kill all potential knowledge about the {object}s map.
    state = state->KillMaps(object, zone());
    Type const new_value_type = NodeProperties::GetType(new_value);
    if (new_value_type.IsHeapConstant()) {
      // Record the new {object} map information.
      AllowHandleDereference handle_dereference;
      ZoneHandleSet<Map> object_maps(
          Handle<Map>::cast(new_value_type.AsHeapConstant()->Value()));
      state = state->SetMaps(object, object_maps, zone());
    }
  } else {
    int field_index = FieldIndexOf(access);
    if (field_index >= 0) {
      Node* const old_value = state->LookupField(object, field_index);
      if (old_value == new_value) {
        // This store is fully redundant.
        return Replace(effect);
      }
      // Kill all potentially aliasing fields and record the new value.
      state = state->KillField(object, field_index, access.name, zone());
      state =
          state->AddField(object, field_index, new_value, access.name, zone());
    } else {
      // Unsupported StoreField operator.
      state = state->KillFields(object, access.name, zone());
    }
  }
  return UpdateState(node, state);
}

Reduction LoadElimination::ReduceLoadElement(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  // Only handle loads that do not require truncations.
  ElementAccess const& access = ElementAccessOf(node->op());
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
      if (Node* replacement = state->LookupElement(
              object, index, access.machine_type.representation())) {
        // Make sure we don't resurrect dead {replacement} nodes.
        // Skip lowering if the type of the {replacement} node is not a subtype
        // of the original {node}'s type.
        // TODO(tebbi): We should insert a {TypeGuard} for the intersection of
        // these two types here once we properly handle {Type::None} everywhere.
        if (!replacement->IsDead() && NodeProperties::GetType(replacement)
                                          .Is(NodeProperties::GetType(node))) {
          ReplaceWithValue(node, replacement, effect);
          return Replace(replacement);
        }
      }
      state = state->AddElement(object, index, node,
                                access.machine_type.representation(), zone());
      return UpdateState(node, state);
  }
  return NoChange();
}

Reduction LoadElimination::ReduceStoreElement(Node* node) {
  ElementAccess const& access = ElementAccessOf(node->op());
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const new_value = NodeProperties::GetValueInput(node, 2);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();
  Node* const old_value =
      state->LookupElement(object, index, access.machine_type.representation());
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
      state = state->AddElement(object, index, new_value,
                                access.machine_type.representation(), zone());
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

LoadElimination::AbstractState const* LoadElimination::UpdateStateForPhi(
    AbstractState const* state, Node* effect_phi, Node* phi) {
  int predecessor_count = phi->InputCount() - 1;
  // TODO(jarin) Consider doing a union here. At the moment, we just keep this
  // consistent with AbstractState::Merge.

  // Check if all the inputs have the same maps.
  AbstractState const* input_state =
      node_states_.Get(NodeProperties::GetEffectInput(effect_phi, 0));
  ZoneHandleSet<Map> object_maps;
  if (!input_state->LookupMaps(phi->InputAt(0), &object_maps)) return state;
  for (int i = 1; i < predecessor_count; i++) {
    input_state =
        node_states_.Get(NodeProperties::GetEffectInput(effect_phi, i));
    ZoneHandleSet<Map> input_maps;
    if (!input_state->LookupMaps(phi->InputAt(i), &input_maps)) return state;
    if (input_maps != object_maps) return state;
  }
  return state->SetMaps(phi, object_maps, zone());
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

  // For each phi, try to compute the new state for the phi from
  // the inputs.
  AbstractState const* state_with_phis = state;
  for (Node* use : control->uses()) {
    if (use->opcode() == IrOpcode::kPhi) {
      state_with_phis = UpdateStateForPhi(state_with_phis, node, use);
    }
  }

  return UpdateState(node, state_with_phis);
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
  struct TransitionElementsKindInfo {
    ElementsTransition transition;
    Node* object;
  };
  ZoneVector<TransitionElementsKindInfo> element_transitions_(zone());
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
            state = state->KillField(object,
                                     FieldIndexOf(JSObject::kElementsOffset),
                                     MaybeHandle<Name>(), zone());
            break;
          }
          case IrOpcode::kMaybeGrowFastElements: {
            Node* const object = NodeProperties::GetValueInput(current, 0);
            state = state->KillField(object,
                                     FieldIndexOf(JSObject::kElementsOffset),
                                     MaybeHandle<Name>(), zone());
            break;
          }
          case IrOpcode::kTransitionElementsKind: {
            ElementsTransition transition = ElementsTransitionOf(current->op());
            Node* const object = NodeProperties::GetValueInput(current, 0);
            ZoneHandleSet<Map> object_maps;
            if (!state->LookupMaps(object, &object_maps) ||
                !ZoneHandleSet<Map>(transition.target())
                     .contains(object_maps)) {
              element_transitions_.push_back({transition, object});
            }
            break;
          }
          case IrOpcode::kTransitionAndStoreElement: {
            Node* const object = NodeProperties::GetValueInput(current, 0);
            // Invalidate what we know about the {object}s map.
            state = state->KillMaps(object, zone());
            // Kill the elements as well.
            state = state->KillField(object,
                                     FieldIndexOf(JSObject::kElementsOffset),
                                     MaybeHandle<Name>(), zone());
            break;
          }
          case IrOpcode::kStoreField: {
            FieldAccess const& access = FieldAccessOf(current->op());
            Node* const object = NodeProperties::GetValueInput(current, 0);
            if (access.offset == HeapObject::kMapOffset) {
              // Invalidate what we know about the {object}s map.
              state = state->KillMaps(object, zone());
            } else {
              int field_index = FieldIndexOf(access);
              if (field_index < 0) {
                state = state->KillFields(object, access.name, zone());
              } else {
                state =
                    state->KillField(object, field_index, access.name, zone());
              }
            }
            break;
          }
          case IrOpcode::kStoreElement: {
            Node* const object = NodeProperties::GetValueInput(current, 0);
            Node* const index = NodeProperties::GetValueInput(current, 1);
            state = state->KillElement(object, index, zone());
            break;
          }
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

  // Finally, we apply the element transitions. For each transition, we will try
  // to only invalidate information about nodes that can have the transition's
  // source map. The trouble is that an object can be transitioned by some other
  // transition to the source map. In that case, the other transition will
  // invalidate the information, so we are mostly fine.
  //
  // The only bad case is
  //
  //    mapA   ---fast--->   mapB   ---slow--->   mapC
  //
  // If we process the slow transition first on an object that has mapA, we will
  // ignore the transition because the object does not have its source map
  // (mapB). When we later process the fast transition, we invalidate the
  // object's map, but we keep the information about the object's elements. This
  // is wrong because the elements will be overwritten by the slow transition.
  //
  // Note that the slow-slow case is fine because either of the slow transition
  // will invalidate the elements field, so the processing order does not
  // matter.
  //
  // To handle the bad case properly, we first kill the maps using all
  // transitions. We kill the the fields later when all the transitions are
  // already reflected in the map information.

  for (const TransitionElementsKindInfo& t : element_transitions_) {
    AliasStateInfo alias_info(state, t.object, t.transition.source());
    state = state->KillMaps(alias_info, zone());
  }
  for (const TransitionElementsKindInfo& t : element_transitions_) {
    switch (t.transition.mode()) {
      case ElementsTransition::kFastTransition:
        break;
      case ElementsTransition::kSlowTransition: {
        AliasStateInfo alias_info(state, t.object, t.transition.source());
        state = state->KillField(alias_info,
                                 FieldIndexOf(JSObject::kElementsOffset),
                                 MaybeHandle<Name>(), zone());
        break;
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
  DCHECK_LT(0, field_index);
  return field_index - 1;
}

// static
int LoadElimination::FieldIndexOf(FieldAccess const& access) {
  MachineRepresentation rep = access.machine_type.representation();
  switch (rep) {
    case MachineRepresentation::kNone:
    case MachineRepresentation::kBit:
    case MachineRepresentation::kSimd128:
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
      if (kDoubleSize != kPointerSize) {
        return -1;  // We currently only track pointer size fields.
      }
      break;
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      // TODO(bmeurer): Check that we never do overlapping load/stores of
      // individual parts of Float64 values.
      break;
  }
  if (access.base_is_tagged != kTaggedBase) {
    return -1;  // We currently only track tagged objects.
  }
  return FieldIndexOf(access.offset);
}

CommonOperatorBuilder* LoadElimination::common() const {
  return jsgraph()->common();
}

Graph* LoadElimination::graph() const { return jsgraph()->graph(); }

Isolate* LoadElimination::isolate() const { return jsgraph()->isolate(); }

Factory* LoadElimination::factory() const { return jsgraph()->factory(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
