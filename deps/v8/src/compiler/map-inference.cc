// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/map-inference.h"

#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects/map-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

MapInference::MapInference(JSHeapBroker* broker, Node* object, Effect effect)
    : broker_(broker), object_(object) {
  auto result =
      NodeProperties::InferMapsUnsafe(broker_, object_, effect, &maps_);
  maps_state_ = (result == NodeProperties::kUnreliableMaps)
                    ? kUnreliableDontNeedGuard
                    : kReliableOrGuarded;
  DCHECK_EQ(maps_.is_empty(), result == NodeProperties::kNoMaps);
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

bool MapInference::HaveMaps() const { return !maps_.is_empty(); }

bool MapInference::AllOfInstanceTypesAreJSReceiver() const {
  return AllOfInstanceTypesUnsafe(
      static_cast<bool (*)(InstanceType)>(&InstanceTypeChecker::IsJSReceiver));
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
  CHECK(HaveMaps());

  auto instance_type = [f](MapRef map) { return f(map.instance_type()); };
  return std::all_of(maps_.begin(), maps_.end(), instance_type);
}

bool MapInference::AnyOfInstanceTypesUnsafe(
    std::function<bool(InstanceType)> f) const {
  CHECK(HaveMaps());

  auto instance_type = [f](MapRef map) { return f(map.instance_type()); };

  return std::any_of(maps_.begin(), maps_.end(), instance_type);
}

ZoneRefSet<Map> const& MapInference::GetMaps() {
  SetNeedGuardIfUnreliable();
  return maps_;
}

bool MapInference::Is(MapRef expected_map) {
  if (!HaveMaps()) return false;
  if (maps_.size() != 1) return false;
  return maps_.at(0).equals(expected_map);
}

void MapInference::InsertMapChecks(JSGraph* jsgraph, Effect* effect,
                                   Control control,
                                   const FeedbackSource& feedback) {
  CHECK(HaveMaps());
  CHECK(feedback.IsValid());
  *effect = jsgraph->graph()->NewNode(
      jsgraph->simplified()->CheckMaps(CheckMapsFlag::kNone, maps_, feedback),
      object_, *effect, control);
  SetGuarded();
}

bool MapInference::RelyOnMapsViaStability(
    CompilationDependencies* dependencies) {
  CHECK(HaveMaps());
  return RelyOnMapsHelper(dependencies, nullptr, nullptr, Control{nullptr}, {});
}

bool MapInference::RelyOnMapsPreferStability(
    CompilationDependencies* dependencies, JSGraph* jsgraph, Effect* effect,
    Control control, const FeedbackSource& feedback) {
  CHECK(HaveMaps());
  if (Safe()) return false;
  if (RelyOnMapsViaStability(dependencies)) return true;
  CHECK(RelyOnMapsHelper(nullptr, jsgraph, effect, control, feedback));
  return false;
}

bool MapInference::RelyOnMapsHelper(CompilationDependencies* dependencies,
                                    JSGraph* jsgraph, Effect* effect,
                                    Control control,
                                    const FeedbackSource& feedback) {
  if (Safe()) return true;

  auto is_stable = [](MapRef map) { return map.is_stable(); };
  if (dependencies != nullptr &&
      std::all_of(maps_.begin(), maps_.end(), is_stable)) {
    for (MapRef map : maps_) {
      dependencies->DependOnStableMap(map);
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
