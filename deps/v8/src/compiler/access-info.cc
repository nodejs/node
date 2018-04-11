// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "src/accessors.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-info.h"
#include "src/compiler/type-cache.h"
#include "src/field-index-inl.h"
#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool CanInlineElementAccess(Handle<Map> map) {
  if (!map->IsJSObjectMap()) return false;
  if (map->is_access_check_needed()) return false;
  if (map->has_indexed_interceptor()) return false;
  ElementsKind const elements_kind = map->elements_kind();
  if (IsFastElementsKind(elements_kind)) return true;
  if (IsFixedTypedArrayElementsKind(elements_kind) &&
      elements_kind != BIGUINT64_ELEMENTS &&
      elements_kind != BIGINT64_ELEMENTS) {
    return true;
  }
  return false;
}


bool CanInlinePropertyAccess(Handle<Map> map) {
  // We can inline property access to prototypes of all primitives, except
  // the special Oddball ones that have no wrapper counterparts (i.e. Null,
  // Undefined and TheHole).
  STATIC_ASSERT(ODDBALL_TYPE == LAST_PRIMITIVE_TYPE);
  if (map->IsBooleanMap()) return true;
  if (map->instance_type() < LAST_PRIMITIVE_TYPE) return true;
  return map->IsJSObjectMap() && !map->is_dictionary_map() &&
         !map->has_named_interceptor() &&
         // TODO(verwaest): Whitelist contexts to which we have access.
         !map->is_access_check_needed();
}

}  // namespace


std::ostream& operator<<(std::ostream& os, AccessMode access_mode) {
  switch (access_mode) {
    case AccessMode::kLoad:
      return os << "Load";
    case AccessMode::kStore:
      return os << "Store";
    case AccessMode::kStoreInLiteral:
      return os << "StoreInLiteral";
  }
  UNREACHABLE();
}

ElementAccessInfo::ElementAccessInfo() {}

ElementAccessInfo::ElementAccessInfo(MapHandles const& receiver_maps,
                                     ElementsKind elements_kind)
    : elements_kind_(elements_kind), receiver_maps_(receiver_maps) {}

// static
PropertyAccessInfo PropertyAccessInfo::NotFound(MapHandles const& receiver_maps,
                                                MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(holder, receiver_maps);
}

// static
PropertyAccessInfo PropertyAccessInfo::DataConstant(
    MapHandles const& receiver_maps, Handle<Object> constant,
    MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(kDataConstant, holder, constant, receiver_maps);
}

// static
PropertyAccessInfo PropertyAccessInfo::DataField(
    PropertyConstness constness, MapHandles const& receiver_maps,
    FieldIndex field_index, MachineRepresentation field_representation,
    Type* field_type, MaybeHandle<Map> field_map, MaybeHandle<JSObject> holder,
    MaybeHandle<Map> transition_map) {
  Kind kind = constness == kConst ? kDataConstantField : kDataField;
  return PropertyAccessInfo(kind, holder, transition_map, field_index,
                            field_representation, field_type, field_map,
                            receiver_maps);
}

// static
PropertyAccessInfo PropertyAccessInfo::AccessorConstant(
    MapHandles const& receiver_maps, Handle<Object> constant,
    MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(kAccessorConstant, holder, constant, receiver_maps);
}

// static
PropertyAccessInfo PropertyAccessInfo::ModuleExport(
    MapHandles const& receiver_maps, Handle<Cell> cell) {
  return PropertyAccessInfo(kModuleExport, MaybeHandle<JSObject>(), cell,
                            receiver_maps);
}

PropertyAccessInfo::PropertyAccessInfo()
    : kind_(kInvalid),
      field_representation_(MachineRepresentation::kNone),
      field_type_(Type::None()) {}

PropertyAccessInfo::PropertyAccessInfo(MaybeHandle<JSObject> holder,
                                       MapHandles const& receiver_maps)
    : kind_(kNotFound),
      receiver_maps_(receiver_maps),
      holder_(holder),
      field_representation_(MachineRepresentation::kNone),
      field_type_(Type::None()) {}

PropertyAccessInfo::PropertyAccessInfo(Kind kind, MaybeHandle<JSObject> holder,
                                       Handle<Object> constant,
                                       MapHandles const& receiver_maps)
    : kind_(kind),
      receiver_maps_(receiver_maps),
      constant_(constant),
      holder_(holder),
      field_representation_(MachineRepresentation::kNone),
      field_type_(Type::Any()) {}

PropertyAccessInfo::PropertyAccessInfo(
    Kind kind, MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map,
    FieldIndex field_index, MachineRepresentation field_representation,
    Type* field_type, MaybeHandle<Map> field_map,
    MapHandles const& receiver_maps)
    : kind_(kind),
      receiver_maps_(receiver_maps),
      transition_map_(transition_map),
      holder_(holder),
      field_index_(field_index),
      field_representation_(field_representation),
      field_type_(field_type),
      field_map_(field_map) {}

bool PropertyAccessInfo::Merge(PropertyAccessInfo const* that,
                               AccessMode access_mode, Zone* zone) {
  if (this->kind_ != that->kind_) return false;
  if (this->holder_.address() != that->holder_.address()) return false;

  switch (this->kind_) {
    case kInvalid:
      break;

    case kDataField:
    case kDataConstantField: {
      // Check if we actually access the same field (we use the
      // GetFieldAccessStubKey method here just like the ICs do
      // since that way we only compare the relevant bits of the
      // field indices).
      if (this->field_index_.GetFieldAccessStubKey() ==
          that->field_index_.GetFieldAccessStubKey()) {
        switch (access_mode) {
          case AccessMode::kLoad: {
            if (this->field_representation_ != that->field_representation_) {
              if (!IsAnyTagged(this->field_representation_) ||
                  !IsAnyTagged(that->field_representation_)) {
                return false;
              }
              this->field_representation_ = MachineRepresentation::kTagged;
            }
            if (this->field_map_.address() != that->field_map_.address()) {
              this->field_map_ = MaybeHandle<Map>();
            }
            break;
          }
          case AccessMode::kStore:
          case AccessMode::kStoreInLiteral: {
            // For stores, the field map and field representation information
            // must match exactly, otherwise we cannot merge the stores. We
            // also need to make sure that in case of transitioning stores,
            // the transition targets match.
            if (this->field_map_.address() != that->field_map_.address() ||
                this->field_representation_ != that->field_representation_ ||
                this->transition_map_.address() !=
                    that->transition_map_.address()) {
              return false;
            }
            break;
          }
        }
        // Merge the field type.
        this->field_type_ =
            Type::Union(this->field_type_, that->field_type_, zone);
        // Merge the receiver maps.
        this->receiver_maps_.insert(this->receiver_maps_.end(),
                                    that->receiver_maps_.begin(),
                                    that->receiver_maps_.end());
        return true;
      }
      return false;
    }

    case kDataConstant:
    case kAccessorConstant: {
      // Check if we actually access the same constant.
      if (this->constant_.address() == that->constant_.address()) {
        this->receiver_maps_.insert(this->receiver_maps_.end(),
                                    that->receiver_maps_.begin(),
                                    that->receiver_maps_.end());
        return true;
      }
      return false;
    }

    case kNotFound: {
      this->receiver_maps_.insert(this->receiver_maps_.end(),
                                  that->receiver_maps_.begin(),
                                  that->receiver_maps_.end());
      return true;
    }
    case kModuleExport: {
      return false;
    }
  }

  UNREACHABLE();
}

Handle<Cell> PropertyAccessInfo::export_cell() const {
  DCHECK_EQ(kModuleExport, kind_);
  return Handle<Cell>::cast(constant_);
}

AccessInfoFactory::AccessInfoFactory(CompilationDependencies* dependencies,
                                     Handle<Context> native_context, Zone* zone)
    : dependencies_(dependencies),
      native_context_(native_context),
      isolate_(native_context->GetIsolate()),
      type_cache_(TypeCache::Get()),
      zone_(zone) {
  DCHECK(native_context->IsNativeContext());
}


bool AccessInfoFactory::ComputeElementAccessInfo(
    Handle<Map> map, AccessMode access_mode, ElementAccessInfo* access_info) {
  // Check if it is safe to inline element access for the {map}.
  if (!CanInlineElementAccess(map)) return false;
  ElementsKind const elements_kind = map->elements_kind();
  *access_info = ElementAccessInfo(MapHandles{map}, elements_kind);
  return true;
}

bool AccessInfoFactory::ComputeElementAccessInfos(
    MapHandles const& maps, AccessMode access_mode,
    ZoneVector<ElementAccessInfo>* access_infos) {
  if (access_mode == AccessMode::kLoad) {
    // For polymorphic loads of similar elements kinds (i.e. all tagged or all
    // double), always use the "worst case" code without a transition.  This is
    // much faster than transitioning the elements to the worst case, trading a
    // TransitionElementsKind for a CheckMaps, avoiding mutation of the array.
    ElementAccessInfo access_info;
    if (ConsolidateElementLoad(maps, &access_info)) {
      access_infos->push_back(access_info);
      return true;
    }
  }

  // Collect possible transition targets.
  MapHandles possible_transition_targets;
  possible_transition_targets.reserve(maps.size());
  for (Handle<Map> map : maps) {
    if (Map::TryUpdate(map).ToHandle(&map)) {
      if (CanInlineElementAccess(map) &&
          IsFastElementsKind(map->elements_kind()) &&
          GetInitialFastElementsKind() != map->elements_kind()) {
        possible_transition_targets.push_back(map);
      }
    }
  }

  // Separate the actual receiver maps and the possible transition sources.
  MapHandles receiver_maps;
  receiver_maps.reserve(maps.size());
  MapTransitionList transitions(maps.size());
  for (Handle<Map> map : maps) {
    if (Map::TryUpdate(map).ToHandle(&map)) {
      // Don't generate elements kind transitions from stable maps.
      Map* transition_target = map->is_stable()
                                   ? nullptr
                                   : map->FindElementsKindTransitionedMap(
                                         possible_transition_targets);
      if (transition_target == nullptr) {
        receiver_maps.push_back(map);
      } else {
        transitions.push_back(std::make_pair(map, handle(transition_target)));
      }
    }
  }

  for (Handle<Map> receiver_map : receiver_maps) {
    // Compute the element access information.
    ElementAccessInfo access_info;
    if (!ComputeElementAccessInfo(receiver_map, access_mode, &access_info)) {
      return false;
    }

    // Collect the possible transitions for the {receiver_map}.
    for (auto transition : transitions) {
      if (transition.second.is_identical_to(receiver_map)) {
        access_info.transitions().push_back(transition);
      }
    }

    // Schedule the access information.
    access_infos->push_back(access_info);
  }
  return true;
}


bool AccessInfoFactory::ComputePropertyAccessInfo(
    Handle<Map> map, Handle<Name> name, AccessMode access_mode,
    PropertyAccessInfo* access_info) {
  // Check if it is safe to inline property access for the {map}.
  if (!CanInlinePropertyAccess(map)) return false;

  // Compute the receiver type.
  Handle<Map> receiver_map = map;

  // Property lookups require the name to be internalized.
  name = isolate()->factory()->InternalizeName(name);

  // We support fast inline cases for certain JSObject getters.
  if (access_mode == AccessMode::kLoad &&
      LookupSpecialFieldAccessor(map, name, access_info)) {
    return true;
  }

  MaybeHandle<JSObject> holder;
  do {
    // Lookup the named property on the {map}.
    Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
    int const number = descriptors->SearchWithCache(isolate(), *name, *map);
    if (number != DescriptorArray::kNotFound) {
      PropertyDetails const details = descriptors->GetDetails(number);
      if (access_mode == AccessMode::kStore ||
          access_mode == AccessMode::kStoreInLiteral) {
        // Don't bother optimizing stores to read-only properties.
        if (details.IsReadOnly()) {
          return false;
        }
        // Check for store to data property on a prototype.
        if (details.kind() == kData && !holder.is_null()) {
          // Store to property not found on the receiver but on a prototype, we
          // need to transition to a new data property.
          // Implemented according to ES6 section 9.1.9 [[Set]] (P, V, Receiver)
          return LookupTransition(receiver_map, name, holder, access_info);
        }
      }
      if (details.location() == kField) {
        if (details.kind() == kData) {
          int index = descriptors->GetFieldIndex(number);
          Representation details_representation = details.representation();
          FieldIndex field_index =
              FieldIndex::ForPropertyIndex(*map, index, details_representation);
          Type* field_type = Type::NonInternal();
          MachineRepresentation field_representation =
              MachineRepresentation::kTagged;
          MaybeHandle<Map> field_map;
          if (details_representation.IsSmi()) {
            field_type = Type::SignedSmall();
            field_representation = MachineRepresentation::kTaggedSigned;
          } else if (details_representation.IsDouble()) {
            field_type = type_cache_.kFloat64;
            field_representation = MachineRepresentation::kFloat64;
          } else if (details_representation.IsHeapObject()) {
            // Extract the field type from the property details (make sure its
            // representation is TaggedPointer to reflect the heap object case).
            field_representation = MachineRepresentation::kTaggedPointer;
            Handle<FieldType> descriptors_field_type(
                descriptors->GetFieldType(number), isolate());
            if (descriptors_field_type->IsNone()) {
              // Store is not safe if the field type was cleared.
              if (access_mode == AccessMode::kStore) return false;

              // The field type was cleared by the GC, so we don't know anything
              // about the contents now.
            } else if (descriptors_field_type->IsClass()) {
              // Add proper code dependencies in case of stable field map(s).
              Handle<Map> field_owner_map(map->FindFieldOwner(number),
                                          isolate());
              dependencies()->AssumeFieldOwner(field_owner_map);

              // Remember the field map, and try to infer a useful type.
              field_type = Type::For(descriptors_field_type->AsClass());
              field_map = descriptors_field_type->AsClass();
            }
          }
          *access_info = PropertyAccessInfo::DataField(
              details.constness(), MapHandles{receiver_map}, field_index,
              field_representation, field_type, field_map, holder);
          return true;
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          // TODO(turbofan): Add support for general accessors?
          return false;
        }

      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          DCHECK(!FLAG_track_constant_fields);
          *access_info = PropertyAccessInfo::DataConstant(
              MapHandles{receiver_map},
              handle(descriptors->GetValue(number), isolate()), holder);
          return true;
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          if (map->instance_type() == JS_MODULE_NAMESPACE_TYPE) {
            DCHECK(map->is_prototype_map());
            Handle<PrototypeInfo> proto_info =
                Map::GetOrCreatePrototypeInfo(map, isolate());
            DCHECK(proto_info->weak_cell()->IsWeakCell());
            Handle<JSModuleNamespace> module_namespace(
                JSModuleNamespace::cast(
                    WeakCell::cast(proto_info->weak_cell())->value()),
                isolate());
            Handle<Cell> cell(
                Cell::cast(module_namespace->module()->exports()->Lookup(
                    isolate(), name, Smi::ToInt(name->GetHash()))),
                isolate());
            if (cell->value()->IsTheHole(isolate())) {
              // This module has not been fully initialized yet.
              return false;
            }
            *access_info = PropertyAccessInfo::ModuleExport(
                MapHandles{receiver_map}, cell);
            return true;
          }
          Handle<Object> accessors(descriptors->GetValue(number), isolate());
          if (!accessors->IsAccessorPair()) return false;
          Handle<Object> accessor(
              access_mode == AccessMode::kLoad
                  ? Handle<AccessorPair>::cast(accessors)->getter()
                  : Handle<AccessorPair>::cast(accessors)->setter(),
              isolate());
          if (!accessor->IsJSFunction()) {
            CallOptimization optimization(accessor);
            if (!optimization.is_simple_api_call()) return false;
            if (optimization.IsCrossContextLazyAccessorPair(*native_context_,
                                                            *map)) {
              return false;
            }

            CallOptimization::HolderLookup lookup;
            holder =
                optimization.LookupHolderOfExpectedType(receiver_map, &lookup);
            if (lookup == CallOptimization::kHolderNotFound) return false;
            DCHECK_IMPLIES(lookup == CallOptimization::kHolderIsReceiver,
                           holder.is_null());
            DCHECK_IMPLIES(lookup == CallOptimization::kHolderFound,
                           !holder.is_null());
            if (V8_UNLIKELY(FLAG_runtime_stats)) return false;
          }
          if (access_mode == AccessMode::kLoad) {
            Handle<Name> cached_property_name;
            if (FunctionTemplateInfo::TryGetCachedPropertyName(isolate(),
                                                               accessor)
                    .ToHandle(&cached_property_name)) {
              if (ComputePropertyAccessInfo(map, cached_property_name,
                                            access_mode, access_info)) {
                return true;
              }
            }
          }
          *access_info = PropertyAccessInfo::AccessorConstant(
              MapHandles{receiver_map}, accessor, holder);
          return true;
        }
      }
      UNREACHABLE();
    }

    // Don't search on the prototype chain for special indices in case of
    // integer indexed exotic objects (see ES6 section 9.4.5).
    if (map->IsJSTypedArrayMap() && name->IsString() &&
        IsSpecialIndex(isolate()->unicode_cache(), String::cast(*name))) {
      return false;
    }

    // Don't search on the prototype when storing in literals
    if (access_mode == AccessMode::kStoreInLiteral) {
      return LookupTransition(receiver_map, name, holder, access_info);
    }

    // Don't lookup private symbols on the prototype chain.
    if (name->IsPrivate()) return false;

    // Walk up the prototype chain.
    if (!map->prototype()->IsJSObject()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      Handle<JSFunction> constructor;
      if (Map::GetConstructorFunction(map, native_context())
              .ToHandle(&constructor)) {
        map = handle(constructor->initial_map(), isolate());
        DCHECK(map->prototype()->IsJSObject());
      } else if (map->prototype()->IsNull(isolate())) {
        // Store to property not found on the receiver or any prototype, we need
        // to transition to a new data property.
        // Implemented according to ES6 section 9.1.9 [[Set]] (P, V, Receiver)
        if (access_mode == AccessMode::kStore) {
          return LookupTransition(receiver_map, name, holder, access_info);
        }
        // The property was not found, return undefined or throw depending
        // on the language mode of the load operation.
        // Implemented according to ES6 section 9.1.8 [[Get]] (P, Receiver)
        *access_info =
            PropertyAccessInfo::NotFound(MapHandles{receiver_map}, holder);
        return true;
      } else {
        return false;
      }
    }
    Handle<JSObject> map_prototype(JSObject::cast(map->prototype()), isolate());
    if (map_prototype->map()->is_deprecated()) {
      // Try to migrate the prototype object so we don't embed the deprecated
      // map into the optimized code.
      JSObject::TryMigrateInstance(map_prototype);
    }
    map = handle(map_prototype->map(), isolate());
    holder = map_prototype;
  } while (CanInlinePropertyAccess(map));
  return false;
}

bool AccessInfoFactory::ComputePropertyAccessInfo(
    MapHandles const& maps, Handle<Name> name, AccessMode access_mode,
    PropertyAccessInfo* access_info) {
  ZoneVector<PropertyAccessInfo> access_infos(zone());
  if (ComputePropertyAccessInfos(maps, name, access_mode, &access_infos) &&
      access_infos.size() == 1) {
    *access_info = access_infos.front();
    return true;
  }
  return false;
}

bool AccessInfoFactory::ComputePropertyAccessInfos(
    MapHandles const& maps, Handle<Name> name, AccessMode access_mode,
    ZoneVector<PropertyAccessInfo>* access_infos) {
  for (Handle<Map> map : maps) {
    if (Map::TryUpdate(map).ToHandle(&map)) {
      PropertyAccessInfo access_info;
      if (!ComputePropertyAccessInfo(map, name, access_mode, &access_info)) {
        return false;
      }
      // Try to merge the {access_info} with an existing one.
      bool merged = false;
      for (PropertyAccessInfo& other_info : *access_infos) {
        if (other_info.Merge(&access_info, access_mode, zone())) {
          merged = true;
          break;
        }
      }
      if (!merged) access_infos->push_back(access_info);
    }
  }
  return true;
}

namespace {

Maybe<ElementsKind> GeneralizeElementsKind(ElementsKind this_kind,
                                           ElementsKind that_kind) {
  if (IsHoleyOrDictionaryElementsKind(this_kind)) {
    that_kind = GetHoleyElementsKind(that_kind);
  } else if (IsHoleyOrDictionaryElementsKind(that_kind)) {
    this_kind = GetHoleyElementsKind(this_kind);
  }
  if (this_kind == that_kind) return Just(this_kind);
  if (IsDoubleElementsKind(that_kind) == IsDoubleElementsKind(this_kind)) {
    if (IsMoreGeneralElementsKindTransition(that_kind, this_kind)) {
      return Just(this_kind);
    }
    if (IsMoreGeneralElementsKindTransition(this_kind, that_kind)) {
      return Just(that_kind);
    }
  }
  return Nothing<ElementsKind>();
}

}  // namespace

bool AccessInfoFactory::ConsolidateElementLoad(MapHandles const& maps,
                                               ElementAccessInfo* access_info) {
  if (maps.empty()) return false;
  InstanceType instance_type = maps.front()->instance_type();
  ElementsKind elements_kind = maps.front()->elements_kind();
  for (Handle<Map> map : maps) {
    if (!CanInlineElementAccess(map) || map->instance_type() != instance_type) {
      return false;
    }
    if (!GeneralizeElementsKind(elements_kind, map->elements_kind())
             .To(&elements_kind)) {
      return false;
    }
  }
  *access_info = ElementAccessInfo(maps, elements_kind);
  return true;
}

bool AccessInfoFactory::LookupSpecialFieldAccessor(
    Handle<Map> map, Handle<Name> name, PropertyAccessInfo* access_info) {
  // Check for special JSObject field accessors.
  FieldIndex field_index;
  if (Accessors::IsJSObjectFieldAccessor(map, name, &field_index)) {
    Type* field_type = Type::NonInternal();
    MachineRepresentation field_representation = MachineRepresentation::kTagged;
    if (map->IsStringMap()) {
      DCHECK(Name::Equals(factory()->length_string(), name));
      // The String::length property is always a smi in the range
      // [0, String::kMaxLength].
      field_type = type_cache_.kStringLengthType;
      field_representation = MachineRepresentation::kTaggedSigned;
    } else if (map->IsJSArrayMap()) {
      DCHECK(Name::Equals(factory()->length_string(), name));
      // The JSArray::length property is a smi in the range
      // [0, FixedDoubleArray::kMaxLength] in case of fast double
      // elements, a smi in the range [0, FixedArray::kMaxLength]
      // in case of other fast elements, and [0, kMaxUInt32] in
      // case of other arrays.
      if (IsDoubleElementsKind(map->elements_kind())) {
        field_type = type_cache_.kFixedDoubleArrayLengthType;
        field_representation = MachineRepresentation::kTaggedSigned;
      } else if (IsFastElementsKind(map->elements_kind())) {
        field_type = type_cache_.kFixedArrayLengthType;
        field_representation = MachineRepresentation::kTaggedSigned;
      } else {
        field_type = type_cache_.kJSArrayLengthType;
      }
    }
    // Special fields are always mutable.
    *access_info =
        PropertyAccessInfo::DataField(kMutable, MapHandles{map}, field_index,
                                      field_representation, field_type);
    return true;
  }
  return false;
}


bool AccessInfoFactory::LookupTransition(Handle<Map> map, Handle<Name> name,
                                         MaybeHandle<JSObject> holder,
                                         PropertyAccessInfo* access_info) {
  // Check if the {map} has a data transition with the given {name}.
  Map* transition =
      TransitionsAccessor(map).SearchTransition(*name, kData, NONE);
  if (transition == nullptr) return false;

  Handle<Map> transition_map(transition);
  int const number = transition_map->LastAdded();
  PropertyDetails const details =
      transition_map->instance_descriptors()->GetDetails(number);
  // Don't bother optimizing stores to read-only properties.
  if (details.IsReadOnly()) return false;
  // TODO(bmeurer): Handle transition to data constant?
  if (details.location() != kField) return false;
  int const index = details.field_index();
  Representation details_representation = details.representation();
  FieldIndex field_index = FieldIndex::ForPropertyIndex(*transition_map, index,
                                                        details_representation);
  Type* field_type = Type::NonInternal();
  MaybeHandle<Map> field_map;
  MachineRepresentation field_representation = MachineRepresentation::kTagged;
  if (details_representation.IsSmi()) {
    field_type = Type::SignedSmall();
    field_representation = MachineRepresentation::kTaggedSigned;
  } else if (details_representation.IsDouble()) {
    field_type = type_cache_.kFloat64;
    field_representation = MachineRepresentation::kFloat64;
  } else if (details_representation.IsHeapObject()) {
    // Extract the field type from the property details (make sure its
    // representation is TaggedPointer to reflect the heap object case).
    field_representation = MachineRepresentation::kTaggedPointer;
    Handle<FieldType> descriptors_field_type(
        transition_map->instance_descriptors()->GetFieldType(number),
        isolate());
    if (descriptors_field_type->IsNone()) {
      // Store is not safe if the field type was cleared.
      return false;
    } else if (descriptors_field_type->IsClass()) {
      // Add proper code dependencies in case of stable field map(s).
      Handle<Map> field_owner_map(transition_map->FindFieldOwner(number),
                                  isolate());
      dependencies()->AssumeFieldOwner(field_owner_map);

      // Remember the field map, and try to infer a useful type.
      field_type = Type::For(descriptors_field_type->AsClass());
      field_map = descriptors_field_type->AsClass();
    }
  }
  dependencies()->AssumeMapNotDeprecated(transition_map);
  // Transitioning stores are never stores to constant fields.
  *access_info = PropertyAccessInfo::DataField(
      kMutable, MapHandles{map}, field_index, field_representation, field_type,
      field_map, holder, transition_map);
  return true;
}


Factory* AccessInfoFactory::factory() const { return isolate()->factory(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
