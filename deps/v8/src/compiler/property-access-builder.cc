// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/property-access-builder.h"

#include "src/base/optional.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/access-info.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/simplified-operator.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/heap-number.h"
#include "src/objects/internal-index.h"
#include "src/objects/lookup.h"
#include "src/objects/property-details.h"

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

bool HasOnlyStringMaps(JSHeapBroker* broker, ZoneVector<MapRef> const& maps) {
  for (MapRef map : maps) {
    if (!map.IsStringMap()) return false;
  }
  return true;
}

namespace {

bool HasOnlyNumberMaps(JSHeapBroker* broker, ZoneVector<MapRef> const& maps) {
  for (MapRef map : maps) {
    if (map.instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

}  // namespace

bool PropertyAccessBuilder::TryBuildStringCheck(JSHeapBroker* broker,
                                                ZoneVector<MapRef> const& maps,
                                                Node** receiver, Effect* effect,
                                                Control control) {
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

bool PropertyAccessBuilder::TryBuildNumberCheck(JSHeapBroker* broker,
                                                ZoneVector<MapRef> const& maps,
                                                Node** receiver, Effect* effect,
                                                Control control) {
  if (HasOnlyNumberMaps(broker, maps)) {
    // Monomorphic number access (we also deal with Smis here).
    *receiver = *effect =
        graph()->NewNode(simplified()->CheckNumber(FeedbackSource()), *receiver,
                         *effect, control);
    return true;
  }
  return false;
}

void PropertyAccessBuilder::BuildCheckMaps(Node* object, Effect* effect,
                                           Control control,
                                           ZoneVector<MapRef> const& maps) {
  HeapObjectMatcher m(object);
  if (m.HasResolvedValue()) {
    MapRef object_map = m.Ref(broker()).map();
    if (object_map.is_stable()) {
      for (MapRef map : maps) {
        if (map.equals(object_map)) {
          dependencies()->DependOnStableMap(object_map);
          return;
        }
      }
    }
  }
  ZoneHandleSet<Map> map_set;
  CheckMapsFlags flags = CheckMapsFlag::kNone;
  for (MapRef map : maps) {
    map_set.insert(map.object(), graph()->zone());
    if (map.is_migration_target()) {
      flags |= CheckMapsFlag::kTryMigrateInstance;
    }
  }
  *effect = graph()->NewNode(simplified()->CheckMaps(flags, map_set), object,
                             *effect, control);
}

Node* PropertyAccessBuilder::BuildCheckValue(Node* receiver, Effect* effect,
                                             Control control,
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
    PropertyAccessInfo const& access_info, Node* lookup_start_object) {
  base::Optional<JSObjectRef> holder = access_info.holder();
  if (holder.has_value()) {
    return jsgraph()->Constant(holder.value());
  }
  return lookup_start_object;
}

MachineRepresentation PropertyAccessBuilder::ConvertRepresentation(
    Representation representation) {
  switch (representation.kind()) {
    case Representation::kSmi:
      return MachineRepresentation::kTaggedSigned;
    case Representation::kDouble:
      return MachineRepresentation::kFloat64;
    case Representation::kHeapObject:
      return MachineRepresentation::kTaggedPointer;
    case Representation::kTagged:
      return MachineRepresentation::kTagged;
    default:
      UNREACHABLE();
  }
}

base::Optional<Node*> PropertyAccessBuilder::FoldLoadDictPrototypeConstant(
    PropertyAccessInfo const& access_info) {
  DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
  DCHECK(access_info.IsDictionaryProtoDataConstant());

  InternalIndex index = access_info.dictionary_index();
  base::Optional<ObjectRef> value =
      access_info.holder()->GetOwnDictionaryProperty(index, dependencies());
  if (!value) return {};

  for (MapRef map : access_info.lookup_start_object_maps()) {
    Handle<Map> map_handle = map.object();
    // Non-JSReceivers that passed AccessInfoFactory::ComputePropertyAccessInfo
    // must have different lookup start map.
    if (!map_handle->IsJSReceiverMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      JSFunction constructor =
          Map::GetConstructorFunction(
              *map_handle, *broker()->target_native_context().object())
              .value();
      map = MakeRef(broker(), constructor.initial_map());
      DCHECK(map.object()->IsJSObjectMap());
    }
    dependencies()->DependOnConstantInDictionaryPrototypeChain(
        map, access_info.name(), value.value(), PropertyKind::kData);
  }

  return jsgraph()->Constant(value.value());
}

Node* PropertyAccessBuilder::TryFoldLoadConstantDataField(
    NameRef const& name, PropertyAccessInfo const& access_info,
    Node* lookup_start_object) {
  if (!access_info.IsFastDataConstant()) return nullptr;

  // First, determine if we have a constant holder to load from.
  base::Optional<JSObjectRef> holder = access_info.holder();

  // If {access_info} has a holder, just use it.
  if (!holder.has_value()) {
    // Otherwise, try to match the {lookup_start_object} as a constant.
    HeapObjectMatcher m(lookup_start_object);
    if (!m.HasResolvedValue() || !m.Ref(broker()).IsJSObject()) return nullptr;

    // Let us make sure the actual map of the constant lookup_start_object is
    // among the maps in {access_info}.
    MapRef lookup_start_object_map = m.Ref(broker()).map();
    if (std::find_if(access_info.lookup_start_object_maps().begin(),
                     access_info.lookup_start_object_maps().end(),
                     [&](MapRef map) {
                       return map.equals(lookup_start_object_map);
                     }) == access_info.lookup_start_object_maps().end()) {
      // The map of the lookup_start_object is not in the feedback, let us bail
      // out.
      return nullptr;
    }
    holder = m.Ref(broker()).AsJSObject();
  }

  base::Optional<ObjectRef> value =
      holder->GetOwnFastDataProperty(access_info.field_representation(),
                                     access_info.field_index(), dependencies());
  return value.has_value() ? jsgraph()->Constant(*value) : nullptr;
}

Node* PropertyAccessBuilder::BuildLoadDataField(NameRef const& name,
                                                Node* holder,
                                                FieldAccess& field_access,
                                                bool is_inobject, Node** effect,
                                                Node** control) {
  Node* storage = holder;
  if (!is_inobject) {
    storage = *effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForJSObjectPropertiesOrHashKnownPointer()),
        storage, *effect, *control);
  }
  if (field_access.machine_type.representation() ==
      MachineRepresentation::kFloat64) {
    if (dependencies() == nullptr) {
      FieldAccess const storage_access = {kTaggedBase,
                                          field_access.offset,
                                          name.object(),
                                          MaybeHandle<Map>(),
                                          Type::Any(),
                                          MachineType::AnyTagged(),
                                          kPointerWriteBarrier,
                                          LoadSensitivity::kCritical,
                                          field_access.const_field_info};
      storage = *effect = graph()->NewNode(
          simplified()->LoadField(storage_access), storage, *effect, *control);
      // We expect the loaded value to be a heap number here. With
      // in-place field representation changes it is possible this is a
      // no longer a heap number without map transitions. If we haven't taken
      // a dependency on field representation, we should verify the loaded
      // value is a heap number.
      storage = *effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                           storage, *effect, *control);
      Node* map = *effect =
          graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                           storage, *effect, *control);
      Node* is_heap_number =
          graph()->NewNode(simplified()->ReferenceEqual(), map,
                           jsgraph()->HeapNumberMapConstant());
      *effect = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kNotAHeapNumber),
          is_heap_number, *effect, *control);
    } else {
      FieldAccess const storage_access = {kTaggedBase,
                                          field_access.offset,
                                          name.object(),
                                          MaybeHandle<Map>(),
                                          Type::OtherInternal(),
                                          MachineType::TaggedPointer(),
                                          kPointerWriteBarrier,
                                          LoadSensitivity::kCritical,
                                          field_access.const_field_info};
      storage = *effect = graph()->NewNode(
          simplified()->LoadField(storage_access), storage, *effect, *control);
    }
    field_access.offset = HeapNumber::kValueOffset;
    field_access.name = MaybeHandle<Name>();
  }
  Node* value = *effect = graph()->NewNode(
      simplified()->LoadField(field_access), storage, *effect, *control);
  return value;
}

Node* PropertyAccessBuilder::BuildMinimorphicLoadDataField(
    NameRef const& name, MinimorphicLoadPropertyAccessInfo const& access_info,
    Node* lookup_start_object, Node** effect, Node** control) {
  DCHECK_NULL(dependencies());
  MachineRepresentation const field_representation =
      ConvertRepresentation(access_info.field_representation());

  FieldAccess field_access = {
      kTaggedBase,
      access_info.offset(),
      name.object(),
      MaybeHandle<Map>(),
      access_info.field_type(),
      MachineType::TypeForRepresentation(field_representation),
      kFullWriteBarrier,
      LoadSensitivity::kCritical,
      ConstFieldInfo::None()};
  return BuildLoadDataField(name, lookup_start_object, field_access,
                            access_info.is_inobject(), effect, control);
}

Node* PropertyAccessBuilder::BuildLoadDataField(
    NameRef const& name, PropertyAccessInfo const& access_info,
    Node* lookup_start_object, Node** effect, Node** control) {
  DCHECK(access_info.IsDataField() || access_info.IsFastDataConstant());

  if (Node* value = TryFoldLoadConstantDataField(name, access_info,
                                                 lookup_start_object)) {
    return value;
  }

  MachineRepresentation const field_representation =
      ConvertRepresentation(access_info.field_representation());
  Node* storage = ResolveHolder(access_info, lookup_start_object);

  FieldAccess field_access = {
      kTaggedBase,
      access_info.field_index().offset(),
      name.object(),
      MaybeHandle<Map>(),
      access_info.field_type(),
      MachineType::TypeForRepresentation(field_representation),
      kFullWriteBarrier,
      LoadSensitivity::kCritical,
      access_info.GetConstFieldInfo()};
  if (field_representation == MachineRepresentation::kTaggedPointer ||
      field_representation == MachineRepresentation::kCompressedPointer) {
    // Remember the map of the field value, if its map is stable. This is
    // used by the LoadElimination to eliminate map checks on the result.
    base::Optional<MapRef> field_map = access_info.field_map();
    if (field_map.has_value()) {
      if (field_map->is_stable()) {
        dependencies()->DependOnStableMap(field_map.value());
        field_access.map = field_map->object();
      }
    }
  }
  return BuildLoadDataField(name, storage, field_access,
                            access_info.field_index().is_inobject(), effect,
                            control);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
