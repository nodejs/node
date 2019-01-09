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
#include "src/lookup.h"
#include "src/objects/heap-number.h"

#include "src/field-index-inl.h"
#include "src/isolate-inl.h"

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

bool HasOnlyStringMaps(MapHandles const& maps) {
  for (auto map : maps) {
    if (!map->IsStringMap()) return false;
  }
  return true;
}

namespace {

bool HasOnlyNumberMaps(MapHandles const& maps) {
  for (auto map : maps) {
    if (map->instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

}  // namespace

bool PropertyAccessBuilder::TryBuildStringCheck(MapHandles const& maps,
                                                Node** receiver, Node** effect,
                                                Node* control) {
  if (HasOnlyStringMaps(maps)) {
    // Monormorphic string access (ignoring the fact that there are multiple
    // String maps).
    *receiver = *effect =
        graph()->NewNode(simplified()->CheckString(VectorSlotPair()), *receiver,
                         *effect, control);
    return true;
  }
  return false;
}

bool PropertyAccessBuilder::TryBuildNumberCheck(MapHandles const& maps,
                                                Node** receiver, Node** effect,
                                                Node* control) {
  if (HasOnlyNumberMaps(maps)) {
    // Monomorphic number access (we also deal with Smis here).
    *receiver = *effect =
        graph()->NewNode(simplified()->CheckNumber(VectorSlotPair()), *receiver,
                         *effect, control);
    return true;
  }
  return false;
}

namespace {

bool NeedsCheckHeapObject(Node* receiver) {
  switch (receiver->opcode()) {
    case IrOpcode::kConvertReceiver:
    case IrOpcode::kHeapConstant:
    case IrOpcode::kJSCloneObject:
    case IrOpcode::kJSConstruct:
    case IrOpcode::kJSConstructForwardVarargs:
    case IrOpcode::kJSConstructWithArrayLike:
    case IrOpcode::kJSConstructWithSpread:
    case IrOpcode::kJSCreate:
    case IrOpcode::kJSCreateArguments:
    case IrOpcode::kJSCreateArray:
    case IrOpcode::kJSCreateArrayFromIterable:
    case IrOpcode::kJSCreateArrayIterator:
    case IrOpcode::kJSCreateAsyncFunctionObject:
    case IrOpcode::kJSCreateBoundFunction:
    case IrOpcode::kJSCreateClosure:
    case IrOpcode::kJSCreateCollectionIterator:
    case IrOpcode::kJSCreateEmptyLiteralArray:
    case IrOpcode::kJSCreateEmptyLiteralObject:
    case IrOpcode::kJSCreateGeneratorObject:
    case IrOpcode::kJSCreateIterResultObject:
    case IrOpcode::kJSCreateKeyValueArray:
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:
    case IrOpcode::kJSCreateLiteralRegExp:
    case IrOpcode::kJSCreateObject:
    case IrOpcode::kJSCreatePromise:
    case IrOpcode::kJSCreateStringIterator:
    case IrOpcode::kJSCreateTypedArray:
    case IrOpcode::kJSGetSuperConstructor:
    case IrOpcode::kJSToName:
    case IrOpcode::kJSToObject:
    case IrOpcode::kJSToString:
    case IrOpcode::kTypeOf:
      return false;
    case IrOpcode::kPhi: {
      Node* control = NodeProperties::GetControlInput(receiver);
      if (control->opcode() != IrOpcode::kMerge) return true;
      for (int i = 0; i < receiver->InputCount() - 1; ++i) {
        if (NeedsCheckHeapObject(receiver->InputAt(i))) return true;
      }
      return false;
    }
    default:
      return true;
  }
}

}  // namespace

Node* PropertyAccessBuilder::BuildCheckHeapObject(Node* receiver, Node** effect,
                                                  Node* control) {
  if (NeedsCheckHeapObject(receiver)) {
    receiver = *effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                          receiver, *effect, control);
  }
  return receiver;
}

void PropertyAccessBuilder::BuildCheckMaps(
    Node* receiver, Node** effect, Node* control,
    std::vector<Handle<Map>> const& receiver_maps) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    Handle<Map> receiver_map(m.Value()->map(), isolate());
    if (receiver_map->is_stable()) {
      for (Handle<Map> map : receiver_maps) {
        if (map.is_identical_to(receiver_map)) {
          dependencies()->DependOnStableMap(MapRef(broker(), receiver_map));
          return;
        }
      }
    }
  }
  ZoneHandleSet<Map> maps;
  CheckMapsFlags flags = CheckMapsFlag::kNone;
  for (Handle<Map> map : receiver_maps) {
    maps.insert(map, graph()->zone());
    if (map->is_migration_target()) {
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
    return jsgraph()->Constant(holder);
  }
  return receiver;
}

Node* PropertyAccessBuilder::TryBuildLoadConstantDataField(
    Handle<Name> name, PropertyAccessInfo const& access_info, Node* receiver) {
  // Optimize immutable property loads.
  HeapObjectMatcher m(receiver);
  if (m.HasValue() && m.Value()->IsJSObject()) {
    // TODO(ishell): Use something simpler like
    //
    // Handle<Object> value =
    //     JSObject::FastPropertyAt(Handle<JSObject>::cast(m.Value()),
    //                              Representation::Tagged(), field_index);
    //
    // here, once we have the immutable bit in the access_info.

    // TODO(turbofan): Given that we already have the field_index here, we
    // might be smarter in the future and not rely on the LookupIterator.
    LookupIterator it(isolate(), m.Value(), name,
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    if (it.state() == LookupIterator::DATA) {
      bool is_readonly_non_configurable =
          it.IsReadOnly() && !it.IsConfigurable();
      if (is_readonly_non_configurable ||
          (FLAG_track_constant_fields && access_info.IsDataConstantField())) {
        Node* value = jsgraph()->Constant(JSReceiver::GetDataProperty(&it));
        if (!is_readonly_non_configurable) {
          // It's necessary to add dependency on the map that introduced
          // the field.
          DCHECK(access_info.IsDataConstantField());
          DCHECK(!it.is_dictionary_holder());
          MapRef map(broker(),
                     handle(it.GetHolder<HeapObject>()->map(), isolate()));
          map.SerializeOwnDescriptors();  // TODO(neis): Remove later.
          dependencies()->DependOnFieldType(map, it.GetFieldDescriptorIndex());
        }
        return value;
      }
    }
  }
  return nullptr;
}

Node* PropertyAccessBuilder::BuildLoadDataField(
    Handle<Name> name, PropertyAccessInfo const& access_info, Node* receiver,
    Node** effect, Node** control) {
  DCHECK(access_info.IsDataField() || access_info.IsDataConstantField());
  receiver = ResolveHolder(access_info, receiver);
  if (Node* value =
          TryBuildLoadConstantDataField(name, access_info, receiver)) {
    return value;
  }

  FieldIndex const field_index = access_info.field_index();
  Type const field_type = access_info.field_type();
  MachineRepresentation const field_representation =
      access_info.field_representation();
  Node* storage = receiver;
  if (!field_index.is_inobject()) {
    storage = *effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectPropertiesOrHash()),
        storage, *effect, *control);
  }
  FieldAccess field_access = {
      kTaggedBase,
      field_index.offset(),
      name,
      MaybeHandle<Map>(),
      field_type,
      MachineType::TypeForRepresentation(field_representation),
      kFullWriteBarrier,
      LoadSensitivity::kCritical};
  if (field_representation == MachineRepresentation::kFloat64) {
    if (!field_index.is_inobject() || field_index.is_hidden_field() ||
        !FLAG_unbox_double_fields) {
      FieldAccess const storage_access = {kTaggedBase,
                                          field_index.offset(),
                                          name,
                                          MaybeHandle<Map>(),
                                          Type::OtherInternal(),
                                          MachineType::TaggedPointer(),
                                          kPointerWriteBarrier,
                                          LoadSensitivity::kCritical};
      storage = *effect = graph()->NewNode(
          simplified()->LoadField(storage_access), storage, *effect, *control);
      field_access.offset = HeapNumber::kValueOffset;
      field_access.name = MaybeHandle<Name>();
    }
  } else if (field_representation == MachineRepresentation::kTaggedPointer) {
    // Remember the map of the field value, if its map is stable. This is
    // used by the LoadElimination to eliminate map checks on the result.
    Handle<Map> field_map;
    if (access_info.field_map().ToHandle(&field_map)) {
      if (field_map->is_stable()) {
        dependencies()->DependOnStableMap(MapRef(broker(), field_map));
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
