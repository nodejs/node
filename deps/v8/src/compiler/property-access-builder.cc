// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/property-access-builder.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/access-info.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects/heap-number.h"
#include "src/objects/lookup.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/field-index-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

Graph* PropertyAccessBuilder::graph() const { return jsgraph()->graph(); }

Isolate* PropertyAccessBuilder::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* PropertyAccessBuilder::common() const {
  return jsgraph()->common();
}

SimplifiedOperatorBuilder* PropertyAccessBuilder::simplified() const {
  return jsgraph()->simplified();
}

bool HasOnlyStringMaps(JSHeapBroker* broker,
                       ZoneVector<Handle<Map>> const& maps) {
  for (auto map : maps) {
    MapRef map_ref(broker, map);
    if (!map_ref.IsStringMap()) return false;
  }
  return true;
}

namespace {

bool HasOnlyNumberMaps(JSHeapBroker* broker,
                       ZoneVector<Handle<Map>> const& maps) {
  for (auto map : maps) {
    MapRef map_ref(broker, map);
    if (map_ref.instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

}  // namespace

bool PropertyAccessBuilder::TryBuildStringCheck(
    JSHeapBroker* broker, ZoneVector<Handle<Map>> const& maps, Node** receiver,
    Node** effect, Node* control) {
  if (HasOnlyStringMaps(broker, maps)) {
    // Monormorphic string access (ignoring the fact that there are multiple
    // String maps).
    *receiver = *effect =
        graph()->NewNode(simplified()->CheckString(FeedbackSource()), *receiver,
                         *effect, control);
    return true;
  }
  return false;
}

bool PropertyAccessBuilder::TryBuildNumberCheck(
    JSHeapBroker* broker, ZoneVector<Handle<Map>> const& maps, Node** receiver,
    Node** effect, Node* control) {
  if (HasOnlyNumberMaps(broker, maps)) {
    // Monomorphic number access (we also deal with Smis here).
    *receiver = *effect =
        graph()->NewNode(simplified()->CheckNumber(FeedbackSource()), *receiver,
                         *effect, control);
    return true;
  }
  return false;
}

void PropertyAccessBuilder::BuildCheckMaps(
    Node* receiver, Node** effect, Node* control,
    ZoneVector<Handle<Map>> const& receiver_maps) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    MapRef receiver_map = m.Ref(broker()).map();
    if (receiver_map.is_stable()) {
      for (Handle<Map> map : receiver_maps) {
        if (MapRef(broker(), map).equals(receiver_map)) {
          dependencies()->DependOnStableMap(receiver_map);
          return;
        }
      }
    }
  }
  ZoneHandleSet<Map> maps;
  CheckMapsFlags flags = CheckMapsFlag::kNone;
  for (Handle<Map> map : receiver_maps) {
    MapRef receiver_map(broker(), map);
    maps.insert(receiver_map.object(), graph()->zone());
    if (receiver_map.is_migration_target()) {
      flags |= CheckMapsFlag::kTryMigrateInstance;
    }
  }
  *effect = graph()->NewNode(simplified()->CheckMaps(flags, maps), receiver,
                             *effect, control);
}

Node* PropertyAccessBuilder::BuildCheckValue(Node* receiver, Node** effect,
                                             Node* control,
                                             Handle<HeapObject> value) {
  HeapObjectMatcher m(receiver);
  if (m.Is(value)) return receiver;
  Node* expected = jsgraph()->HeapConstant(value);
  Node* check =
      graph()->NewNode(simplified()->ReferenceEqual(), receiver, expected);
  *effect =
      graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kWrongValue),
                       check, *effect, control);
  return expected;
}

Node* PropertyAccessBuilder::ResolveHolder(
    PropertyAccessInfo const& access_info, Node* receiver) {
  Handle<JSObject> holder;
  if (access_info.holder().ToHandle(&holder)) {
    return jsgraph()->Constant(ObjectRef(broker(), holder));
  }
  return receiver;
}

MachineRepresentation PropertyAccessBuilder::ConvertRepresentation(
    Representation representation) {
  switch (representation.kind()) {
    case Representation::kSmi:
      return MachineType::RepCompressedTaggedSigned();
    case Representation::kDouble:
      return MachineRepresentation::kFloat64;
    case Representation::kHeapObject:
      return MachineType::RepCompressedTaggedPointer();
    case Representation::kTagged:
      return MachineType::RepCompressedTagged();
    default:
      UNREACHABLE();
  }
}

Node* PropertyAccessBuilder::TryBuildLoadConstantDataField(
    NameRef const& name, PropertyAccessInfo const& access_info,
    Node* receiver) {
  if (!access_info.IsDataConstant()) return nullptr;

  // First, determine if we have a constant holder to load from.
  Handle<JSObject> holder;
  // If {access_info} has a holder, just use it.
  if (!access_info.holder().ToHandle(&holder)) {
    // Otherwise, try to match the {receiver} as a constant.
    HeapObjectMatcher m(receiver);
    if (!m.HasValue() || !m.Ref(broker()).IsJSObject()) return nullptr;

    // Let us make sure the actual map of the constant receiver is among
    // the maps in {access_info}.
    MapRef receiver_map = m.Ref(broker()).map();
    if (std::find_if(access_info.receiver_maps().begin(),
                     access_info.receiver_maps().end(), [&](Handle<Map> map) {
                       return MapRef(broker(), map).equals(receiver_map);
                     }) == access_info.receiver_maps().end()) {
      // The map of the receiver is not in the feedback, let us bail out.
      return nullptr;
    }
    holder = m.Ref(broker()).AsJSObject().object();
  }

  JSObjectRef holder_ref(broker(), holder);
  base::Optional<ObjectRef> value = holder_ref.GetOwnDataProperty(
      access_info.field_representation(), access_info.field_index());
  if (!value.has_value()) {
    return nullptr;
  }
  return jsgraph()->Constant(*value);
}

Node* PropertyAccessBuilder::BuildLoadDataField(
    NameRef const& name, PropertyAccessInfo const& access_info, Node* receiver,
    Node** effect, Node** control) {
  DCHECK(access_info.IsDataField() || access_info.IsDataConstant());
  if (Node* value =
          TryBuildLoadConstantDataField(name, access_info, receiver)) {
    return value;
  }

  FieldIndex const field_index = access_info.field_index();
  Type const field_type = access_info.field_type();
  MachineRepresentation const field_representation =
      ConvertRepresentation(access_info.field_representation());
  Node* storage = ResolveHolder(access_info, receiver);
  if (!field_index.is_inobject()) {
    storage = *effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForJSObjectPropertiesOrHashKnownPointer()),
        storage, *effect, *control);
  }
  FieldAccess field_access = {
      kTaggedBase,
      field_index.offset(),
      name.object(),
      MaybeHandle<Map>(),
      field_type,
      MachineType::TypeForRepresentation(field_representation),
      kFullWriteBarrier,
      LoadSensitivity::kCritical,
      access_info.GetConstFieldInfo()};
  if (field_representation == MachineRepresentation::kFloat64) {
    if (!field_index.is_inobject() || !FLAG_unbox_double_fields) {
      FieldAccess const storage_access = {
          kTaggedBase,
          field_index.offset(),
          name.object(),
          MaybeHandle<Map>(),
          Type::OtherInternal(),
          MachineType::TypeCompressedTaggedPointer(),
          kPointerWriteBarrier,
          LoadSensitivity::kCritical,
          access_info.GetConstFieldInfo()};
      storage = *effect = graph()->NewNode(
          simplified()->LoadField(storage_access), storage, *effect, *control);
      field_access.offset = HeapNumber::kValueOffset;
      field_access.name = MaybeHandle<Name>();
    }
  } else if (field_representation == MachineRepresentation::kTaggedPointer ||
             field_representation ==
                 MachineRepresentation::kCompressedPointer) {
    // Remember the map of the field value, if its map is stable. This is
    // used by the LoadElimination to eliminate map checks on the result.
    Handle<Map> field_map;
    if (access_info.field_map().ToHandle(&field_map)) {
      MapRef field_map_ref(broker(), field_map);
      if (field_map_ref.is_stable()) {
        dependencies()->DependOnStableMap(field_map_ref);
        field_access.map = field_map;
      }
    }
  }
  Node* value = *effect = graph()->NewNode(
      simplified()->LoadField(field_access), storage, *effect, *control);
  return value;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
