// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/map-inference.h"

#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/vector-slot-pair.h"
#include "src/objects/map-inl.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {
namespace compiler {

MapInference::MapInference(JSHeapBroker* broker, Node* object, Node* effect)
    : broker_(broker), object_(object) {
  ZoneHandleSet<Map> maps;
  auto result =
      NodeProperties::InferReceiverMaps(broker_, object_, effect, &maps);
  maps_.insert(maps_.end(), maps.begin(), maps.end());
  maps_state_ = (result == NodeProperties::kUnreliableReceiverMaps)
                    ? kUnreliableDontNeedGuard
                    : kReliableOrGuarded;
  DCHECK_EQ(maps_.empty(), result == NodeProperties::kNoReceiverMaps);
}

MapInference::~MapInference() { CHECK(Safe()); }

bool MapInference::Safe() const { return maps_state_ != kUnreliableNeedGuard; }

void MapInference::SetNeedGuardIfUnreliable() {
  CHECK(HaveMaps());
  if (maps_state_ == kUnreliableDontNeedGuard) {
    maps_state_ = kUnreliableNeedGuard;
  }
}

void MapInference::SetGuarded() { maps_state_ = kReliableOrGuarded; }

bool MapInference::HaveMaps() const { return !maps_.empty(); }

bool MapInference::AllOfInstanceTypesAreJSReceiver() const {
  return AllOfInstanceTypesUnsafe(InstanceTypeChecker::IsJSReceiver);
}

bool MapInference::AllOfInstanceTypesAre(InstanceType type) const {
  CHECK(!InstanceTypeChecker::IsString(type));
  return AllOfInstanceTypesUnsafe(
      [type](InstanceType other) { return type == other; });
}

bool MapInference::AnyOfInstanceTypesAre(InstanceType type) const {
  CHECK(!InstanceTypeChecker::IsString(type));
  return AnyOfInstanceTypesUnsafe(
      [type](InstanceType other) { return type == other; });
}

bool MapInference::AllOfInstanceTypes(std::function<bool(InstanceType)> f) {
  SetNeedGuardIfUnreliable();
  return AllOfInstanceTypesUnsafe(f);
}

bool MapInference::AllOfInstanceTypesUnsafe(
    std::function<bool(InstanceType)> f) const {
  // TODO(neis): Brokerize the MapInference.
  AllowHandleDereference allow_handle_deref;
  CHECK(HaveMaps());

  return std::all_of(maps_.begin(), maps_.end(),
                     [f](Handle<Map> map) { return f(map->instance_type()); });
}

bool MapInference::AnyOfInstanceTypesUnsafe(
    std::function<bool(InstanceType)> f) const {
  AllowHandleDereference allow_handle_deref;
  CHECK(HaveMaps());

  return std::any_of(maps_.begin(), maps_.end(),
                     [f](Handle<Map> map) { return f(map->instance_type()); });
}

MapHandles const& MapInference::GetMaps() {
  SetNeedGuardIfUnreliable();
  return maps_;
}

void MapInference::InsertMapChecks(JSGraph* jsgraph, Node** effect,
                                   Node* control,
                                   const VectorSlotPair& feedback) {
  CHECK(HaveMaps());
  CHECK(feedback.IsValid());
  ZoneHandleSet<Map> maps;
  for (Handle<Map> map : maps_) maps.insert(map, jsgraph->graph()->zone());
  *effect = jsgraph->graph()->NewNode(
      jsgraph->simplified()->CheckMaps(CheckMapsFlag::kNone, maps, feedback),
      object_, *effect, control);
  SetGuarded();
}

bool MapInference::RelyOnMapsViaStability(
    CompilationDependencies* dependencies) {
  CHECK(HaveMaps());
  return RelyOnMapsHelper(dependencies, nullptr, nullptr, nullptr, {});
}

bool MapInference::RelyOnMapsPreferStability(
    CompilationDependencies* dependencies, JSGraph* jsgraph, Node** effect,
    Node* control, const VectorSlotPair& feedback) {
  CHECK(HaveMaps());
  if (Safe()) return false;
  if (RelyOnMapsViaStability(dependencies)) return true;
  CHECK(RelyOnMapsHelper(nullptr, jsgraph, effect, control, feedback));
  return false;
}

bool MapInference::RelyOnMapsHelper(CompilationDependencies* dependencies,
                                    JSGraph* jsgraph, Node** effect,
                                    Node* control,
                                    const VectorSlotPair& feedback) {
  if (Safe()) return true;

  auto is_stable = [](Handle<Map> map) { return map->is_stable(); };
  if (dependencies != nullptr &&
      std::all_of(maps_.cbegin(), maps_.cend(), is_stable)) {
    for (Handle<Map> map : maps_) {
      dependencies->DependOnStableMap(MapRef(broker_, map));
    }
    SetGuarded();
    return true;
  } else if (feedback.IsValid()) {
    InsertMapChecks(jsgraph, effect, control, feedback);
    return true;
  } else {
    return false;
  }
}

Reduction MapInference::NoChange() {
  SetGuarded();
  maps_.clear();  // Just to make some CHECKs fail if {this} gets used after.
  return Reducer::NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
