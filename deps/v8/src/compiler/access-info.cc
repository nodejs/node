// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "src/compiler/access-info.h"

#include "src/accessors.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/type-cache.h"
#include "src/counters.h"
#include "src/field-index-inl.h"
#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/objects-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/templates.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

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
    case AccessMode::kHas:
      return os << "Has";
  }
  UNREACHABLE();
}

ElementAccessInfo::ElementAccessInfo() = default;

ElementAccessInfo::ElementAccessInfo(MapHandles const& receiver_maps,
                                     ElementsKind elements_kind)
    : elements_kind_(elements_kind), receiver_maps_(receiver_maps) {
  CHECK(!receiver_maps.empty());
}

// static
PropertyAccessInfo PropertyAccessInfo::NotFound(MapHandles const& receiver_maps,
                                                MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(kNotFound, holder, receiver_maps);
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
    Type field_type, MaybeHandle<Map> field_map, MaybeHandle<JSObject> holder,
    MaybeHandle<Map> transition_map) {
  Kind kind =
      constness == PropertyConstness::kConst ? kDataConstantField : kDataField;
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

// static
PropertyAccessInfo PropertyAccessInfo::StringLength(
    MapHandles const& receiver_maps) {
  return PropertyAccessInfo(kStringLength, MaybeHandle<JSObject>(),
                            receiver_maps);
}

PropertyAccessInfo::PropertyAccessInfo()
    : kind_(kInvalid),
      field_representation_(MachineRepresentation::kNone),
      field_type_(Type::None()) {}

PropertyAccessInfo::PropertyAccessInfo(Kind kind, MaybeHandle<JSObject> holder,
                                       MapHandles const& receiver_maps)
    : kind_(kind),
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
    Type field_type, MaybeHandle<Map> field_map,
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
      return that->kind_ == kInvalid;

    case kDataField:
    case kDataConstantField: {
      // Check if we actually access the same field (we use the
      // GetFieldAccessStubKey method here just like the ICs do
      // since that way we only compare the relevant bits of the
      // field indices).
      if (this->field_index_.GetFieldAccessStubKey() ==
          that->field_index_.GetFieldAccessStubKey()) {
        switch (access_mode) {
          case AccessMode::kHas:
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

    case kNotFound:
    case kStringLength: {
      this->receiver_maps_.insert(this->receiver_maps_.end(),
                                  that->receiver_maps_.begin(),
                                  that->receiver_maps_.end());
      return true;
    }
    case kModuleExport:
      return false;
  }
}

Handle<Cell> PropertyAccessInfo::export_cell() const {
  DCHECK_EQ(kModuleExport, kind_);
  return Handle<Cell>::cast(constant_);
}

AccessInfoFactory::AccessInfoFactory(JSHeapBroker* broker,
                                     CompilationDependencies* dependencies,
                                     Zone* zone)
    : broker_(broker),
      dependencies_(dependencies),
      type_cache_(TypeCache::Get()),
      zone_(zone) {}

bool AccessInfoFactory::ComputeElementAccessInfo(
    Handle<Map> map, AccessMode access_mode,
    ElementAccessInfo* access_info) const {
  // Check if it is safe to inline element access for the {map}.
  MapRef map_ref(broker(), map);
  if (!CanInlineElementAccess(map_ref)) return false;
  ElementsKind const elements_kind = map_ref.elements_kind();
  *access_info = ElementAccessInfo(MapHandles{map}, elements_kind);
  return true;
}

bool AccessInfoFactory::ComputeElementAccessInfos(
    FeedbackNexus nexus, MapHandles const& maps, AccessMode access_mode,
    ZoneVector<ElementAccessInfo>* access_infos) const {
  ElementAccessFeedback const* processed =
      FLAG_concurrent_inlining
          ? broker()->GetElementAccessFeedback(FeedbackSource(nexus))
          : broker()->ProcessFeedbackMapsForElementAccess(maps);
  if (processed == nullptr) return false;

  if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
    // For polymorphic loads of similar elements kinds (i.e. all tagged or all
    // double), always use the "worst case" code without a transition.  This is
    // much faster than transitioning the elements to the worst case, trading a
    // TransitionElementsKind for a CheckMaps, avoiding mutation of the array.
    ElementAccessInfo access_info;
    if (ConsolidateElementLoad(*processed, &access_info)) {
      access_infos->push_back(access_info);
      return true;
    }
  }

  for (Handle<Map> receiver_map : processed->receiver_maps) {
    // Compute the element access information.
    ElementAccessInfo access_info;
    if (!ComputeElementAccessInfo(receiver_map, access_mode, &access_info)) {
      return false;
    }

    // Collect the possible transitions for the {receiver_map}.
    for (auto transition : processed->transitions) {
      if (transition.second.equals(receiver_map)) {
        access_info.AddTransitionSource(transition.first);
      }
    }

    // Schedule the access information.
    access_infos->push_back(access_info);
  }
  return true;
}

PropertyAccessInfo AccessInfoFactory::ComputeDataFieldAccessInfo(
    Handle<Map> receiver_map, Handle<Map> map, MaybeHandle<JSObject> holder,
    int number, AccessMode access_mode) const {
  DCHECK_NE(number, DescriptorArray::kNotFound);
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
  PropertyDetails const details = descriptors->GetDetails(number);
  int index = descriptors->GetFieldIndex(number);
  Representation details_representation = details.representation();
  if (details_representation.IsNone()) {
    // The ICs collect feedback in PREMONOMORPHIC state already,
    // but at this point the {receiver_map} might still contain
    // fields for which the representation has not yet been
    // determined by the runtime. So we need to catch this case
    // here and fall back to use the regular IC logic instead.
    return {};
  }
  FieldIndex field_index =
      FieldIndex::ForPropertyIndex(*map, index, details_representation);
  Type field_type = Type::NonInternal();
#ifdef V8_COMPRESS_POINTERS
  MachineRepresentation field_representation =
      MachineRepresentation::kCompressed;
#else
  MachineRepresentation field_representation = MachineRepresentation::kTagged;
#endif
  MaybeHandle<Map> field_map;
  MapRef map_ref(broker(), map);
  if (details_representation.IsSmi()) {
    field_type = Type::SignedSmall();
#ifdef V8_COMPRESS_POINTERS
    field_representation = MachineRepresentation::kCompressedSigned;
#else
    field_representation = MachineRepresentation::kTaggedSigned;
#endif
    map_ref.SerializeOwnDescriptors();  // TODO(neis): Remove later.
    dependencies()->DependOnFieldRepresentation(map_ref, number);
  } else if (details_representation.IsDouble()) {
    field_type = type_cache_->kFloat64;
    field_representation = MachineRepresentation::kFloat64;
  } else if (details_representation.IsHeapObject()) {
    // Extract the field type from the property details (make sure its
    // representation is TaggedPointer to reflect the heap object case).
    field_representation = MachineRepresentation::kTaggedPointer;
    Handle<FieldType> descriptors_field_type(descriptors->GetFieldType(number),
                                             isolate());
    if (descriptors_field_type->IsNone()) {
      // Store is not safe if the field type was cleared.
      if (access_mode == AccessMode::kStore) return {};

      // The field type was cleared by the GC, so we don't know anything
      // about the contents now.
    }
    map_ref.SerializeOwnDescriptors();  // TODO(neis): Remove later.
    dependencies()->DependOnFieldRepresentation(map_ref, number);
    if (descriptors_field_type->IsClass()) {
      dependencies()->DependOnFieldType(map_ref, number);
      // Remember the field map, and try to infer a useful type.
      Handle<Map> map(descriptors_field_type->AsClass(), isolate());
      field_type = Type::For(MapRef(broker(), map));
      field_map = MaybeHandle<Map>(map);
    }
  }
  return PropertyAccessInfo::DataField(
      details.constness(), MapHandles{receiver_map}, field_index,
      field_representation, field_type, field_map, holder);
}

PropertyAccessInfo AccessInfoFactory::ComputeAccessorDescriptorAccessInfo(
    Handle<Map> receiver_map, Handle<Name> name, Handle<Map> map,
    MaybeHandle<JSObject> holder, int number, AccessMode access_mode) const {
  DCHECK_NE(number, DescriptorArray::kNotFound);
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
  SLOW_DCHECK(number == descriptors->Search(*name, *map));
  if (map->instance_type() == JS_MODULE_NAMESPACE_TYPE) {
    DCHECK(map->is_prototype_map());
    Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(map->prototype_info()),
                                     isolate());
    Handle<JSModuleNamespace> module_namespace(
        JSModuleNamespace::cast(proto_info->module_namespace()), isolate());
    Handle<Cell> cell(
        Cell::cast(module_namespace->module()->exports()->Lookup(
            ReadOnlyRoots(isolate()), name, Smi::ToInt(name->GetHash()))),
        isolate());
    if (cell->value()->IsTheHole(isolate())) {
      // This module has not been fully initialized yet.
      return {};
    }
    return PropertyAccessInfo::ModuleExport(MapHandles{receiver_map}, cell);
  }
  if (access_mode == AccessMode::kHas) {
    // HasProperty checks don't call getter/setters, existence is sufficient.
    return PropertyAccessInfo::AccessorConstant(MapHandles{receiver_map},
                                                Handle<Object>(), holder);
  }
  Handle<Object> accessors(descriptors->GetStrongValue(number), isolate());
  if (!accessors->IsAccessorPair()) return {};
  Handle<Object> accessor(access_mode == AccessMode::kLoad
                              ? Handle<AccessorPair>::cast(accessors)->getter()
                              : Handle<AccessorPair>::cast(accessors)->setter(),
                          isolate());
  if (!accessor->IsJSFunction()) {
    CallOptimization optimization(isolate(), accessor);
    if (!optimization.is_simple_api_call() ||
        optimization.IsCrossContextLazyAccessorPair(
            *broker()->native_context().object(), *map)) {
      return {};
    }

    CallOptimization::HolderLookup lookup;
    holder = optimization.LookupHolderOfExpectedType(receiver_map, &lookup);
    if (lookup == CallOptimization::kHolderNotFound) return {};
    DCHECK_IMPLIES(lookup == CallOptimization::kHolderIsReceiver,
                   holder.is_null());
    DCHECK_IMPLIES(lookup == CallOptimization::kHolderFound, !holder.is_null());
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) return {};
  }
  if (access_mode == AccessMode::kLoad) {
    Handle<Name> cached_property_name;
    if (FunctionTemplateInfo::TryGetCachedPropertyName(isolate(), accessor)
            .ToHandle(&cached_property_name)) {
      PropertyAccessInfo access_info =
          ComputePropertyAccessInfo(map, cached_property_name, access_mode);
      if (!access_info.IsInvalid()) return access_info;
    }
  }
  return PropertyAccessInfo::AccessorConstant(MapHandles{receiver_map},
                                              accessor, holder);
}

PropertyAccessInfo AccessInfoFactory::ComputePropertyAccessInfo(
    Handle<Map> map, Handle<Name> name, AccessMode access_mode) const {
  CHECK(name->IsUniqueName());

  if (access_mode == AccessMode::kHas && !map->IsJSReceiverMap()) return {};

  // Check if it is safe to inline property access for the {map}.
  if (!CanInlinePropertyAccess(map)) return {};

  // We support fast inline cases for certain JSObject getters.
  if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
    PropertyAccessInfo access_info = LookupSpecialFieldAccessor(map, name);
    if (!access_info.IsInvalid()) return access_info;
  }

  // Remember the receiver map. We use {map} as loop variable.
  Handle<Map> receiver_map = map;
  MaybeHandle<JSObject> holder;
  while (true) {
    // Lookup the named property on the {map}.
    Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
    int const number = descriptors->Search(*name, *map);
    if (number != DescriptorArray::kNotFound) {
      PropertyDetails const details = descriptors->GetDetails(number);
      if (access_mode == AccessMode::kStore ||
          access_mode == AccessMode::kStoreInLiteral) {
        // Don't bother optimizing stores to read-only properties.
        if (details.IsReadOnly()) return {};
        if (details.kind() == kData && !holder.is_null()) {
          // This is a store to a property not found on the receiver but on a
          // prototype. According to ES6 section 9.1.9 [[Set]], we need to
          // create a new data property on the receiver. We can still optimize
          // if such a transition already exists.
          return LookupTransition(receiver_map, name, holder);
        }
      }
      if (details.location() == kField) {
        if (details.kind() == kData) {
          return ComputeDataFieldAccessInfo(receiver_map, map, holder, number,
                                            access_mode);
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          // TODO(turbofan): Add support for general accessors?
          return {};
        }
      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          DCHECK(!FLAG_track_constant_fields);
          return PropertyAccessInfo::DataConstant(
              MapHandles{receiver_map},
              handle(descriptors->GetStrongValue(number), isolate()), holder);
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          return ComputeAccessorDescriptorAccessInfo(
              receiver_map, name, map, holder, number, access_mode);
        }
      }
      UNREACHABLE();
    }

    // The property wasn't found on {map}. Look on the prototype if appropriate.

    // Don't search on the prototype chain for special indices in case of
    // integer indexed exotic objects (see ES6 section 9.4.5).
    if (map->IsJSTypedArrayMap() && name->IsString() &&
        IsSpecialIndex(String::cast(*name))) {
      return {};
    }

    // Don't search on the prototype when storing in literals.
    if (access_mode == AccessMode::kStoreInLiteral) {
      return LookupTransition(receiver_map, name, holder);
    }

    // Don't lookup private symbols on the prototype chain.
    if (name->IsPrivate()) return {};

    // Walk up the prototype chain.
    if (!map->prototype()->IsJSObject()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      Handle<JSFunction> constructor;
      if (Map::GetConstructorFunction(map, broker()->native_context().object())
              .ToHandle(&constructor)) {
        map = handle(constructor->initial_map(), isolate());
        DCHECK(map->prototype()->IsJSObject());
      } else if (map->prototype()->IsNull(isolate())) {
        // Store to property not found on the receiver or any prototype, we need
        // to transition to a new data property.
        // Implemented according to ES6 section 9.1.9 [[Set]] (P, V, Receiver)
        if (access_mode == AccessMode::kStore) {
          return LookupTransition(receiver_map, name, holder);
        }
        // The property was not found (access returns undefined or throws
        // depending on the language mode of the load operation.
        // Implemented according to ES6 section 9.1.8 [[Get]] (P, Receiver)
        return PropertyAccessInfo::NotFound(MapHandles{receiver_map}, holder);
      } else {
        return {};
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

    if (!CanInlinePropertyAccess(map)) return {};

    // Successful lookup on prototype chain needs to guarantee that all
    // the prototypes up to the holder have stable maps. Let us make sure
    // the prototype maps are stable here.
    CHECK(map->is_stable());
  }
}

PropertyAccessInfo AccessInfoFactory::ComputePropertyAccessInfo(
    MapHandles const& maps, Handle<Name> name, AccessMode access_mode) const {
  ZoneVector<PropertyAccessInfo> raw_access_infos(zone());
  ComputePropertyAccessInfos(maps, name, access_mode, &raw_access_infos);
  ZoneVector<PropertyAccessInfo> access_infos(zone());
  if (FinalizePropertyAccessInfos(raw_access_infos, access_mode,
                                  &access_infos) &&
      access_infos.size() == 1) {
    return access_infos.front();
  }
  return {};
}

void AccessInfoFactory::ComputePropertyAccessInfos(
    MapHandles const& maps, Handle<Name> name, AccessMode access_mode,
    ZoneVector<PropertyAccessInfo>* access_infos) const {
  DCHECK(access_infos->empty());
  for (Handle<Map> map : maps) {
    access_infos->push_back(ComputePropertyAccessInfo(map, name, access_mode));
  }
}

bool AccessInfoFactory::FinalizePropertyAccessInfos(
    ZoneVector<PropertyAccessInfo> infos, AccessMode access_mode,
    ZoneVector<PropertyAccessInfo>* result) const {
  DCHECK(result->empty());
  for (auto it = infos.begin(), end = infos.end(); it != end; ++it) {
    bool merged = false;
    for (auto ot = it + 1; ot != end; ++ot) {
      if (ot->Merge(&(*it), access_mode, zone())) {
        merged = true;
        break;
      }
    }
    if (it->IsInvalid()) return false;
    if (!merged) result->push_back(*it);
  }
  CHECK(!result->empty());
  return true;
}

namespace {

Maybe<ElementsKind> GeneralizeElementsKind(ElementsKind this_kind,
                                           ElementsKind that_kind) {
  if (IsHoleyElementsKind(this_kind)) {
    that_kind = GetHoleyElementsKind(that_kind);
  } else if (IsHoleyElementsKind(that_kind)) {
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

bool AccessInfoFactory::ConsolidateElementLoad(
    ElementAccessFeedback const& processed,
    ElementAccessInfo* access_info) const {
  ElementAccessFeedback::MapIterator it = processed.all_maps(broker());
  MapRef first_map = it.current();
  InstanceType instance_type = first_map.instance_type();
  ElementsKind elements_kind = first_map.elements_kind();
  MapHandles maps;
  for (; !it.done(); it.advance()) {
    MapRef map = it.current();
    if (map.instance_type() != instance_type || !CanInlineElementAccess(map)) {
      return false;
    }
    if (!GeneralizeElementsKind(elements_kind, map.elements_kind())
             .To(&elements_kind)) {
      return false;
    }
    maps.push_back(map.object());
  }

  *access_info = ElementAccessInfo(maps, elements_kind);
  return true;
}

PropertyAccessInfo AccessInfoFactory::LookupSpecialFieldAccessor(
    Handle<Map> map, Handle<Name> name) const {
  // Check for String::length field accessor.
  if (map->IsStringMap()) {
    if (Name::Equals(isolate(), name, isolate()->factory()->length_string())) {
      return PropertyAccessInfo::StringLength(MapHandles{map});
    }
    return {};
  }
  // Check for special JSObject field accessors.
  FieldIndex field_index;
  if (Accessors::IsJSObjectFieldAccessor(isolate(), map, name, &field_index)) {
    Type field_type = Type::NonInternal();
    MachineRepresentation field_representation = MachineRepresentation::kTagged;
    if (map->IsJSArrayMap()) {
      DCHECK(
          Name::Equals(isolate(), isolate()->factory()->length_string(), name));
      // The JSArray::length property is a smi in the range
      // [0, FixedDoubleArray::kMaxLength] in case of fast double
      // elements, a smi in the range [0, FixedArray::kMaxLength]
      // in case of other fast elements, and [0, kMaxUInt32] in
      // case of other arrays.
      if (IsDoubleElementsKind(map->elements_kind())) {
        field_type = type_cache_->kFixedDoubleArrayLengthType;
        field_representation = MachineRepresentation::kTaggedSigned;
      } else if (IsFastElementsKind(map->elements_kind())) {
        field_type = type_cache_->kFixedArrayLengthType;
        field_representation = MachineRepresentation::kTaggedSigned;
      } else {
        field_type = type_cache_->kJSArrayLengthType;
      }
    }
    // Special fields are always mutable.
    return PropertyAccessInfo::DataField(PropertyConstness::kMutable,
                                         MapHandles{map}, field_index,
                                         field_representation, field_type);
  }
  return {};
}

PropertyAccessInfo AccessInfoFactory::LookupTransition(
    Handle<Map> map, Handle<Name> name, MaybeHandle<JSObject> holder) const {
  // Check if the {map} has a data transition with the given {name}.
  Map transition =
      TransitionsAccessor(isolate(), map).SearchTransition(*name, kData, NONE);
  if (transition.is_null()) return {};

  Handle<Map> transition_map(transition, isolate());
  int const number = transition_map->LastAdded();
  PropertyDetails const details =
      transition_map->instance_descriptors()->GetDetails(number);
  // Don't bother optimizing stores to read-only properties.
  if (details.IsReadOnly()) return {};
  // TODO(bmeurer): Handle transition to data constant?
  if (details.location() != kField) return {};
  int const index = details.field_index();
  Representation details_representation = details.representation();
  FieldIndex field_index = FieldIndex::ForPropertyIndex(*transition_map, index,
                                                        details_representation);
  Type field_type = Type::NonInternal();
  MaybeHandle<Map> field_map;
  MachineRepresentation field_representation = MachineRepresentation::kTagged;
  MapRef transition_map_ref(broker(), transition_map);
  if (details_representation.IsSmi()) {
    field_type = Type::SignedSmall();
    field_representation = MachineRepresentation::kTaggedSigned;
    transition_map_ref.SerializeOwnDescriptors();  // TODO(neis): Remove later.
    dependencies()->DependOnFieldRepresentation(transition_map_ref, number);
  } else if (details_representation.IsDouble()) {
    field_type = type_cache_->kFloat64;
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
      return {};
    }
    transition_map_ref.SerializeOwnDescriptors();  // TODO(neis): Remove later.
    dependencies()->DependOnFieldRepresentation(transition_map_ref, number);
    if (descriptors_field_type->IsClass()) {
      dependencies()->DependOnFieldType(transition_map_ref, number);
      // Remember the field map, and try to infer a useful type.
      Handle<Map> map(descriptors_field_type->AsClass(), isolate());
      field_type = Type::For(MapRef(broker(), map));
      field_map = MaybeHandle<Map>(map);
    }
  }
  dependencies()->DependOnTransition(MapRef(broker(), transition_map));
  // Transitioning stores are never stores to constant fields.
  return PropertyAccessInfo::DataField(
      PropertyConstness::kMutable, MapHandles{map}, field_index,
      field_representation, field_type, field_map, holder, transition_map);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
